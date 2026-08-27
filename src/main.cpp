#include <Arduino.h>
#include <M5CoreS3.h>        
#include <WiFi.h>            
#include <PubSubClient.h>    
#include <PZEM004Tv30.h> 

void wlanVerbinden(){
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);

  WiFi.begin("Xiaomi 17T Pro", "1234567890");

  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(500);
  }
}

void setup() {
  // put your setup code here, to run once:
  wlanVerbinden();
}

void loop() {
  // put your main code here, to run repeatedly:
}
