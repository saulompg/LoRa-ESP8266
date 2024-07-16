// --------------------------------------------------------------------- DIRETIVAS DE PRÉ-PROCESSAMENTO
#include <SPI.h>
#include <LoRa.h>
#include <WiFiManager.h>
#include <PubSubClient.h>

// --------------------------------------------------------------------- VARIÁVEIS AUXILIARES
// --------------------------------------------------------------------- WIFI
IPAddress ip_esp(192,168,1,20);
IPAddress gateway(192,168,1,1);
IPAddress mask(255,255,255,0);
// --------------------------------------------------------------------- MQTT
IPAddress mqtt_server(192, 168, 1, 10);
const int mqtt_port = 1883;
const char* mqtt_user = "esp_mqtt";
const char* mqtt_password = "4u70m4c40ESP";
WiFiClient esp_client;
PubSubClient client(esp_client);
// --------------------------------------------------------------------- TÓPICOS AVAILABLE
const char* topic_esp_available = "homeassistant/master/available";
// --------------------------------------------------------------------- TÓPICOS SUBSCRIBER
const char* topic_led_set = "homeassistant/slave/led/set";
const char* topic_switch1_set = "homeassistant/slave/switch/1/set";
// --------------------------------------------------------------------- TÓPICOS PUBLISHER
const char* topic_led_state = "homeassistant/slave/led/state";
const char* topic_switch1_state = "homeassistant/slave/switch/1/state";
// --------------------------------------------------------------------- CONFIGURAÇÃO LoRa GPIO
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
// --------------------------------------------------------------------- MENSAGEM
volatile bool dataReceived = false;                                     // Acusa o recebimento de dados
byte localAddress = 0x01;                                               // endereço deste dispositivo
byte slaveAddress = 0x02;                                               // endereço do slave 1
byte broadcast = 0xFF;                                                  // endereço broadcast
// --------------------------------------------------------------------- TIMER
unsigned long lastSendTime = 0;
const int interval = 30000;

// --------------------------------------------------------------------- DECLARAÇÃO DAS FUNÇÕES
// --------------------------------------------------------------------- CONFIGURAÇÃO WiFi
void setupWiFi(void) {
  WiFiManager wm;
  wm.setSTAStaticIPConfig(ip_esp, gateway, mask);
  bool res = wm.autoConnect("ESP_MASTER", "automacaoESP");
  
  if (!res) {
    Serial.println("Falha ao conectar");
    // Considerar reiniciar o ESP se não conseguir conectar após várias tentativas
    // ESP.restart();
  } else {
    Serial.println("Conexão bem sucedida");
  }
}

// --------------------------------------------------------------------- CONFIGURAÇÃO MQTT
void setupMQTT(void) {
  client.setServer(mqtt_server, mqtt_port);                           // Define endereço do servidor e porta
  client.setCallback(callback);                                       // Define função callback
}

// --------------------------------------------------------------------- CALLBACK MQTT
void callback(char* topic, byte* payload, unsigned int length) {
  // Converte Array de Bytes em String
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  // Exibe o tópico e a mensagem recebida
  Serial.print("Recebido MQTT[");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(message);

  // Identifica no tópico para quem o comando será enviado
  String command;
  if (String(topic) == topic_led_set) {
    command = "LED:" + message;
  } else if (String(topic) == topic_switch1_set) {
    command = "SWITCH1:" + message;
  }

  // Envia o comando para o Slave via LoRa
  if (!command.isEmpty()) {
    Serial.print("Enviado LoRa[0x" + String(slaveAddress) + "]: ");
    Serial.println(command);
    sendMessage(command, slaveAddress);
  }
}

// --------------------------------------------------------------------- CONEXÃO MQTT
void connectMQTT(void) {
  while(!client.connected()) {
    Serial.print("Estabelecendo conexão com o servidor: ");
    if(client.connect("ESP8266Master", mqtt_user, mqtt_password)) {
      Serial.println("Conectado!");
      client.publish(topic_esp_available, "online");
      client.subscribe(topic_led_set);
      client.subscribe(topic_switch1_set);
    } else {
      Serial.print("Falha, rc=");
      Serial.print(client.state());
      Serial.println(" tentando novamente em 5 segundos");
      delay(5000);
    }
  }
}

// --------------------------------------------------------------------- CONFIGURAÇÃO LoRa
void setupLoRa(long frequency) {
  LoRa.setPins(ss, rst, dio0);                                        // Define configuração LoRa
  if (!LoRa.begin(frequency)) {                                    // Inicializa a comunicação LoRa
    Serial.println("Erro ao iniciar LoRa");
    while (1);
  }
  LoRa.onReceive(onReceive);                                          // Define o que será realizado ao receber uma mensagem LoRa
  // LoRa.setTxPower(20);
  // LoRa.setSpreadingFactor(12);
  // LoRa.setSignalBandwidth(62.5E3);
  // LoRa.setCodingRate4(8);
  // LoRa.setSyncWord(0x2A);
  
  delay(5000);
}

// --------------------------------------------------------------------- SUBROTINA PARA ENVIO DE DADOS
void sendMessage(String outgoing, byte address) {
  LoRa.beginPacket();                                                 // inicializa o envio dos pacotes
  LoRa.write(address);                                            // adiciona o endereço de destino
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
    Serial.println("Erro: tamanho da mensagem não corresponde\n");
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

// --------------------------------------------------------------------- ATUALIZA ESTADO NO DASHBOARD VIA MQTT
void feedback(String message) {
  String state;
  String topic;
  if (message.startsWith("LED:")) {
    state = message.substring(4);
    topic = topic_led_state;
  } else if (message.startsWith("SWITCH1:")) {
    state = message.substring(8);
    topic = topic_switch1_state;
  } else if (message.startsWith("STATE")) {
    Serial.println(message);
  }

  if(!topic.isEmpty() && !state.isEmpty()) {
    Serial.println("Enviado MQTT[" + topic + "]: " + state);
    Serial.println();
    client.publish(topic.c_str(), state.c_str());
  }
}

// --------------------------------------------------------------------- FUNÇÃO SETUP
void setup() {
  Serial.begin(115200);                                               // Inicia comunicação Serial
  setupWiFi();                                                        // Configura Access Point ou Conecta à rede 
  setupMQTT();                                                        // Define servidor MQTT a ser conectado e função callback ao receber mensagem
  setupLoRa(433E6);                                                   // Configura e inicia o LoRa (433 MHz ou 915 MHz)
  LoRa.receive();                                                     // Coloca o LoRa em modo receptor
  Serial.println("\nESP8266 MASTER iniciado");
}

// --------------------------------------------------------------------- FUNÇÃO LOOP
void loop() {
  // ------------------------------------------------------------------- ESTABELECE CONEXÃO COM O SERVIDOR MQTT
  if(!client.connected()) {
    connectMQTT();
  }
  // ------------------------------------------------------------------- MANTÉM INSTÂNCIA DO MQTT
  client.loop();
  // ------------------------------------------------------------------- SOLICITA AOS SLAVES O STATUS DOS DISPOSITIVOS
  if (millis() - lastSendTime >= interval && !dataReceived) {
    Serial.println("Solicitando dados do ESP8266 Slave\n");
    String message = "STATE:REQUEST";
    sendMessage(message, slaveAddress);
    lastSendTime = millis();
  }
  // ------------------------------------------------------------------- EXECUTA AO RECEBER UMA MENSAGEM
  if (dataReceived) {
    receivedData();
    dataReceived = false;
  }
}