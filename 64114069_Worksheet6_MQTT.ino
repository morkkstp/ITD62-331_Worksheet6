#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <time.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <ArduinoJson.h>

const char* ssid = "IoTC604";
const char* password = "ccsadmin";
const char* mqtt_server = "192.168.1.112";

float temperature;
float humidity;
DHT dht11(D4, DHT11);

static unsigned long lastTime = 0;
unsigned long timerDelay = 5000;

const int LED_pin = D6;

WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE (50)
char msg[MSG_BUFFER_SIZE];
int value = 0;

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.print("WiFi Connected, IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  String statusled = "";

  for (int i = 0; i < length; i++) {
    statusled += (char)payload[i];
  }

  if (String(topic) == "ledstatus" && statusled == "off_led") {
    digitalWrite(LED_pin, LOW);
  } else if (String(topic) == "ledstatus" && statusled == "on_led") {
    digitalWrite(LED_pin, HIGH);
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.println("Attempting MQTT connection...");
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println("Connected.");
      client.subscribe("dht11value");
      client.subscribe("ledstatus");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  // dht11 Setup
  temperature = 0.0;
  humidity = 0.0;
  dht11.begin();

  // LED Setup
  pinMode(LED_pin, OUTPUT);

  // Initialize a NTPClient to get time and Set offset time in seconds to adjust for your timezone: GMT +7 = 25200
  timeClient.begin();
  timeClient.setTimeOffset(25200);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  unsigned long now = millis();
  if (now - lastMsg > 3000) {
    lastMsg = now;

    temperature = dht11.readTemperature();
    humidity = dht11.readHumidity();

    //Get a timestamp
    timeClient.update();
    time_t epochTime = timeClient.getEpochTime();
    String formattedTime = timeClient.getFormattedTime();
    struct tm* ptm = gmtime((time_t*)&epochTime);
    int currentDay = ptm->tm_mday;
    int currentMonth = ptm->tm_mon + 1;
    int currentYear = ptm->tm_year + 1900;
    String currentDate = String(currentDay) + "-" + String(currentMonth) + "-" + String(currentYear) + " " + String(formattedTime);

    // Create JSON object
    StaticJsonDocument<200> jsonDocument;
    jsonDocument["timestamp"] = currentDate;
    jsonDocument["humidity"] = humidity;
    jsonDocument["temperature"] = temperature;
    jsonDocument["ip_Address"] = mqtt_server;

    // Serialize JSON data to String
    String jsonData;
    serializeJson(jsonDocument, jsonData);

    client.publish("dht11value", jsonData.c_str());
  }
}
