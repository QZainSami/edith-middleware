
#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <U8g2lib.h>
#include <string.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include <math.h>
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
bool dashboardMode = false;
unsigned long speakingUntil = 0;
void drawDashboard();
void drawTimerScreen();
bool hasTimerDisplay();
void drawTimerBadge();
void beginCompanion();
void handleCompanion();

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
  oled.drawRBox(27, 43, 26, 8, 2);
  oled.drawRBox(75, 43, 26, 8, 2);
  oled.drawBox(30, 50, 20, 3);
  oled.drawBox(78, 50, 20, 3);
  oled.setFont(u8g2_font_5x7_tr);
  oled.drawStr(104, 30, "z");
  oled.drawStr(109, 23, "z");
  oled.drawStr(114, 16, "Z");
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
  curve(56, 52, 64, 58, 72, 52, 2);
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
  if (speakingUntil != 0 && (long)(speakingUntil - millis()) <= 0) speakingUntil = 0;
  if (speakingUntil != 0 && (long)(speakingUntil - millis()) > 0) {
    oled.setDrawColor(0);
    oled.drawBox(51, 46, 27, 18);
    oled.setDrawColor(1);
    const int openings[] = {2, 5, 3, 7, 2, 4, 6, 2};
    int opening = openings[(millis() / 110) % 8];
    oled.drawRFrame(56, 51 - opening / 2, 16, opening + 3, 2);
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

  if (hasTimerDisplay()) {
    drawTimerScreen();
    oled.sendBuffer();
    return;
  }

  if (dashboardMode) {
    drawDashboard();
    oled.sendBuffer();
    return;
  }

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
  drawTimerBadge();
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

  dashboardMode = false;
  speakingUntil = 0;
  beginTransition((Mood)selected);

  String json = "{\"success\":true,\"mood\":\"";
  json += requested;
  json += "\"}";

  server.send(200, "application/json", json);
}



// Version 2 state snapshots. Network worker never touches the OLED.
struct CloudSnapshot {
  char id[37];
  int mood;
  bool dashboard;
  char speechId[37];
  uint32_t speechMs;
  uint32_t epoch;
  int utcOffset;
  char city[17];
  int weatherCode, temperature10, humidity, weatherAge;
  char timerId[37];
  int32_t timerMs;
};
QueueHandle_t cloudCommands = nullptr;
QueueHandle_t cloudReports = nullptr;
char appliedCloudId[37] = "";
char activeSpeechId[37] = "";
char activeTimerId[37] = "";
CloudSnapshot displayState = {};
uint32_t clockReceived = 0;
uint32_t companionTimerDeadlineMs = 0;
bool timerPresent = false;
uint32_t timerDurationMs = 1;

bool validId(const String &id) {
  if (id == "-") return true;
  if (id.length() != 36) return false;
  for (size_t i = 0; i < id.length(); ++i)
    if (!(isxdigit((unsigned char)id[i]) || id[i] == '-')) return false;
  return true;
}

bool parseSnapshot(const String &body, CloudSnapshot &out) {
  String fields[16];
  int pos = 0;
  for (int i = 0; i < 16; ++i) {
    int end = body.indexOf('\n', pos);
    if (end < 0) return false;
    fields[i] = body.substring(pos, end);
    pos = end + 1;
  }
  if (fields[0] != "EDITH2" || !validId(fields[1]) || !validId(fields[3]) ||
      !validId(fields[5]) || !validId(fields[14]) ||
      (fields[4] != "face" && fields[4] != "dashboard")) return false;
  for (int i : {6, 7, 8, 10, 11, 12, 13, 15}) {
    if (!fields[i].length()) return false;
    for (size_t j = (fields[i][0] == '-' ? 1 : 0); j < fields[i].length(); ++j)
      if (!isdigit((unsigned char)fields[i][j])) return false;
    if (fields[i] == "-") return false;
  }
  int mood = -1;
  for (int i = 0; i < 8; ++i) if (fields[2] == moodNames[i]) mood = i;
  if (mood < 0) return false;
  out = {};
  fields[1].toCharArray(out.id, sizeof(out.id));
  out.mood = mood;
  out.dashboard = fields[4] == "dashboard";
  fields[5].toCharArray(out.speechId, sizeof(out.speechId));
  out.speechMs = fields[6].toInt();
  out.epoch = strtoul(fields[7].c_str(), nullptr, 10);
  out.utcOffset = fields[8].toInt();
  fields[9].toCharArray(out.city, sizeof(out.city));
  out.weatherCode = fields[10].toInt();
  out.temperature10 = fields[11].toInt();
  out.humidity = fields[12].toInt();
  out.weatherAge = fields[13].toInt();
  fields[14].toCharArray(out.timerId, sizeof(out.timerId));
  out.timerMs = fields[15].toInt();
  return out.speechMs <= 20000 && out.utcOffset >= -720 && out.utcOffset <= 840 &&
         out.timerMs >= -1 && out.timerMs <= 86400000 && out.weatherCode >= -1 && out.weatherCode <= 99 &&
         out.humidity >= -1 && out.humidity <= 100 && out.epoch > 1700000000;
}

int32_t remainingTimer() {
  return timerPresent ? max((int32_t)0, (int32_t)(companionTimerDeadlineMs - millis())) : -1;
}

void formatTimer(char *buffer, size_t size) {
  int32_t remaining = remainingTimer();
  unsigned long seconds = (remaining + 999) / 1000;
  if (seconds >= 3600)
    snprintf(buffer, size, "%02lu:%02lu:%02lu", seconds/3600, (seconds/60)%60, seconds%60);
  else snprintf(buffer, size, "%02lu:%02lu", seconds/60, seconds%60);
}

bool hasTimerDisplay() { return timerPresent; }

void drawTimerScreen() {
  oled.clearBuffer();
  oled.setDrawColor(1);
  int32_t remaining = remainingTimer();
  oled.setFont(u8g2_font_5x7_tr);
  const char *title = remaining == 0 ? "TIME UP" : "FOCUS TIMER";
  oled.drawStr((128 - oled.getStrWidth(title))/2, 12, title);
  char text[16]; formatTimer(text, sizeof(text));
  oled.setFont(remaining >= 3600000 ? u8g2_font_logisoso16_tn : u8g2_font_logisoso24_tn);
  oled.drawStr((128 - oled.getStrWidth(text))/2, 43, text);
  oled.drawRFrame(14, 53, 100, 5, 2);
  int fill = remaining == 0 ? 98 : constrain((int)(98.0 * remaining / timerDurationMs), 0, 98);
  if (fill > 0) oled.drawBox(15, 54, fill, 3);
  // A quiet orbiting dot; finished timers stay steady instead of flashing.
  if (remaining > 0) {
    float phase = (millis() % 6000) * 6.2831853f / 6000.0f;
    oled.drawDisc(64 + (int)(53*cosf(phase)), 30 + (int)(18*sinf(phase)), 1);
  }
}

void drawTimerBadge() {
  if (!timerPresent) return;
  oled.setFont(u8g2_font_4x6_tr);
  if (remainingTimer() == 0) {
    if ((millis()/500)%2) oled.drawStr(1, 7, "TIME UP!");
  } else {
    char timer[16]; formatTimer(timer, sizeof(timer));
    oled.drawStr(1, 7, timer);
  }
}

void drawDashboard() {
  oled.clearBuffer(); oled.setDrawColor(1);
  oled.drawFrame(0, 0, 128, 64);
  oled.drawHLine(0, 12, 128);
  oled.drawVLine(83, 13, 43);
  oled.setFont(u8g2_font_5x7_tr);
  if (displayState.epoch) {
    time_t epoch = displayState.epoch + (millis()-clockReceived)/1000 + displayState.utcOffset*60;
    struct tm current; gmtime_r(&epoch, &current);
    char date[24], clock[8];
    strftime(date, sizeof(date), "%a %d %b", &current);
    strftime(clock, sizeof(clock), "%H:%M", &current);
    oled.drawStr(4, 9, date);
    oled.setFont(u8g2_font_logisoso24_tn);
    oled.drawStr(3, 43, clock);
  } else {
    oled.drawStr(4, 9, "EDITH DASHBOARD");
    oled.drawStr(6, 35, "Sync clock...");
  }
  if (WiFi.status() == WL_CONNECTED) oled.drawDisc(121, 6, 2);
  else oled.drawCircle(121, 6, 2);
  oled.setFont(u8g2_font_5x7_tr);
  if (displayState.weatherAge >= 0 && displayState.weatherCode >= 0) {
    bool clear = displayState.weatherCode <= 1;
    if (clear) {
      oled.drawCircle(105, 22, 5);
      oled.drawLine(105,14,105,16); oled.drawLine(105,28,105,30);
      oled.drawLine(97,22,99,22); oled.drawLine(111,22,113,22);
    } else {
      oled.drawCircle(102,23,4); oled.drawCircle(108,21,5); oled.drawHLine(98,27,16);
      if (displayState.weatherCode >= 51) {
        oled.drawLine(101,29,100,32); oled.drawLine(109,29,108,32);
      }
    }
    char temperature[12], humidity[12];
    snprintf(temperature, sizeof(temperature), "%.1fC", displayState.temperature10/10.0);
    snprintf(humidity, sizeof(humidity), "RH %d%%", displayState.humidity);
    oled.drawStr(87, 42, temperature); oled.drawStr(87, 52, humidity);
    if (displayState.weatherAge + (int)((millis()-clockReceived)/1000) > 1800)
      oled.drawStr(119, 21, "*");
  } else {
    oled.drawStr(88, 28, "WEATHER"); oled.drawStr(88, 41, "--");
  }
  oled.setFont(u8g2_font_4x6_tr);
  oled.drawStr(4, 53, displayState.city[0] ? displayState.city : "Set city in UI");
  oled.drawHLine(1, 56, 126);
  oled.setFont(u8g2_font_5x7_tr);
  if (timerPresent) {
    char timer[16]; formatTimer(timer, sizeof(timer));
    oled.drawStr(4, 63, remainingTimer() == 0 ? "TIME UP!" : "TIMER");
    if (remainingTimer() > 0) oled.drawStr(58, 63, timer);
    else if ((millis()/500)%2) oled.drawFrame(0,0,128,64);
  } else oled.drawStr(4, 63, "RESTING  z z z");
}

void cloudWorker(void *) {
  String origin = EDITH_SERVER_URL;
  while (origin.endsWith("/")) origin.remove(origin.length() - 1);
  bool tls = origin.startsWith("https://");
  if ((!tls && !(EDITH_ALLOW_HTTP_TEST && origin.startsWith("http://"))) ||
      strlen(EDITH_DEVICE_TOKEN) < 32 || (tls && strlen(EDITH_ROOT_CA) < 100)) {
    Serial.println("Cloud configuration incomplete; animation remains active.");
    vTaskDelete(nullptr); return;
  }
  unsigned long waitMs = 1500;
  for (;;) {
    bool success = false;
    if (WiFi.status() == WL_CONNECTED && (!tls || time(nullptr) > 1700000000)) {
      CloudSnapshot report = {};
      xQueuePeek(cloudReports, &report, 0);
      WiFiClient plain;
      WiFiClientSecure secure;
      if (tls) secure.setCACert(EDITH_ROOT_CA);
      HTTPClient http;
      http.setConnectTimeout(3000); http.setTimeout(5000); http.setReuse(false);
      bool started = tls ? http.begin(secure, origin + "/api/device/poll-v2")
                         : http.begin(plain, origin + "/api/device/poll-v2");
      if (started) {
        http.addHeader("Authorization", String("Bearer ") + EDITH_DEVICE_TOKEN);
        http.addHeader("X-Edith-Ack", report.id);
        http.addHeader("X-Edith-Mood", moodNames[report.mood]);
        http.addHeader("X-Edith-Mode", report.dashboard ? "dashboard" : "face");
        int code = http.POST("");
        if (code == 200 && http.getSize() > 0 && http.getSize() <= 512) {
          CloudSnapshot command;
          if (parseSnapshot(http.getString(), command)) {
            xQueueOverwrite(cloudCommands, &command); success = true;
          }
        }
      }
      http.end();
    }
    waitMs = success ? 1500 : min(waitMs*2, 30000UL);
    vTaskDelay(pdMS_TO_TICKS(waitMs));
  }
}

void beginCompanion() {
  cloudCommands = xQueueCreate(1, sizeof(CloudSnapshot));
  cloudReports = xQueueCreate(1, sizeof(CloudSnapshot));
  if (!cloudCommands || !cloudReports) {
    Serial.println("Companion queues could not be allocated."); return;
  }
  CloudSnapshot initial = {}; initial.mood = (int)currentMood;
  xQueueOverwrite(cloudReports, &initial);
  configTime(0, 0, "pool.ntp.org", "time.google.com");
  if (EDITH_CLOUD_ENABLED && xTaskCreate(cloudWorker, "edith-cloud", 12288, nullptr, 1, nullptr) != pdPASS)
    Serial.println("Cloud task could not start.");
}

void handleCompanion() {
  CloudSnapshot command;
  if (!cloudCommands || server.arg("plain").length() > 512 || !parseSnapshot(server.arg("plain"), command)) {
    server.send(400, "application/json", "{\"error\":\"Invalid companion snapshot\"}"); return;
  }
  xQueueOverwrite(cloudCommands, &command);
  server.send(200, "application/json", "{\"queued\":true}");
}

void updateCloud() {
  if (!cloudCommands || !cloudReports) return;
  CloudSnapshot command;
  if (xQueueReceive(cloudCommands, &command, 0) == pdTRUE) {
    bool wake = dashboardMode && !command.dashboard;
    dashboardMode = command.dashboard;
    displayState = command; clockReceived = millis();
    if (command.id[0] != '-' && (strcmp(appliedCloudId, command.id) || wake)) {
      beginTransition((Mood)command.mood);
      strlcpy(appliedCloudId, command.id, sizeof(appliedCloudId));
    }
    if (dashboardMode) { transitioning = false; blinking = false; speakingUntil = 0; }
    if (strcmp(activeSpeechId, command.speechId)) {
      strlcpy(activeSpeechId, command.speechId, sizeof(activeSpeechId));
      if (!dashboardMode) speakingUntil = millis() + command.speechMs;
    }
    if (command.timerMs < 0) { timerPresent = false; activeTimerId[0] = 0; }
    else {
      if (strcmp(activeTimerId, command.timerId) || abs(remainingTimer()-command.timerMs) > 3000) {
        if (strcmp(activeTimerId, command.timerId) != 0) timerDurationMs = max((uint32_t)1, (uint32_t)command.timerMs);
        companionTimerDeadlineMs = millis() + command.timerMs;
      }
      timerPresent = true;
      strlcpy(activeTimerId, command.timerId, sizeof(activeTimerId));
    }
  }
  if (!transitioning) {
    CloudSnapshot report = {};
    strlcpy(report.id, appliedCloudId, sizeof(report.id));
    report.mood = (int)currentMood; report.dashboard = dashboardMode;
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
  server.on("/companion", HTTP_POST, handleCompanion);

  server.onNotFound([]()
                    { server.send(
                          404,
                          "application/json",
                          "{\"error\":\"Not found\"}"); });

  } // local HTTP routes

  connectWiFi();
  if (EDITH_LOCAL_HTTP_ENABLED) server.begin();
  beginCompanion();

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
