// BIBLIOTECAS
#include <SPI.h>
#include <LoRa.h>

#define LED_BUILTIN 2

// FUNÇÕES
void setupLoRa(long frequency);
void sendMessage(String outgoing, byte destination);
void onReceive(int packetSize);
void showData(void);
void callback(void);
void setupLed(uint8_t LED, uint8_t STATUS);
void ledBlink(uint8_t LED);
void ledToogle(uint8_t LED);

/* 
  CONEXÃO LoRa RA-02:ESP8266
  NSS   GPIO15 - D8
  MOSI  GPIO13 - D7
  MISO  GPIO12 - D6
  SCK   GPIO14 - D5
  RST   GPIO4  - D2
  DIO0  GPIO5  - D1
*/

// CONFIGURAÇÃO LoRa GPIO
const int ss = 15;
const int rst = 4;
const int dio0 = 5;

// VARIÁVEIS
String outgoing;
byte msgCount = 0;
byte localAddress = 0x02;
byte masterAddress = 0x01;
byte broadcast = 0xFF;

// TEMPO
long lastSendTime = 0;
int interval = 2000;

// PINOUT
byte led1State;

volatile bool dataReceived = false;

void setup() {
  Serial.begin(115200);
  setupLed(LED_BUILTIN, HIGH);
  setupLoRa(433E6);

  Serial.println("\nLoRa Receiver - iniciado com sucesso!");
  
  LoRa.onReceive(onReceive);
  LoRa.receive();
}

void loop() {
  if(dataReceived) {
    showData();
    callback();
    dataReceived = false;
  }
}

// CONFIGURAÇÃO LoRa
void setupLoRa(long frequency) {
  LoRa.setPins(ss, rst, dio0);                                        // Define configuração LoRa
  while (!LoRa.begin(frequency)) {                                        // Inicializa a comunicação LoRa
    Serial.println(".");
    delay(500);
  }

  // LoRa.setTxPower(20);
  // LoRa.setSpreadingFactor(12);
  // LoRa.setSignalBandwidth(62.5E3);
  // LoRa.setCodingRate4(8);
  // LoRa.setSyncWord(0x2A);
  
  delay(5000);
}

// SUBROTINA PARA ENVIO DE DADOS
void sendMessage(String outgoing, byte destination) {
  LoRa.beginPacket();                                                 // inicializa o envio dos pacotes
  LoRa.write(destination);                                            // adiciona o endereço de destino
  LoRa.write(localAddress);                                           // adiciona o endereço do emissor
  LoRa.write(msgCount);                                               // adiciona ID da mensagem
  LoRa.write(outgoing.length());                                      // adiciona comprimento do payload
  LoRa.print(outgoing);                                               // adiciona payload
  LoRa.endPacket();                                                   // finaliza o envio dos pacotes
  msgCount++;                                                         // incrementa ID da mensagem
}

// SUBROTINA PARA RECEBIMENTO DE DADOS
void onReceive(int packetSize) {
  if (packetSize == 0) return;                                        // se não há pacotes, retorne
  dataReceived = true;
}

// SUBROTINA PARA PROCESSAR DADOS DA MENSAGEM RECEBIDA
void showData(void) {
  // lendo BYTES do cabeçalho do pacote
  int recipient = LoRa.read();                                        // endereço do destinatário
  byte sender = LoRa.read();                                          // endereço do remetente
  byte incomingMsgId = LoRa.read();                                   // ID da mensagem
  byte incomingLength = LoRa.read();                                  // comprimento da mensagem

  String incoming = "";                                               // percorre conteúdo da mensagem e armazena os dados
  while (LoRa.available()) {
    incoming += (char)LoRa.read();
  } 

  if (incomingLength != incoming.length()) {                          // verifica se o comprimento recebido corresponde ao enviado
    Serial.println("erro: tamanho da mensagem não corresponde\n");
    return;
  }

  if(recipient != localAddress && recipient != 0xFF) {                 // verifica se a mensagem é encaminhada a este destinatário
    Serial.println("Essa mensagem não é pra mim.\n");
    return;
  }

  // EXIBE CONTEÚDO DA MENSAGEM
  Serial.println("Recebido de: 0x" + String(sender, HEX));
  Serial.println("Enviado para: 0x" + String(recipient, HEX));
  Serial.println("ID da mensagem: " + String(incomingMsgId));
  Serial.println("Mensagem: " + incoming);
  Serial.println("RSSI: " + String(LoRa.packetRssi()));
  Serial.println("Snr: " + String(LoRa.packetSnr()));
  Serial.println();
}

// RESPONDE MENSAGEM AO MASTER
void callback(void) {
  if (incoming == "GET_STATUS") { 
    ledToogle(LED_BUILTIN);
    led1State = !led1State;
    
    Serial.println("Enviando mensagem para o ESP8266 Master\n");
    
    String message = "LED STATE: " + String(led1State);
    sendMessage(message, masterAddress);
    
    LoRa.receive();
  }
}

// CONFIGURA LED GPIO
void setupLed(uint8_t LED, uint8_t STATUS) {
  pinMode(LED, OUTPUT);
  digitalWrite(LED, STATUS);
}

// PISCA O LED
void ledBlink(uint8_t LED) {
  digitalWrite(LED, !digitalRead(LED));
  delay(200);
  digitalWrite(LED, !digitalRead(LED));
}

// ALTERA ESTADO DO LED
void ledToogle(uint8_t LED) {
  digitalWrite(LED, !digitalRead(LED));
}