
#include <RFM-6x-Weather.h>

/*
* Function taken from Luc Small (http://lucsmall.com), itself
* derived from the OneWire Arduino library. Modifications to
* the polynomial according to Fine Offset's CRC8 calulations.
*/
uint8_t RFM6xWeather::_crc8( uint8_t *addr, uint8_t len)
{
	uint8_t crc = 0;

	// Indicated changes are from reference CRC-8 function in OneWire library
	while (len--) {
		uint8_t inbyte = *addr++;
		uint8_t i;
		for (i = 8; i; i--) {
			uint8_t mix = (crc ^ inbyte) & 0x80; // changed from & 0x01
			crc <<= 1; // changed from right shift
			if (mix) crc ^= 0x31;// changed from 0x8C;
			inbyte <<= 1; // changed from right shift
		}
	}
	return crc;
}

bool RFM6xWeather::CRC_ok(uint8_t buffer[RFM6xW_PACKET_LEN])
{
  bool match;

  match = _crc8(buffer, RFM6xW_PACKET_LEN-1) == buffer[RFM6xW_PACKET_LEN-1];
  if (!match) {
    Serial.println("Non-matching CRC");
  };
  return match;
}


// Override of the existing readFifo
//
// The existing function is hard-coded for variable packet length, and
// I want fixed packet length, and am not using in-payload addressing (only the 2dd4)
//
//TODO: why is all the SPI stuff hard coded here? 
void RFM6xWeather::readFifo()
{
    ATOMIC_BLOCK_START;
    digitalWrite(_slaveSelectPin, LOW); // TODO: is this required? Does SPI not dot this?
    _spi.transfer(RH_RF69_REG_00_FIFO); // Send the start address with the write mask off
    uint8_t payloadlen = RFM6xW_PACKET_LEN-RFM6xW_HEADER_LEN; // Use fixed packet length
    // Get the rest of the headers

    //_rxHeaderId    = _spi.transfer(0);  //Packet type in Finite Offset weather stations

    // And now the real payload
    for (_bufLen = 0; _bufLen < (payloadlen); _bufLen++)
      _buf[_bufLen] = _spi.transfer(0);
    if(CRC_ok(_buf)){
      _rxGood++;
      _rxBufValid = true;
    }

    digitalWrite(_slaveSelectPin, HIGH);
    ATOMIC_BLOCK_END;
    // Any junk remaining in the FIFO will be cleared next time we go to receive mode.
}



bool RFM6xWeather::init()
{
  Serial.println("init");
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
    // Get the interrupt cause
    uint8_t irqflags2 = spiRead(RH_RF69_REG_28_IRQFLAGS2);
    if (_mode == RHModeTx && (irqflags2 & RH_RF69_IRQFLAGS2_PACKETSENT))
    {
	// A transmitter message has been fully sent
	setModeIdle(); // Clears FIFO
	_txGood++;
    }
    // Must look for PAYLOADREADY, not CRCOK, since only PAYLOADREADY occurs _after_ AES decryption
    // has been done
    if (_mode == RHModeRx && (irqflags2 & RH_RF69_IRQFLAGS2_PAYLOADREADY))
    {
	// A complete message has been received 
	_lastRssi = -((int8_t)(spiRead(RH_RF69_REG_24_RSSIVALUE) >> 1));
	_lastPreambleTime = millis();

	setModeIdle();
	// Save it in our buffer
	readFifo();
    }
}
