#include <WiFi.h>
#include <PubSubClient.h>
#include <MHZ19.h>
#include <ArduinoJson.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <DHT.h>

// Wi-Fi Configuration
const char* ssid = "";
const char* password = "";

// MQTT Configuration
const char* mqtt_server = ""; // update based on the broker IP in this case the raspberrypi
const int mqtt_port = 1883;
const char* mqtt_user = "berlin/loco";  
const char* mqtt_password = "loco";
const char* sensor_topic = "sensors/esp32-01/data";
// mosquitto_sub -h localhost -p 1883 -t "sensors/esp32-01/data" -u "berlin/loco" -P "loco"

// Define Sensor ID
const char* SENSOR_NAME = "ESP32_T_H_Co2";

// Pin Definitions
#define MHZ19_RX 16  // GPIO16 - sensor TX - transmitter
#define MHZ19_TX 17  // GPIO17 - sensor RX - receiver
#define DHT_PIN  18  // GPIO18 for DHT22 - from left to right 4 pins: energy, data transfer, nothing, ground
#define DHT_TYPE DHT22
#define BAUDRATE 9600

// Sensor objects
MHZ19 mhz19;
DHT dht(DHT_PIN, DHT_TYPE);

// Wi-Fi and MQTT Clients
WiFiClient espClient;
PubSubClient client(espClient);

// NTP Client setup
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000); // Adjust the time offset to your timezone in seconds

// Function to connect to Wi-Fi and initialize NTP client
void setup_wifi() {
    Serial.print("Connecting to WiFi: ");
    Serial.println(ssid);

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected!");
    timeClient.begin();
    timeClient.update();
}

// Function to reconnect to MQTT if disconnected
void reconnect_mqtt() {
    while (!client.connected()) {
        Serial.print("Connecting to MQTT broker...");
        if (client.connect("ESP32Client", mqtt_user, mqtt_password)) {
            Serial.println(" Connected!");
        } else {
            Serial.print(" Failed (Error ");
            Serial.print(client.state());
            Serial.println("), retrying in 5 seconds...");
            delay(5000);
        }
    }
}

void setup() {
    Serial.begin(9600);

    // Start MH-Z19D using Serial2 (ESP32 hardware serial)
    Serial2.begin(BAUDRATE, SERIAL_8N1, MHZ19_RX, MHZ19_TX);
    mhz19.begin(Serial2);
    mhz19.autoCalibration(false);

    // Start DHT22
    dht.begin();

    // Connect to Wi-Fi and start NTP client
    setup_wifi();

    // Set MQTT server
    client.setServer(mqtt_server, mqtt_port);
}

void loop() {
    // Reconnect MQTT if disconnected
    if (!client.connected()) {
        reconnect_mqtt();
    }
    client.loop();  // Keep MQTT connection alive

    // Update time from NTP server
    timeClient.update();

    // Read CO₂ from MH-Z19D
    float co2_mhz19 = mhz19.getCO2();

    // Read humidity from DHT22
    float humidity_dht = dht.readHumidity();
    float temp_dht = dht.readTemperature();

    // Get the Unix timestamp in seconds
    unsigned long epochTime = timeClient.getEpochTime();

    // Convert seconds to milliseconds (13 digits)
    unsigned long long eventTime = epochTime * 1000ULL + millis() / 1000;

    // Create JSON message
    StaticJsonDocument<256> message;
    message["sensorId"] = 1000;
    message["eventTime"] = eventTime;
    message["temperature"] = temp_dht;
    message["humidity"] = humidity_dht;
    message["co2"] = co2_mhz19;

    // Serialize JSON to a buffer
    char messageBuffer[256];
    serializeJson(message, messageBuffer);

    // Publish the JSON payload to MQTT
    client.publish(sensor_topic, messageBuffer);

    // Print JSON to Serial Monitor
    Serial.println(messageBuffer);

    delay(30000);  // Send data every 30 seconds
}
