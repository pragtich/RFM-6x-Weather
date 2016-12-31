// Updates to RFM69 module specific to weather stations reception
// Intended for use with Alecto WS3000 and RFM65 receiver, but should work on RFM6x family and several Fine Offset type weather stations
//
//


#ifndef RFM6xWEATHER_h
#define RFM6xWEATHER_h

#include <RH_RF69.h>


// TODO: come up with a smarter way to choose packet length and handle multiple lengths
// Somehow choose the longest packet length and use the CRC or first byte to
// identify any incoming packets.
#define RFM6xW_PACKET_LEN 9
#define RFM6xW_HEADER_LEN 0

namespace RFM6xWeather {
  class Observation {
  public:
    Observation(uint8_t buffer[RFM6xW_PACKET_LEN]);
    uint8_t ID = 0;
    float temp = 0.0;
    float wind = 0.0;
    float gust = 0.0;
    uint8_t RH = 0;
    float rain = 0.0;
  };


class Receiver : public RH_RF69
{
 public:
 Receiver(uint8_t slaveSelectPin = SS, uint8_t interruptPin = 2, RHGenericSPI& spi = hardware_spi)
   : RH_RF69(slaveSelectPin, interruptPin, spi)
    {
    };
  
  bool init() override;
  void handleInterrupt() override;
  
  // protected:
  void readFifo() override;

 protected:
  uint8_t _crc8( uint8_t *addr, uint8_t len);
  bool CRC_ok(uint8_t buffer[RFM6xW_PACKET_LEN]);
};


};

#endif
