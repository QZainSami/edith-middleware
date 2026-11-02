
#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <U8g2lib.h>
#include <string.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// ==========================================
// E.D.I.T.H. AI DESKTOP COMPANION
// Phase 3.5 - Smooth OLED Expressions
// ESP32-S3 + SH1106 128x64 OLED
// ==========================================

// EDIT YOUR WI-FI DETAILS
#include "edith_config.h"

// OLED
#define SDA_PIN 8
#define SCL_PIN 9

U8G2_SH1106_128X64_NONAME_F_HW_I2C oled(
    U8G2_R0,
    U8X8_PIN_NONE);

WebServer server(80);

// ==========================================
// EXPRESSIONS
// ==========================================

enum Mood
{
  NORMAL,
  HAPPY,
  SAD,
  ANGRY,
  SLEEPY,
  CURIOUS,
  THINKING,
  UWU
};

const char *moodNames[] = {
    "normal",
    "happy",
    "sad",
    "angry",
    "sleepy",
    "curious",
    "thinking",
    "uwu"};

Mood currentMood = NORMAL;

// ==========================================
// ANIMATION CONFIGURATION
// ==========================================

const unsigned long FRAME_INTERVAL = 30;
const unsigned long TRANSITION_TIME = 300;
const unsigned long BLINK_INTERVAL = 3500;
const unsigned long BLINK_TIME = 160;
const unsigned long WIFI_RETRY_TIME = 5000;

unsigned long lastFrame = 0;
unsigned long lastBlink = 0;
unsigned long blinkStart = 0;
unsigned long lastWiFiRetry = 0;

bool blinking = false;
bool transitioning = false;

unsigned long transitionStart = 0;

// SH1106 framebuffer, 128x64, 1024 bytes
const int BUFFER_SIZE = 1024;

uint8_t fromFrame[BUFFER_SIZE];
uint8_t toFrame[BUFFER_SIZE];

// 8x8 ordered dither matrix for smooth dissolve
const uint8_t bayer[8][8] = {
    {0, 48, 12, 60, 3, 51, 15, 63},
    {32, 16, 44, 28, 35, 19, 47, 31},
    {8, 56, 4, 52, 11, 59, 7, 55},
    {40, 24, 36, 20, 43, 27, 39, 23},
    {2, 50, 14, 62, 1, 49, 13, 61},
    {34, 18, 46, 30, 33, 17, 45, 29},
    {10, 58, 6, 54, 9, 57, 5, 53},
    {42, 26, 38, 22, 41, 25, 37, 21}};

// ==========================================
// HELPER DRAWING FUNCTIONS
// ==========================================

// Smooth curve made from connected line segments
void curve(
    int x0, int y0,
    int x1, int y1,
    int x2, int y2,
    int thickness = 2)
{
  const int SEGMENTS = 18;

  for (int i = 0; i < SEGMENTS; i++)
  {
    float t1 = (float)i / SEGMENTS;
    float t2 = (float)(i + 1) / SEGMENTS;

    float a1 = 1.0f - t1;
    float a2 = 1.0f - t2;

    int px1 = (int)(a1 * a1 * x0 + 2 * a1 * t1 * x1 + t1 * t1 * x2);

    int py1 = (int)(a1 * a1 * y0 + 2 * a1 * t1 * y1 + t1 * t1 * y2);

    int px2 = (int)(a2 * a2 * x0 + 2 * a2 * t2 * x1 + t2 * t2 * x2);

    int py2 = (int)(a2 * a2 * y0 + 2 * a2 * t2 * y1 + t2 * t2 * y2);

    for (int j = 0; j < thickness; j++)
    {
      oled.drawLine(
          px1, py1 + j, px2, py2 + j);
    }
  }
}

// ==========================================
// EYES
// ==========================================

void normalEyes()
{
  oled.drawRBox(22, 12, 28, 34, 11);
  oled.drawRBox(78, 12, 28, 34, 11);
}

void happyEyes()
{
  // Two cheerful upward arches
  curve(22, 36, 36, 11, 50, 36, 4);
  curve(78, 36, 92, 11, 106, 36, 4);
}

// void sadEyes()
// {
//   // Horizontal parentheses: (   )

//   // Left eye (
//   curve(43, 15, 19, 29, 43, 43, 3);

//   // Right eye )
//   curve(85, 15, 109, 29, 85, 43, 3);
// }

void sadEyes()
{
  // Two downward drooping arches
  curve(22, 36, 36, 61, 50, 36, 4);
  curve(78, 36, 92, 61, 106, 36, 4);
}

void angryEyes()
{
  oled.drawRBox(22, 24, 28, 19, 6);
  oled.drawRBox(78, 24, 28, 19, 6);

  // Inward-sloping eyebrows
  oled.drawLine(20, 14, 51, 23);
  oled.drawLine(77, 23, 108, 14);

  oled.drawLine(20, 15, 51, 24);
  oled.drawLine(77, 24, 108, 15);
}

void sleepyEyes()
{
  curve(21, 30, 36, 40, 51, 30, 3);
  curve(77, 30, 92, 40, 107, 30, 3);
}

void curiousEyes()
{
  oled.drawRBox(19, 10, 33, 38, 12);
  oled.drawRBox(82, 20, 22, 26, 9);
}

void thinkingEyes()
{
  normalEyes();

  // Pupils looking sideways
  oled.setDrawColor(0);

  oled.drawRBox(25, 24, 12, 15, 5);
  oled.drawRBox(81, 24, 12, 15, 5);

  oled.setDrawColor(1);
}

void uwuEyes()
{
  // Cute closed U-shaped eyes
  curve(21, 20, 36, 49, 51, 20, 4);
  curve(77, 20, 92, 49, 107, 20, 4);
}

void blinkEyes()
{
  oled.drawHLine(22, 31, 28);
  oled.drawHLine(78, 31, 28);

  oled.drawHLine(22, 32, 28);
  oled.drawHLine(78, 32, 28);
}

// ==========================================
// MOUTHS
// ==========================================

void normalMouth()
{
  curve(57, 52, 64, 55, 71, 52, 2);
}

void happyMouth()
{
  curve(54, 48, 64, 61, 74, 48, 2);
}

void sadMouth()
{
  // Small frown
  curve(57, 56, 64, 46, 71, 56, 2);
}

void angryMouth()
{
  oled.drawLine(56, 53, 72, 50);
  oled.drawLine(56, 54, 72, 51);
}

void sleepyMouth()
{
  oled.drawCircle(64, 52, 3);
}

void curiousMouth()
{
  oled.drawCircle(64, 51, 4);
}

void thinkingMouth()
{
  oled.drawLine(58, 52, 70, 52);
}

void uwuMouth()
{
  // Small w mouth
  curve(56, 48, 60, 58, 64, 51, 2);
  curve(64, 51, 68, 58, 72, 48, 2);
}

// ==========================================
// FACE RENDERER
// ==========================================

void renderFace(Mood mood, bool blink = false)
{
  oled.clearBuffer();
  oled.setDrawColor(1);

  if (blink)
  {
    blinkEyes();
    normalMouth();
    return;
  }

  switch (mood)
  {
  case NORMAL:
    normalEyes();
    normalMouth();
    break;

  case HAPPY:
    happyEyes();
    happyMouth();
    break;

  case SAD:
    sadEyes();
    sadMouth();
    break;

  case ANGRY:
    angryEyes();
    angryMouth();
    break;

  case SLEEPY:
    sleepyEyes();
    sleepyMouth();
    break;

  case CURIOUS:
    curiousEyes();
    curiousMouth();
    break;

  case THINKING:
    thinkingEyes();
    thinkingMouth();
    break;

  case UWU:
    uwuEyes();
    uwuMouth();
    break;
  }
}

// ==========================================
// SMOOTH TRANSITION ENGINE
// ==========================================

// Smoothstep easing, avoids abrupt starts/stops
float smoothstep(float t)
{
  if (t < 0.0f)
    t = 0.0f;
  if (t > 1.0f)
    t = 1.0f;

  return t * t * (3.0f - 2.0f * t);
}

void beginTransition(Mood nextMood)
{
  if (nextMood == currentMood && !transitioning)
  {
    return;
  }

  // Save the currently visible frame.
  // Also works when a new command interrupts a transition.
  memcpy(
      fromFrame,
      oled.getBufferPtr(),
      BUFFER_SIZE);

  currentMood = nextMood;
  transitioning = true;
  transitionStart = millis();

  blinking = false;
  lastBlink = millis();

  // Render destination into a temporary image.
  renderFace(currentMood);
  memcpy(
      toFrame,
      oled.getBufferPtr(),
      BUFFER_SIZE);

  // Restore the starting image until the first update.
  memcpy(
      oled.getBufferPtr(),
      fromFrame,
      BUFFER_SIZE);

  Serial.print("EDITH transitioning to ");
  Serial.println(moodNames[(int)currentMood]);
}

void updateTransition(unsigned long now)
{
  float progress =
      (float)(now - transitionStart) /
      (float)TRANSITION_TIME;

  if (progress >= 1.0f)
  {
    transitioning = false;
    renderFace(currentMood);
    oled.sendBuffer();
    return;
  }

  float eased = smoothstep(progress);

  // Map transition progress to 64 dither levels
  int threshold = (int)(eased * 64.0f);

  uint8_t *output = oled.getBufferPtr();

  // Blend the two monochrome framebuffers.
  for (int page = 0; page < 8; page++)
  {
    for (int x = 0; x < 128; x++)
    {

      int index = page * 128 + x;
      uint8_t result = 0;

      for (int bit = 0; bit < 8; bit++)
      {
        int y = page * 8 + bit;

        bool useNew =
            bayer[y & 7][x & 7] < threshold;

        uint8_t source = useNew
                             ? toFrame[index]
                             : fromFrame[index];

        result |= source & (1 << bit);
      }

      output[index] = result;
    }
  }

  oled.sendBuffer();
}

// ==========================================
// ANIMATION UPDATE
// ==========================================

void updateAnimation()
{
  unsigned long now = millis();

  if (now - lastFrame < FRAME_INTERVAL)
  {
    return;
  }

  lastFrame = now;

  // Transitions always take priority
  if (transitioning)
  {
    updateTransition(now);
    return;
  }

  // Automatic blinking
  bool shouldBlink =
      currentMood == NORMAL ||
      currentMood == CURIOUS;

  if (shouldBlink &&
      !blinking &&
      now - lastBlink >= BLINK_INTERVAL)
  {

    blinking = true;
    blinkStart = now;
  }

  if (blinking &&
      now - blinkStart >= BLINK_TIME)
  {

    blinking = false;
    lastBlink = now;
  }

  renderFace(currentMood, blinking);
  oled.sendBuffer();
}

// ==========================================
// STARTUP SCREEN
// ==========================================

void showStartup()
{
  oled.clearBuffer();

  oled.setFont(u8g2_font_ncenB14_tr);
  oled.drawStr(32, 28, "EDITH");

  oled.setFont(u8g2_font_5x7_tr);
  oled.drawStr(27, 46, "INITIALIZING");

  oled.sendBuffer();

  delay(1000);
}

// ==========================================
// WEB CONTROL CENTER
// ==========================================

const char WEB_PAGE[] PROGMEM = R"EDITH(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport"
content="width=device-width,initial-scale=1">

<title>EDITH Control Center</title>

<style>
*{box-sizing:border-box}

body{
    margin:0;
    min-height:100vh;
    background:#0b1120;
    font-family:Arial,sans-serif;
    color:#f8fafc;
    display:flex;
    justify-content:center;
    align-items:center;
    padding:18px;
}

.panel{
    background:#172033;
    border:1px solid #334155;
    border-radius:22px;
    padding:25px;
    width:100%;
    max-width:420px;
    text-align:center;
}

h1{
    color:#38bdf8;
    letter-spacing:4px;
    margin:6px 0;
}

.subtitle{
    color:#94a3b8;
    font-size:14px;
}

.status{
    background:#0f172a;
    padding:18px;
    margin:22px 0;
    border-radius:14px;
}

.status small{
    color:#94a3b8;
}

#mood{
    font-size:24px;
    font-weight:bold;
    color:#38bdf8;
    margin:9px 0;
}

.buttons{
    display:grid;
    grid-template-columns:1fr 1fr;
    gap:12px;
}

button{
    background:#2563eb;
    color:white;
    border:none;
    padding:15px 8px;
    border-radius:11px;
    font-size:15px;
    cursor:pointer;
}

button:hover{
    background:#1d4ed8;
}

button.active{
    background:#0891b2;
}

#message{
    margin-top:19px;
    font-size:13px;
    color:#94a3b8;
    min-height:18px;
}
</style>
</head>

<body>
<div class="panel">

<h1>E.D.I.T.H.</h1>
<p class="subtitle">AI Desktop Companion</p>

<div class="status">
<small>Current expression</small>
<div id="mood">NORMAL</div>
<small id="connection">Connecting...</small>
</div>

<div class="buttons">
<button data-mood="normal" onclick="setMood('normal')">Normal</button>
<button data-mood="happy" onclick="setMood('happy')">Happy</button>
<button data-mood="sad" onclick="setMood('sad')">Sad</button>
<button data-mood="angry" onclick="setMood('angry')">Angry</button>
<button data-mood="sleepy" onclick="setMood('sleepy')">Sleepy</button>
<button data-mood="curious" onclick="setMood('curious')">Curious</button>
<button data-mood="thinking" onclick="setMood('thinking')">Thinking</button>
<button data-mood="uwu" onclick="setMood('uwu')">UwU</button>
</div>

<div id="message">EDITH is ready.</div>
</div>

<script>
function updateUI(mood) {
    document.getElementById("mood").textContent =
        mood.toUpperCase();

    document.querySelectorAll("button").forEach(b => {
        b.classList.toggle(
            "active",
            b.dataset.mood === mood
        );
    });
}

async function setMood(mood) {
    const message = document.getElementById("message");
    message.textContent = "Updating expression...";

    try {
        const response = await fetch(
            "/mood?name=" + encodeURIComponent(mood)
        );

        if (!response.ok) {
            throw new Error("Command failed");
        }

        const data = await response.json();

        updateUI(data.mood);
        message.textContent = "Smooth transition started.";

    } catch (error) {
        message.textContent = "Connection failed.";
    }
}

async function refreshStatus() {
    try {
        const response = await fetch("/status");
        if (!response.ok) throw new Error();

        const data = await response.json();

        updateUI(data.mood);

        document.getElementById("connection")
            .textContent = data.transitioning
            ? "Animating"
            : "Connected";

    } catch (error) {
        document.getElementById("connection")
            .textContent = "Disconnected";
    }
}

refreshStatus();
setInterval(refreshStatus, 1000);
</script>

</body>
</html>
)EDITH";

// ==========================================
// HTTP ROUTES
// ==========================================

void handleHome()
{
  server.send_P(
      200,
      "text/html; charset=utf-8",
      WEB_PAGE);
}

void handleStatus()
{
  String json = "{\"device\":\"EDITH\",";
  json += "\"mood\":\"";
  json += moodNames[(int)currentMood];
  json += "\",\"online\":true,";
  json += "\"transitioning\":";
  json += transitioning ? "true}" : "false}";

  server.send(200, "application/json", json);
}

void handleMood()
{
  if (!server.hasArg("name"))
  {
    server.send(
        400,
        "application/json",
        "{\"error\":\"Missing mood\"}");
    return;
  }

  String requested = server.arg("name");
  requested.toLowerCase();

  int selected = -1;

  for (int i = 0; i < 8; i++)
  {
    if (requested == moodNames[i])
    {
      selected = i;
      break;
    }
  }

  if (selected < 0)
  {
    server.send(
        400,
        "application/json",
        "{\"error\":\"Unknown mood\"}");
    return;
  }

  beginTransition((Mood)selected);

  String json = "{\"success\":true,\"mood\":\"";
  json += requested;
  json += "\"}";

  server.send(200, "application/json", json);
}


// Background network worker only exchanges snapshots with the animation loop.
// OLED/U8g2 functions always run in loop(), never on this worker.
struct CloudSnapshot {
  char id[37];
  int mood;
};
QueueHandle_t cloudCommands = nullptr;
QueueHandle_t cloudReports = nullptr;
char appliedCloudId[37] = "";

void cloudWorker(void *) {
  String origin = EDITH_SERVER_URL;
  while (origin.endsWith("/")) origin.remove(origin.length() - 1);
  bool tls = origin.startsWith("https://");
  if ((!tls && !(EDITH_ALLOW_HTTP_TEST && origin.startsWith("http://"))) ||
      strlen(EDITH_DEVICE_TOKEN) < 32 || (tls && strlen(EDITH_ROOT_CA) < 100)) {
    Serial.println("Cloud configuration incomplete; local animation remains active.");
    vTaskDelete(nullptr);
    return;
  }
  unsigned long waitMs = 1500;
  char offered[37] = "";
  for (;;) {
    bool success = false;
    if (WiFi.status() == WL_CONNECTED && (!tls || time(nullptr) > 1700000000)) {
      CloudSnapshot report = {};
      xQueuePeek(cloudReports, &report, 0);
      WiFiClient plain;
      WiFiClientSecure secure;
      if (tls) secure.setCACert(EDITH_ROOT_CA);
      HTTPClient http;
      http.setConnectTimeout(3000);
      http.setTimeout(5000);
      http.setReuse(false);
      bool started = tls ? http.begin(secure, origin + "/api/device/poll")
                         : http.begin(plain, origin + "/api/device/poll");
      if (started) {
        http.addHeader("Authorization", String("Bearer ") + EDITH_DEVICE_TOKEN);
        http.addHeader("X-Edith-Ack", report.id);
        http.addHeader("X-Edith-Mood", moodNames[report.mood]);
        int code = http.POST("");
        if (code == 204) success = true;
        if (code == 200 && http.getSize() > 0 && http.getSize() <= 64) {
          String body = http.getString();
          int split = body.indexOf('\n');
          String id = body.substring(0, split);
          String mood = body.substring(split + 1);
          mood.trim();
          bool validId = split == 36;
          for (size_t i = 0; i < id.length() && validId; ++i)
            validId = isxdigit((unsigned char)id[i]) || id[i] == '-';
          int selected = -1;
          for (int i = 0; i < 8; ++i) if (mood == moodNames[i]) selected = i;
          if (validId && selected >= 0) {
            success = true;
            if (id != offered) {
              CloudSnapshot command = {};
              id.toCharArray(command.id, sizeof(command.id));
              command.mood = selected;
              xQueueOverwrite(cloudCommands, &command);
              strlcpy(offered, command.id, sizeof(offered));
            }
          }
        }
      }
      http.end();
    }
    waitMs = success ? 1500 : min(waitMs * 2, 30000UL);
    vTaskDelay(pdMS_TO_TICKS(waitMs));
  }
}

void beginCloud() {
  if (!EDITH_CLOUD_ENABLED) return;
  cloudCommands = xQueueCreate(1, sizeof(CloudSnapshot));
  cloudReports = xQueueCreate(1, sizeof(CloudSnapshot));
  if (!cloudCommands || !cloudReports) {
    Serial.println("Cloud queues could not be allocated.");
    return;
  }
  CloudSnapshot initial = {};
  initial.mood = (int)currentMood;
  xQueueOverwrite(cloudReports, &initial);
  configTime(0, 0, "pool.ntp.org", "time.google.com");
  if (xTaskCreate(cloudWorker, "edith-cloud", 12288, nullptr, 1, nullptr) != pdPASS)
    Serial.println("Cloud task could not start.");
}

void updateCloud() {
  if (!EDITH_CLOUD_ENABLED || !cloudCommands || !cloudReports) return;
  CloudSnapshot command;
  if (xQueueReceive(cloudCommands, &command, 0) == pdTRUE) {
    beginTransition((Mood)command.mood);
    strlcpy(appliedCloudId, command.id, sizeof(appliedCloudId));
  }
  // Acknowledge after the transition has completed, not merely on receipt.
  if (!transitioning) {
    CloudSnapshot report = {};
    strlcpy(report.id, appliedCloudId, sizeof(report.id));
    report.mood = (int)currentMood;
    xQueueOverwrite(cloudReports, &report);
  }
}

// ==========================================
// WI-FI
// ==========================================

void connectWiFi()
{
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.println("EDITH connecting to Wi-Fi...");
}

void checkWiFi()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    return;
  }

  unsigned long now = millis();

  if (now - lastWiFiRetry >= WIFI_RETRY_TIME)
  {
    lastWiFiRetry = now;
    WiFi.reconnect();
  }
}

// ==========================================
// SETUP
// ==========================================

void setup()
{
  Serial.begin(115200);

  Wire.begin(SDA_PIN, SCL_PIN);
  oled.begin();

  showStartup();

  renderFace(NORMAL);
  oled.sendBuffer();

  if (EDITH_LOCAL_HTTP_ENABLED) {
  server.on("/", HTTP_GET, handleHome);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/mood", HTTP_GET, handleMood);

  server.onNotFound([]()
                    { server.send(
                          404,
                          "application/json",
                          "{\"error\":\"Not found\"}"); });

  } // local HTTP routes

  connectWiFi();
  if (EDITH_LOCAL_HTTP_ENABLED) server.begin();
  beginCloud();

  lastBlink = millis();

  Serial.println("EDITH Control Center ready!");
}

// ==========================================
// MAIN LOOP
// ==========================================

void loop()
{
  // Handle requests without blocking animations
  if (WiFi.status() == WL_CONNECTED)
  {
    if (EDITH_LOCAL_HTTP_ENABLED) server.handleClient();
  }

  checkWiFi();

  updateCloud();

  // Keep face updating independently
  updateAnimation();
  delay(1);

  // Print connected IP
  static String previousIP = "";

  if (WiFi.status() == WL_CONNECTED)
  {
    String ip = WiFi.localIP().toString();

    if (ip != previousIP)
    {
      previousIP = ip;

      Serial.println();
      Serial.println("========================");
      Serial.println("EDITH ONLINE");
      Serial.print("IP: ");
      Serial.println(ip);
      Serial.println("========================");
    }
  }
  else
  {
    previousIP = "";
  }
}
