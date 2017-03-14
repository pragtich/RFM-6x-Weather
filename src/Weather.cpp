#define RAW_PACKAGES 1

#include <ESP8266WiFi.h>
#include <ESP8266WiFiSTA.h>
#include <PubSubClient.h>
#include <RingBuf.h>

#include <RFM-6x-Weather.h>

#include "secret.h" 		// Contains the stuff that I don't want on github, SSID and PASS

//D1 = GPIO5
#define RFM_INT 5
#define RFM_CS 15
#define LEDPIN 2

const char* mqtt_server = "10.0.0.99";
const char* weather_topic = "weather";


#define MAX_MQTT_LEN 100
typedef struct {
  char topic[MAX_MQTT_LEN];
  char message[MAX_MQTT_LEN];
} mqtt_msg;


#define PRINT_WITH_UNIT(a, b) {\
    Serial.print(a); \
    Serial.println(b);\
  }


RFM6xWeather::Receiver rfm(15, RFM_INT, hardware_spi);
ESP8266WiFiSTAClass Station;
WiFiClient espClient;
PubSubClient mqtt(espClient);

uint8_t buffer[RFM6xW_PACKET_LEN], n;
RingBuf *observations_w = RingBuf_new(sizeof(RFM6xWeather::WeatherMessage), 10);
RingBuf *mqtt_msgs = RingBuf_new(sizeof(mqtt_msg), 20);

void process_messages() {
  /* I am assuming only weather observations in the queue! */
  if (!observations_w->isEmpty(observations_w)){
    RFM6xWeather::WeatherMessage obs;

    if (observations_w->pull(observations_w, &obs)){
      // do stuff with my message
      Serial.print("Observed weather at t=");
      Serial.println(millis());

      #ifdef RAW_PACKAGES
      for (int i=0; i < RFM6xW_PACKET_LEN; i++)
        {
          Serial.print(obs.rawmsg[i], HEX);
          Serial.print(" ");
        };
        Serial.println();
      #endif


      PRINT_WITH_UNIT("ID:", obs.ID);

      PRINT_WITH_UNIT(obs.temp, " ℃");
      PRINT_WITH_UNIT(obs.RH, " %");
      PRINT_WITH_UNIT(obs.rain, " mm");
      PRINT_WITH_UNIT(obs.wind, " m/s");
      PRINT_WITH_UNIT(obs.gust, " gust m/s");

      mqtt_msg msg;

      /* Temperature message */
      sprintf(msg.topic, "%s/%s/T", weather_topic, String(obs.ID, HEX).c_str());
      strcpy(msg.message, String(obs.temp).c_str());
      mqtt_msgs->add(mqtt_msgs, &msg);

      /* RH message */
      sprintf(msg.topic, "%s/%s/RH", weather_topic, String(obs.ID, HEX).c_str());
      strcpy(msg.message, String(obs.RH).c_str());
      mqtt_msgs->add(mqtt_msgs, &msg);

      /* Rain message */
      sprintf(msg.topic, "%s/%s/Rain", weather_topic, String(obs.ID, HEX).c_str());
      strcpy(msg.message, String(obs.rain).c_str());
      mqtt_msgs->add(mqtt_msgs, &msg);

      /* RH message */
      sprintf(msg.topic, "%s/%s/Wind", weather_topic, String(obs.ID, HEX).c_str());
      strcpy(msg.message, String(obs.wind).c_str());
      mqtt_msgs->add(mqtt_msgs, &msg);

      #ifdef RAW_PACKAGES
      /* Raw packet */
      sprintf(msg.topic, "%s/%s/RAW", weather_topic, String(obs.ID, HEX).c_str());
      char raw[RFM6xW_HEADER_LEN*2+1];
      memset(raw, 0, sizeof(raw));
      for (int i = 0; i < RFM6xW_PACKET_LEN; i++)
      {
        sprintf(&(raw[2*i]), "%02x", obs.rawmsg[i]);
      }
      strcpy(msg.message, raw);
      mqtt_msgs->add(mqtt_msgs, &msg);
      #endif
    }
  }
}

void send_messages(){
  if (!mqtt_msgs->isEmpty(mqtt_msgs)){
    mqtt_msg msg;

    if (mqtt_msgs->pull(mqtt_msgs, &msg)){
      mqtt.publish(msg.topic, msg.message);
    }
  }
}

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

  if (observations_w->add(observations_w, obs) == -1){
    Serial.println("Weather message buffer full; message dropped");
  }


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
   /*rfm.set_time_handler(observed_t);
   rfm.set_unknown_handler(observed_u);
   */
   mqtt.setServer(mqtt_server, 1883);
   mqtt.setCallback(mqtt_callback);
   mqtt.publish("outTopic", "hello world");

   #ifdef RAW_PACKAGES
   Serial.println("Printing raw packages");
   #endif
}



void loop(){
  rfm.run();
  yield();
  process_messages();
  yield();
  if (!mqtt.connected()) {
    reconnect();
  }
  yield();
  mqtt.loop();
  yield();
  send_messages();
  yield();
}
