
#include <Arduino.h>
#include <esp_random.h>
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
#ifndef EDITH_SECOND_BACKEND_ENABLED
#define EDITH_SECOND_BACKEND_ENABLED 0
#endif
#include "setup_portal.h"

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
  UWU,
  SHY,
  BLUSH,
  UNKNOWN
};

const char *moodNames[] = {
    "normal",
    "happy",
    "sad",
    "angry",
    "sleepy",
    "curious",
    "thinking",
    "uwu", "shy", "blush", "unknown"};
const int MOOD_COUNT = sizeof(moodNames) / sizeof(moodNames[0]);

Mood currentMood = NORMAL;
bool dashboardMode = false;
bool stopwatchMode = false;
bool drawExtraTimeScreen();
unsigned long speakingUntil = 0;
uint32_t lastUserActivity = 0;
uint32_t lastDemoFace = 0;
bool userHasControlledFace = false;
void noteUserActivity() { lastUserActivity = millis(); userHasControlledFace = true; }
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

// Cosmetic only: the clean framebuffer is restored immediately after transmission.
const uint32_t GLITCH_MIN_INTERVAL_MS = 25000;
const uint32_t GLITCH_MAX_INTERVAL_MS = 70000;
bool timeAlertVisible();

// Render-only guest appearance: never changes mood, command acknowledgements,
// clock state, speech timing or timer deadlines.
void drawEvilGlitchFace(uint32_t age) {
  oled.clearBuffer(); oled.setDrawColor(1);
  int lean = (int)(2*sinf(age*.012f));
  oled.drawTriangle(19,20+lean, 53,32, 24,40);
  oled.drawTriangle(109,20-lean, 75,32, 104,40);
  oled.drawLine(18,13+lean,53,25); oled.drawLine(75,25,110,13-lean);
  oled.drawLine(18,14+lean,53,26); oled.drawLine(75,26,110,14-lean);
  // Wide segmented grin with black tooth separators.
  oled.drawRBox(40,43,48,14,6);
  oled.setDrawColor(0);
  oled.drawTriangle(40,43,88,43,64,49);
  for (int x=46; x<87; x+=7) oled.drawVLine(x,46,11);
  oled.setDrawColor(1);
  oled.drawPixel(14,25); oled.drawPixel(114,25);
}

void presentDisplay() {
  static uint8_t cleanFrame[BUFFER_SIZE], effectFrame[BUFFER_SIZE];
  static bool scheduled = false, active = false;
  static uint32_t nextAt = 0, began = 0;
  uint32_t now = millis();
  auto schedule = [&]() {
    nextAt = now + GLITCH_MIN_INTERVAL_MS + esp_random() % (GLITCH_MAX_INTERVAL_MS - GLITCH_MIN_INTERVAL_MS + 1);
    scheduled = true;
  };
  // Alerts and setup credentials always stay readable.
  if (timeAlertVisible() || setupPortalActive) {
    active = false; schedule(); oled.sendBuffer(); return;
  }
  if (!scheduled) schedule();
  if (!active && (int32_t)(now-nextAt) >= 0) { active = true; began = now; }
  const uint32_t age = now-began;
  if (active && age >= 1220) { active = false; schedule(); }
  if (!active) { oled.sendBuffer(); return; }

  uint8_t *pixels = oled.getBufferPtr();
  memcpy(cleanFrame,pixels,BUFFER_SIZE);
  // 0-120ms tearing, 120-1120ms evil face, 100ms exit tearing.
  if (age >= 120 && age < 1120) drawEvilGlitchFace(age-120);
  memcpy(effectFrame,pixels,BUFFER_SIZE);
  bool tear = age < 120 || age >= 1120 || (age%240 < 45);
  if (tear) {
    int band = 1 + esp_random()%6;
    int shift = (int)(esp_random()%11)-5;
    if (!shift) shift = 3;
    for (int x=0; x<128; ++x) {
      pixels[band*128+x] = effectFrame[band*128+(x-shift+128)%128];
      int other = (band+3)%8;
      pixels[other*128+x] = effectFrame[other*128+(x+shift+128)%128];
    }
    for(int i=0;i<12;++i) pixels[esp_random()%BUFFER_SIZE] ^= (uint8_t)(1U<<(esp_random()%8));
  }
  oled.sendBuffer();
  // Restore the actual current screen, including transitions running underneath.
  memcpy(pixels,cleanFrame,BUFFER_SIZE);
}

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
  int drift = (int)(2*sinf(millis()/850.0f));
  oled.drawStr(104, 30-drift, "z");
  oled.drawStr(109, 23-drift, "z");
  oled.drawStr(114, 16-drift, "Z");
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

void expressiveBrows(Mood mood, bool speaking) {
  float t = millis()/1000.0f;
  int left = (int)lroundf(sinf(t*(speaking?11.0f:1.7f))*(speaking?2.0f:1.0f));
  int right = (int)lroundf(sinf(t*(speaking?9.0f:1.4f)+1.0f)*(speaking?2.0f:1.0f));
  int y = mood == SLEEPY ? 34 : ((mood == SHY || mood == BLUSH || mood == UNKNOWN || mood == UWU) ? 14 : 6);
  if (mood == ANGRY) {
    oled.setDrawColor(0); oled.drawBox(17,6,94,18); oled.setDrawColor(1);
    curve(20,14+left,36,17+left,51,23+right,2);
    curve(77,23+left,93,17+right,108,14+right,2);
  } else if (mood == SAD) {
    curve(23,22+left,36,15+left,49,13+right,2);
    curve(79,13+left,92,15+right,105,22+right,2);
  } else {
    curve(25,y+left+1,36,y+left-2,47,y+left,2);
    int ry = mood == CURIOUS ? y+6 : y;
    curve(81,ry+right,92,ry+right-2,103,ry+right+1,2);
  }
}

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

  case SHY:
  case BLUSH:
    curve(25, 27, 36, 43, 47, 27, 3);
    curve(81, 27, 92, 43, 103, 27, 3);
    if (mood == SHY) oled.drawDisc(64, 53, 2);
    else uwuMouth();
    for (int x = 19; x <= 91; x += 72) for (int i = 0; i < 3; ++i)
      oled.drawLine(x + i*5, 44, x + i*5 - 2, 49);
    break;
  case UNKNOWN:
    oled.drawRBox(23, 26, 27, 11, 3);
    oled.drawRBox(78, 26, 27, 11, 3);
    curve(57, 52, 65, 59, 73, 49, 2);
    for (int x = 19; x <= 91; x += 72) for (int i = 0; i < 3; ++i)
      oled.drawLine(x + i*5, 44, x + i*5 - 2, 49);
    break;
  case UWU:
    uwuEyes();
    uwuMouth();
    break;
  }
  if (speakingUntil != 0 && (long)(speakingUntil - millis()) <= 0) speakingUntil = 0;
  bool speaking = speakingUntil != 0 && (long)(speakingUntil - millis()) > 0;
  expressiveBrows(mood, speaking);
  if (mood == THINKING && !speaking) {
    for(int i=0;i<3;++i) {
      if(i == (int)((millis()/420)%3)) oled.drawDisc(57+i*7,59,1);
      else oled.drawPixel(57+i*7,59);
    }
  }
  if (speaking) {
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
    presentDisplay();
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

  presentDisplay();
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

  if (drawExtraTimeScreen()) { presentDisplay(); return; }

  if (hasTimerDisplay()) {
    drawTimerScreen();
    presentDisplay();
    return;
  }

  if (dashboardMode) {
    drawDashboard();
    presentDisplay();
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

  if (!transitioning && !blinking && !setupPortalActive &&
      (speakingUntil == 0 || (int32_t)(speakingUntil-millis()) <= 0) &&
      (!userHasControlledFace || now-lastUserActivity >= 60000) && now-lastDemoFace >= 8000) {
    lastDemoFace = now;
    beginTransition((Mood)(((int)currentMood+1)%MOOD_COUNT));
    return;
  }
  renderFace(currentMood, blinking);
  drawTimerBadge();
  presentDisplay();
}

// ==========================================
// STARTUP SCREEN
// ==========================================

void showStartup()
{
  for(int frame=0;frame<40;++frame) {
    oled.clearBuffer(); oled.setDrawColor(1);
    oled.drawFrame(3,3,122,58);
    oled.setFont(u8g2_font_5x7_tr); oled.drawStr(10,13,"EDITH // CORE BOOT");
    oled.setFont(u8g2_font_ncenB14_tr);
    int shift=(frame%11==0 && frame<27)?2:0;
    oled.drawStr(32+shift,36,"EDITH");
    if(frame<26) {
      oled.setDrawColor(0); oled.drawBox(12,(frame*7)%38+15,102,2); oled.setDrawColor(1);
      for(int n=0;n<5;++n) oled.drawHLine(9+(frame*17+n*23)%100,17+(frame*3+n*9)%25,3+(n*5)%13);
    }
    oled.setFont(u8g2_font_4x6_tr);
    oled.drawStr(12,46,frame<14?"SIGNAL ACQUIRED":frame<28?"SYNCING NEURAL CORE":"SYSTEM READY");
    oled.drawFrame(11,51,106,4); oled.drawBox(12,52,(frame+1)*104/40,2);
    oled.sendBuffer(); delay(35);
  }
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
<button onclick="setMood('shy')">Shy</button><button onclick="setMood('blush')">Blush</button><button onclick="setMood('unknown')">Unknown</button>
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

  for (int i = 0; i < MOOD_COUNT; i++)
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
  stopwatchMode = false;
  speakingUntil = 0;
  noteUserActivity();
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
  bool stopwatch, stopwatchRunning;
  char stopwatchId[37], alarmId[37];
  uint32_t stopwatchMs;
  int32_t alarmMs;
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
char activeStopwatchId[37] = "", activeAlarmId[37] = "";
uint32_t stopwatchBase = 0, stopwatchReceived = 0, alarmEnd = 0;
bool stopwatchRunning = false, alarmPresent = false;
uint32_t stopwatchElapsed() {
  uint32_t elapsed=stopwatchBase+(stopwatchRunning?(uint32_t)(millis()-stopwatchReceived):0U);
  return elapsed>359999990U?359999990U:elapsed;
}

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
      (fields[4] != "face" && fields[4] != "dashboard" && fields[4] != "stopwatch")) return false;
  for (int i : {6, 7, 8, 10, 11, 12, 13, 15}) {
    if (!fields[i].length()) return false;
    for (size_t j = (fields[i][0] == '-' ? 1 : 0); j < fields[i].length(); ++j)
      if (!isdigit((unsigned char)fields[i][j])) return false;
    if (fields[i] == "-") return false;
  }
  int mood = -1;
  for (int i = 0; i < MOOD_COUNT; ++i) if (fields[2] == moodNames[i]) mood = i;
  if (mood < 0) return false;
  out = {};
  fields[1].toCharArray(out.id, sizeof(out.id));
  out.mood = mood;
  out.dashboard = fields[4] == "dashboard";
  out.stopwatch = fields[4] == "stopwatch";
  out.alarmMs = -1;
  strcpy(out.stopwatchId, "-"); strcpy(out.alarmId, "-");
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
  if (pos < (int)body.length()) {
    String extra[6];
    for (int i=0; i<6; ++i) {
      int end=body.indexOf('\n',pos); if(end<0) return false;
      extra[i]=body.substring(pos,end); pos=end+1;
    }
    if(extra[0]!="TOOLS1" || !validId(extra[1]) || !validId(extra[4]) ||
       (extra[3]!="0" && extra[3]!="1")) return false;
    for(int i : {2,5}) {
      if(!extra[i].length() || extra[i]=="-") return false;
      for(size_t j=(extra[i][0]=='-'?1:0);j<extra[i].length();++j)
        if(!isdigit((unsigned char)extra[i][j])) return false;
    }
    long long sw=strtoll(extra[2].c_str(),nullptr,10), alarm=strtoll(extra[5].c_str(),nullptr,10);
    if(sw<0 || sw>359999990 || alarm < -1 || alarm>604800000) return false;
    extra[1].toCharArray(out.stopwatchId,sizeof(out.stopwatchId));
    extra[4].toCharArray(out.alarmId,sizeof(out.alarmId));
    out.stopwatchMs=sw; out.stopwatchRunning=extra[3]=="1"; out.alarmMs=alarm;
  }
  return out.speechMs <= 20000 && out.utcOffset >= -720 && out.utcOffset <= 840 &&
         out.timerMs >= -1 && out.timerMs <= 86400000 && out.weatherCode >= -1 && out.weatherCode <= 99 &&
         out.humidity >= -1 && out.humidity <= 100 && out.epoch > 1700000000;
}

int32_t remainingTimer() {
  return timerPresent ? max((int32_t)0, (int32_t)(companionTimerDeadlineMs - millis())) : -1;
}

bool timeAlertVisible() {
  return (alarmPresent && (int32_t)(alarmEnd-millis()) <= 0) ||
         (timerPresent && remainingTimer() == 0);
}

void formatTimer(char *buffer, size_t size) {
  int32_t remaining = remainingTimer();
  unsigned long seconds = (remaining + 999) / 1000;
  if (seconds >= 3600)
    snprintf(buffer, size, "%02lu:%02lu:%02lu", seconds/3600, (seconds/60)%60, seconds%60);
  else snprintf(buffer, size, "%02lu:%02lu", seconds/60, seconds%60);
}

bool drawExtraTimeScreen() {
  bool due=alarmPresent && (int32_t)(alarmEnd-millis())<=0;
  if(!due && (!stopwatchMode || timerPresent)) return false;
  oled.clearBuffer(); oled.setDrawColor(1);
  oled.setFont(u8g2_font_5x7_tr);
  if(due) {
    oled.drawRFrame(3,3,122,58,7);
    oled.drawStr(45,16,"ALARM");
    oled.drawCircle(64,31,9); oled.drawLine(64,25,64,32); oled.drawLine(64,32,70,35);
    int pulse = (int)((millis()/650)%3);
    oled.drawCircle(64,31,12+pulse);
    oled.drawDisc(64,47,1);
    oled.drawStr(14,57,"DISMISS IN THE APP");
  } else {
    uint32_t elapsed=stopwatchElapsed();
    char value[16]; uint32_t seconds=elapsed/1000;
    oled.drawStr(38,10,"STOPWATCH");
    oled.setFont(seconds>=3600?u8g2_font_logisoso16_tn:u8g2_font_logisoso24_tn);
    if(seconds>=3600) snprintf(value,sizeof(value),"%02lu:%02lu:%02lu",seconds/3600,(seconds/60)%60,seconds%60);
    else snprintf(value,sizeof(value),"%02lu:%02lu",seconds/60,seconds%60);
    oled.drawStr((128-oled.getStrWidth(value))/2,39,value);
    oled.setFont(u8g2_font_5x7_tr); snprintf(value,sizeof(value),".%02lu",(elapsed/10)%100);
    oled.drawStr(56,49,value);
    oled.drawHLine(15,54,98); oled.drawStr(44,63,stopwatchRunning && elapsed<359999990UL?"RUNNING":"PAUSED");
    oled.drawLine(3,18,3,30); oled.drawLine(124,18,124,30);
    if(stopwatchRunning) {
      int tick=(millis()/160)%10;
      oled.drawPixel(3,33+tick); oled.drawPixel(124,42-tick);
    }
    if(stopwatchRunning) oled.drawDisc(16+(millis()/60)%96,54,1);
  }
  return true;
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
  if (remaining > 0) {
    int tick = (millis()/180)%7;
    oled.drawVLine(3,25+tick,3); oled.drawVLine(124,31-tick,3);
  }
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
  oled.drawRFrame(0, 0, 128, 64, 4);
  int comet = 5 + (millis()/75)%116;
  oled.drawHLine(max(4,comet-5),2,min(6,comet-3));
  oled.drawHLine(3, 12, 122);
  oled.drawVLine(85, 16, 37);
  oled.drawHLine(3, 55, 122);
  oled.setFont(u8g2_font_5x7_tr);
  int second = (millis()/1000)%60;
  if (displayState.epoch) {
    time_t epoch = displayState.epoch + (millis()-clockReceived)/1000 + displayState.utcOffset*60;
    struct tm current; gmtime_r(&epoch, &current);
    second = current.tm_sec;
    char date[24], clock[8];
    strftime(date, sizeof(date), "%a %d %b", &current);
    strftime(clock, sizeof(clock), "%H:%M", &current);
    oled.drawStr(5, 9, date);
    oled.setFont(u8g2_font_logisoso24_tn);
    oled.drawStr(4, 40, clock);
    // Twelve quiet second markers make the clock feel alive without flashing it.
    for (int i = 0; i < 12; ++i) {
      if (i <= second/5) oled.drawBox(7+i*6, 43, 3, 1);
      else oled.drawPixel(8+i*6, 43);
    }
  } else {
    oled.drawStr(5, 9, "EDITH / REST");
    oled.drawStr(7, 33, "Syncing...");
  }
  // Crescent + connection indicator, with no invented battery percentage.
  oled.drawDisc(105, 6, 3); oled.setDrawColor(0); oled.drawDisc(107, 5, 3); oled.setDrawColor(1);
  for (int i = 0; i < 3; ++i) {
    if (WiFi.status() == WL_CONNECTED) oled.drawBox(115+i*3, 9-(i+1)*2, 2, (i+1)*2);
    else oled.drawPixel(115+i*3, 8);
  }
  oled.setFont(u8g2_font_5x7_tr);
  if (displayState.weatherAge >= 0 && displayState.weatherCode >= 0) {
    int code = displayState.weatherCode;
    if (code <= 1) {
      oled.drawCircle(106, 22, 5);
      oled.drawLine(106,14,106,16); oled.drawLine(106,28,106,30);
      oled.drawLine(98,22,100,22); oled.drawLine(112,22,114,22);
      oled.drawPixel(100,16); oled.drawPixel(112,16);
      oled.drawPixel(100,28); oled.drawPixel(112,28);
      float sunshine = millis()/1800.0f;
      oled.drawPixel(106+(int)(8*cosf(sunshine)),22+(int)(8*sinf(sunshine)));
    } else {
      int breeze = (int)lroundf(sinf(millis()/1400.0f));
      oled.drawCircle(101+breeze,23,4); oled.drawCircle(108+breeze,21,5); oled.drawHLine(97+breeze,27,17);
      if (code == 45 || code == 48) { oled.drawHLine(97,29,17); oled.drawHLine(100,31,13); }
      else if (code >= 71 && code <= 77) {
        oled.drawLine(101,29,101,33); oled.drawHLine(99,31,5); oled.drawPixel(111,31);
      } else if (code >= 95) {
        oled.drawLine(108,28,104,31); oled.drawLine(104,31,108,31); oled.drawLine(108,31,105,34);
      } else if (code >= 51) {
        int rain = (millis()/220)%3;
        oled.drawLine(101,28+rain,100,30+rain); oled.drawLine(110,30-rain,109,32-rain);
      }
    }
    char temperature[12], humidity[12];
    snprintf(temperature, sizeof(temperature), "%.1fC", displayState.temperature10/10.0);
    snprintf(humidity, sizeof(humidity), "RH %d%%", displayState.humidity);
    oled.setClipWindow(88, 34, 126, 54);
    oled.drawStr(89, 42, temperature); oled.drawStr(89, 52, humidity);
    oled.setMaxClipWindow();
    if (displayState.weatherAge + (int)((millis()-clockReceived)/1000) > 1800) oled.drawStr(120, 21, "*");
  } else { oled.drawStr(94, 30, "--"); oled.setFont(u8g2_font_4x6_tr); oled.drawStr(90, 44, "NO DATA"); }
  oled.setFont(u8g2_font_4x6_tr);
  oled.setClipWindow(5, 46, 83, 54);
  const char *city = displayState.city[0] ? displayState.city : "Set weather city";
  int overflow = max(0, (int)oled.getStrWidth(city)-76);
  int cityOffset = 0;
  if(overflow > 0) {
    // Pause at each end, then slide slowly. Long city names become readable.
    uint32_t travel = overflow*90UL;
    uint32_t cycle = 3000UL+2*travel;
    uint32_t position = millis()%cycle;
    if(position >= 1500 && position < 1500+travel) cityOffset=(position-1500)/90;
    else if(position >= 1500+travel && position < 3000+travel) cityOffset=overflow;
    else if(position >= 3000+travel) cityOffset=overflow-(position-3000-travel)/90;
  }
  // This font advances four pixels per character. Keep x nonnegative for U8g2.
  oled.drawStr(5-cityOffset%4, 52, city+cityOffset/4);
  oled.setMaxClipWindow();
  if (alarmPresent) {
    int32_t seconds = max((int32_t)0, (int32_t)(alarmEnd-millis()))/1000;
    char alarm[25]; snprintf(alarm, sizeof(alarm), "ALARM IN %02ld:%02ld", (long)seconds/3600, (long)(seconds/60)%60);
    oled.drawStr(5, 62, alarm);
  } else {
    oled.drawStr(5, 62, "REST MODE");
    const int phase = (millis()/1200)%3;
    for (int i = 0; i < 3; ++i) {
      if (i == phase) oled.drawDisc(108+i*6, 60, 1);
      else oled.drawPixel(108+i*6, 60);
    }
  }
}

void cloudWorker(void *) {
  String origins[2] = {savedConnection.origin, savedConnection.origin2};
  String tokens[2] = {savedConnection.token, savedConnection.token2};
  bool valid[2] = {false,false};
  for(int i=0;i<2;++i) {
    while(origins[i].endsWith("/")) origins[i].remove(origins[i].length()-1);
    valid[i] = validBackendOrigin(origins[i]) && tokens[i].length() >= 32;
    for(size_t j=0;j<tokens[i].length();++j)
      if((unsigned char)tokens[i][j]<=32 || (unsigned char)tokens[i][j]>=127) valid[i]=false;
  }
  if (!valid[0] && !valid[1]) {
    Serial.println("No valid backend configured; hold BOOT to configure.");
    vTaskDelete(nullptr); return;
  }
  int selected = valid[0] ? 0 : 1;
  unsigned int failures = 0;
  unsigned long waitMs = 1500;
  for (;;) {
    bool success = false;
    bool connected = WiFi.status() == WL_CONNECTED;
    bool tls = origins[selected].startsWith("https://");
    if (connected) {
      // An unsynchronised TLS clock also counts toward fallback, so a local
      // HTTP secondary can still work when there is no internet/NTP access.
      if (!tls || time(nullptr) > 1700000000) {
      CloudSnapshot report = {};
      xQueuePeek(cloudReports, &report, 0);
      WiFiClient plain; WiFiClientSecure secure;
      if (tls) secure.setCACert(EDITH_ROOT_CA);
      HTTPClient http;
      http.setConnectTimeout(3000); http.setTimeout(5000); http.setReuse(false);
      http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
      bool started = tls ? http.begin(secure, origins[selected] + "/api/device/poll-v2")
                         : http.begin(plain, origins[selected] + "/api/device/poll-v2");
      if (started) {
        // A URL is always tried with its own token; never try another token here.
        http.addHeader("Authorization", String("Bearer ") + tokens[selected]);
        http.addHeader("X-Edith-Ack", report.id);
        http.addHeader("X-Edith-Mood", moodNames[report.mood]);
        http.addHeader("X-Edith-Mode", report.stopwatch ? "stopwatch" : (report.dashboard ? "dashboard" : "face"));
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
      if (success) { failures = 0; waitMs = 1500; }
      else if (++failures >= 3 && valid[1-selected]) {
        selected = 1-selected; failures = 0; waitMs = 1500;
        Serial.print("Trying backend slot "); Serial.println(selected+1);
      } else waitMs = min(waitMs*2,15000UL);
    }
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
    bool wake = (dashboardMode || stopwatchMode) && !command.dashboard && !command.stopwatch;
    if ((command.id[0] != '-' && strcmp(appliedCloudId, command.id)) ||
        (command.speechMs > 0 && strcmp(activeSpeechId,command.speechId)) || dashboardMode != command.dashboard || stopwatchMode != command.stopwatch)
      noteUserActivity();
    dashboardMode = command.dashboard;
    stopwatchMode = command.stopwatch;
    uint32_t currentSw = stopwatchElapsed();
    if(strcmp(activeStopwatchId,command.stopwatchId) || abs((int32_t)currentSw-(int32_t)command.stopwatchMs)>1000) {
      stopwatchBase=command.stopwatchMs; stopwatchReceived=millis();
    }
    stopwatchRunning=command.stopwatchRunning;
    strlcpy(activeStopwatchId,command.stopwatchId,sizeof(activeStopwatchId));
    if(command.alarmMs<0) alarmPresent=false;
    else {
      int32_t remaining=alarmPresent ? max((int32_t)0,(int32_t)(alarmEnd-millis())) : -1;
      if(!alarmPresent || strcmp(activeAlarmId,command.alarmId) || abs(remaining-command.alarmMs)>3000)
        alarmEnd=millis()+command.alarmMs;
      alarmPresent=true;
    }
    strlcpy(activeAlarmId,command.alarmId,sizeof(activeAlarmId));
    displayState = command; clockReceived = millis();
    if (command.id[0] != '-' && (strcmp(appliedCloudId, command.id) || wake)) {
      beginTransition((Mood)command.mood);
      strlcpy(appliedCloudId, command.id, sizeof(appliedCloudId));
    }
    if (dashboardMode || stopwatchMode || command.alarmMs == 0) { transitioning = false; blinking = false; speakingUntil = 0; }
    if (strcmp(activeSpeechId, command.speechId)) {
      strlcpy(activeSpeechId, command.speechId, sizeof(activeSpeechId));
      if (!dashboardMode && !stopwatchMode) speakingUntil = millis() + command.speechMs;
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
    report.mood = (int)currentMood; report.dashboard = dashboardMode; report.stopwatch = stopwatchMode;
    xQueueOverwrite(cloudReports, &report);
  }
}

// ==========================================
// WI-FI
// ==========================================

void connectWiFi()
{
  loadConnectionSettings();
  if (EDITH_SETUP_BUTTON_PIN >= 0) pinMode(EDITH_SETUP_BUTTON_PIN, INPUT_PULLUP);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  tryWifiProfile();

  Serial.println("EDITH connecting to Wi-Fi...");
}

void checkWiFi()
{
  // Keep the setup AP channel stable while a phone is entering settings.
  if (setupPortalActive) return;
  if (WiFi.status() == WL_CONNECTED)
  {
    return;
  }

  unsigned long now = millis();

  if (now - lastWiFiRetry >= WIFI_RETRY_TIME)
  {
    lastWiFiRetry = now;
    if (millis() - wifiAttemptAt >= 12000) {
      if (savedConnection.count) wifiProfile = (wifiProfile + 1) % savedConnection.count;
      WiFi.disconnect();
      tryWifiProfile();
    }
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
  installSetupRoutes();
  server.on("/", HTTP_GET, []() {
    if (setupPortalActive && server.client().localIP() == WiFi.softAPIP()) handleSetupPage();
    else if (EDITH_LOCAL_HTTP_ENABLED) handleHome();
    else server.send(404, "text/plain", "Hold BOOT for 3 seconds to open setup Wi-Fi.");
  });
  server.begin();
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
  if (WiFi.status() == WL_CONNECTED || setupPortalActive) server.handleClient();
  serviceSetupPortal();
  checkWiFi();

  updateCloud();

  // Keep face updating independently
  if (setupPortalActive) {
    static uint32_t lastSetupFrame = 0;
    if (millis() - lastSetupFrame >= 100) { lastSetupFrame = millis(); drawSetupScreen(); }
  } else updateAnimation();
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
