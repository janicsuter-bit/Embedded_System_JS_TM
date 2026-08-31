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
int Fehler_Variabel_Messgeraet = 0;

//Knopf auf dem Bildschirm
LGFX_Button knopfEinAus;
bool verbraucherEin = false;

//Pin für das Relais
#define Relais_Pin 8

//Variabel Bild Wechsel
int Variabel_Seiten_Wechsel = 0;


void wlanVerbinden(){
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  

  wifiMulti.addAP("Xiaomi 17T Pro", "1234567890");
  wifiMulti.addAP("Teko Olten", "ol4600-ch");
  wifiMulti.addAP("Netz 3", "Passwort 3");//Wlan eintragen(Timo)

  if (wifiMulti.run(20000) == WL_CONNECTED) {
  Serial.print("Wlan verbunden");
  Serial.print(WiFi.SSID());
  Serial.print(", IP: ");
  Serial.println(WiFi.localIP());
} else {
  Serial.println("Wlan verbindung fehlgeschlagen");
}
}
//Messung
void Messung(){
  Spannung = Messgeraet.voltage();
  Strom = Messgeraet.current();
  Leistung = Messgeraet.power();
  Energie  = Messgeraet.energy();
  Frequenz = Messgeraet.frequency();

  if (isnan(Spannung) or isnan(Strom) or isnan(Leistung) or isnan(Energie) or isnan(Frequenz)){
    Serial.print("Fehlerhafte Messung");
    Fehler_Variabel_Messgeraet = 1;

  }else if (WiFi.status() != WL_CONNECTED)
  {
    Serial.print("Wlan-Verbindungs unterbruch");
    Fehler_Variabel_Messgeraet = 2;
  }else{
    Serial.print("erfolgreiche Messung");
    Fehler_Variabel_Messgeraet = 0;
  }
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
//nicht veränderbare Anzeigen Display
void Nicht_Veraenderbare_Anzeigen(){
  CoreS3.Display.clear();
  CoreS3.Display.clear(WHITE);
  CoreS3.Display.setTextSize(2);
  CoreS3.Display.setTextColor(BLACK);
  CoreS3.Display.setCursor(0, 30);
  CoreS3.Display.print("Spannung:");
  CoreS3.Display.setCursor(0, 60);
  CoreS3.Display.print("Strom:");
  CoreS3.Display.setCursor(0, 90);
  CoreS3.Display.print("Leistung:");
  CoreS3.Display.setCursor(00, 120);
  CoreS3.Display.print("Energie:");
  CoreS3.Display.setCursor(0, 150);
  CoreS3.Display.print("Frequenz:");
  knopfEinAus.drawButton();
}

//Anzeige der Messwerte
void Wiedergabe_Bildschirm_Messwerte(){

  CoreS3.Display.setTextSize(2);
  CoreS3.Display.setTextColor(BLACK);
  CoreS3.Display.fillRect(160, 30, 100, 20, WHITE);
  CoreS3.Display.setCursor(160, 30);
  CoreS3.Display.print(Spannung);
  CoreS3.Display.print("V");
  CoreS3.Display.fillRect(160, 60, 100, 20, WHITE);
  CoreS3.Display.setCursor(160, 60);
  CoreS3.Display.print(Strom);
  CoreS3.Display.print("A");
  CoreS3.Display.fillRect(160, 90, 100, 20, WHITE);
  CoreS3.Display.setCursor(160, 90);
  CoreS3.Display.print(Leistung);
  CoreS3.Display.print("W");
  CoreS3.Display.fillRect(160, 120, 100, 20, WHITE);
  CoreS3.Display.setCursor(160, 120);
  CoreS3.Display.print(Energie);
  CoreS3.Display.print("Ws");
  CoreS3.Display.fillRect(160, 150, 100, 20, WHITE);
  CoreS3.Display.setCursor(160, 150);
  CoreS3.Display.print(Frequenz);
  CoreS3.Display.print("Hz"); 
}
//Anzeige wenn das Messgerät nicht verbunden ist
void Wiedergabe_Bildschirm_Fehler_Messgeraet(){
  CoreS3.Display.clear();
  CoreS3.Display.clear(WHITE);
  CoreS3.Display.setTextSize(2);
  CoreS3.Display.setTextColor(BLACK);
  CoreS3.Display.setCursor(78, 50);// anpassen ort
  CoreS3.Display.print("Messgerät störung");
  Serial.print("Messgerät störung");
}
//Anzeige wenn das Wlan nicht verbunden ist
void Wiedergabe_Bildschirm_Fehler_Wlan(){
  CoreS3.Display.clear();
  CoreS3.Display.clear(WHITE);
  CoreS3.Display.setTextSize(2);
  CoreS3.Display.setTextColor(BLACK);
  CoreS3.Display.setCursor(36, 50);// anpassen ort
  CoreS3.Display.print("Wlan verbindung verloren");
  Serial.print("Wlan verbindung Verloren");

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

void Mess_Task(void *pvParameters){
  for (;;){
    Messung();
    Wert_Wiedergabe_Monitoring();
    if (Fehler_Variabel_Messgeraet == 1)
    {
      Wiedergabe_Bildschirm_Fehler_Messgeraet();
      Variabel_Seiten_Wechsel = 1;
    }else if (Fehler_Variabel_Messgeraet == 2){
      Wiedergabe_Bildschirm_Fehler_Wlan();
      Variabel_Seiten_Wechsel = 1;
    }else if (Fehler_Variabel_Messgeraet == 0 && Variabel_Seiten_Wechsel == 0){
      Wiedergabe_Bildschirm_Messwerte();
    }else if (Fehler_Variabel_Messgeraet == 0 && Variabel_Seiten_Wechsel == 1){
      Nicht_Veraenderbare_Anzeigen();
      Wiedergabe_Bildschirm_Messwerte();
      Variabel_Seiten_Wechsel = 0;
    }
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}


void Relais_Task(void *pvParameters){
for (;;){
  Touch_Screen_Ueberpruefung();
  if (Fehler_Variabel_Messgeraet == 1)
    {
      Serial.print("Fehler Messgerät");
      verbraucherEin = false;
      digitalWrite(Relais_Pin, verbraucherEin);
    }else if (Fehler_Variabel_Messgeraet == 2){
      Serial.print("Wlan nicht verbunden");
      verbraucherEin = false;
      digitalWrite(Relais_Pin, verbraucherEin);
    }else if (Fehler_Variabel_Messgeraet == 0){
      digitalWrite(Relais_Pin, verbraucherEin);
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}


void setup() {

  auto cfg = M5.config(); //Struktur mit Einstellungen für den Start
  cfg.output_power = true;   // 5V-Ausgang aktivieren
  CoreS3.begin(cfg);
  knopfEinAus.initButton(&CoreS3.Display, 140, 210, 80, 40,TFT_BLACK, TFT_BLACK, TFT_WHITE, "Ein/Aus", 1.5, 1.5);
  
  //Pin einlesen
  pinMode(Relais_Pin, OUTPUT);

  Serial.begin(115200);
  wlanVerbinden();

  xTaskCreate(Mess_Task, "Mess_Task", 4096, NULL, 1, NULL);
  xTaskCreate(Relais_Task, "Relais_Task", 4096, NULL, 1, NULL);
  Nicht_Veraenderbare_Anzeigen();
  
}

