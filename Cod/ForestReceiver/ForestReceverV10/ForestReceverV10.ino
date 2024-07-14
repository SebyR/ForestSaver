
#include <LoRa.h>
#include "boards.h"
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "time.h"

#define API_KEY "*******"
#define DATABASE_URL "*******"

#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

FirebaseData fbdo;

FirebaseAuth auth;
FirebaseConfig config;

const char* ssid = "*****";
const char* password = "******";

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 7200;
const int   daylightOffset_sec = 3600;

String foc = "", risc = "", druj = "", Lat = "", Lng = "", ID = "";
int numID;
float la2, ln2, bat;
int nrModule = 4;
String dat[4][6];

bool signupOK = false;



void setup()
{
  initBoard();
  
  delay(1500);
  Serial.begin(115200);

  Serial.println("LoRa Receiver");

  LoRa.setPins(RADIO_CS_PIN, RADIO_RST_PIN, RADIO_DIO0_PIN);
  if (!LoRa.begin(LoRa_frequency)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }

  Serial.println(WiFi.localIP());

  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("ok");
    signupOK = true;
  }
  else {
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }
  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

}

// id ? , bat | , durj ! , foc & , risc * , La $ , Ln +

void loop()
{
  struct tm timeinfo;
  getLocalTime(&timeinfo);
  // try to parse packet
  int nrModule = 4;
  String dat[nrModule][6];
  int packetSize = LoRa.parsePacket();
  String foc = "", risc = "", druj = "", Lat = "", Lng = "", ID = "";
  char timp[150];
  strftime(timp,150,"%d %B %Y %H:%M:%S", &timeinfo);
  if (packetSize) {
    // received a packet
    Serial.print("Received packet '");
    String recv = "";
    // read packet
    while (LoRa.available()) {
      recv += LoRa.readStringUntil('\n');
    }

    Firebase.RTDB.setString(&fbdo, "Date/" + String(timp) , recv);
    
    if (recv.indexOf("ID:") != -1) {
      numID = recv.substring(recv.indexOf("ID:") + 3, recv.indexOf("?")).toInt();
    }

    if (recv.indexOf("B:") != -1) {
      bat = recv.substring(recv.indexOf("B:") + 2 , recv.indexOf("|")).toFloat();
          Firebase.RTDB.setFloat(&fbdo, "ACT/ID" + String(numID) + "/Baterie", bat);
          Serial.println(bat);
    }

    if (recv.indexOf("AD! ") != -1) {
      druj = "Alerta drujba";
          Firebase.RTDB.setString(&fbdo, "ACT/ID" + String(numID) + "/DRUJBA", druj);
    }
    else{
      Firebase.RTDB.setString(&fbdo, "ACT/ID" + String(numID) + "/DRUJBA", "Nu");
    }

    if (recv.indexOf("AF:") != -1) {
      foc = recv.substring(recv.indexOf("AF:"), recv.indexOf("&"));
          Firebase.RTDB.setString(&fbdo, "ACT/ID" + String(numID) + "/FOC", foc);
    }
    else{
      Firebase.RTDB.setString(&fbdo, "ACT/ID" + String(numID) + "/FOC", "Nu");
    }

    if (recv.indexOf("R:") != -1) {
      risc = recv.substring(recv.indexOf("R:"), recv.indexOf("*"));
          Firebase.RTDB.setString(&fbdo, "ACT/ID" + String(numID) + "/RISC", risc);
    }
    else{
      Firebase.RTDB.setString(&fbdo, "ACT/ID" + String(numID) + "/RISC", "Nu                                                                                                                                                                                                                                                                                                                              ");
    }

    if (recv.indexOf("La:") != -1) {
      la2 = recv.substring(recv.indexOf("La:") + 3, recv.indexOf("$")).toFloat();
          Firebase.RTDB.setFloat(&fbdo, "ACT/ID" + String(numID) + "/La", la2);
    }

    if (recv.indexOf("Ln:") != -1) {
      ln2 = recv.substring(recv.indexOf("Ln:") + 3, recv.indexOf("+")).toFloat();
          Firebase.RTDB.setFloat(&fbdo, "ACT/ID" + String(numID) + "/Ln", ln2);
    }

    Serial.print(recv);
    Serial.print("' with RSSI ");
    Serial.println(LoRa.packetRssi());


#ifdef HAS_DISPLAY
    if (u8g2) {
      u8g2->clearBuffer();
      char buf[256];
      u8g2->drawStr(0, 12, "Received OK!");
      u8g2->drawStr(0, 26, recv.c_str());
      u8g2->sendBuffer();
    }
#endif
  }
}
