
#include <RFM-6x-Weather.h>
#include <stdint.h>

int RFM6xWeather::BCD2int(uint16_t b){
  return 10*((b & 0xf0) >> 4)+ (b & 0x0f);
}

// https://forum.arduino.cc/index.php?topic=38107.0
void RFM6xWeather::PrintHex8(uint8_t *data, uint8_t length) // prints 8-bit data in hex with leading zeroes
{
       Serial.print("0x");
       for (int i=0; i<length; i++) {
         if (data[i]<0x10) {Serial.print("0");}
         Serial.print(data[i],HEX);
         Serial.print(" ");
       }
}

bool RFM6xWeather::CRC_ok(uint8_t buffer[RFM6xW_PACKET_LEN], uint8_t len)
{
  bool match;

  match = _crc8(buffer, len-1) == buffer[len-1];
  return match;
}

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

  
// Override of the existing readFifo
//
// The existing function is hard-coded for variable packet length, and
// I want fixed packet length, and am not using in-payload addressing (only the 2dd4)
//
//TODO: why is all the SPI stuff hard coded here? 
 void RFM6xWeather::Receiver::readFifo()
{
    ATOMIC_BLOCK_START;
    digitalWrite(_slaveSelectPin, LOW); // TODO: is this required? Does SPI not dot this?
    _spi.transfer(RH_RF69_REG_00_FIFO); // Send the start address with the write mask off
    uint8_t payloadlen = RFM6xW_PACKET_LEN-RFM6xW_HEADER_LEN; // Use fixed packet length

    /* Now read the entire buffer */
    for (_bufLen = 0; _bufLen < (payloadlen); _bufLen++)
      _buf[_bufLen] = _spi.transfer(0);

    if (decode_message(_buf, &the_message)){
      if (the_message.type == WEATHER){
	if (callback_weather){
	  (*callback_weather)(the_message.pmessage.w);
	}
      } else if (the_message.type == TIME){
	if (callback_time){
	  (*callback_time)(the_message.pmessage.t);
	}
      } 
      
    } else {
      if (the_message.type == UNKNOWN){
	if (callback_unknown){
	  (*callback_unknown)(the_message.pmessage.u);
	}
      }
    }
    the_message.type = NONE;
    
    digitalWrite(_slaveSelectPin, HIGH);
    ATOMIC_BLOCK_END;
    // Any junk remaining in the FIFO will be cleared next time we go to receive mode.
}

 bool RFM6xWeather::Receiver::init()
{
  if (!RH_RF69::init())
    return false;

  the_message.type = NONE;
  
  const ModemConfig config = {0x00, 0x07, 0x44, 0x0, 0x0, 1<<4|2|1<<6, 0x8b, 1<<3};
  const uint8_t syncwords[3] = {0xaa, 0x2d, 0xd4};
  
  RH_RF69::setModemRegisters(&config);
  spiWrite(RH_RF69_REG_38_PAYLOADLENGTH, RFM6xW_PACKET_LEN);
  RH_RF69::setFrequency(868.3);
  RH_RF69::setSyncWords(syncwords, sizeof(syncwords));

  _deviceForInterrupt[0] = this;
  
  return true;
}

 void RFM6xWeather::Receiver::handleInterrupt()
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


void RFM6xWeather::Receiver::set_time_handler(void (*handler)(struct TimeMessage*)){
  if (handler){
    callback_time = handler;
  }
}


void RFM6xWeather::Receiver::set_weather_handler(void (*handler)(struct WeatherMessage*)){
  if (handler){
    callback_weather = handler;
  }
}

void RFM6xWeather::Receiver::set_unknown_handler(void (*handler)(struct UnknownMessage*)){
  if (handler){
    callback_unknown = handler;
  }
}

/*
DCF Time Message Format: 
This is for Wh1080; WS-3000 is 1 byte shorter
AAAABBBB BBBBCCCC DDEEEEEE FFFFFFFF GGGGGGGG HHHHHHHH IIIJJJJJ KKKKKKKK LMMMMMMM NNNNNNNN
0        1        2        3        4        5        6        7        8        9
Hours Minutes Seconds Year       MonthDay      ?      Checksum
0xB4    0xFA    0x59    0x06    0x42    0x13    0x43    0x02    0x45    0x74

with:
AAAA = 1011    Message type: 0xB: DCF77 time stamp
BBBBBBBB       Station ID / rolling code: Changes with battery insertion.
CCCC           Unknown
DD             Unknown
EEEEEE         Hours, BCD
FFFFFFFF       Minutes, BCD
GGGGGGGG       Seconds, BCD
HHHHHHHH       Year, last two digits, BCD
III            Unknown
JJJJJ          Month number, BCD
KKKKKKKK       Day in month, BCD
L              Unknown status bit
MMMMMMM        Unknown
NNNNNNNN       CRC8 - reverse Dallas One-wire CRC
*/
bool RFM6xWeather::Receiver::decode_message(uint8_t buffer[RFM6xW_PACKET_LEN], struct message *msg){
  switch (buffer[0]&0xF0) {
  case 0x50:

    if (CRC_ok(buffer, 9)){
      msg->type = WEATHER;
      msg->pmessage.w = new struct WeatherMessage;

      uint16_t bintemp;
  
      msg->pmessage.w->ID = (buffer[0]&0x0f)<<4 | (buffer[1]&0xf0)>>4;
      bintemp = ((buffer[1]&0x07)<<8 | buffer[2]);
    
      msg->pmessage.w->temp = bintemp / 10.0;
      if (buffer[1]&0x08) {
	msg->pmessage.w->temp = -msg->pmessage.w->temp;
      }
      msg->pmessage.w->RH = buffer[3];
      msg->pmessage.w->wind = buffer[4] * 0.34; //Or is it 1.22?
      msg->pmessage.w->gust = buffer[5] * 0.34;
      msg->pmessage.w->rain = ((buffer[6]<<8 | buffer[7]) - 0x030c) * 0.03;

      return true;
    }
    break;
  case 0x60:
    if (CRC_ok(buffer, 9)){
      msg->type = TIME;
      msg->pmessage.t = new struct TimeMessage;

      msg->pmessage.t->ID = (buffer[0]&0x0f)<<4 | (buffer[1]&0xf0)>>4;
      msg->pmessage.t->year = BCD2int(buffer[5]) + 2000;
      msg->pmessage.t->month = BCD2int(buffer[6] & 0x1f);
      msg->pmessage.t->day = BCD2int(buffer[7]);
      msg->pmessage.t->hour = BCD2int(buffer[2] & 0x3F);
      msg->pmessage.t->minute = BCD2int(buffer[3]);
      msg->pmessage.t->second = BCD2int(buffer[4]);

      return true;
    }
    break;
  }

  msg->type = UNKNOWN;
  msg->pmessage.u->ID =  (buffer[0]&0x0f)<<4 | (buffer[1]&0xf0)>>4;
  memcpy(msg->pmessage.u->message, buffer, RFM6xW_PACKET_LEN);
  return false;
}


