// Updates to RFM69 module specific to weather stations reception
// Intended for use with Alecto WS3000 and RFM65 receiver, but should work on RFM6x family and several Fine Offset type weather stations
//
//


#ifndef RFM6xWEATHER_h
#define RFM6xWEATHER_h

#include <RH_RF69.h>


/* Constants */

// TODO: come up with a smarter way to choose packet length and handle multiple lengths
// Somehow choose the longest packet length and use the CRC or first byte to
// identify any incoming packets.
#define RFM6xW_PACKET_LEN 10
#define RFM6xW_HEADER_LEN 0




namespace RFM6xWeather {

  /* Helper functions */
  int BCD2int(uint16_t b);
  void PrintHex8(uint8_t *data, uint8_t length);
  bool CRC_ok(uint8_t buffer[RFM6xW_PACKET_LEN], uint8_t len);
  uint8_t _crc8( uint8_t *addr, uint8_t len);


  struct UnknownMessage {
    uint8_t ID = 0;
    uint8_t message[RFM6xW_PACKET_LEN];
  };


  struct WeatherMessage {
    uint8_t ID = 0;
    float temp = 0.0;
    float wind = 0.0;
    float gust = 0.0;
    uint8_t RH = 0;
    float rain = 0.0;
#ifdef RAW_PACKAGES
    uint8_t rawmsg[RFM6xW_PACKET_LEN];
#endif
  };

  struct TimeMessage {
    uint8_t ID = 0;
    int year = 0;
    uint8_t month = 0;
    uint8_t day = 0;
    uint8_t hour = 0;
    uint8_t minute = 0;
    uint8_t second = 0;
  };

  enum message_t {NONE, WEATHER, TIME, UNKNOWN};

  union message_p {
    struct WeatherMessage *w;
    struct TimeMessage *t;
    struct UnknownMessage *u;
  };

  struct message {
    enum message_t type;
    union message_p pmessage;
  };
  /* Class Receiver */

class Receiver : public RH_RF69
{
 public:
 Receiver(uint8_t slaveSelectPin = SS, uint8_t interruptPin = 2, RHGenericSPI& spi = hardware_spi)
   : RH_RF69(slaveSelectPin, interruptPin, spi)
    {
    };
  void run(void);

  bool init() override;
  void handleInterrupt() override;
  void readFifo() override;

  void set_time_handler(void (*handler)(struct TimeMessage*));
  void set_weather_handler(void (*handler)(struct WeatherMessage*));
  void set_unknown_handler(void (*handler)(struct UnknownMessage*));

  void set_min_time(int time);

 protected:
  void (*callback_weather)(WeatherMessage*) = NULL;
  void (*callback_time)(TimeMessage*) = NULL;
  void (*callback_unknown)(UnknownMessage*) = NULL;
  struct message the_message;
  int mintime = 100;		/* minimal time between messages from same ID (ms) */
  uint8_t _lastID = 0;
  int _prelastPreambleTime = 0;

  bool decode_message(uint8_t buffer[RFM6xW_PACKET_LEN], struct message *msg);
};


};

#endif
