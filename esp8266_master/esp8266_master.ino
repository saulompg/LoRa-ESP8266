// --------------------------------------------------------------------- DIRETIVAS DE PRÉ-PROCESSAMENTO
#include <SPI.h>
#include <LoRa.h>
#include <WiFiManager.h>
#include <PubSubClient.h>

// --------------------------------------------------------------------- VARIÁVEIS AUXILIARES
static const int RX = 0x3;
// --------------------------------------------------------------------- WIFI
IPAddress ip_esp(192,168,1,20);
IPAddress gateway(192,168,1,1);
IPAddress mask(255,255,255,0);
// --------------------------------------------------------------------- MQTT
// IPAddress mqtt_server(192, 168, 1, 10);
IPAddress mqtt_server(192, 168, 188, 248); 
const int mqtt_port = 1883;
const char* mqtt_user = "esp_mqtt";
const char* mqtt_password = "4u70m4c40ESP";
WiFiClient esp_client;
PubSubClient client(esp_client);
// --------------------------------------------------------------------- TÓPICOS AVAILABLE
const char* topic_esp_available = "homeassistant/master/available";
// --------------------------------------------------------------------- TÓPICOS SUBSCRIBER
const char* topic_led_set = "homeassistant/slave/led/set";
const char* topic_relay1_set = "homeassistant/slave/relay/1/set";
const char* topic_relay2_set = "homeassistant/slave/relay/2/set";
// --------------------------------------------------------------------- TÓPICOS PUBLISHER
const char* topic_temp_value = "homeassistant/slave/sensor/ds18b20/get";
const char* topic_led_state = "homeassistant/slave/led/state";
const char* topic_relay1_state = "homeassistant/slave/relay/1/state";
const char* topic_relay2_state = "homeassistant/slave/relay/2/state";
// --------------------------------------------------------------------- LoRa GPIO
const int ss = D0;
const int rst = RX;
const int dio0 = D8;
// --------------------------------------------------------------------- MENSAGEM
volatile bool dataReceived = false;                                   // Acusa o recebimento de dados
byte localAddress = 0x01;                                             // endereço deste dispositivo
byte slaveAddress = 0x02;                                             // endereço do slave 1
byte broadcast = 0xFF;                                                // endereço broadcast
// --------------------------------------------------------------------- TIMER
unsigned long lastSendTime = 0;
const int interval = 120000;

// --------------------------------------------------------------------- DECLARAÇÃO DAS FUNÇÕES
// --------------------------------------------------------------------- CONFIGURAÇÃO WiFi
void setupWiFi(void) {
  WiFiManager wm;
  // wm.setSTAStaticIPConfig(ip_esp, gateway, mask);
  bool res = wm.autoConnect("ESP_MASTER", "automacaoESP");
  // ------------------------------------------------------------------- Verifica conexão WiFi
  if (!res) {
    Serial.println("Falha ao conectar");
    while(1);                                                         // Reinicia o ESP8266
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
  // ------------------------------------------------------------------- Converte Array de Bytes em String
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  // ------------------------------------------------------------------- Exibe o tópico e a mensagem recebida
  Serial.print("Recebido MQTT[");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(message);
  // ------------------------------------------------------------------- Identifica no tópico para quem o comando será enviado
  String command;
  if (String(topic) == topic_led_set) {
    command = "LED:" + message;
  } else if (String(topic) == topic_relay1_set) {
    command = "RELAY1:" + message;
  } else if (String(topic) == topic_relay2_set) {
    command = "RELAY2:" + message;
  }
  // ------------------------------------------------------------------- Envia o comando para o Slave via LoRa
  if (!command.isEmpty()) {
    Serial.print("Enviado LoRa[0x" + String(slaveAddress) + "]: ");
    Serial.println(command);
    sendMessage(command, slaveAddress);
  }
  digitalWrite(LED_BUILTIN, LOW);
  delay(50);
  digitalWrite(LED_BUILTIN, HIGH);
}

void topicSubscribe(void) {
  client.subscribe(topic_led_set);
  client.subscribe(topic_relay1_set);
  client.subscribe(topic_relay2_set);
}

// --------------------------------------------------------------------- CONEXÃO MQTT
void connectMQTT(void) {
  while(!client.connected()) {
    Serial.print("Estabelecendo conexão com o servidor: ");
    if(client.connect("ESP8266Master", mqtt_user, mqtt_password)) {
      Serial.println("Conectado!");
      client.publish(topic_esp_available, "online");
      topicSubscribe();
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
  if (!LoRa.begin(frequency)) {                                       // Inicializa a comunicação LoRa
    Serial.println("Erro ao iniciar LoRa");
    while (1);
  }
  LoRa.onReceive(onReceive);                                          // Define o que será realizado ao receber uma mensagem LoRa
  LoRa.setSyncWord(0x2A);
  // LoRa.setTxPower(20);
  // LoRa.setSpreadingFactor(12);
  // LoRa.setSignalBandwidth(62.5E3);
  // LoRa.setCodingRate4(8);
  delay(5000);
}

// --------------------------------------------------------------------- SUBROTINA PARA ENVIO DE DADOS
void sendMessage(String outgoing, byte address) {
  LoRa.beginPacket();                                                 // inicializa o envio dos pacotes
  LoRa.write(address);                                                // adiciona o endereço de destino
  LoRa.write(localAddress);                                           // adiciona o endereço do emissor
  LoRa.write(outgoing.length());                                      // adiciona comprimento do payload
  LoRa.print(outgoing);                                               // adiciona payload
  LoRa.endPacket();                                                   // finaliza o envio dos pacotes
  LoRa.receive();                                                     // Volta para o modo receive
}

// --------------------------------------------------------------------- SUBROTINA PARA RECEBIMENTO DE DADOS
void onReceive(int packetSize) {
  if (packetSize == 0) return;                                        // se não há pacotes, retorne
  dataReceived = true;                                                // atualiza a flag
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
    Serial.println("erro no comprimento da mensagem recebida via LoRa");
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
  // ------------------------------------------------------------------- Envia ao MQTT Feedback do Estado do Acionamento
  feedback(incoming);
}

// --------------------------------------------------------------------- ATUALIZA ESTADO NO DASHBOARD VIA MQTT
void feedback(String message) {
  String state, topic;
  if (message == "LAST:STATE") {
    topicSubscribe();
  } else if (message.startsWith("LED:")) { // -------------------------- RECEBE STATUS DO LED
    state = message.substring(4);
    topic = topic_led_state;
  } else if (message.startsWith("RELAY1:")) { // ----------------------- RECEBE STATUS DO RELAY 1
    state = message.substring(7);
    topic = topic_relay1_state;
  } else if (message.startsWith("RELAY2:")) { // ----------------------- RECEBE STATUS DO RELAY 2
    state = message.substring(7);
    topic = topic_relay2_state;
  } else if (message.startsWith("TEMP:")) { // ------------------------- RECEBE VALOR DO SENSOR DE TEMPERATURA
    state = message.substring(5);
    topic = topic_temp_value;
  } else if(message.startsWith("STATE")) { // -------------------------- RECEBE STATUS DE TODOS OS DISPOSITIVOS
    // ----------------------------------------------------------------- Identifica Index de cada item
    int temp_index = message.indexOf("TEMP:") + 5;
    int led_index = message.indexOf("LED:") + 4;
    int relay1_state_index = message.indexOf("RELAY1:") + 7;
    int relay2_state_index = message.indexOf("RELAY2:") + 7;
    // ----------------------------------------------------------------- Identifica Status de cada item
    String temp_value = message.substring(temp_index, message.indexOf(" ", temp_index));
    String led_value = message.substring(led_index, message.indexOf(" ", led_index));
    String relay1_state = message.substring(relay1_state_index, message.indexOf(" ", relay1_state_index));
    String relay2_state = message.substring(relay2_state_index, message.indexOf(" ", relay2_state_index));
    // ----------------------------------------------------------------- Envia Status de cada item via MQTT
    sendMQTT(String(topic_temp_value), temp_value);
    sendMQTT(String(topic_led_state), led_value);
    sendMQTT(String(topic_relay1_state), relay1_state);
    sendMQTT(String(topic_relay2_state), relay2_state);
    Serial.println();
    return;
  }
  // ------------------------------------------------------------------- ENVIA STATUS VIA MQTT
  if(!topic.isEmpty() && !state.isEmpty()) {
    sendMQTT(topic, state);
    Serial.println();
  }
}

void sendMQTT(String topic, String msg) {
  Serial.println("Enviado MQTT[" + topic + "]: " + msg);
  client.publish(topic.c_str(), msg.c_str());
}

// --------------------------------------------------------------------- FUNÇÃO SETUP
void setup() {
  Serial.begin(115200);                                               // Inicia comunicação Serial
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
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
  // ------------------------------------------------------------------- SOLICITA AO SLAVE O STATUS DOS DISPOSITIVOS
  if (millis() - lastSendTime >= interval && !dataReceived) {
    Serial.println("Solicitando dados do ESP8266 Slave");
    String message = "TEMP:GET";
    sendMessage(message, slaveAddress);
    lastSendTime = millis();
  }
  // ------------------------------------------------------------------- EXECUTA AO RECEBER UMA MENSAGEM
  if (dataReceived) {
    receivedData();
    dataReceived = false;
  }
}