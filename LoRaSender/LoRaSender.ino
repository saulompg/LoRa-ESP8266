// -------------------------------------------------------------------------- DIRETIVAS DE PRÉ-PROCESSAMENTO
#include <SPI.h>
#include <LoRa.h>
#include <string>
#include <WiFiManager.h>
#include <PubSubClient.h>

#define LED_BUILTIN 2

// -------------------------------------------------------------------------- VARIÁVEIS AUXILIARES
// -------------------------------------------------------------------------- WIFI
IPAddress ip_esp(192,168,1,11);
IPAddress gateway(192,168,1,1);
IPAddress mask(255,255,255,0);
// -------------------------------------------------------------------------- MQTT
IPAddress mqtt_server(192, 168, 1, 10);
const int mqtt_port = 1883;
const char* mqtt_user = "esp_mqtt";
const char* mqtt_password = "4u70m4c40ESP";
const char* topic_master_available = "homeassistant/master/available";
const char* topic_slave_led_set = "homeassistant/master/slave/led/set";
const char* topic_slave_led_state = "homeassistant/master/slave/led/state";
WiFiClient esp_client;
PubSubClient client(esp_client);
// -------------------------------------------------------------------------- CONFIGURAÇÃO LoRa GPIO
/* 
  CONEXÃO LoRa RA-02:ESP8266
  NSS   GPIO15 - D8
  MOSI  GPIO13 - D7
  MISO  GPIO12 - D6
  SCK   GPIO14 - D5
  RST   GPIO4  - D2
  DIO0  GPIO5  - D1
*/
const int ss = 15;
const int rst = 4;
const int dio0 = 5;
// -------------------------------------------------------------------------- MENSAGEM
volatile bool dataReceived = false;                                        // Acusa o recebimento de dados
String incoming;
String outgoing;                                                           // String de saída
byte msgCount = 0;                                                         // ID da mensagem
byte localAddress = 0x01;                                                  // endereço deste dispositivo
byte slave1Address = 0x02;                                                 // endereço do slave 1
byte slave2Address = 0x03;                                                 // endereço do slave 2
byte broadcast = 0xFF;                                                     // endereço broadcast
byte slv = 0;                                                              // escravo selecionado
byte led_state = 0x00;
// -------------------------------------------------------------------------- TIMER
unsigned long lastSendTime = 0;
const int interval = 5000;

// -------------------------------------------------------------------------- DECLARAÇÃO DAS FUNÇÕES
// -------------------------------------------------------------------------- CONFIGURAÇÃO WiFi
void setupWiFi(void) {
  WiFiManager wm;
  // wm.setSTAStaticIPConfig(ip_esp, gateway, mask);
  bool res = wm.autoConnect("ESP_MASTER", "automacaoESP32");
  
  if (!res) {
    Serial.println("Falha ao conectar");
    // Considerar reiniciar o ESP se não conseguir conectar após várias tentativas
    // ESP.restart();
  } else {
    Serial.println("Conexão bem sucedida");
  }
}

// -------------------------------------------------------------------------- CONEXÃO MQTT
void connectMQTT(void) {
  while(!client.connected()) {
    Serial.print("Estabelecendo conexão com o servidor: ");
    if(client.connect("ESP8266Client", mqtt_user, mqtt_password)) {
      Serial.println("Conectado!");
      client.publish(topic_master_available, "online");
      client.subscribe(topic_slave_led_set);
    } else {
      Serial.print("Falha, rc=");
      Serial.print(client.state());
      Serial.println(" tentando novamente em 5 segundos");
      delay(5000);
    }
  }
}

// -------------------------------------------------------------------------- CALLBACK MQTT
void callback(char* topic, byte* payload, unsigned int length) {
  // Converte Array de Bytes em String e exibe conteúdo da mensagem
  std::string incoming(reinterpret_cast<char const*>(payload), length);
  Serial.print("Mensagem recebida [");
  Serial.print(topic);
  Serial.printf("] : %s\n", incoming.c_str());

  if(String(topic) == topic_slave_led_set) {
    if (incoming == "ON") {
      sendMessage(incoming.c_str(), slave1Address);
    } else if (incoming == "OFF"){
      sendMessage(incoming.c_str(), slave1Address);
    }
  }
}

// -------------------------------------------------------------------------- CONFIGURAÇÃO LoRa
void setupLoRa(long frequency) {
  LoRa.setPins(ss, rst, dio0);                                             // Define configuração LoRa
  while (!LoRa.begin(frequency)) {                                         // Inicializa a comunicação LoRa
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

// -------------------------------------------------------------------------- SUBROTINA PARA ENVIO DE DADOS
void sendMessage(String outgoing, byte destination) {
  LoRa.beginPacket();                                                      // inicializa o envio dos pacotes
  LoRa.write(destination);                                                 // adiciona o endereço de destino
  LoRa.write(localAddress);                                                // adiciona o endereço do emissor
  LoRa.write(msgCount);                                                    // adiciona ID da mensagem
  LoRa.write(outgoing.length());                                           // adiciona comprimento do payload
  LoRa.print(outgoing);                                                    // adiciona payload
  LoRa.endPacket();                                                        // finaliza o envio dos pacotes
  msgCount++;                                                              // incrementa ID da mensagem
}

// -------------------------------------------------------------------------- SUBROTINA PARA RECEBIMENTO DE DADOS
void onReceive(int packetSize) {
  if (packetSize == 0) return;                                             // se não há pacotes, retorne
  dataReceived = true;
}

// -------------------------------------------------------------------------- SUBROTINA PARA PROCESSAR DADOS DA MENSAGEM RECEBIDA
void showData(void) {
  // ------------------------------------------------------------------------ LENDO BYTES DO CABEÇALHO DO PACOTE
  int recipient = LoRa.read();                                             // endereço do destinatário
  byte sender = LoRa.read();                                               // endereço do remetente
  byte incomingMsgId = LoRa.read();                                        // ID da mensagem
  byte incomingLength = LoRa.read();                                       // comprimento da mensagem
  // ------------------------------------------------------------------------ PERCORRE CONTEÚDO DA MENSAGEM E ARMAZENA OS DADOS
  incoming = "";
  while (LoRa.available()) {
    incoming += (char)LoRa.read();
  } 
  // ------------------------------------------------------------------------ VERIFICA SE O COMPRIMENTO RECEBIDO CORRESPONDE AO ENVIADO
  if (incomingLength != incoming.length()) {
    Serial.println("erro: tamanho da mensagem não corresponde\n");
    return;
  }
  // ------------------------------------------------------------------------ VERIFICA SE A MENSAGEM É ENCAMINHADA A ESTE DESTINATÁRIO
  if(recipient != localAddress && recipient != 0xFF) {
    Serial.println("Essa mensagem não é pra mim.\n");
    return;
  }
  // ------------------------------------------------------------------------ EXIBE CONTEÚDO DA MENSAGEM
  Serial.println("Recebido de: 0x" + String(sender, HEX));
  // Serial.println("Enviado para: 0x" + String(recipient, HEX));
  // Serial.println("ID da mensagem: " + String(incomingMsgId));
  Serial.println("Mensagem: " + incoming);
  // Serial.println("RSSI: " + String(LoRa.packetRssi()));
  // Serial.println("Snr: " + String(LoRa.packetSnr()));
  Serial.println();
}

// -------------------------------------------------------------------------- CONFIGURA GPIO LED
void setupLed(uint8_t LED, uint8_t STATUS) {
  pinMode(LED, OUTPUT);
  digitalWrite(LED, STATUS);
}
// -------------------------------------------------------------------------- PISCA O LED
void ledBlink(uint8_t LED) {
  digitalWrite(LED, !digitalRead(LED));
  delay(200);
  digitalWrite(LED, !digitalRead(LED));
}
// -------------------------------------------------------------------------- ALTERA ESTADO DO LED
void ledToogle(uint8_t LED) {
  digitalWrite(LED, !digitalRead(LED));
}

// -------------------------------------------------------------------------- FUNÇÃO SETUP
void setup() {
  Serial.begin(115200);                                               // Inicia comunicação Serial
  setupWiFi();                                                        // Configura Access Point ou Conecta à rede 
  client.setServer(mqtt_server, mqtt_port);                           // Define endereço do servidor e porta
  client.setCallback(callback);                                       // Define função callback
  setupLed(LED_BUILTIN, HIGH);                                        // Define LED pin como saída
  setupLoRa(433E6);                                                   // Configura e inicia o LoRa (433 MHz ou 915 MHz)
  LoRa.onReceive(onReceive);                                          // Define o que será realizado ao receber uma mensagem LoRa
  LoRa.receive();                                                     // Coloca o LoRa em modo receptor
  Serial.println("\nESP8266 MASTER - iniciado com sucesso!");
}

// -------------------------------------------------------------------------- FUNÇÃO LOOP
void loop() {
  // ------------------------------------------------------------------------ ESTABELECE CONEXÃO COM O SERVIDOR MQTT
  if(!client.connected()) {
    connectMQTT();
  }
  // ------------------------------------------------------------------------ MANTÉM INSTÂNCIA DO MQTT
  client.loop();
  // ------------------------------------------------------------------------ SOLICITA AOS SLAVES O STATUS DOS DISPOSITIVOS
  if (millis() - lastSendTime >= interval && !dataReceived) {
    Serial.println("Solicitando dados do ESP8266 Slave\n");
    String message = "GET_STATE";
    sendMessage(message, slave1Address);
    LoRa.receive();                                                        // Volta para o modo receive
    // ledBlink(LED_BUILTIN);
    lastSendTime = millis();
  }
  // ------------------------------------------------------------------------ EXECUTA AO RECEBER UMA MENSAGEM
  if (dataReceived) {
    showData();
    if(incoming == "LED STATE: 1") {
      client.publish(topic_slave_led_state, "ON");
    } else {
      client.publish(topic_slave_led_state, "OFF");
    }
    dataReceived = false;
  }
  // ------------------------------------------------------------------------ 

}