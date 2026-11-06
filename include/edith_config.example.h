#pragma once
// Private local configuration. Do not commit this file.
static const char *WIFI_SSID = "YOUR_WIFI_OR_HOTSPOT";
static const char *WIFI_PASSWORD = "YOUR_PASSWORD";
// These are first-boot defaults. Saved setup-page settings override them.
// Hold the board's BOOT button for 3 seconds WHILE RUNNING to open setup.
#define EDITH_SETUP_BUTTON_PIN 0 // -1 disables setup activation
#define EDITH_CLOUD_ENABLED 0
#define EDITH_ALLOW_HTTP_TEST 0
#define EDITH_LOCAL_HTTP_ENABLED (!EDITH_CLOUD_ENABLED)
static const char *EDITH_SERVER_URL = ""; // origin; change later via setup Wi-Fi
static const char *EDITH_DEVICE_TOKEN = ""; // same as backend EDITH_DEVICE_TOKEN
// Optional second URL/token pair. Alternatively add it through the setup page.
// Never declare two constants with the same name.
#define EDITH_SECOND_BACKEND_ENABLED 0 // change to 1 to use the defaults below
static const char *EDITH_SERVER_URL_2 = "";
static const char *EDITH_DEVICE_TOKEN_2 = "";
// Paste the trusted root CA PEM for the actual deployed hostname.
static const char *EDITH_ROOT_CA = R"PEM(
)PEM";
