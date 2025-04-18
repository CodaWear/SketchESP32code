#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>

const char* ssid = "ESP32_AP";
const char* password = "12345678";

#define SOIL_SENSOR_PIN 36 
#define PUMP_PWM_PIN    25

Preferences preferences;
AsyncWebServer server(80);

int moistureThreshold = 40;
int currentMoisture = 0;
String controlMode = "Sensor"; // or "Manual"
int manualSpeed = 100;

String moistureOptions[] = {"20", "30", "40", "50", "60", "70", "80"};

// === HTML ===
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>ESP32 Plant Watering</title>
  <script>
    function toggleSpeedField() {
      var mode = document.getElementById("mode").value;
      document.getElementById("manualSpeedDiv").style.display = (mode === "Manual") ? "block" : "none";
    }

    function fetchData() {
      fetch("/status").then(response => response.text()).then(data => {
        document.getElementById("status").innerHTML = data;
      });
    }

    setInterval(fetchData, 1000);
  </script>
</head>
<body onload="toggleSpeedField()">
  <h2>Plant Watering Settings</h2>
  <form action="/submit" method="POST">
    <label for="mode">Mode:</label><br>
    <select id="mode" name="mode" onchange="toggleSpeedField()">
      <option value="Sensor" %SELECT_SENSOR%>Sensor (Auto)</option>
      <option value="Manual" %SELECT_MANUAL%>Manual (Fixed Speed)</option>
    </select><br><br>

    <div id="manualSpeedDiv">
      <label for="speed">Manual Speed (0–255):</label><br>
      <input type="number" name="speed" min="0" max="255" value="%SPEED%"><br><br>
    </div>

    <label for="moisture">Moisture Threshold (%):</label><br>
    <select name="moisture">
      <option value="20" %OPT_20%>20%</option>
      <option value="30" %OPT_30%>30%</option>
      <option value="40" %OPT_40%>40%</option>
      <option value="50" %OPT_50%>50%</option>
      <option value="60" %OPT_60%>60%</option>
      <option value="70" %OPT_70%>70%</option>
      <option value="80" %OPT_80%>80%</option>
    </select><br><br>

    <input type="submit" value="Update Settings">
  </form>

  <h3>Live Status</h3>
  <div id="status">Loading...</div>
</body>
</html>
)rawliteral";

// === HTML Placeholder Processor ===
String processor(const String& var) {
  if (var == "MOISTURE") return String(currentMoisture);
  if (var == "MODE") return controlMode;
  if (var == "SPEED") return String(manualSpeed);
  if (var == "SELECT_SENSOR") return (controlMode == "Sensor") ? "selected" : "";
  if (var == "SELECT_MANUAL") return (controlMode == "Manual") ? "selected" : "";

  for (String opt : moistureOptions) {
    if (var == "OPT_" + opt) return (moistureThreshold == opt.toInt()) ? "selected" : "";
  }
  return "";
}

void setup() {
  Serial.begin(115200);
  pinMode(PUMP_PWM_PIN, OUTPUT);
  ledcAttach(PUMP_PWM_PIN, 5000, 8);          // 5 kHz, 8-bit resolution

  // Load preferences
  preferences.begin("plant", false);
  moistureThreshold = preferences.getInt("moisture", 40);
  manualSpeed = preferences.getInt("manualSpeed", 100);
  controlMode = preferences.getString("mode", "Sensor");
  preferences.end();

  // Start Access Point
  WiFi.softAP(ssid, password);
  Serial.print("AP IP Address: ");
  Serial.println(WiFi.softAPIP());

  // Serve main page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    String page = index_html;
    page.replace("%MOISTURE%", String(currentMoisture));
    page.replace("%MODE%", controlMode);
    page.replace("%SPEED%", String(manualSpeed));
    page.replace("%SELECT_SENSOR%", controlMode == "Sensor" ? "selected" : "");
    page.replace("%SELECT_MANUAL%", controlMode == "Manual" ? "selected" : "");
    for (String opt : moistureOptions) {
      page.replace("%OPT_" + opt + "%", (moistureThreshold == opt.toInt()) ? "selected" : "");
    }
    request->send(200, "text/html", page);
  });

  // Handle settings form
  server.on("/submit", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("mode", true)) {
      controlMode = request->getParam("mode", true)->value();
    }
    if (request->hasParam("moisture", true)) {
      moistureThreshold = request->getParam("moisture", true)->value().toInt();
    }
    if (request->hasParam("speed", true)) {
      manualSpeed = request->getParam("speed", true)->value().toInt();
    }

    preferences.begin("plant", false);
    preferences.putInt("moisture", moistureThreshold);
    preferences.putInt("manualSpeed", manualSpeed);
    preferences.putString("mode", controlMode);
    preferences.end();

    request->redirect("/");
  });

  // Real-time status endpoint
  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    String status = "Moisture: " + String(currentMoisture) + "%<br>Mode: " + controlMode + "<br>";
    if (controlMode == "Manual") {
      status += "Pump Speed: " + String(manualSpeed) + "<br>";
    }
    request->send(200, "text/html", status);
  });

  server.begin();
}

void loop() {
  int sensorValue = analogRead(SOIL_SENSOR_PIN);
  currentMoisture = map(sensorValue, 4095, 0, 0, 100); // Adjust mapping if needed

  Serial.printf("Moisture: %d%% | Mode: %s | Speed: %d\n", currentMoisture, controlMode.c_str(), manualSpeed);

  if (controlMode == "Sensor") {
    ledcWrite(0, currentMoisture < moistureThreshold ? 180 : 0);
  } else {
    ledcWrite(0, manualSpeed);
  }

  delay(1000);
}
