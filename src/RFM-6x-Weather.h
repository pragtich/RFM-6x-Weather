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
#define RFM6xW_PACKET_LEN 9
#define RFM6xW_HEADER_LEN 0




namespace RFM6xWeather {

  /* Helper functions */
  int BCD2int(uint16_t b);
  void PrintHex8(uint8_t *data, uint8_t length);
  bool CRC_ok(uint8_t buffer[RFM6xW_PACKET_LEN], uint8_t len);
  uint8_t _crc8( uint8_t *addr, uint8_t len);


  struct WeatherMessage {
    uint8_t ID = 0;
    float temp = 0.0;
    float wind = 0.0;
    float gust = 0.0;
    uint8_t RH = 0;
    float rain = 0.0;

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

  enum message_t {NONE, WEATHER, TIME};
  union message_p {
    struct WeatherMessage *w;
    struct TimeMessage *t;
  };
  
  struct message {
    enum message_t type;
    union message_p pmessage;
  };
  /* Class Message */
  /*  class testMessage {
  public:
    static Message* make_message(uint8_t *buffer);
    uint8_t ID = 0;

    virtual ~Message(){};
  private:
    static uint8_t _crc8( uint8_t *addr, uint8_t len);
    static bool CRC_ok(uint8_t buffer[RFM6xW_PACKET_LEN], uint8_t len);
    };*/

  /* Class Observation */
  /*  class Observation: public Message {
  public:
    Observation(uint8_t buffer[RFM6xW_PACKET_LEN]);
    float temp = 0.0;
    float wind = 0.0;
    float gust = 0.0;
    uint8_t RH = 0;
    float rain = 0.0;
  };
  */
  /* Class Receiver */
  
class Receiver : public RH_RF69
{
 public:
 Receiver(uint8_t slaveSelectPin = SS, uint8_t interruptPin = 2, RHGenericSPI& spi = hardware_spi)
   : RH_RF69(slaveSelectPin, interruptPin, spi)
    {
    };
  
  bool init() override;
  void handleInterrupt() override;
  void readFifo() override;

  void set_time_handler(void (*handler)(struct TimeMessage*));
  void set_weather_handler(void (*handler)(struct WeatherMessage*));
  
 protected:
  void (*callback_weather)(WeatherMessage*) = NULL;
  void (*callback_time)(TimeMessage*) = NULL;
  struct message the_message;

  bool decode_message(uint8_t buffer[RFM6xW_PACKET_LEN], struct message *msg);
};


};

#endif
