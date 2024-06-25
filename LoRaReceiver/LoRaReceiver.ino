#include <SPI.h>
#include <LoRa.h>

/* 
  CONEXÃO LoRa RA-02:ESP8266
  NSS   GPIO15 - D8
  MOSI  GPIO13 - D7
  MISO  GPIO12 - D6
  SCK   GPIO14 - D5
  RST   GPIO2  - D4
  DIO0  GPIO5  - D1
*/

const int ssPin = 15;   // NSS
const int rstPin = 2;   // RST
const int dio0Pin = 5;  // DIO0
const int ledPin = 4;   // LED
int SyncWord = 0x22;
int state = 0;

void setup() {
  // Inicia comunicação Serial
  Serial.begin(115200);
  // Define D2 (GPIO4) como saída
  pinMode(ledPin, OUTPUT);

  // Define configuração LoRa
  LoRa.setPins(ssPin, rstPin, dio0Pin);
  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa falhou");
    while (1);
  }
  LoRa.setSpreadingFactor(12);
  LoRa.setSignalBandwidth(62.5E3);
  LoRa.setCodingRate4(8);                   
  LoRa.setSyncWord(SyncWord);
  
  delay(5000);

  Serial.println("\nLoRa iniciado");
}

void loop() {
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    Serial.print("Pacote recebido: ");
    
    while(LoRa.available()) {
      Serial.print((char) LoRa.read());
    }
    
    // print RSSI do pacote
    Serial.print(" - RSSI: ");
    Serial.println(LoRa.packetRssi());

    state = !state;
    digitalWrite(ledPin, state);
  }
}