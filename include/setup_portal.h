#pragma once
#include <Preferences.h>
#include <ctype.h>

#ifndef EDITH_SETUP_BUTTON_PIN
#define EDITH_SETUP_BUTTON_PIN 0
#endif

// One atomic NVS blob. These credentials are private to the device, but ordinary
// development firmware does not enable flash encryption.
struct SavedNetwork { char ssid[33]; char password[65]; };
struct LegacySavedConnection {
  uint32_t version;
  char origin[193];
  char token[257];
  uint8_t count;
  SavedNetwork networks[3];
};
struct SavedConnection : LegacySavedConnection {
  char origin2[193];
  char token2[257];
};
SavedConnection savedConnection = {};
bool setupPortalActive = false;
uint32_t setupOpenedAt = 0, setupRebootAt = 0, wifiAttemptAt = 0;
uint8_t wifiProfile = 0;
String setupSsid, setupPassword, setupNonce;

String htmlEscape(String value) {
  value.replace("&", "&amp;"); value.replace("<", "&lt;");
  value.replace(">", "&gt;"); value.replace("\"", "&quot;"); value.replace("'", "&#39;");
  return value;
}

bool validBackendOrigin(String origin) {
  bool tls = origin.startsWith("https://");
  if (!tls && !(EDITH_ALLOW_HTTP_TEST && origin.startsWith("http://"))) return false;
  String authority = origin.substring(tls ? 8 : 7);
  if (authority.isEmpty() || origin.length() > 192) return false;
  int colon = authority.indexOf(':');
  String host = colon < 0 ? authority : authority.substring(0, colon);
  if (host.isEmpty() || host.startsWith(".") || host.endsWith(".")) return false;
  for (size_t i = 0; i < host.length(); ++i) {
    char c = host[i];
    if (!isalnum((unsigned char)c) && c != '.' && c != '-') return false;
  }
  if (colon >= 0) {
    String port = authority.substring(colon + 1);
    if (port.isEmpty() || port.length() > 5) return false;
    for (size_t i = 0; i < port.length(); ++i) if (!isdigit((unsigned char)port[i])) return false;
    if (port.toInt() < 1 || port.toInt() > 65535) return false;
  }
  return !tls || strlen(EDITH_ROOT_CA) >= 100;
}

void loadConnectionSettings() {
  SavedConnection candidate = {};
  Preferences prefs;
  bool found = false;
  if (prefs.begin("edith-connect", true)) {
    if (prefs.getBytesLength("settings") == sizeof(candidate)) {
      prefs.getBytes("settings", &candidate, sizeof(candidate));
      found = candidate.version == 2 && candidate.count <= 3;
    } else if (prefs.getBytesLength("settings") == sizeof(LegacySavedConnection)) {
      LegacySavedConnection old = {};
      prefs.getBytes("settings", &old, sizeof(old));
      if (old.version == 1 && old.count <= 3) {
        static_cast<LegacySavedConnection &>(candidate) = old;
        candidate.version = 2;
#if EDITH_SECOND_BACKEND_ENABLED
        strlcpy(candidate.origin2, EDITH_SERVER_URL_2, sizeof(candidate.origin2));
        strlcpy(candidate.token2, EDITH_DEVICE_TOKEN_2, sizeof(candidate.token2));
#endif
        found = true;
      }
    }
    prefs.end();
  }
  if (found) {
    candidate.origin[192] = 0; candidate.token[256] = 0;
    candidate.origin2[192] = 0; candidate.token2[256] = 0;
    for (int i = 0; i < 3; ++i) { candidate.networks[i].ssid[32] = 0; candidate.networks[i].password[64] = 0; }
    savedConnection = candidate;
  } else {
    savedConnection.version = 2;
#if EDITH_SECOND_BACKEND_ENABLED
    strlcpy(savedConnection.origin2, EDITH_SERVER_URL_2, sizeof(savedConnection.origin2));
    strlcpy(savedConnection.token2, EDITH_DEVICE_TOKEN_2, sizeof(savedConnection.token2));
#endif
    strlcpy(savedConnection.origin, EDITH_SERVER_URL, sizeof(savedConnection.origin));
    strlcpy(savedConnection.token, EDITH_DEVICE_TOKEN, sizeof(savedConnection.token));
    if (strlen(WIFI_SSID)) {
      savedConnection.count = 1;
      strlcpy(savedConnection.networks[0].ssid, WIFI_SSID, 33);
      strlcpy(savedConnection.networks[0].password, WIFI_PASSWORD, 65);
    }
  }
}

void tryWifiProfile() {
  wifiAttemptAt = millis();
  if (!savedConnection.count) return;
  const SavedNetwork &profile = savedConnection.networks[wifiProfile];
  WiFi.begin(profile.ssid, profile.password);
}

void openSetupPortal() {
  if (setupPortalActive) return;
  char suffix[5], password[13], nonce[17];
  snprintf(suffix, sizeof(suffix), "%04X", (unsigned int)(ESP.getEfuseMac() & 0xffff));
  snprintf(password, sizeof(password), "%08lX%04lX", (unsigned long)esp_random(), (unsigned long)(esp_random() & 0xffff));
  snprintf(nonce, sizeof(nonce), "%08lX%08lX", (unsigned long)esp_random(), (unsigned long)esp_random());
  setupSsid = String("EDITH-Setup-") + suffix;
  setupPassword = password; setupNonce = nonce;
  WiFi.mode(WIFI_AP_STA);
  IPAddress station = WiFi.localIP();
  IPAddress address(192, 168, (station[0] == 192 && station[1] == 168 && station[2] == 4) ? 74 : 4, 1);
  if (!WiFi.softAPConfig(address, address, IPAddress(255,255,255,0))) {
    Serial.println("Could not configure setup Wi-Fi address."); return;
  }
  if (!WiFi.softAP(setupSsid.c_str(), setupPassword.c_str(), 1, false, 2)) {
    Serial.println("Could not start setup Wi-Fi."); return;
  }
  setupPortalActive = true; setupOpenedAt = millis();
  Serial.print("Setup network: "); Serial.println(setupSsid);
  Serial.print("Setup page: http://"); Serial.println(WiFi.softAPIP());
  Serial.println("Temporary Wi-Fi password is shown on the OLED. Setup closes after 5 minutes.");
}

bool setupClientAllowed() {
  // Never expose credential editing through the station/LAN interface.
  if (!setupPortalActive || server.client().localIP() != WiFi.softAPIP()) {
    server.send(403, "text/plain", "Hold EDITH's BOOT button for 3 seconds, then join its setup Wi-Fi.");
    return false;
  }
  server.sendHeader("Cache-Control", "no-store");
  server.sendHeader("X-Content-Type-Options", "nosniff");
  server.sendHeader("Content-Security-Policy", "default-src 'none'; style-src 'unsafe-inline'; form-action 'self'; frame-ancestors 'none'; base-uri 'none'");
  return true;
}

void handleSetupPage() {
  if (!setupClientAllowed()) return;
  String page = R"HTML(<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>EDITH setup</title>
<style>*{box-sizing:border-box}body{margin:0;background:#0c1721;color:#e4eff5;font:16px system-ui;padding:24px}main{max-width:520px;margin:auto}h1{letter-spacing:6px;color:#cafa72}p{line-height:1.55;color:#adbdcb}form{padding:22px;background:#192735;border:1px solid #344b5b;border-radius:22px}label{display:block;margin-top:18px}input{width:100%;padding:13px;margin-top:7px;border-radius:10px;border:1px solid #536b7b;background:#0d1b27;color:white;font:inherit}small{display:block;color:#a9bdca;margin-top:5px}button{width:100%;margin-top:25px;padding:14px;border:0;border-radius:12px;background:#cafa72;color:#17221a;font-weight:bold;font-size:16px}.tag{font:12px monospace;color:#78dbef}input[type=checkbox]{width:auto}</style>
<main><span class="tag">CONNECTION CONSOLE / EDITH</span><h1>HELLO AGAIN.</h1><p>Give EDITH a network and a home for its backend. Settings stay on the ESP after restarting.</p><form method="post" action="/setup/save">
)HTML";
  page += "<input type=hidden name=nonce value=\"" + setupNonce + "\">";
  page += "<label>Wi-Fi name<input name=ssid maxlength=32 required value=\"" + htmlEscape(savedConnection.count ? savedConnection.networks[0].ssid : "") + "\"></label>";
  page += R"HTML(<label>Wi-Fi password<input name=password type=password maxlength=64 autocomplete=new-password></label><small>Leave blank to keep this saved network's password. For a different network, enter its password.</small><label><input type=checkbox name=open value=yes> This network has no password</label>)HTML";
  page += "<label>Python backend address<input name=origin maxlength=193 required placeholder=\"http://192.168.1.20:8000\" value=\"" + htmlEscape(savedConnection.origin) + "\"></label>";
  page += R"HTML(<small>Use the laptop/PC address, including http:// or https:// and the port. Do not enter the ESP or LM Studio address.</small><label>Device token (optional)<input name=token type=password maxlength=256 autocomplete=new-password></label><small>Leave blank to keep the existing device token. This is EDITH_DEVICE_TOKEN, not your app login password.</small>)HTML";
  page += "<label>Second backend address (optional)<input name=origin2 maxlength=193 placeholder=\"http://OTHER-LAPTOP-IP:8000\" value=\"" + htmlEscape(savedConnection.origin2) + "\"></label>";
  page += R"HTML(<label>Second device token<input name=token2 type=password maxlength=256 autocomplete=new-password></label><small>Use the token belonging to the second backend. Blank keeps its existing token; changing this address requires entering its token again.</small><label><input type=checkbox name=clear2 value=yes> Remove the second backend</label><button>Save &amp; restart EDITH</button></form><p>Up to three Wi-Fi networks are remembered; the most recently saved is tried first. Setup closes after five minutes. HTTPS requires the trusted root certificate already configured in firmware.</p></main></html>)HTML";
  server.send(200, "text/html; charset=utf-8", page);
}

void handleSetupSave() {
  if (!setupClientAllowed()) return;
  if (setupRebootAt) { server.send(409, "text/plain", "Restart already scheduled."); return; }
  if (!server.hasArg("nonce") || server.arg("nonce") != setupNonce) { server.send(403, "text/plain", "Setup session changed. Reload the page."); return; }
  String ssid = server.arg("ssid"), password = server.arg("password"), origin = server.arg("origin"), token = server.arg("token");
  origin.trim(); token.trim();
  while (origin.endsWith("/")) origin.remove(origin.length()-1);
  int existing = -1;
  for (int i = 0; i < savedConnection.count; ++i) if (ssid == savedConnection.networks[i].ssid) existing = i;
  if (password.isEmpty() && existing >= 0) password = savedConnection.networks[existing].password;
  bool open = server.arg("open") == "yes";
  if (open) password = "";
  bool hexKey = password.length() == 64;
  if (hexKey) for (size_t i = 0; i < password.length(); ++i) if (!isxdigit((unsigned char)password[i])) hexKey = false;
  if (ssid.isEmpty() || ssid.length() > 32 || (!open && !hexKey && (password.length() < 8 || password.length() > 63))) {
    server.send(400, "text/plain", "Enter a Wi-Fi name (up to 32 bytes) and an 8-63 character password, or select the open-network checkbox. Go back to correct it."); return;
  }
  if (!validBackendOrigin(origin)) {
    server.send(400, "text/plain", "Use a backend origin such as http://192.168.1.20:8000, with no path, query or login. HTTP requires EDITH_ALLOW_HTTP_TEST=1; HTTPS requires EDITH_ROOT_CA in firmware."); return;
  }
  if (token.isEmpty()) token = savedConnection.token;
  for (size_t i = 0; i < token.length(); ++i) {
    if ((unsigned char)token[i] <= 32 || (unsigned char)token[i] >= 127) {
      server.send(400, "text/plain", "Device token cannot contain spaces or control characters."); return;
    }
  }
  if (EDITH_CLOUD_ENABLED && (token.length() < 32 || token.length() > 256)) { server.send(400, "text/plain", "Device token must be 32-256 characters and match the backend."); return; }
  String origin2 = server.hasArg("origin2") ? server.arg("origin2") : String(savedConnection.origin2);
  String token2 = server.arg("token2"); origin2.trim(); token2.trim();
  while (origin2.endsWith("/")) origin2.remove(origin2.length()-1);
  if (server.arg("clear2") == "yes") { origin2 = ""; token2 = ""; }
  else if (!origin2.isEmpty()) {
    if (!validBackendOrigin(origin2)) { server.send(400,"text/plain","Second backend address is invalid."); return; }
    if (token2.isEmpty() && origin2 == savedConnection.origin2) token2 = savedConnection.token2;
    if (token2.length() < 32 || token2.length() > 256) { server.send(400,"text/plain","Enter the second backend's own 32-256 character device token."); return; }
    for(size_t i=0;i<token2.length();++i) if((unsigned char)token2[i]<=32 || (unsigned char)token2[i]>=127) {
      server.send(400,"text/plain","Second token cannot contain spaces or control characters."); return;
    }
  } else token2 = "";
  SavedConnection next = savedConnection;
  strlcpy(next.origin2,origin2.c_str(),sizeof(next.origin2));
  strlcpy(next.token2,token2.c_str(),sizeof(next.token2));
  memset(next.networks, 0, sizeof(next.networks));
  strlcpy(next.networks[0].ssid, ssid.c_str(), 33);
  strlcpy(next.networks[0].password, password.c_str(), 65);
  next.count = 1;
  for (int i = 0; i < savedConnection.count && next.count < 3; ++i)
    if (i != existing) next.networks[next.count++] = savedConnection.networks[i];
  strlcpy(next.origin, origin.c_str(), sizeof(next.origin));
  strlcpy(next.token, token.c_str(), sizeof(next.token));
  Preferences prefs;
  if (!prefs.begin("edith-connect", false)) { server.send(500, "text/plain", "Could not open device storage. Your old settings remain unchanged."); return; }
  size_t written = prefs.putBytes("settings", &next, sizeof(next)); prefs.end();
  if (written != sizeof(next)) { server.send(500, "text/plain", "Could not save settings. Try again."); return; }
  server.send(200, "text/html; charset=utf-8", "<meta name=viewport content='width=device-width,initial-scale=1'><h1>Saved! EDITH is restarting.</h1><p>Reconnect your phone to your usual Wi-Fi or re-enable its hotspot. To change settings again, hold BOOT for three seconds while EDITH is running. The Android app must use the backend you intend to control.</p>");
  setupRebootAt = millis() + 1200;
}

void installSetupRoutes() {
  server.on("/setup", HTTP_GET, handleSetupPage);
  server.on("/setup/save", HTTP_POST, handleSetupSave);
}

void serviceSetupPortal() {
  if (setupRebootAt && (int32_t)(millis()-setupRebootAt) >= 0) ESP.restart();
  static uint32_t pressedAt = 0;
  static bool held = false;
  if (EDITH_SETUP_BUTTON_PIN >= 0 && digitalRead(EDITH_SETUP_BUTTON_PIN) == LOW) {
    if (!pressedAt) pressedAt = millis();
    if (!held && millis()-pressedAt >= 3000) { held = true; openSetupPortal(); }
  } else { pressedAt = 0; held = false; }
  if (setupPortalActive && !setupRebootAt && millis()-setupOpenedAt >= 300000) {
    setupPortalActive = false; WiFi.softAPdisconnect(true); WiFi.mode(WIFI_STA);
    setupPassword = ""; setupNonce = "";
  }
}

void drawSetupScreen() {
  oled.clearBuffer(); oled.setDrawColor(1);
  oled.drawRFrame(0, 0, 128, 64, 5);
  oled.setFont(u8g2_font_5x7_tr);
  oled.drawStr(5, 10, "EDITH / CONNECT"); oled.drawHLine(4, 13, 120);
  int scan = 5+(millis()/65)%110;
  oled.drawHLine(scan,15,6);
  float orbit = millis()/300.0f;
  oled.drawCircle(119,7,3);
  oled.drawDisc(119+(int)(3*cosf(orbit)),7+(int)(3*sinf(orbit)),1);
  oled.drawStr(5, 24, setupRebootAt ? "SAVED! RESTARTING..." : setupSsid.c_str());
  String password = "KEY " + setupPassword;
  oled.drawStr(5, 35, password.c_str());
  String address = WiFi.softAPIP().toString();
  oled.drawStr(5, 46, address.c_str());
  oled.setFont(u8g2_font_4x6_tr);
  oled.drawStr(5, 58, "Join Wi-Fi, open IP in browser");
  oled.sendBuffer();
}
