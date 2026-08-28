#include <Arduino.h>
#include <M5CoreS3.h>        
#include <WiFi.h>      
#include <WiFiMulti.h>      
#include <PubSubClient.h>    
#include <PZEM004Tv30.h> 

WiFiMulti wifiMulti;

// Pin zuordnung Strommesser
#define PZEM_TX_PIN 17
#define PZEM_RX_PIN 18
PZEM004Tv30 Messgeraet(Serial2, PZEM_RX_PIN, PZEM_TX_PIN);

//Variabeln für Auswertung messgerät
float Spannung = 0;
float Strom = 0;
float Leistung = 0;
float Energie = 0;
float Frequenz = 0;


void wlanVerbinden(){
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  

  wifiMulti.addAP("Xiaomi 17T Pro", "1234567890");
  wifiMulti.addAP("Teko Olten", "ol4600-ch");
  wifiMulti.addAP("Netz 3", "Passwort 3");//Wlan eintragen(Timo)

  const uint32_t start = millis();

  if (wifiMulti.run(20000) == WL_CONNECTED) {
  Serial.print("Wlan verbunden");
  Serial.print(WiFi.SSID());
  Serial.print(", IP: ");
  Serial.println(WiFi.localIP());
} else {
  Serial.println("Welan verbindung fehlgeschlagen");
}
}
void Messung(){
  Spannung = Messgeraet.voltage();
  Strom = Messgeraet.current();
  Leistung = Messgeraet.power();
  Energie  = Messgeraet.energy();
  Frequenz = Messgeraet.frequency();
}
void Wert_Wiedergabe_Monitoring(){
  Serial.print("Spannung: ");
Serial.print(Spannung, 1);
Serial.print("V  Strom: ");
Serial.print(Strom, 3);
Serial.print("A  Leistung: ");
Serial.print(Leistung, 1);
Serial.print("W  Energie: ");
Serial.print(Energie, 3);
Serial.print("kWh  Frequenz: ");
Serial.print(Frequenz, 1);
Serial.println("Hz");

}


void setup() {

  auto cfg = M5.config(); //Struktur mit Einstellungen für den Start
  cfg.output_power = true;   // 5V-Ausgang aktivieren
  CoreS3.begin(cfg);


  Serial.begin(115200);
  wlanVerbinden();
  
}

void loop() {
  Messung();
  Wert_Wiedergabe_Monitoring();



  delay(500);
}
