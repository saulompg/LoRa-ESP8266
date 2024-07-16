// ----------------------------------------------------------------------- DIRETIVAS DE PRÉ-PROCESSAMENTO
#include <SPI.h>
#include <LoRa.h>

// ----------------------------------------------------------------------- CONFIGURAÇÃO LoRa GPIO
/* 
  CONEXÃO LoRa RA-02:ESP8266
  NSS   GPIO05 - D1
  MOSI  GPIO13 - D7
  MISO  GPIO12 - D6
  SCK   GPIO14 - D5
  RST   GPIO16 - D0
  DIO0  GPIO04 - D2
*/
const int ss = 5;
const int rst = 16;
const int dio0 = 4;

// ----------------------------------------------------------------------- VARIÁVEIS AUXILIARES
// ----------------------------------------------------------------------- MENSAGEM
volatile bool dataReceived = false;
byte localAddress = 0x02;
byte masterAddress = 0x01;
byte broadcast = 0xFF;

// ----------------------------------------------------------------------- PINOUT RELAY
#define LED_BUILTIN 2
const int relay1Pin = 15;

// ----------------------------------------------------------------------- DECLARAÇÃO DAS FUNÇÕES
// ----------------------------------------------------------------------- CONFIGURAÇÃO LoRa
void setupLoRa(long frequency) {
  LoRa.setPins(ss, rst, dio0);                                          // Define configuração LoRa
  while (!LoRa.begin(frequency)) {                                      // Inicializa a comunicação LoRa
    Serial.println(".");
    delay(500);
  }
  LoRa.onReceive(onReceive);
  // LoRa.setTxPower(20);
  // LoRa.setSpreadingFactor(12);
  // LoRa.setSignalBandwidth(62.5E3);
  // LoRa.setCodingRate4(8);
  // LoRa.setSyncWord(0x2A);
  delay(5000);
}

// --------------------------------------------------------------------- SUBROTINA PARA ENVIO DE DADOS
void sendMessage(String outgoing, byte destination) {
  LoRa.beginPacket();                                                 // inicializa o envio dos pacotes
  LoRa.write(destination);                                            // adiciona o endereço de destino
  LoRa.write(localAddress);                                           // adiciona o endereço do emissor
  LoRa.write(outgoing.length());                                      // adiciona comprimento do payload
  LoRa.print(outgoing);                                               // adiciona payload
  LoRa.endPacket();                                                   // finaliza o envio dos pacotes
  LoRa.receive();                                                     // Volta para o modo receive
}

// --------------------------------------------------------------------- SUBROTINA PARA RECEBIMENTO DE DADOS
void onReceive(int packetSize) {
  if (packetSize == 0) return;                                        // se não há pacotes, retorne
  dataReceived = true;
}

// --------------------------------------------------------------------- SUBROTINA PARA PROCESSAR DADOS DA MENSAGEM RECEBIDA
void receivedData(void) {
  // ------------------------------------------------------------------- LENDO BYTES DO CABEÇALHO DO PACOTE
  int recipient = LoRa.read();                                        // endereço do destinatário
  byte sender = LoRa.read();                                          // endereço do remetente
  byte incomingLength = LoRa.read();                                  // comprimento da mensagem
  // ------------------------------------------------------------------- VERIFICA SE A MENSAGEM É ENCAMINHADA A ESTE DESTINATÁRIO
  if(recipient != localAddress && recipient != 0xFF) {
    Serial.println("Essa mensagem não é pra mim.\n");
    return;
  }
  // ------------------------------------------------------------------- PERCORRE CONTEÚDO DA MENSAGEM E ARMAZENA OS DADOS
  String incoming;
  while (LoRa.available()) {
    incoming += (char)LoRa.read();
  } 
  // ------------------------------------------------------------------- VERIFICA SE O COMPRIMENTO RECEBIDO CORRESPONDE AO ENVIADO
  if (incomingLength != incoming.length()) {
    Serial.println("erro: tamanho da mensagem não corresponde\n");
    return;
  }
  // ------------------------------------------------------------------- EXIBE CONTEÚDO DA MENSAGEM
  // Serial.println("Recebido de: 0x" + String(sender, HEX));
  // Serial.println("Enviado para: 0x" + String(recipient, HEX));
  // Serial.println("ID da mensagem: " + String(incomingMsgId));
  // Serial.println("Mensagem: " + incoming);
  // Serial.println("RSSI: " + String(LoRa.packetRssi()));
  // Serial.println("Snr: " + String(LoRa.packetSnr()));
  Serial.print("Recebido LoRa[0x" + String(sender, HEX) + "]: ");
  Serial.println(incoming);

  // Envia ao MQTT Feedback do Estado do Acionamento
  feedback(incoming);
}

// -------------------------------------------------------------------------- SUBROTINA PARA PROCESSAR COMANDOS DO MASTER
void feedback(String command) {
  String feedback;
  
  if (command == "STATE:REQUEST") {
      // Envia o estado atual dos relés
      feedback = "STATE|";
      feedback += "LED:" + String(digitalRead(LED_BUILTIN) ? "ON" : "OFF") + "|";
      feedback += "RELAY1:" + String(digitalRead(relay1Pin) ? "ON" : "OFF");
  } else if (command.startsWith("LED:")) {
    String state = command.substring(4);
    if (state == "ON") {
      digitalWrite(LED_BUILTIN, LOW);
      feedback = "LED:ON";
    } else if (state == "OFF") {
      digitalWrite(LED_BUILTIN, HIGH);
      feedback = "LED:OFF";
    }
  } else if (command.startsWith("SWITCH1:")) {
    String state = command.substring(8);
    if (state == "ON") {
      digitalWrite(relay1Pin, HIGH);
      feedback = "SWITCH1:ON";
    } else if (state == "OFF") {
      digitalWrite(relay1Pin, LOW);
      feedback = "SWITCH1:OFF";
    }
  } 
  
  Serial.print("Enviado LoRa[0x" + String(masterAddress) + "]: ");
  Serial.println(feedback);
  sendMessage(feedback, masterAddress);
}

// --------------------------------------------------------------------- FUNÇÃO SETUP
void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(relay1Pin, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  digitalWrite(relay1Pin, LOW);
  setupLoRa(433E6);
  LoRa.receive();
  Serial.println("\nESP8266 SLAVE iniciado");
}

// --------------------------------------------------------------------- FUNÇÃO LOOP
void loop() {
  if(dataReceived) {
    receivedData();
    dataReceived = false;
  }
}