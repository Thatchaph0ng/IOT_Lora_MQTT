#include <SPI.h>
#include <RH_RF95.h>
#include <WiFiS3.h>
#include <MQTTClient.h>

#define RFM95_CS 10
#define RFM95_RST 7
#define RFM95_INT 2

// Change to 434.0 or other frequency, must match RX's freq!
#define RF95_FREQ 911.72
const char WIFI_SSID[] = "";          // CHANGE TO YOUR WIFI SSID
const char WIFI_PASSWORD[] = "";  // CHANGE TO YOUR WIFI PASSWORD

const char MQTT_BROKER_ADRRESS[] = "mqtt-dashboard.com";  // CHANGE TO MQTT BROKER'S ADDRESS
const int MQTT_PORT = 1883;
const char MQTT_CLIENT_ID[] = "arduino-uno-r4-client";  // CHANGE IT AS YOU DESIRE
const char MQTT_USERNAME[] = "";                        // CHANGE IT IF REQUIRED, empty if not required
const char MQTT_PASSWORD[] = "";                        // CHANGE IT IF REQUIRED, empty if not required

// The MQTT topics that Arduino should publish/subscribe
const char PUBLISH_TOPIC[] = "arduino-uno-r4/send";       // CHANGE IT AS YOU DESIRE
const char SUBSCRIBE_TOPIC[] = "arduino-uno-r4/receive";  // CHANGE IT AS YOU DESIRE

WiFiClient network;
MQTTClient mqtt = MQTTClient(256);
unsigned long lastPublishTime = 0;
// Singleton instance of the radio driver
RH_RF95 rf95(RFM95_CS, RFM95_INT);

void setup() {
  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH);

  int status = WL_IDLE_STATUS;
  while (status != WL_CONNECTED) {
    Serial.print("Arduino UNO R4 - Attempting to connect to SSID: ");
    Serial.println(WIFI_SSID);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    // wait 10 seconds for connection:
    delay(10000);
  }
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  connectToMQTT();

  while (!Serial)
    ;
  Serial.begin(9600);
  delay(100);

  Serial.println("Arduino LoRa RX Test!");

  // manual reset
  digitalWrite(RFM95_RST, LOW);
  delay(10);
  digitalWrite(RFM95_RST, HIGH);
  delay(10);

  while (!rf95.init()) {
    Serial.println("LoRa radio init failed");
    while (1)
      ;
  }
  Serial.println("LoRa radio init OK!");

  // Defaults after init are 434.0MHz, modulation GFSK_Rb250Fd250, +13dbM
  if (!rf95.setFrequency(RF95_FREQ)) {
    Serial.println("setFrequency failed");
    while (1)
      ;
  }
  Serial.print("Set Freq to: ");
  Serial.println(RF95_FREQ);

  // The default transmitter power is 13dBm, using PA_BOOST.
  rf95.setTxPower(23, false);
}

void loop() {
  if (rf95.available()) {
    // Start measuring time on air
    unsigned long startTime = millis();
    
    // Should be a message for us now
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);
    mqtt.loop();

    if (rf95.recv(buf, &len)) {
      RH_RF95::printBuffer("Received: ", buf, len);

      // End measuring time on air
      unsigned long endTime = millis();
      
      // Calculate time on air
      unsigned long timeOnAir = endTime - startTime;
      
      // Convert received data to string
      char message[len + 1];  // Create a buffer for the message (size len + 1 for null terminator)
      memcpy(message, buf, len);  // Copy data from buf to message
      message[len] = '\0';  // Null terminator to indicate end of string

      Serial.print("Got: ");
      Serial.println(message);  // Print the converted message
      Serial.print("RSSI: ");
      Serial.println(rf95.lastRssi(), DEC);

      // Send RSSI and time on air to MQTT
      sendToMQTT(message, rf95.lastRssi(), timeOnAir);

      // Send a reply to LoRa client
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
  // Connect to the MQTT broker
  mqtt.begin(MQTT_BROKER_ADRRESS, MQTT_PORT, network);

  // Create a handler for incoming messages
  mqtt.onMessage(messageHandler);

  Serial.print("Arduino UNO R4 - Connecting to MQTT broker");

  while (!mqtt.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD)) {
    Serial.print("...");
    delay(100);
  }
  Serial.println();

  if (!mqtt.connected()) {
    Serial.println("Arduino UNO R4 - MQTT broker Timeout!");
    return;
  }

  // Subscribe to a topic, the incoming messages are processed by messageHandler() function
  if (mqtt.subscribe(SUBSCRIBE_TOPIC))
    Serial.print("Arduino UNO R4 - Subscribed to the topic: ");
  else
    Serial.print("Arduino UNO R4 - Failed to subscribe to the topic: ");

  Serial.println(SUBSCRIBE_TOPIC);
  Serial.println("Arduino UNO R4 - MQTT broker Connected!");
}

void sendToMQTT(const char* message, int rssi, unsigned long timeOnAir) {
  // Prepare message to include RSSI and time on air
  String mqttMessage = String("Message: ") + message + 
                        ", RSSI: " + String(rssi) + 
                        ", Time on Air: " + String(timeOnAir) + " ms";
  
  // Send the message to MQTT
  mqtt.publish(PUBLISH_TOPIC, mqttMessage);
  Serial.println("Arduino UNO R4 - sent to MQTT:");
  Serial.print("- topic: ");
  Serial.println(PUBLISH_TOPIC);
  Serial.print("- payload: ");
  Serial.println(mqttMessage);
}

void messageHandler(String &topic, String &payload) {
  Serial.println("Arduino UNO R4 - received from MQTT:");
  Serial.println("- topic: " + topic);
  Serial.println("- payload:");
  Serial.println(payload);

  // You can process the incoming data, then control something
  /*
  process something
  */
}
