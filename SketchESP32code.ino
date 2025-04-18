#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>

const char* ssid = "ESP32_AP";
const char* password = "12345678";

// Pins
#define SOIL_SENSOR_PIN 3
#define PUMP_PWM_PIN    9

// Globals
Preferences preferences;
AsyncWebServer server(80);

int moistureThreshold = 40;
int currentMoisture = 0;
String controlMode = "Sensor"; // or "Manual"
int manualSpeed = 100;

String moistureOptions[] = {"20", "30", "40", "50", "60", "70", "80"};

// Log buffer
String logBuffer = "";
void log(const String& msg) {
  Serial.println(msg);
  logBuffer += msg + "\n";
  if (logBuffer.length() > 2048) {
    logBuffer = logBuffer.substring(logBuffer.length() - 2048);
  }
}

// HTML Page
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

      function updateLogs() {
        fetch('/logs')
          .then(response => response.text())
          .then(data => {
            const logBox = document.getElementById('logBox');
            logBox.textContent = data;
            logBox.scrollTop = logBox.scrollHeight;
          });
      }

      setInterval(updateLogs, 1000);
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

    <p>Current Moisture: %MOISTURE%%</p>
    <p>Mode: %MODE%</p>

    <h3>System Logs</h3>
    <pre id="logBox" style="background:#eee; padding:10px; height:200px; overflow:auto; border:1px solid #ccc;"></pre>
  </body>
</html>
)rawliteral";

// Replace placeholders
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
  ledcAttach(PUMP_PWM_PIN, 5000, 8);

  preferences.begin("plant", false);
  moistureThreshold = preferences.getInt("moisture", 40);
  manualSpeed = preferences.getInt("manualSpeed", 100);
  controlMode = preferences.getString("mode", "Sensor");
  preferences.end();

  WiFi.softAP(ssid, password);
  log("Access Point started.");
  log("Connect to WiFi: " + String(ssid));
  log("IP: " + WiFi.softAPIP().toString());

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

  server.on("/submit", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("mode", true)) {
      controlMode = request->getParam("mode", true)->value();
      log("Mode changed to: " + controlMode);
    }

    if (request->hasParam("moisture", true)) {
      moistureThreshold = request->getParam("moisture", true)->value().toInt();
      log("Moisture threshold set to: " + String(moistureThreshold));
    }

    if (request->hasParam("speed", true)) {
      manualSpeed = request->getParam("speed", true)->value().toInt();
      log("Manual speed set to: " + String(manualSpeed));
    }

    preferences.begin("plant", false);
    preferences.putInt("moisture", moistureThreshold);
    preferences.putInt("manualSpeed", manualSpeed);
    preferences.putString("mode", controlMode);
    preferences.end();

    request->redirect("/");
  });

  server.on("/logs", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", logBuffer);
  });

  server.begin();
}

void loop() {
  int sensorValue = analogRead(SOIL_SENSOR_PIN);
  currentMoisture = map(sensorValue, 4095, 0, 0, 100); // Adjust for your sensor
  log("Moisture: " + String(currentMoisture) + "% | Mode: " + controlMode);

  if (controlMode == "Sensor") {
    if (currentMoisture < moistureThreshold) {
      ledcWrite(0, 180); // 70% PWM
      log("Pump ON");
    } else {
      ledcWrite(0, 0);
      log("Pump OFF");
    }
  } else if (controlMode == "Manual") {
    ledcWrite(0, manualSpeed);
    log("Manual pump speed: " + String(manualSpeed));
  }

  delay(1000);
}
