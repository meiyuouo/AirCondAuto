#include "Credentials.h"
#include "Arduino.h"
#include <SoftwareSerial.h>
#include <LWiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#define ARDUINOJSON_ENABLE_PROGMEM 0
#include <ArduinoJson.h>
#include <string.h>
#include <IRremote.h>
#include "LTimer.h"
LTimer timer0(LTIMER_0);//

#define SOFT_TX 2
#define SOFT_RX 3
// [ PTQS1005 RX (white)] to [ Arduino PIN 10/2 ]
// [ PTQS1005 TX (blue)] to [ Arduino PIN 11/3 ]
unsigned int pm25;
unsigned int TVOC;
unsigned int HCHO;
unsigned int CO2;
float temp;
float humi;
SoftwareSerial mySerial(SOFT_RX, SOFT_TX); // RX, TX
unsigned char gucRxBuf[100];

WiFiUDP ntpUdp;
NTPClient timeClient(ntpUdp, "time.google.com");

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASS;
const char* mqttServer = "iot.cht.com.tw";
const char* mqttUserName = MQTT_TOKEN;
const char* mqttPwd =  MQTT_TOKEN;
const char* clientID = "test";
const char* topic = "/v1/device/25093689587/rawdata";
const char* subTopic = "/v2/device/25093689587/sensor/test/command";

unsigned long prevMillis = 0;  // 暫存經過時間（毫秒）
const long interval = 5000;  // 上傳資料的間隔時間，20秒。

WiFiClient espClient;

// IR block
const int buttonPin = 6;     // the number of the pushbutton pin
const int ledPin = 7;
int ledStatus = 0;
int connected;

IRsend irsend;
unsigned int rawData[211] = {2650, 3250, 400, 1250, 400, 450, 400, 1250, 400, 1200, 450, 450, 400, 450, 400, 1200, 450, 400, 450, 1200, 400, 450, 400, 1200, 450, 400, 450, 1200, 400, 1200, 450, 1200, 400, 450, 400, 450, 450, 1200, 400, 450, 450, 400, 450, 1200, 450, 1200, 400, 450, 450, 1200, 400, 450, 400, 1200, 450, 400, 450, 1200, 400, 450, 400, 450, 400, 450, 450, 1200, 450, 1200, 400, 1250, 400, 1200, 450, 1200, 450, 400, 450, 450, 400, 450, 400, 450, 400, 450, 450, 1200, 400, 1250, 400, 450, 450, 400, 450, 400, 450, 1200, 450, 400, 450, 1200, 400, 400, 450, 1200, 450, 400, 450, 400, 450, 1150, 450, 450, 400, 450, 450, 400, 450, 450, 400, 450, 400, 450, 400, 450, 400, 450, 400, 450, 450, 400, 450, 450, 400, 450, 400, 450, 400, 450, 400, 450, 450, 400, 450, 400, 450, 450, 400, 450, 400, 450, 400, 450, 450, 400, 450, 450, 400, 450, 400, 450, 400, 450, 400, 450, 400, 450, 450, 400, 450, 450, 400, 450, 400, 450, 400, 450, 400, 450, 450, 400, 450, 450, 400, 450, 400, 1250, 400, 450, 400, 450, 450, 400, 450, 400, 450, 1200, 450, 400, 450, 450, 400, 1200, 450, 1200, 450, 400, 450, 400, 450, 400, 450};
//unsigned int rawData[67] = {7850,4300,600,500,600,550,550,550,550,550,550,1550,600,1600,600,500,600,550,550,1650,550,1650,550,1650,550,1650,550,550,600,550,550,1600,600,1600,600,500,600,550,550,550,550,1650,550,550,550,550,550,550,550,550,550,1650,600,1600,600,1600,600,500,600,1600,600,1650,550,1650,550,1650,550};
StaticJsonDocument<1024> doc;
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.println("Enter callback");
  Serial.println(topic);
  for (int i = 0; i < length; i++) {
    Serial.print(char(payload[i]));
  }
  Serial.println();
  deserializeJson(doc, payload, length);
  const char* cmd = doc["cmd"];
  Serial.println(cmd);

  if (strcmp(cmd, "owo") == 0) {
    Serial.println("accept owo doing...");
  } else if (strcmp(cmd, "ww") == 0) {
    Serial.println("accept ww doing...");
  } else if (strcmp(cmd, "switch") == 0) {
    digitalWrite(ledPin, HIGH);
    irsend.sendRaw(rawData, 211, 38);
    digitalWrite(ledPin, LOW);
    Serial.println("sending ir code...");
  }

  Serial.println();
  Serial.println();
}
PubSubClient mqtt_client(mqttServer, 1883, callback, espClient);

//wifi 連線
void connect_wifi() {
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connecting...");
    WiFi.begin(ssid, password);
    delay(500);
  }
  Serial.println("WiFi connected");
}

//MQTT 連線 連失敗就過 5 秒重連
void reconnect() {
  do {
    connect_wifi();
    if (mqtt_client.connect(clientID, mqttUserName, mqttPwd)) {
      Serial.println("MQTT connected");
      mqtt_client.subscribe(subTopic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqtt_client.state());
      delay(500);
    }
    connected = mqtt_client.connected() && (WiFi.status() == WL_CONNECTED);
  } while(!connected);
}

//用 MQTT 傳資料到 CHT IoT
template <typename T>
void sendMsg(String id, T values) {

  String msgStr = "";      // 暫存MQTT訊息字串
  msgStr = msgStr + "[{\"id\":\"" + id + "\",\"time\":\"" + timeClient.getFormattedDate() + "\",\"value\":[\"" + values + "\"]}]"; //2020-05-04T"+timeClient.getFormattedTime()+"\",\"value\":[\"" + values + "\"]}]";

  // 宣告字元陣列
  byte arrSize = msgStr.length() + 1;
  char msg[arrSize];

  Serial.print("Publish message: ");
  Serial.println(msgStr);
  msgStr.toCharArray(msg, arrSize); // 把String字串轉換成字元陣列格式
  mqtt_client.publish(topic, msg);       // 發布MQTT主題與訊息
}


void pressBtnToSend() {
  digitalWrite(ledPin, HIGH);
  irsend.sendRaw(rawData, 211, 38);
  digitalWrite(ledPin, LOW);
}

void checkWifiConnected(void *data) {
  if (!connected) {
    ledStatus = !ledStatus;
  } else {
    ledStatus = LOW;
  }
  digitalWrite(ledPin, ledStatus);
}

void setup() {
  Serial.begin(9600);
  mySerial.begin(9600); //PTQS
  
  timer0.begin();//

  pinMode(ledPin, OUTPUT);
  attachInterrupt(buttonPin, pressBtnToSend, RISING);

  timer0.start(500, LTIMER_REPEAT_MODE, checkWifiConnected, NULL);
  
  connect_wifi(); //連上 wifi
  reconnect();
  timeClient.begin();
}


int count;

void loop() {
  timeClient.update();

  //連上 MQTT
  connected = mqtt_client.connected() && (WiFi.status() == WL_CONNECTED);
  if (!connected) {
    Serial.println("disconnected");
    reconnect();
  }

  mqtt_client.loop();

  // 等待20秒
  if (millis() - prevMillis > interval) {
    prevMillis = millis();
    Serial.println("ready to send IR");
    sendMsg("test", count++);
  }
}
