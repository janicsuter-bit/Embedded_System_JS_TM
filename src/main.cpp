#include <Arduino.h>
#include <M5CoreS3.h>        
#include <WiFi.h>      
#include <WiFiMulti.h>      
#include <PubSubClient.h>    
#include <PZEM004Tv30.h> 
#include "pw.h"

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
LGFX_Button knopfZurueck;
bool verbraucherEin = false;

//Pin für das Relais
#define Relais_Pin 9
#define RELAIS_EIN LOW
#define RELAIS_AUS HIGH

//Festlegen des Maximalstromes. Abschaltung bei erreichung.
#define STROM_MAX 5.0
#define UEBERSTROM_ZYKLEN  3

//Variabel Bild Wechsel
int Variabel_Seiten_Wechsel = 0;

WiFiClient netzClient;
PubSubClient mqtt(netzClient);

const char* TOPIC_MESSWERT = "strommesser/cores3/messwert";
const char* TOPIC_BEFEHL   = "strommesser/cores3/relais/befehl";
const char* TOPIC_STATUS   = "strommesser/cores3/relais/status";
const char* TOPIC_ONLINE   = "strommesser/cores3/status";
const char* CLIENT_ID      = "cores3-strommesser";

struct Messwerte {
  float spannung;
  float strom;
  float leistung;
  float energie;
  float frequenz;
  int   fehler;
};

QueueHandle_t qMesswerte;
QueueHandle_t qAnzeige;
QueueHandle_t qBefehle;
QueueHandle_t qStatus;

// Störungsflags: sperren das Einschalten und lösen den Abwurf aus.
// Getrennt, damit eine Störung die andere nicht überschreibt.
volatile bool stoerungSensor = false;
volatile bool stoerungNetz   = false;
volatile bool stoerungUeberstrom = false;
volatile bool manuellAus = false;


void wlanVerbinden(){
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  

  wifiMulti.addAP(WLAN_SSID_1, WLAN_PASS_1);
  wifiMulti.addAP(WLAN_SSID_2, WLAN_PASS_2);
  // wifiMulti.addAP(WLAN_SSID_3, WLAN_PASS_3);
  wifiMulti.addAP(WLAN_SSID_4, WLAN_PASS_4);

  for (int versuch = 1; versuch <= 4; versuch++) {
  Serial.print("WLAN-Versuch "); Serial.println(versuch);
  if (wifiMulti.run(10000) == WL_CONNECTED) break;
  delay(500);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Wlan verbunden: ");
    Serial.print(WiFi.SSID());
    Serial.print(", IP: ");
    Serial.print(WiFi.localIP());
    Serial.print(", Gateway: ");
    Serial.println(WiFi.gatewayIP());
  } else {
  Serial.println("Wlan verbindung fehlgeschlagen");
}

}

// Callback Funktion ruft die Bibliothek selbstständig auf, sobald eine Nachricht auf einem abonnierten Topic eintrifft.
void mqttEmpfangen(char* topic, byte* nutzlast, unsigned int laenge) {
  String text = "";
  for (unsigned int i = 0; i < laenge; i++) {
    text += (char)nutzlast[i];
  }

  if (text == "ein" || text == "aus") {
  bool wunsch = (text == "ein");
  manuellAus = !wunsch;
  xQueueSend(qBefehle, &wunsch, 0);
}
}
// Waehlt den Broker anhand des verbundenen WLAN.
// Jede SSID hat ihren eigenen Zweig; unbekanntes Netz faellt auf Schule zurueck.
const char* brokerWaehlen() {
  String netz = WiFi.SSID();
  Serial.print("SSID: ["); Serial.print(netz); Serial.println("]");

  if (netz == WLAN_SSID_1) { Serial.println("-> Broker Schule"); return MQTT_BROKER_SCHULE; }
  if (netz == WLAN_SSID_2) { Serial.println("-> Broker Mobile"); return MQTT_BROKER_MOBILE; }
  if (netz == WLAN_SSID_3) { Serial.println("-> Broker Heim");   return MQTT_BROKER_HEIM; }
  if (netz == WLAN_SSID_4) { Serial.println("-> Broker Hotspot"); return MQTT_BROKER_HOTSPOT_TIMO; }

  Serial.println("-> SSID unbekannt, nehme Schule");
  return MQTT_BROKER_SCHULE;
}

bool mqttVerbinden() {
  mqtt.setServer(brokerWaehlen(), MQTT_PORT);
  mqtt.setCallback(mqttEmpfangen);
  mqtt.setBufferSize(512);
  mqtt.setKeepAlive(15);

  bool ok = mqtt.connect(CLIENT_ID, MQTT_USER, MQTT_PASS,
                         TOPIC_ONLINE, 1, true, "offline");
  if (ok) {
    mqtt.publish(TOPIC_ONLINE, "online", true);
    mqtt.publish(TOPIC_STATUS, verbraucherEin ? "ein" : "aus", true);
    mqtt.subscribe(TOPIC_BEFEHL, 1);
  }
  if (!ok) {
  Serial.print("MQTT fehlgeschlagen, Broker: ");
  Serial.print(brokerWaehlen());
  Serial.print(" rc=");
  Serial.println(mqtt.state());
  }
  return ok;
}

// Messwerte bekommen JSON. Werden gemeinsasm gesendet. Node-RED wandelt in Felder um. Nur eine MQTT Nachricht.
void messwerteSenden(const Messwerte& m) {
  char nutzlast[192];
  snprintf(nutzlast, sizeof(nutzlast),
    "{\"spannung\":%.1f,\"strom\":%.3f,\"leistung\":%.1f,"
    "\"energie\":%.3f,\"frequenz\":%.1f,\"fehler\":%d}",
    m.spannung, m.strom, m.leistung, m.energie, m.frequenz, m.fehler);
  mqtt.publish(TOPIC_MESSWERT, nutzlast);
}

//Messung (benutzen noch globale Variablen. Evtl. in Struct ändern)
// Inkl. Sensorüberwachung
void Messung(){
  Spannung = Messgeraet.voltage();
  Strom = Messgeraet.current();
  Leistung = Messgeraet.power();
  Energie  = Messgeraet.energy();
  Frequenz = Messgeraet.frequency();

  if (isnan(Spannung) or isnan(Strom) or isnan(Leistung) or isnan(Energie) or isnan(Frequenz)){
    Fehler_Variabel_Messgeraet = 1;
  } else {
    Fehler_Variabel_Messgeraet = 0;
  }
}

void Wiedergabe_Bildschirm_Initialisierung(){
  CoreS3.Display.clear();
  CoreS3.Display.clear(WHITE);
  CoreS3.Display.setTextSize(2);
  CoreS3.Display.setTextColor(BLACK);
  CoreS3.Display.setCursor(60, 50);
  CoreS3.Display.print("Initialisiere...");
  Serial.print("Initialisiere...");
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
//Wiedergabe anzeige falls relais aus ist
void Wiedergabe_Bildschirm_Messwerte_NaN(){
  CoreS3.Display.setTextSize(2);
  CoreS3.Display.setTextColor(BLACK);
  CoreS3.Display.fillRect(160, 30, 100, 20, WHITE);
  CoreS3.Display.setCursor(160, 30);
  CoreS3.Display.print("NaN");
  CoreS3.Display.fillRect(160, 60, 100, 20, WHITE);
  CoreS3.Display.setCursor(160, 60);
  CoreS3.Display.print("NaN");
  CoreS3.Display.fillRect(160, 90, 100, 20, WHITE);
  CoreS3.Display.setCursor(160, 90);
  CoreS3.Display.print("NaN");
  CoreS3.Display.fillRect(160, 120, 100, 20, WHITE);
  CoreS3.Display.setCursor(160, 120);
  CoreS3.Display.print("NaN");
  CoreS3.Display.fillRect(160, 150, 100, 20, WHITE);
  CoreS3.Display.setCursor(160, 150);
  CoreS3.Display.print("NaN");
}

//Anzeige der Messwerte. Bekommt Messsatz und nicht globale Variablen. So gehört Anzeige und MQTT Paket immer zusammen.
void Wiedergabe_Bildschirm_Messwerte(const Messwerte& m){

  CoreS3.Display.setTextSize(2);
  CoreS3.Display.setTextColor(BLACK);
  CoreS3.Display.fillRect(160, 30, 100, 20, WHITE);
  CoreS3.Display.setCursor(160, 30);
  CoreS3.Display.print(m.spannung);
  CoreS3.Display.print("V");
  CoreS3.Display.fillRect(160, 60, 100, 20, WHITE);
  CoreS3.Display.setCursor(160, 60);
  CoreS3.Display.print(m.strom);
  CoreS3.Display.print("A");
  CoreS3.Display.fillRect(160, 90, 100, 20, WHITE);
  CoreS3.Display.setCursor(160, 90);
  CoreS3.Display.print(m.leistung);
  CoreS3.Display.print("W");
  CoreS3.Display.fillRect(160, 120, 100, 20, WHITE);
  CoreS3.Display.setCursor(160, 120);
  CoreS3.Display.print(m.energie);
  CoreS3.Display.print("kWh");
  CoreS3.Display.fillRect(160, 150, 100, 20, WHITE);
  CoreS3.Display.setCursor(160, 150);
  CoreS3.Display.print(m.frequenz);
  CoreS3.Display.print("Hz");
}

//Anzeige wenn das Messgerät nicht verbunden ist
void Wiedergabe_Bildschirm_Fehler_Messgeraet(){
  CoreS3.Display.clear();
  CoreS3.Display.clear(WHITE);
  CoreS3.Display.setTextSize(2);
  CoreS3.Display.setTextColor(BLACK);
  CoreS3.Display.setCursor(48, 50);// anpassen ort
  CoreS3.Display.print("Messgeraet stoerung");
  Serial.println("Messgeraet stoerung");
}
//Anzeige wenn das Wlan nicht verbunden ist
void Wiedergabe_Bildschirm_Fehler_Wlan(){
  CoreS3.Display.clear();
  CoreS3.Display.clear(WHITE);
  CoreS3.Display.setTextSize(2);
  CoreS3.Display.setTextColor(BLACK);
  CoreS3.Display.setCursor(15, 50);// anpassen ort
  CoreS3.Display.print("Wlan verbindung verloren");
  Serial.print("Wlan verbindung Verloren");
}

void Wiedergabe_Bildschirm_Stoerung_Ueberstrom(){
  CoreS3.Display.clear();
  CoreS3.Display.clear(WHITE);
  CoreS3.Display.setTextSize(2);
  CoreS3.Display.setTextColor(BLACK);
  CoreS3.Display.setCursor(35, 50);// anpassen ort
  CoreS3.Display.print("Ueberstrom ausgeloest");
  knopfZurueck.drawButton();
  Serial.print("Ueberstrom ausgeloest");
}

void Touch_Screen_Ueberpruefung(){
  CoreS3.update();
  auto Beruehrungs_Ort = CoreS3.Touch.getDetail();
  bool gedrueckt = Beruehrungs_Ort.isPressed() && knopfEinAus.contains(Beruehrungs_Ort.x, Beruehrungs_Ort.y);
  knopfEinAus.press(gedrueckt);
  bool gedruecktZurueck = Beruehrungs_Ort.isPressed() && knopfZurueck.contains(Beruehrungs_Ort.x, Beruehrungs_Ort.y);
  knopfZurueck.press(gedruecktZurueck);

if (knopfEinAus.justPressed()) {
  verbraucherEin = !verbraucherEin;
  if (verbraucherEin) {
  Serial.println("Verbraucher: EIN");
} else {
  Serial.println("Verbraucher: AUS");
}
}
}

// Mess-Task: füllt ein Struct statt globale Variablen. Keine Displayaufrufe. Messungen alle 1000ms.
void Mess_Task(void *pvParameters) {
  TickType_t letzterStart = xTaskGetTickCount();
  Messwerte m;
  uint8_t ueberstromZaehler = 0;

  for (;;) {
    Messung();
    m.spannung = Spannung;
    m.strom    = Strom;
    m.leistung = Leistung;
    m.energie  = Energie;
    m.frequenz = Frequenz;
    m.fehler   = Fehler_Variabel_Messgeraet;

    xQueueSend(qMesswerte, &m, 0);
    xQueueSend(qAnzeige,   &m, 0);

    bool fehlerJetzt = (m.fehler != 0);
      if (verbraucherEin) {
        if (fehlerJetzt && !stoerungSensor) {
          bool aus = false;
          xQueueSend(qBefehle, &aus, 0);
      }
      stoerungSensor = fehlerJetzt;
    } else {
      stoerungSensor = false;
    }

    if (m.strom > STROM_MAX) {
      ueberstromZaehler++;
      if (ueberstromZaehler >= UEBERSTROM_ZYKLEN && !stoerungUeberstrom) {
        stoerungUeberstrom = true;
        bool aus = false;
        xQueueSend(qBefehle, &aus, 0);
      }
    } else {
      ueberstromZaehler = 0;
    }

    vTaskDelayUntil(&letzterStart, pdMS_TO_TICKS(1000));
  }
}

void Kommunikations_Task(void *pvParameters) {
  uint32_t naechsterVersuch = 0;
  Messwerte m;
  bool status;

  for (;;) {
    bool netzWeg = (WiFi.status() != WL_CONNECTED);
    if (netzWeg && !stoerungNetz) {
      bool aus = false;
      xQueueSend(qBefehle, &aus, 0);
    }
    stoerungNetz = netzWeg;

    if (WiFi.status() != WL_CONNECTED) {
      if (millis() > naechsterVersuch) {
        wifiMulti.run(5000);
        naechsterVersuch = millis() + 10000;
      }
    }
    else if (!mqtt.connected()) {
      if (millis() > naechsterVersuch) {
        mqttVerbinden();
        naechsterVersuch = millis() + 5000;
      }
    } else {
      mqtt.loop();
      if (xQueueReceive(qMesswerte, &m, 0) == pdTRUE) messwerteSenden(m);
      if (xQueueReceive(qStatus, &status, 0) == pdTRUE)
        mqtt.publish(TOPIC_STATUS, status ? "ein" : "aus", true);
    }
    
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}
// Relais Ein- und Ausschalten. Wenn Störung, darf es sich nicht einschalten lassen. 
void Relais_Task(void *pvParameters) {
  bool wunsch;
  for (;;) {
    if (xQueueReceive(qBefehle, &wunsch, portMAX_DELAY) == pdTRUE) {
      if (wunsch && (stoerungSensor || stoerungNetz || stoerungUeberstrom)) wunsch = false;
      verbraucherEin = wunsch;
      digitalWrite(Relais_Pin, verbraucherEin ? RELAIS_EIN : RELAIS_AUS);
      xQueueSend(qStatus, &verbraucherEin, 0);
    }
  }
}

// Anzeige-Task und Touch-Button. Genau ein Task. Mutex dadurch nicht nötig.
void Anzeige_Task(void *pvParameters) {
  Messwerte m;
  int letzterZustand = -1;
  for (;;) {
    CoreS3.update();
    auto ort = CoreS3.Touch.getDetail();
    knopfEinAus.press(ort.isPressed() &&
                      knopfEinAus.contains(ort.x, ort.y));
    knopfZurueck.press(ort.isPressed() &&
                      knopfZurueck.contains(ort.x, ort.y));
                      

   if (stoerungUeberstrom) {
      if (knopfZurueck.justPressed()) {
        stoerungUeberstrom = false;
      }
    } else {
      if (knopfEinAus.justPressed()) {
        bool wunsch = !verbraucherEin;
        manuellAus = !wunsch;
        xQueueSend(qBefehle, &wunsch, 0);
      }
    }

    if (xQueueReceive(qAnzeige, &m, 0) == pdTRUE) {
      int neuerZustand = stoerungUeberstrom ? 2
                        : manuellAus         ? 4
                        : (m.fehler != 0)   ? 1
                        : stoerungNetz      ? 3
                        :                      0;
      bool kommtVonVollbildFehler = (letzterZustand == 1 || letzterZustand == 2 || letzterZustand == 3);

      if (neuerZustand == 0) {
        if (kommtVonVollbildFehler) Nicht_Veraenderbare_Anzeigen();
        Wiedergabe_Bildschirm_Messwerte(m);
      } else if (neuerZustand == 4) {
        if (kommtVonVollbildFehler) Nicht_Veraenderbare_Anzeigen();
        Wiedergabe_Bildschirm_Messwerte_NaN();
      } else if (neuerZustand != letzterZustand) {
        if      (neuerZustand == 2) 
        Wiedergabe_Bildschirm_Stoerung_Ueberstrom();
        else if (neuerZustand == 1) 
        Wiedergabe_Bildschirm_Fehler_Messgeraet();
        else                         
        Wiedergabe_Bildschirm_Fehler_Wlan();
      }
      letzterZustand = neuerZustand;
    }
    vTaskDelay(pdMS_TO_TICKS(200));
  }
}

void setup() {

  auto cfg = M5.config(); //Struktur mit Einstellungen für den Start
  cfg.output_power = true;   // 5V-Ausgang aktivieren
  CoreS3.begin(cfg);
  knopfEinAus.initButton(&CoreS3.Display, 160, 210, 80, 40,TFT_BLACK, TFT_BLACK, TFT_WHITE, "Ein/Aus", 1.5, 1.5);
  knopfZurueck.initButton(&CoreS3.Display, 160, 210, 80, 40,TFT_BLACK, TFT_BLACK, TFT_WHITE, "Zurueck", 1.5, 1.5);
  //Pin einlesen
  digitalWrite(Relais_Pin, RELAIS_AUS);
  pinMode(Relais_Pin, OUTPUT);

  Serial.begin(115200);
  Wiedergabe_Bildschirm_Initialisierung();
  delay(200); 
  wlanVerbinden();

  digitalWrite(Relais_Pin, RELAIS_AUS);
  pinMode(Relais_Pin, OUTPUT);

  qMesswerte = xQueueCreate(5, sizeof(Messwerte));
  qAnzeige   = xQueueCreate(2, sizeof(Messwerte));
  qBefehle   = xQueueCreate(5, sizeof(bool));
  qStatus    = xQueueCreate(5, sizeof(bool));

  xTaskCreate(Mess_Task,           "Messung", 4096, NULL, 3, NULL);
  xTaskCreate(Relais_Task,         "Relais",  2048, NULL, 4, NULL);
  xTaskCreate(Kommunikations_Task, "Komm",    8192, NULL, 2, NULL);
  xTaskCreate(Anzeige_Task,        "Anzeige", 4096, NULL, 1, NULL);
  
}




void loop() { vTaskDelay(pdMS_TO_TICKS(1000)); }

