#include <StringSplitter.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include<stdio.h>
#include<stdlib.h>

WiFiUDP udp;
String req;

unsigned int localPort = 8888;
char packetBuffer[UDP_TX_PACKET_MAX_SIZE + 1];
char cstr[100];

struct Config {
  String espName;
  String wifiName;
  String wifiPass;
  String wifiMode;
};

char _buffer[64];

const char *filename = "/config.txt";
Config config;


void loadConfiguration(const char *filename, Config &config) {
  String espId = "ESP8266-";
  espId += String(random(0xffff), HEX);
  File file = SPIFFS.open(filename, "r");
  if (!file) {
    Serial.println("Erro ao abrir o arquivo!");
    config.espName = espId ;
    config.wifiName = "";
    config.wifiPass = "";
    config.wifiMode = "0";
  } else {
    Serial.println("Arquivo aberto!");
    StaticJsonDocument<512> doc;

    DeserializationError error = deserializeJson(doc, file);
    if (error)
      Serial.println(F("Failed to read file, using default configuration"));

    String teste = doc["espName"];
    if (teste != "") {
      config.espName = doc["espName"] | "nome";
      config.wifiName = doc["wifiName"] | "wifi";
      config.wifiPass = doc["wifiPass"] | "pass";
      config.wifiMode = doc["wifiMode"] | "modo";
    }
  }

  file.close();
}

void saveConfiguration(const char *filename, const Config &config) {
  SPIFFS.remove(filename);

  File f = SPIFFS.open(filename, "w+");
  if (!f) {
    Serial.println("Erro ao abrir o arquivo!");
  } else {
    Serial.println("Arquivo Criado!");
    StaticJsonDocument<256> doc;

    doc["espName"] = config.espName;
    doc["wifiName"] = config.wifiName;
    doc["wifiPass"] = config.wifiPass;
    doc["wifiMode"] = config.wifiMode;

    if (serializeJson(doc, f) == 0) {
      Serial.println(F("Failed to write to file"));
    }
  }
  f.close();
}

void printFile(const char *filename) {
  File file = SPIFFS.open(filename, "r");
  if (!file) {
    Serial.println("Erro ao abrir o arquivo!");
    return;
  } else {
    while (file.available()) {
      int l = file.readBytesUntil('\n', _buffer, sizeof(_buffer));
      _buffer[l] = 0;
      String msg1 = String(_buffer);
      Serial.println(msg1);
    }
  }
  Serial.println();

  file.close();
}

void setup() {
  Serial.begin(115200);
  while (!Serial) continue;

  Serial.println();

  Serial.println("Sistem UDP");

  Serial.println("Abrindo o sistema de arquivos");

  while (!SPIFFS.begin()) {
    Serial.println(F("Falha em abrir o sistema de arquivos!"));
    hold(1000);
  }
  hold(3000);
  pinMode(D1, OUTPUT);
  pinMode(D4, OUTPUT);
  digitalWrite(D1, LOW);
  digitalWrite(D4, HIGH);

  Serial.println();
  Serial.println(F("Carregar as configurações salvas!"));
  loadConfiguration(filename, config);

  Serial.println(F("Recriando/Criando arquivo de configurações!"));
  saveConfiguration(filename, config);

  Serial.println(F("Printando os arquivos!"));
  printFile(filename);

  int erro = 0;

  if (config.wifiMode == "0") {
    Serial.println("Criando rede wifi!");
    if(config.wifiName == ""){
      WiFi.softAP(config.espName, config.wifiPass);
    }else{
      WiFi.softAP(config.wifiName, config.wifiPass);
    }
    
    Serial.println("Wifi name: " + config.wifiName + " Wifi pass: " + config.wifiPass + " Modo '0'");
  } else {
    Serial.println("Conectando ao Wifi!");
    WiFi.mode(WIFI_STA);
    WiFi.begin(config.wifiName, config.wifiPass);
    while (WiFi.status() != WL_CONNECTED) {
      Serial.print('.');
      erro++;
      if (erro == 50) {
        Serial.println("Erro ao conectar ao WiFi, resetando parametros!");
        config.wifiMode = "0";
        config.wifiName = config.espName;
        config.wifiPass = "";
        hold(500);
        while(true)
          ESP.reset();
      }
      hold(500);
    }
  }
  udp.begin(localPort);

  digitalWrite(D4, LOW);


}

void loop() {
  listen();
}

void listen() {
  if (udp.parsePacket() > 0) {
    req = "";
    while (udp.available() > 0) {
      char z = udp.read();
      req += z;
    }
    String comando[5];

    StringSplitter *splitter = new StringSplitter(req, '/', 5);  // new StringSplitter(string_to_split, delimiter, limit)
    int itemCount = splitter->getItemCount();

    for (int i = 0; i < itemCount; i++) {
      comando[i] = splitter->getItemAtIndex(i);
    }

    Serial.println("Comando recebido via UDP: " + req);
    req = "";
    if (comando[0] == config.espName) {
      Serial.println("É pra mim!");
      if (comando[1] == "wifi") {
        enviarUDP("Reconfigurando o wifi");
        config.wifiName = comando[2];
        config.wifiPass = comando[3];
        config.wifiMode = comando[4];
        saveConfiguration(filename, config);
        hold(1000);
        while (true)
          ESP.reset();
      } else if (comando[1] == "nome") {
        enviarUDP("Mudando o nome do ESP");
        config.espName = comando[2];
        saveConfiguration(filename, config);
        hold(1000);
        while (true)
          ESP.reset();
      } else if (comando[1] == "reset") {
        enviarUDP("Resetando Config de Fabrica!");
        SPIFFS.remove(filename);
        ESP.reset();
      } else if (comando[1] == "comando") {
        digitalWrite(D1, !digitalRead(D1));
        enviarUDP("Acionando portao");
        hold(2000);
        digitalWrite(D1, !digitalRead(D1));
      }
    } else if (comando[0] == "REDE") {
      if (config.wifiMode == "0") {
        IPAddress myIP = WiFi.softAPIP();
        enviarUDP(IpAddress2String(myIP));
      } else {
        IPAddress myIP = WiFi.localIP();
        enviarUDP(IpAddress2String(myIP));
      }
    }
    hold(100);
  }
}

void hold(const unsigned int &ms) {
  // Non blocking hold
  unsigned long m = millis();
  while (millis() - m < ms) {
    yield();
  }
}
String IpAddress2String(const IPAddress& ipAddress)
{
  return String(ipAddress[0]) + String(".") + \
         String(ipAddress[1]) + String(".") + \
         String(ipAddress[2]) + String(".") + \
         String(ipAddress[3])  ;
}
void enviarUDP(String msg) {
  String sMsgTemp = "PLACA/" + config.espName + "/";
  sMsgTemp += msg;
  sMsgTemp.toCharArray(cstr, 100);
  udp.beginPacket(udp.remoteIP(), 1515);
  udp.println(cstr);
  udp.endPacket();
  Serial.print("Enviando: "); Serial.println(sMsgTemp);
}
