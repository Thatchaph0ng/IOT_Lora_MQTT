#include <SPI.h>
#include <RH_RF95.h>
#include <WiFiS3.h>
#include <MQTTClient.h>

#define RFM95_CS 10
#define RFM95_RST 7
#define RFM95_INT 2

#define RF95_FREQ 911.72
const char WIFI_SSID[] = "";          // CHANGE TO YOUR WIFI SSID
const char WIFI_PASSWORD[] = "";  // CHANGE TO YOUR WIFI PASSWORD

const char MQTT_BROKER_ADDRESS[] = "mqtt-dashboard.com";  // CHANGE TO MQTT BROKER'S ADDRESS
const int MQTT_PORT = 1883;
const char MQTT_CLIENT_ID[] = "arduino-uno-r4-client";  // CHANGE IT AS YOU DESIRE
const char MQTT_USERNAME[] = "";                        // CHANGE IT IF REQUIRED, empty if not required
const char MQTT_PASSWORD[] = "";                        // CHANGE IT IF REQUIRED, empty if not required

const char PUBLISH_TOPIC[] = "arduino-uno-r4/send";       // CHANGE IT AS YOU DESIRE
const char SUBSCRIBE_TOPIC[] = "arduino-uno-r4/receive";  // CHANGE IT AS YOU DESIRE

WiFiClient network;
MQTTClient mqtt = MQTTClient(256);
unsigned long lastPublishTime = 0;
RH_RF95 rf95(RFM95_CS, RFM95_INT);

void setup() {
  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH);

  Serial.begin(9600);
  while (!Serial) ; // Wait for Serial to be ready

  int status = WL_IDLE_STATUS;
  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(WIFI_SSID);
    status = WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    delay(10000);
  }
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  connectToMQTT();

  digitalWrite(RFM95_RST, LOW);
  delay(10);
  digitalWrite(RFM95_RST, HIGH);
  delay(10);

  while (!rf95.init()) {
    Serial.println("LoRa radio init failed");
    while (1) ;
  }
  Serial.println("LoRa radio init OK!");

  if (!rf95.setFrequency(RF95_FREQ)) {
    Serial.println("setFrequency failed");
    while (1) ;
  }
  Serial.print("Set Freq to: ");
  Serial.println(RF95_FREQ);

  rf95.setTxPower(23, false);
}

void loop() {
  mqtt.loop();  // ตรวจสอบข้อความจาก MQTT broker
  
  // ตรวจสอบการรับข้อมูลจาก LoRa
  if (rf95.available()) {
    unsigned long startTime = millis();
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);

    if (rf95.recv(buf, &len)) {
      Serial.print("Raw Received Data: ");
      for (int i = 0; i < len; i++) {
        Serial.print((char)buf[i]);
      }
      Serial.println();

      unsigned long endTime = millis();
      unsigned long timeOnAir = endTime - startTime;

      char message[len + 1];
      memcpy(message, buf, len);
      message[len] = '\0';

      Serial.print("Got: ");
      Serial.println(message);
      Serial.print("RSSI: ");
      Serial.println(rf95.lastRssi(), DEC);

      sendToMQTT(message, rf95.lastRssi(), timeOnAir);

      uint8_t data[] = "Hello Mark";
      rf95.send(data, sizeof(data));
      rf95.waitPacketSent();
      Serial.println("Sent a reply");
    } else {
      Serial.println("Receive failed");
    }
  }
}


void connectToMQTT() {
  mqtt.begin(MQTT_BROKER_ADDRESS, MQTT_PORT, network);
  mqtt.onMessage(messageHandler);

  Serial.print("Connecting to MQTT broker");

  while (!mqtt.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD)) {
    Serial.print(".");
    delay(100);
  }
  Serial.println();

  if (!mqtt.connected()) {
    Serial.println("MQTT broker Timeout!");
    return;
  }

  if (mqtt.subscribe(SUBSCRIBE_TOPIC))
    Serial.print("Subscribed to the topic: ");
  else
    Serial.print("Failed to subscribe to the topic: ");

  Serial.println(SUBSCRIBE_TOPIC);
  Serial.println("MQTT broker Connected!");
}

void sendToMQTT(const char* message, int rssi, unsigned long timeOnAir) {
  String mqttMessage = String("Message: ") + message + 
                        ", RSSI: " + String(rssi) + 
                        ", Time on Air: " + String(timeOnAir) + " ms";
  
  mqtt.publish(PUBLISH_TOPIC, mqttMessage);
  Serial.println("Sent to MQTT:");
  Serial.print("- topic: ");
  Serial.println(PUBLISH_TOPIC);
  Serial.print("- payload: ");
  Serial.println(mqttMessage);
}

void messageHandler(String &topic, String &payload) {
  Serial.println("Received from MQTT:");
  Serial.println("- topic: " + topic);
  Serial.println("- payload:");
  Serial.println(payload);

  // Send the MQTT payload as a LoRa message
  sendLoRaMessage(payload);
}

void sendLoRaMessage(const String &message) {
  // สร้าง buffer สำหรับข้อความ LoRa
  char messageBuffer[RH_RF95_MAX_MESSAGE_LEN];
  message.toCharArray(messageBuffer, sizeof(messageBuffer));  // แปลง String เป็น char array
  
  // ส่งข้อความผ่าน LoRa
  rf95.send((uint8_t*)messageBuffer, strlen(messageBuffer));  // ส่งเฉพาะความยาวของสตริง
  rf95.waitPacketSent();  // รอให้ข้อความถูกส่ง
  
  Serial.print("Sent LoRa message: ");
  Serial.println(messageBuffer);  // แสดงผลข้อความที่ถูกส่ง
}
