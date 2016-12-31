

#include <RFM-6x-Weather.h>
//D1 = GPIO5
#define RFM_INT 5
#define RFM_CS 15
#define LEDPIN 2

RFM6xWeather::Receiver rfm(15, RFM_INT, hardware_spi);

uint8_t buffer[RFM6xW_PACKET_LEN], n;

// https://forum.arduino.cc/index.php?topic=38107.0
void PrintHex8(uint8_t *data, uint8_t length) // prints 8-bit data in hex with leading zeroes
{
       Serial.print("0x");
       for (int i=0; i<length; i++) {
         if (data[i]<0x10) {Serial.print("0");}
         Serial.print(data[i],HEX);
         Serial.print(" ");
       }
}

/*
void myisr(void){
  rfm.handleInterrupt();
}
*/

void setup()
{
  Serial.begin(115200);
   if (!rfm.init())
     Serial.println("Error initializing rfm");
   else
    Serial.println("RFM initialized OK");
   Serial.println(SS);
   pinMode(LEDPIN, OUTPUT);
   digitalWrite(LEDPIN, LOW);
   delay(1000);
   digitalWrite(LEDPIN, HIGH);
   // attachInterrupt(RFM_INT, &myisr, RISING);
}


void loop(){

  if (rfm.available()){
    n = sizeof(buffer);
    if(rfm.recv(buffer, &n)) {
      PrintHex8(buffer, n);
      Serial.println();
    } else {
      Serial.println("Problem reading packet");
    }
  } else {
    yield();
  }
  delay(1000);
}
