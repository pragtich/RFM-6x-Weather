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

void observed_w(struct RFM6xWeather::WeatherMessage *obs) {
  Serial.println("Observed weather:");
  
  PRINT_WITH_UNIT("ID:", obs->ID);

  PRINT_WITH_UNIT(obs->temp, " ℃");
  PRINT_WITH_UNIT(obs->RH, " %");
  PRINT_WITH_UNIT(obs->rain, " mm");
  delete obs;
}

void observed_t(struct RFM6xWeather::TimeMessage *obs) {
  Serial.println("Got time stamp:");
  
  PRINT_WITH_UNIT("ID:", obs->ID);

  Serial.printf("02d:02d on 02d-02d-04d\n", obs->hour, obs->minute, obs->day, obs->month, obs->year);
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

}



void loop(){

  if (rfm.available()){
    n = sizeof(buffer);
    if(rfm.recv(buffer, &n)) {
      //      PrintHex8(buffer, n);
      Serial.println();
    } else {
      Serial.println("Problem reading packet");
    }
  } else {
    yield();
  }
  delay(1000);
}
