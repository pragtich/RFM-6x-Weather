
#include <RFM-6x-Weather.h>

// Override of the existing readFifo
//
// The existing function is hard-coded for variable packet length, and
// I want fixed packet length, and am not using in-payload addressing (only the 2dd4)
//
//TODO: why is all the SPI stuff hard coded here? 
void RFM6xWeather::readFifo()
{
  digitalWrite(2, LOW);
  //Serial.println("reading FIFO");
    ATOMIC_BLOCK_START;
    digitalWrite(_slaveSelectPin, LOW); // TODO: is this required? Does SPI not dot this?
    _spi.transfer(RH_RF69_REG_00_FIFO); // Send the start address with the write mask off
    uint8_t payloadlen = RFM6xW_PACKET_LEN-RFM6xW_HEADER_LEN; // Use fixed packet length
    // Get the rest of the headers

    _rxHeaderId    = _spi.transfer(0);  //Packet type in Finite Offset weather stations

    // And now the real payload
    for (_bufLen = 0; _bufLen < (payloadlen); _bufLen++)
      _buf[_bufLen] = _spi.transfer(0);
    _rxGood++;
    _rxBufValid = true;
    Serial.println("Received buffer");
    Serial.println(_bufLen);
   
    digitalWrite(_slaveSelectPin, HIGH);
    ATOMIC_BLOCK_END;
    // Any junk remaining in the FIFO will be cleared next time we go to receive mode.
}



bool RFM6xWeather::init()
{
  if (!RH_RF69::init())
    return false;

  const ModemConfig config = {0x00, 0x07, 0x44, 0x0, 0x0, 1<<4|2|1<<6, 0x8b, 1<<3};
  const uint8_t syncwords[3] = {0xaa, 0x2d, 0xd4};
  
  RH_RF69::setModemRegisters(&config);
  spiWrite(RH_RF69_REG_38_PAYLOADLENGTH, RFM6xW_PACKET_LEN);
  RH_RF69::setFrequency(868.3);
  RH_RF69::setSyncWords(syncwords, sizeof(syncwords));

  _deviceForInterrupt[0] = this;
  
  return true;
}

void RFM6xWeather::handleInterrupt()
{
  digitalWrite(2, LOW);
  //delay(300);
    // Get the interrupt cause
    uint8_t irqflags2 = spiRead(RH_RF69_REG_28_IRQFLAGS2);
    if (_mode == RHModeTx && (irqflags2 & RH_RF69_IRQFLAGS2_PACKETSENT))
    {
	// A transmitter message has been fully sent
	setModeIdle(); // Clears FIFO
	_txGood++;
//	Serial.println("PACKETSENT");
    }
    // Must look for PAYLOADREADY, not CRCOK, since only PAYLOADREADY occurs _after_ AES decryption
    // has been done
    if (_mode == RHModeRx && (irqflags2 & RH_RF69_IRQFLAGS2_PAYLOADREADY))
    {
	// A complete message has been received with good CRC
	_lastRssi = -((int8_t)(spiRead(RH_RF69_REG_24_RSSIVALUE) >> 1));
	_lastPreambleTime = millis();

	setModeIdle();
	// Save it in our buffer
	readFifo();
	//Serial.println("PAYLOADREADY");
	digitalWrite(2, LOW);

    }
    digitalWrite(2, HIGH);

}
