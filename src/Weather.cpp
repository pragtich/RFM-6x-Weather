#include <ESP8266WiFi.h>
#include <ESP8266WiFiSTA.h>

#include <RFM-6x-Weather.h>

#include "secret.h" 		// Contains the stuff that I don't want on github

//D1 = GPIO5
#define RFM_INT 5
#define RFM_CS 15
#define LEDPIN 2

#define PRINT_WITH_UNIT(a, b) {\
    Serial.print(a); \
    Serial.println(b);\
  }


RFM6xWeather::Receiver rfm(15, RFM_INT, hardware_spi);
ESP8266WiFiSTAClass Station;

uint8_t buffer[RFM6xW_PACKET_LEN], n;

/*
void myisr(void){
  rfm.handleInterrupt();
}
*/

void print_date(RFM6xWeather::TimeMessage *obs){
  Serial.print(obs->hour);
  Serial.print(":");
  Serial.print(obs->minute);
  Serial.print(" ");
  Serial.print(obs->year);
  Serial.print("-");
  Serial.print(obs->month);
  Serial.print("-");
  Serial.println(obs->day);
}

void observed_w(struct RFM6xWeather::WeatherMessage *obs) {
  Serial.print("Observed weather at t=");
  Serial.println(millis());
  
  PRINT_WITH_UNIT("ID:", obs->ID);

  PRINT_WITH_UNIT(obs->temp, " ℃");
  PRINT_WITH_UNIT(obs->RH, " %");
  PRINT_WITH_UNIT(obs->rain, " mm");
  delete obs;
}

void observed_t(struct RFM6xWeather::TimeMessage *obs) {
  Serial.println("Got time stamp:");
  
  PRINT_WITH_UNIT("ID:", obs->ID);

  print_date(obs);
  delete obs;
}

void observed_u(struct RFM6xWeather::UnknownMessage *obs) {
  Serial.println("Got Unknown message:");
  
  PRINT_WITH_UNIT("ID:", obs->ID);

  RFM6xWeather::PrintHex8(obs->message, RFM6xW_PACKET_LEN);
  Serial.println();
  delete obs;
}

void setup()
{
  Serial.begin(115200);

  if(!WiFi.enableAP(false)) {
    Serial.println("Error disabling AP mode");
  }
  Station.begin(SSID, PASS);
  Serial.print("Connecting WiFi");
  while (Station.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  Serial.print("Connected, IP address: ");
  Serial.println(Station.localIP());


  if (!rfm.init())
     Serial.println("Error initializing rfm");
   else
    Serial.println("RFM initialized OK");
   rfm.set_weather_handler(observed_w);
   rfm.set_time_handler(observed_t);
   rfm.set_unknown_handler(observed_u);

}



void loop(){
  rfm.run();
}
