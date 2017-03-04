#include <ESP8266WiFi.h>
#include <ESP8266WiFiSTA.h>
#include <PubSubClient.h>

#include <RFM-6x-Weather.h>

#include "secret.h" 		// Contains the stuff that I don't want on github, SSID and PASS

//D1 = GPIO5
#define RFM_INT 5
#define RFM_CS 15
#define LEDPIN 2

const char* mqtt_server = "10.0.0.99";
const char* weather_topic = "weather/";
#define TOPIC_LEN 99
char topic[TOPIC_LEN];

#define PRINT_WITH_UNIT(a, b) {\
    Serial.print(a); \
    Serial.println(b);\
  }


RFM6xWeather::Receiver rfm(15, RFM_INT, hardware_spi);
ESP8266WiFiSTAClass Station;
WiFiClient espClient;
PubSubClient mqtt(espClient);

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

  String topic = String("weather/") + String(obs->ID, HEX) + String("/");
  String topic_T = topic + String("T");
  String msg_T = String(obs->temp);
  mqtt.publish(topic_T.c_str(), msg_T.c_str());
  
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

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Switch on the LED if an 1 was received as first character
  if ((char)payload[0] == '1') {
    digitalWrite(LEDPIN, LOW);   // Turn the LED on (Note that LOW is the voltage level
    // but actually the LED is on; this is because
    // it is acive low on the ESP-01)
  } else {
    digitalWrite(LEDPIN, HIGH);  // Turn the LED off by making the voltage HIGH
  }

}

void reconnect() {
  // Loop until we're reconnected
  while (!mqtt.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (mqtt.connect(clientId.c_str())) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      mqtt.publish("outTopic", "hello world");
      // ... and resubscribe
      mqtt.subscribe("inTopic");
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqtt.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup()
{
  Serial.begin(115200);
  pinMode(LEDPIN, OUTPUT);
  digitalWrite(LEDPIN, HIGH);

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

   mqtt.setServer(mqtt_server, 1883);
   mqtt.setCallback(mqtt_callback);
   mqtt.publish("outTopic", "hello world");
   // ... and resubscribe
   mqtt.subscribe("inTopic");
}



void loop(){
  rfm.run();
  if (!mqtt.connected()) {
    reconnect();
  }
  mqtt.loop();
}
