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

//Knopf auf dem Bildschirm
LGFX_Button knopfEinAus;
bool verbraucherEin = false;


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
//Messung
void Messung(){
  Spannung = Messgeraet.voltage();
  Strom = Messgeraet.current();
  Leistung = Messgeraet.power();
  Energie  = Messgeraet.energy();
  Frequenz = Messgeraet.frequency();
}
//Wiedergabe der gemessenen Werte im Serial Monotoring
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
void Wiedergabe_Bildschirm(){
  CoreS3.Display.clear();
  CoreS3.Display.clear(WHITE);
  CoreS3.Display.setTextSize(2);
  CoreS3.Display.setTextColor(BLACK);
  CoreS3.Display.setCursor(0, 30);
  CoreS3.Display.print("Spannung:");
  CoreS3.Display.setCursor(160, 30);
  CoreS3.Display.print(Spannung);
  CoreS3.Display.setCursor(0, 60);
  CoreS3.Display.print("Strom:");
  CoreS3.Display.setCursor(160, 60);
  CoreS3.Display.print(Strom);
  CoreS3.Display.setCursor(0, 90);
  CoreS3.Display.print("Leistung:");
  CoreS3.Display.setCursor(160, 90);
  CoreS3.Display.print(Leistung);
  CoreS3.Display.setCursor(00, 120);
  CoreS3.Display.print("Energie:");
  CoreS3.Display.setCursor(160, 120);
  CoreS3.Display.print(Energie);
  CoreS3.Display.setCursor(0, 150);
  CoreS3.Display.print("Frequenz:");
  CoreS3.Display.setCursor(160, 150);
  CoreS3.Display.print(Frequenz);
  knopfEinAus.drawButton(); 
}

void Touch_Screen_Ueberpruefung(){
  CoreS3.update();
  auto Beruehrungs_Ort = CoreS3.Touch.getDetail();
  bool gedrueckt = Beruehrungs_Ort.isPressed() && knopfEinAus.contains(Beruehrungs_Ort.x, Beruehrungs_Ort.y);
  knopfEinAus.press(gedrueckt);

if (knopfEinAus.justPressed()) {
  verbraucherEin = !verbraucherEin;
  if (verbraucherEin) {
  Serial.println("Verbraucher: EIN");
} else {
  Serial.println("Verbraucher: AUS");
}
}
}

void setup() {

  auto cfg = M5.config(); //Struktur mit Einstellungen für den Start
  cfg.output_power = true;   // 5V-Ausgang aktivieren
  CoreS3.begin(cfg);
  knopfEinAus.initButton(&CoreS3.Display, 180, 210, 80, 40,TFT_BLACK, TFT_BLACK, TFT_WHITE, "Ein/Aus", 1.5, 1.5);


  Serial.begin(115200);
  wlanVerbinden();
  
}

void loop() {
  Messung();
  Wert_Wiedergabe_Monitoring();
  Wiedergabe_Bildschirm();
  Touch_Screen_Ueberpruefung();



  delay(500);
}
