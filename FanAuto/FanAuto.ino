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

//#define SOFT_TX 2
//#define SOFT_RX 3
#define LED_PIN 2
boolean led;
boolean payload_led;
int connected;
int count;  //收到的 Msg 序號

//PTQS
//SoftwareSerial mySerial(SOFT_RX, SOFT_TX); // RX, TX
//unsigned char gucRxBuf[100];
//wifi
WiFiUDP ntpUdp;
NTPClient timeClient(ntpUdp, "time.google.com");

const char* ssid = WIFI_SSID;  //wifi name
const char* password = WIFI_PASS;  //wifi password
const char* mqttServer = "iot.cht.com.tw";  // MQTT伺服器位址
const char* mqttUserName = MQTT_TOKEN;  // 使用者名稱
const char* mqttPwd = MQTT_TOKEN;  // MQTT密碼
const char* clientID = "test";      // 用戶端ID，隨意設定。
const char* SubTopic = SUB_TOPIC;
const char* PubTopic = PUB_TOPIC;

//function define
void setup();
void setup_wifi();
void callback(char* SubTopic, byte* payload, unsigned int length);
void reconnect();
void loop();

WiFiClient espClient;
//PubSubClient client(espClient);
PubSubClient client(mqttServer, 1883, callback, espClient);

unsigned long prevMillis = 0;  // 暫存經過時間（毫秒）
const long interval = 5000;  // 上傳資料的間隔時間，20秒。

//************程式開始時 先初始設定好各項************
void setup() {
  Serial.begin(9600);
  delay(500);
  Serial.println("****setup() Start****");
  setup_wifi(); //連上 wifi
  
  client.setServer(mqttServer, 1883);
  client.setCallback(callback);
  while (!client.connected()) {
    Serial.print("Try Connect MQTT to ");
    Serial.println(mqttServer);
    if (client.connect(clientID, mqttUserName, mqttPwd)) {
      Serial.println("MQTT connected Successful");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(", try again in 5 seconds");
      delay(5000);  // 等5秒之後再重試
    }
  }
  Serial.print("client subscribe: ");
  Serial.println(SubTopic);
  client.subscribe(SubTopic);

  timeClient.begin();
  led = false;
  count = 0;
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);//預設繼電器是HIGH，風扇位於NO，所以預設風扇是關閉的
  sendMsg("reset to close Successful", count++);
  
  Serial.println("****setup() End****");
  Serial.println("");
}

//************************wifi 連線************************
void setup_wifi() {
  delay(10);
  Serial.println("**setup_wifi() Start**");
  Serial.print("Try Connect WiFi to ");
  Serial.print(ssid);
  
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected Susseccful");
  //Serial.print("WiFi localIP address: ");
  //Serial.println(WiFi.localIP());
  Serial.println("**setup_wifi() End**");
}

//************************MQTT 連線 連失敗就過 5 秒重連************************
void reconnect() {
  Serial.println("reconnect() Start...");
  do {
    setup_wifi();
    if (client.connect(clientID, mqttUserName, mqttPwd)) {
      Serial.println("MQTT connected");
      client.subscribe(SubTopic);
    } else {
      Serial.print("failed, rc=");
      Serial.println(client.state());
      Serial.println("try again in 5 seconds");
      delay(500);
    }
    connected = client.connected() && (WiFi.status() == WL_CONNECTED);
  } while(!connected);
  Serial.println("MQTT reconnect() End...");
  Serial.println("");
}

//************************callback: 訂閱 SubTopic************************
void callback(char* SubTopic, byte* payload, unsigned int length) {
  Serial.println();
  Serial.print("Message arrived [");
  Serial.print(SubTopic);
  Serial.print("] ");
  //Serial.println(length);  //length = 43
  
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  Serial.print("Fan is ");
  Serial.print(led);
  Serial.print(", and payload[42] is ");
  payload_led = (int)payload[length-1] - 48;
  Serial.println(payload_led);

  if (payload_led == 1){  //Set 風扇:關->開
    led = true;
    digitalWrite(LED_PIN, LOW);
	  sendMsg("open Successful", count++);
    Serial.println("Set Fan to 1 Successful");
  } else {  //Set 風扇:開->關
    led = false;
    digitalWrite(LED_PIN, HIGH);
	  sendMsg("close Successful", count++);
    Serial.println("Set Fan to 0 Successful");
  }
  
}

//************************用 MQTT 傳資料到 CHT IoT************************
template <typename T>
void sendMsg(String ack, T values) {
  String msgStr = "";      // 暫存MQTT訊息字串
  msgStr = msgStr + "[{\"id\":\"" + "Fan1" + "\",\"time\":\""+timeClient.getFormattedDate()+"\",\"ack\":[\"" + ack + ":" + values + "\"]}]";//2020-05-04T"+timeClient.getFormattedTime()+"\",\"value\":[\"" + values + "\"]}]";

  // 宣告字元陣列
  byte arrSize = msgStr.length() + 1;
  char msg[arrSize];

  Serial.print("Publish message: ");
  Serial.println(msgStr);
  msgStr.toCharArray(msg, arrSize); // 把String字串轉換成字元陣列格式
  client.publish(PubTopic, msg);       // 發布MQTT主題與訊息
}

//************************setup 結束後不停重複 loop 內的程式************************
void loop()
{
  timeClient.update();
  //Serial.print("timeClient.getFormattedDate() = ");
  //Serial.println(timeClient.getFormattedDate());
  Serial.print(".");

  //連上 MQTT
  connected = client.connected() && (WiFi.status() == WL_CONNECTED);
  if (!connected) {
    Serial.println("connect fail.");
    reconnect();
  }
  
  client.loop();  // check suscribe data arrive or not
  
  delay(1000);  //每1秒轉換一次狀態if開then關、if關then開
}
