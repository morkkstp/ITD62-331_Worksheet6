#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <time.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>

ESP8266WebServer server(80);

const char* ssid = "IoTC604";
const char* password = "ccsadmin";
const char* mqtt_server = "192.168.1.112";

float temperature;
float humidity;
DHT dht11(D4, DHT11);

static unsigned long lastTime = 0;
unsigned long timerDelay = 5000;

const int light = D6;
bool ledState = false;

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

  // Extract payload to stateParam
  for (int i = 0; i < length; i++) {
    statusled += (char)payload[i];
  }

  // Print received payload for debugging
  Serial.println("Received payload: " + statusled);

  // Check topic and update LED state
  if (String(topic) == "ledstatus") {
    if (statusled == "on_led") {
      digitalWrite(light, HIGH);
    } else if (statusled == "off_led") {
      digitalWrite(light, LOW);
    }
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
  server.begin();
  Serial.println("HTTP server started");

  server.on(F("/"), HTTP_GET, []() {
    String html = "<html><head>";
    html += "<style> body { font-size: 20px; text-align: center; display: flex; flex-direction: column; align-items: center; justify-content: center; height: 100vh; } </style>";
    html += "<script>";
    html += "function toggleLight() {";
    html += "  var xhr = new XMLHttpRequest();";
    html += "  xhr.open('GET', '/light', true);";
    html += "  xhr.send();";
    html += "  xhr.onload = function() {";
    html += "    location.reload();";
    html += "  };";
    html += "}";
    html += "</script>";
    html += "</head><body>";
    html += "<h1>Control LED</h1>";

    if (ledState) {
      html += "<button onclick='toggleLight()' style='font-size: 24px;'>Toggle ON/OFF LED</button>&nbsp;";
    } else {
      html += "<button onclick='toggleLight()' style='font-size: 24px;'>Toggle ON/OFF LED</button>&nbsp;";
    }

    html += "</body></html>";

    server.send(200, "text/html", html);
  });

  server.on("/light", HTTP_GET, []() {
    ledState = !ledState;

    if (ledState) {
      client.publish("ledstatus", "on_led");
    } else {
      client.publish("ledstatus", "off_led");
    }

    server.send(200, "text/plain", "OK");
  });

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  temperature = 0.0;
  humidity = 0.0;
  dht11.begin();

  pinMode(light, OUTPUT);

  // Initialize a NTPClient to get time
  timeClient.begin();

  // Set offset time in seconds to adjust for your timezone: GMT +7 = 25200
  timeClient.setTimeOffset(25200);
}

void loop() {
  server.handleClient();

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
