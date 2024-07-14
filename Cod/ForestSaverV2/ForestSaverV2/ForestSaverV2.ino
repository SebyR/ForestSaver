#include <LoRa.h>
#include "boards.h"
#include <Wire.h>
#include "Adafruit_HTU21DF.h"
#include "MLX90640_API.h"
#include "MLX90640_I2C_Driver.h"
#include "arduinoFFT.h"
#include <TinyGPS++.h>
#include <ESP32Servo.h>


#define SAMPLES 128
#define SAMPLING_FREQUENCY 2048

int ID = 1;

TinyGPSPlus gps;

Servo servo;
int pos = 90;
int sens = -1;

arduinoFFT FFT = arduinoFFT();

unsigned int samplingPeriod;
unsigned long microSeconds;

double vReal[SAMPLES];
double vImag[SAMPLES];

int counter = 0;
float MaximTemp;

double VarfFrecventa;

float baterie;

String foc = "", risc = "", druj = "";
int contDruj = 0;

float temp;
float umid, lastUmid;
int cont;

const byte MLX90640_address = 0x33; //Default 7-bit unshifted address of the MLX90640

#define TA_SHIFT 8 //Default shift for MLX90640 in open air

float mlx90640To[768];
paramsMLX90640 mlx90640;

#define mic A10
float sunet;

Adafruit_HTU21DF htu = Adafruit_HTU21DF();

void setup()
{
  Serial.begin(115200); //Fast serial as possible
  Wire.begin();
  Wire.setClock(400000); //Increase I2C clock speed to 400kHz
  initBoard();
  delay(1500);
  Serial.println("LoRa Sender");
  LoRa.setPins(RADIO_CS_PIN, RADIO_RST_PIN, RADIO_DIO0_PIN);
  if (!LoRa.begin(LoRa_frequency)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }
  while (!Serial); 
  if (isConnected() == false)
  {
    Serial.println("MLX90640 not detected at default I2C address. Please check wiring. Freezing.");
    while (1);
  }

  //Get device parameters - We only have to do this once
  int status;
  uint16_t eeMLX90640[832];
  status = MLX90640_DumpEE(MLX90640_address, eeMLX90640);
  if (status != 0)
    Serial.println("Failed to load system parameters");

  status = MLX90640_ExtractParameters(eeMLX90640, &mlx90640);
  if (status != 0)
    Serial.println("Parameter extraction failed");

  MLX90640_SetRefreshRate(MLX90640_address, 0x03);

  if (!htu.begin()) {
    Serial.println("Couldn't find sensor!");
    while (1);
  }
  samplingPeriod = round(1000000 * (1.0 / SAMPLING_FREQUENCY));
  servo.attach(32);
  servo.write(pos);
  pinMode(mic, INPUT);
}

void loop()
{
  pos = pos + sens * 10;
  if (pos == 20)
  {
    sens = 1;
  }
  if (pos == 160)
  {
    sens = -1;
  }
  foc = ""; risc = ""; druj = "";
  baterie = PMU.getBattVoltage() / 1000.0;
  unsigned long durata = pulseIn(mic, HIGH);
  temp = htu.readTemperature();
  umid = htu.readHumidity();
  CitireCam();
  MaximTemp = mlx90640To[0];
  for (int x = 127 ; x < 768 ; x++)
  {
    if (MaximTemp < mlx90640To[x]) {
      MaximTemp = mlx90640To[x];
    }
  }
  fft();
  //Serial.println(baterie);
  if (VarfFrecventa > 25 && VarfFrecventa < 80) {
    contDruj++;
  }
  if (contDruj >= 5) {
    druj = "AD! ";
    contDruj = 0;
  }
  if (umid < 30) {
    risc = "R:" + String(umid, 2) + "* ";
  }
  if (MaximTemp > 50.00 || temp > 50)  {
    // send packet
    if (MaximTemp >= temp) {
      foc = "AF:" + String(MaximTemp, 2) + "& ";
    }
    else {
      foc = "AF:" + String(temp, 2) + "& ";
    }
  }
  servo.write(pos);
  SendData();

  delay(20000);
}

//functie pentru a gasi frecventa predominanta citita de microfon
void fft() {
  for (int i = 0; i < SAMPLES; i++) {
    microSeconds = micros();

    vReal[i] = analogRead(mic);
    vImag[i] = 0;
    while (micros() < (microSeconds + samplingPeriod)) {
      //nu face nimic
    }
  }
  FFT.Windowing(vReal, SAMPLES, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
  FFT.Compute(vReal, vImag, SAMPLES, FFT_FORWARD);
  FFT.ComplexToMagnitude(vReal, vImag, SAMPLES);
  VarfFrecventa = FFT.MajorPeak(vReal, SAMPLES, SAMPLING_FREQUENCY);
}

//citire camera
void CitireCam() {
  long startTime = millis();
  for (byte x = 0 ; x < 2 ; x++)
  {
    uint16_t mlx90640Frame[834];
    int status = MLX90640_GetFrameData(MLX90640_address, mlx90640Frame);

    float vdd = MLX90640_GetVdd(mlx90640Frame, &mlx90640);
    float Ta = MLX90640_GetTa(mlx90640Frame, &mlx90640);

    float emissivity = 0.95;

    MLX90640_CalculateTo(mlx90640Frame, &mlx90640, emissivity, temp, mlx90640To);//calc
  }
  long stopTime = millis();
}

//citeste GPS-ul
void GPS() {

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

void NivelBat() {
  LoRa.beginPacket();
  LoRa.print("Baterie:");
  LoRa.print(baterie);
  LoRa.endPacket();
}

//Returns true if the MLX90640 is detected on the I2C bus
boolean isConnected() {
  Wire.beginTransmission((uint8_t)MLX90640_address);
  if (Wire.endTransmission() != 0)
    return (false); //Sensor did not ACK
  return (true);
}
