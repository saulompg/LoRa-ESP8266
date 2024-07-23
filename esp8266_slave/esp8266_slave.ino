// --------------------------------------------------------------------- DIRETIVAS DE PRÉ-PROCESSAMENTO
#include <SPI.h>
#include <LoRa.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// --------------------------------------------------------------------- VARIÁVEIS AUXILIARES
static const int RX = 0x3;
// --------------------------------------------------------------------- LoRa GPIO
const int ss = D0;
const int rst = RX;
const int dio0 = D8;
// --------------------------------------------------------------------- MENSAGEM
volatile bool dataReceived = false;                                   // Acusa o recebimento de dados
byte localAddress = 0x02;                                             // endereço deste dispositivo
byte masterAddress = 0x01;                                            // endereço do master
byte broadcast = 0xFF;                                                // endereço broadcast
// --------------------------------------------------------------------- TIMER
unsigned long lastSendTime = 0;
const int interval = 40000;

// --------------------------------------------------------------------- PINOUT RELAY
const int relay1Pin = D1;
const int relay2Pin = D2;
// SENSOR DE TEMPERATURA DS18B20 
const int oneWireBus = D3;  
// define uma instância do OneWire
OneWire oneWire(oneWireBus);
// Envia a referência do OneWire para o sensor de temperatura Dallas
DallasTemperature sensors(&oneWire);
float temperatura;

// --------------------------------------------------------------------- DECLARAÇÃO DAS FUNÇÕES
// --------------------------------------------------------------------- CONFIGURAÇÃO LoRa
void setupLoRa(long frequency) {
  LoRa.setPins(ss, rst, dio0);                                        // Define configuração LoRa
  if (!LoRa.begin(frequency)) {                                       // Inicializa a comunicação LoRa
    Serial.println("Erro ao iniciar LoRa");
    while(1);
  }
  LoRa.onReceive(onReceive);
  LoRa.setSyncWord(0x2A);
  // LoRa.setTxPower(20);
  // LoRa.setSpreadingFactor(12);
  // LoRa.setSignalBandwidth(62.5E3);
  // LoRa.setCodingRate4(8);
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
    Serial.println("erro no comprimento da mensagem recebida via LoRa.\n");
    return;
  }
  // ------------------------------------------------------------------- EXIBE CONTEÚDO DA MENSAGEM
  // Serial.println("Recebido de: 0x" + String(sender, HEX));
  // Serial.println("Enviado para: 0x" + String(recipient, HEX));
  // Serial.println("ID da mensagem: " + String(incomingMsgId));
  // Serial.println("Mensagem: " + incoming);
  // Serial.println("RSSI: " + String(LoRa.packetRssi()));
  // Serial.println("Snr: " + String(LoRa.packetSnr()));
  Serial.println("Recebido LoRa[0x" + String(sender, HEX) + "]: " + incoming);
  // ------------------------------------------------------------------- Envia ao MQTT Feedback do Estado do Acionamento
  feedback(incoming);
}

// --------------------------------------------------------------------- SUBROTINA PARA PROCESSAR COMANDOS DO MASTER
void feedback(String command) {
  String feedback;
  // ------------------------------------------------------------------- DETERMINA MENSAGEM DE FEEDBACK
  if (command.startsWith("LED:")) { // ---------------------- ENVIA STATUS DO LED
    String state = command.substring(4);
    if (state == "ON") {
      digitalWrite(LED_BUILTIN, LOW);
      feedback = "LED:ON";
    } else if (state == "OFF") {
      digitalWrite(LED_BUILTIN, HIGH);
      feedback = "LED:OFF";
    }
  } else if (command.startsWith("RELAY1:")) { // ---------------------- ENVIA STATUS DO RELAY 1
    String state = command.substring(7);
    if (state == "ON") {
      digitalWrite(relay1Pin, LOW);
      feedback = "RELAY1:ON";
    } else if (state == "OFF") {
      digitalWrite(relay1Pin, HIGH);
      feedback = "RELAY1:OFF";
    }
  } else if (command.startsWith("RELAY2:")) { // ---------------------- ENVIA STATUS DO RELAY 2
    String state = command.substring(7);
    if (state == "ON") {
      digitalWrite(relay2Pin, LOW);
      feedback = "RELAY2:ON";
    } else if (state == "OFF") {
      digitalWrite(relay2Pin, HIGH);
      feedback = "RELAY2:OFF";
    }
  } else if (command == "TEMP:GET") { // ----------------------------- ENVIA VALOR DO SENSOR DE TEMPERATURA
    feedback = "TEMP:" + String(temperatura);
  } else if (command == "STATE:REQUEST") { // --------------------------------- ENVIA STATUS DE TODOS OS DISPOSITIVOS
    feedback = "STATE ";
    feedback += "TEMP:" + String(temperatura) + " ";
    feedback += "LED:" + String(!digitalRead(LED_BUILTIN) ? "ON" : "OFF") + " ";
    feedback += "RELAY1:" + String(!digitalRead(relay1Pin) ? "ON" : "OFF") + " ";
    feedback += "RELAY2:" + String(!digitalRead(relay2Pin) ? "ON" : "OFF");
  } 
  // ------------------------------------------------------------------- ENVIA STATUS VIA LoRa
  Serial.println("Enviado LoRa[0x" + String(masterAddress) + "]: " + feedback);
  Serial.println();
  sendMessage(feedback, masterAddress);
}

// --------------------------------------------------------------------- SOLICITA O ULTIMO ESTADO
void lastState() {
  sendMessage("LAST:STATE", masterAddress);
}

// --------------------------------------------------------------------- FUNÇÃO SETUP
void setup() {
  Serial.begin(115200);                                               // Inicia comunicação Serial
  // ------------------------------------------------------------------- DEFINE PINOS COMO SAÍDA
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(relay1Pin, OUTPUT);
  pinMode(relay2Pin, OUTPUT);
  // ------------------------------------------------------------------- DEFINE ESTADO INICIAL 
  digitalWrite(LED_BUILTIN, HIGH);
  digitalWrite(relay1Pin, HIGH);
  digitalWrite(relay2Pin, HIGH);
  // ------------------------------------------------------------------- INICIA O SENSOR DS18B20
  sensors.begin();
  sensors.requestTemperatures();                                      // Obtém valor de temperatura 
  temperatura = sensors.getTempCByIndex(0);
  // ------------------------------------------------------------------- CONFIGURA E INICIA O LoRa (433 MHz)  
  setupLoRa(433E6);
  LoRa.receive();
  Serial.println("\nESP8266 SLAVE iniciado");
  // ------------------------------------------------------------------- SOLICITA AO MASTER O ÚLTIMO ESTADO DOS ACIONAMENTOS
  lastState();
}

// --------------------------------------------------------------------- FUNÇÃO LOOP
void loop() {
  if(dataReceived) {
    receivedData();
    dataReceived = false;                                             // Atualiza Flag
  }

  if (millis() - lastSendTime >= interval && !dataReceived) {
    // Coleta temperatura do sensor
    sensors.requestTemperatures(); 
    temperatura = sensors.getTempCByIndex(0);
    lastSendTime = millis();
  }
}