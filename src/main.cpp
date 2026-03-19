#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <PubSubClient.h>
#include <ESPmDNS.h>
#include "DHT.h"
#include <Preferences.h>
#include <LittleFS.h>

// AP fallback credentials
const char* ap_ssid = "ESP32-DHT";
const char* ap_password = "12345678";

bool mqttConfigured = false;
bool mqttEnabled = false;
bool staConnected = false;

// DHT
#define DHTPIN 4
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

WiFiClient espClient;
PubSubClient mqttClient(espClient);

Preferences wifiPrefs;
Preferences mqttPrefs;

unsigned long lastTime = 0;
unsigned long timerDelay = 30000;

unsigned long lastMqttReconnect = 0;
const unsigned long MQTT_RECONNECT_INTERVAL = 10000;

float temperature;
float humidity;
String deviceId;
String mqttUser;
String mqttPass;

// ─── Helpers ────────────────────────────────────────────────────────────────

String getDeviceId() {
  String id = WiFi.macAddress();
  id.replace(":", "");
  id.toLowerCase();
  return id;
}

String processor(const String& var) {
  if (var == "TEMPERATURE") return String(temperature);
  if (var == "HUMIDITY")    return String(humidity);
  if (var == "STATUS") {
    if (!staConnected)
      return "Modo AP — conectate a 192.168.4.1/config para configurar WiFi";
    return "Conectado a " + WiFi.SSID() + " (IP: " + WiFi.localIP().toString() + ")";
  }
  return String();
}

// ─── WiFi ────────────────────────────────────────────────────────────────────

void startAP() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_password);
  staConnected = false;
  Serial.print("Modo AP iniciado. IP: ");
  Serial.println(WiFi.softAPIP());
}

void onWiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case WIFI_EVENT_AP_STACONNECTED:
      Serial.println("Station connected"); break;
    case WIFI_EVENT_AP_STADISCONNECTED:
      Serial.println("Station disconnected"); break;
    default: break;
  }
}

// ─── MQTT ────────────────────────────────────────────────────────────────────

void mqttConnect() {
  String clientId = "ESP32_" + deviceId;
  Serial.print("Conectando a MQTT...");
  if (mqttClient.connect(clientId.c_str(), mqttUser.c_str(), mqttPass.c_str())) {
    Serial.println("conectado");
    mqttConfigured = false;
  } else {
    Serial.print("falló, rc=");
    Serial.println(mqttClient.state());
  }
}

void publishSensorData() {
  if (!mqttEnabled || !mqttClient.connected()) return;

  if (!mqttConfigured) {
    if (deviceId.length() == 0) deviceId = getDeviceId();

    String baseId     = "esp32_dht_" + deviceId;
    String deviceJson = "{\"identifiers\":[\"" + baseId + "\"],\"name\":\"ESP32 DHT\","
                        "\"model\":\"ESP32\",\"manufacturer\":\"Custom\"}";

    String tempConfig = "{\"name\":\"Temperatura\","
                        "\"state_topic\":\"home/dht/temperature\","
                        "\"unit_of_measurement\":\"°C\","
                        "\"device_class\":\"temperature\","
                        "\"unique_id\":\"" + baseId + "_temperature\","
                        "\"device\":" + deviceJson + "}";

    String humConfig  = "{\"name\":\"Humedad\","
                        "\"state_topic\":\"home/dht/humidity\","
                        "\"unit_of_measurement\":\"%\","
                        "\"device_class\":\"humidity\","
                        "\"unique_id\":\"" + baseId + "_humidity\","
                        "\"device\":" + deviceJson + "}";

    String tempTopic = "homeassistant/sensor/" + baseId + "/temperature/config";
    String humTopic  = "homeassistant/sensor/" + baseId + "/humidity/config";

    mqttClient.publish(tempTopic.c_str(), tempConfig.c_str(), true);
    mqttClient.publish(humTopic.c_str(),  humConfig.c_str(),  true);
    mqttConfigured = true;
    Serial.println("Config MQTT enviada para HA");
  }

  mqttClient.publish("home/dht/temperature", String(temperature).c_str());
  mqttClient.publish("home/dht/humidity",    String(humidity).c_str());
  Serial.println("Datos publicados via MQTT");
}

// ─── WebSocket ───────────────────────────────────────────────────────────────

void notifyClients() {
  String json = "{\"temperature\":\"" + String(temperature) + "\",\"humidity\":\"" + String(humidity) + "\"}";
  ws.textAll(json);
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

// ─── Setup ───────────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  dht.begin();

  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS Mount Failed");
    return;
  }

  // Leer config WiFi
  wifiPrefs.begin("wifi", false);
  String storedSSID     = wifiPrefs.getString("ssid", "");
  String storedPassword = wifiPrefs.getString("password", "");

  if (storedSSID != "") {
    WiFi.mode(WIFI_STA);
    WiFi.begin(storedSSID.c_str(), storedPassword.c_str());
    Serial.print("Conectando a WiFi: ");
    Serial.println(storedSSID);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      Serial.print(".");
      attempts++;
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nConectado a WiFi");
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());
      staConnected = true;
      deviceId = getDeviceId();
      Serial.print("Device ID: ");
      Serial.println(deviceId);
    } else {
      Serial.println("\nFallo en conexión WiFi, iniciando AP");
      startAP();
    }
  } else {
    Serial.println("Sin credenciales WiFi, iniciando AP");
    startAP();
  }

  WiFi.onEvent(onWiFiEvent);

  // Leer config MQTT
  mqttPrefs.begin("mqtt", false);
  String mqttIP   = mqttPrefs.getString("ip", "");
  int    mqttPort = mqttPrefs.getInt("port", 1883);
  mqttUser        = mqttPrefs.getString("user", "");
  mqttPass        = mqttPrefs.getString("pass", "");

  if (staConnected && mqttIP != "") {
    Serial.print("Config MQTT encontrada: ");
    Serial.println(mqttIP);
    Serial.print("Puerto: ");
    Serial.println(mqttPort);
    Serial.print("Usuario: '");
    Serial.print(mqttUser);
    Serial.println("'");
    IPAddress brokerIP;
    if (brokerIP.fromString(mqttIP)) {
      mqttClient.setBufferSize(512);
      mqttClient.setSocketTimeout(1);
      mqttClient.setServer(brokerIP, mqttPort);
      mqttEnabled = true;
    } else {
      Serial.println("IP del broker inválida");
    }
    Serial.println("MQTT configurado — usar /mqtt/connect para conectar");
  } else {
    Serial.println("Sin config MQTT");
  }

  initWebSocket();

  // Páginas HTML
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/index.html", "text/html; charset=utf-8", false, processor);
  });
  server.on("/config", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/config.html", "text/html; charset=utf-8", false);
  });

  // Archivos estáticos
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/style.css", "text/css");
  });
  server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/script.js", "application/javascript");
  });
  server.on("/gear.svg", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/gear.svg", "image/svg+xml");
  });
  server.on("/thermometer.svg", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/thermometer.svg", "image/svg+xml");
  });
  server.on("/droplet.svg", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/droplet.svg", "image/svg+xml");
  });
  server.on("/favicon.svg", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/favicon.svg", "image/svg+xml");
  });

  // Estado MQTT
  server.on("/mqtt/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    String ip   = mqttPrefs.getString("ip", "");
    int    port = mqttPrefs.getInt("port", 1883);
    String user = mqttPrefs.getString("user", "");
    String json = "{\"enabled\":" + String(mqttEnabled ? "true" : "false") +
                  ",\"connected\":" + String(mqttClient.connected() ? "true" : "false") +
                  ",\"ip\":\"" + ip + "\"" +
                  ",\"port\":" + String(port) +
                  ",\"user\":\"" + user + "\"}";
    request->send(200, "application/json", json);
  });

  // Conectar MQTT manualmente desde la UI
  server.on("/mqtt/connect", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!mqttEnabled) {
      request->send(400, "application/json", "{\"ok\":false,\"msg\":\"Sin config MQTT\"}");
      return;
    }
    if (mqttClient.connected()) {
      request->send(200, "application/json", "{\"ok\":true,\"msg\":\"Ya conectado\"}");
      return;
    }
    mqttConnect();
    String json = "{\"ok\":" + String(mqttClient.connected() ? "true" : "false") +
                  ",\"msg\":\"" + String(mqttClient.connected() ? "Conectado" : "No se pudo conectar") + "\"}";
    request->send(200, "application/json", json);
  });

  // Config WiFi POST
  server.on("/config/wifi", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("ssid", true) && request->hasParam("password", true)) {
      wifiPrefs.putString("ssid",     request->getParam("ssid",     true)->value());
      wifiPrefs.putString("password", request->getParam("password", true)->value());
      request->send(200, "text/html", "Credenciales WiFi guardadas. Reiniciando...");
      delay(1000);
      ESP.restart();
    } else {
      request->send(400, "text/plain", "Parámetros faltantes");
    }
  });

  // Config MQTT POST
  server.on("/config/mqtt", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("mqtt_ip", true) && request->hasParam("mqtt_port", true)) {
      mqttPrefs.putString("ip",   request->getParam("mqtt_ip",   true)->value());
      mqttPrefs.putInt   ("port", request->getParam("mqtt_port", true)->value().toInt());
      mqttPrefs.putString("user", request->hasParam("mqtt_user", true) ? request->getParam("mqtt_user", true)->value() : "");
      mqttPrefs.putString("pass", request->hasParam("mqtt_pass", true) ? request->getParam("mqtt_pass", true)->value() : "");
      request->send(200, "text/plain", "Configuración MQTT guardada. Reiniciando...");
      delay(1000);
      ESP.restart();
    } else {
      request->send(400, "text/plain", "Parámetros faltantes");
    }
  });

  // Reset WiFi
  server.on("/reset/wifi", HTTP_GET, [](AsyncWebServerRequest *request) {
    wifiPrefs.remove("ssid");
    wifiPrefs.remove("password");
    request->send(200, "text/html", "Configuración WiFi reseteada. Reiniciando...");
    delay(1000);
    ESP.restart();
  });

  // Reset MQTT
  server.on("/reset/mqtt", HTTP_GET, [](AsyncWebServerRequest *request) {
    mqttPrefs.remove("ip");
    mqttPrefs.remove("port");
    mqttPrefs.remove("user");
    mqttPrefs.remove("pass");
    mqttEnabled = false;
    request->send(200, "text/html", "Configuración MQTT reseteada. Reiniciando...");
    delay(1000);
    ESP.restart();
  });

  server.begin();
  Serial.println("Servidor iniciado");

  // Intentar conexión automática al arrancar si hay config guardada
  if (mqttEnabled) {
    mqttConnect();
  }

  // mDNS
  if (MDNS.begin("esp32-dht")) {
    MDNS.addService("http", "tcp", 80);
    Serial.println("mDNS iniciado — accesible en http://esp32-dht.local");
  }
}

// ─── Loop ────────────────────────────────────────────────────────────────────

void loop() {
  if (mqttEnabled) {
    if (mqttClient.connected()) {
      mqttClient.loop();
    } else if (millis() - lastMqttReconnect > MQTT_RECONNECT_INTERVAL) {
      lastMqttReconnect = millis();
      mqttConnect();
    }
  }

  if ((millis() - lastTime) > timerDelay) {
    float newT = dht.readTemperature();
    float newH = dht.readHumidity();

    if (isnan(newT) || isnan(newH)) {
      Serial.println("Failed to read from DHT sensor!");
      lastTime = millis();
      return;
    }

    temperature = newT;
    humidity = newH;
    Serial.printf("Temperature: %.2f °C\n", temperature);
    Serial.printf("Humidity: %.2f %%\n", humidity);

    notifyClients();
    publishSensorData();
    lastTime = millis();
  }

  ws.cleanupClients();
}
