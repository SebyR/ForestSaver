#include <LoRa.h>
#include "LoRaBoards.h"
#include <Wire.h>
#include "Adafruit_HTU21DF.h"
#include <TinyGPS++.h>


int ID = 3;


int drujPin = 33;
int focPin = 32;

int valFoc,valDruj;


TinyGPSPlus gps;

float baterie;

String foc = "", risc = "", druj = "";

float temp;
float umid;

Adafruit_HTU21DF htu = Adafruit_HTU21DF();

void setup()
{
  Serial.begin(115200); //Fast serial as possible
  Wire.begin();
  Wire.setClock(400000); //Increase I2C clock speed to 400kHz
  setupBoards();
  delay(1500);
  Serial.println("LoRa Sender");
  #ifdef  RADIO_TCXO_ENABLE
      pinMode(RADIO_TCXO_ENABLE, OUTPUT);
      digitalWrite(RADIO_TCXO_ENABLE, HIGH);
  #endif

    Serial.println("LoRa Sender");
    LoRa.setPins(RADIO_CS_PIN, RADIO_RST_PIN, RADIO_DIO0_PIN);
    if (!LoRa.begin(LORA_FREQ_CONFIG)) {
        Serial.println("Starting LoRa failed!");
        while (1);
    }

  //Get device parameters - We only have to do this once
  int status;

  if (!htu.begin()) {
    Serial.println("Couldn't find sensor!");
    while (1);
  }

  pinMode(drujPin, INPUT);
  pinMode(focPin, INPUT);
  
}

void loop()
{
  foc = ""; risc = ""; druj = "";
  valFoc =0; valDruj=0;
  for(int i = 1; i <= 60; i++){
    baterie = PMU->getBattVoltage()/1000;
    valFoc = digitalRead(focPin);
    valDruj = digitalRead(drujPin);
    temp = htu.readTemperature();
    umid = htu.readHumidity();
    if (valDruj == 1) {
      druj = "AD! ";
    }
    if (umid < 30) {
      risc = "R:" + String(umid, 2) + "* ";
    }
    if (valFoc == 1)  {
      foc = "AF:Departe& ";
    }
      if (temp >= 50 ) {
        foc = "AF:" + String(temp, 2) + "& ";
      }
    delay(200);
  }
  SendData();
}


//citeste GPS-ul
void GPS() 
{
  if (millis() > 5000 && gps.charsProcessed() < 10)
  {
    Serial.println(F("No GPS detected: check wiring."));
    while (true);
  }
  if (gps.encode(Serial1.read()) && Serial1.available() > 0) {
    if (gps.location.isValid()) {
      Serial.print(gps.location.lat(), 6);
      Serial.print(F(","));
      Serial.print(gps.location.lng(), 6);
    }
  }
}


void SendData() { // id ? , bat | , durj ! , foc & , risc * , La $ , Ln +
  LoRa.beginPacket();
  LoRa.print("ID:");
  LoRa.print(ID);
  LoRa.print("? ");
  LoRa.print("B:");
  LoRa.print(baterie);
  LoRa.print("|");
  LoRa.print(druj);
  LoRa.print(foc);
  LoRa.print(risc);
  while(Serial1.available() > 0){
    if (gps.encode(Serial1.read())) {
      if (gps.location.isValid()) {
        LoRa.print("La:");
        LoRa.print(String(gps.location.lat(), 6));
        LoRa.print("$");
        LoRa.print(" Ln:");
        LoRa.print(String(gps.location.lng(), 6));
        LoRa.print("+");
        //Serial.print("ceva");
        break;
      }
    }
  }
  LoRa.endPacket();
}

