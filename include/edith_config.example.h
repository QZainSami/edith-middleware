#pragma once
// Private local configuration. Do not commit this file.
static const char *WIFI_SSID = "YOUR_WIFI_OR_HOTSPOT";
static const char *WIFI_PASSWORD = "YOUR_PASSWORD";
#define EDITH_CLOUD_ENABLED 0
#define EDITH_ALLOW_HTTP_TEST 0
#define EDITH_LOCAL_HTTP_ENABLED (!EDITH_CLOUD_ENABLED)
static const char *EDITH_SERVER_URL = ""; // HTTPS origin, no trailing slash
static const char *EDITH_DEVICE_TOKEN = ""; // same as backend EDITH_DEVICE_TOKEN
// Paste the trusted root CA PEM for the actual deployed hostname.
static const char *EDITH_ROOT_CA = R"PEM(
)PEM";
