# EDITH hosted connection

Private Wi-Fi and cloud settings are in include/edith_config.h (gitignored). A placeholder version is provided as include/edith_config.example.h. Pins and expressions remain in src/main.cpp.

EDITH_CLOUD_ENABLED=0 preserves the working local HTTP server. When enabled, a network task polls the backend and sends validated commands through a FreeRTOS queue to the display loop. Local HTTP routes are disabled in cloud mode by default. EDITH_ALLOW_HTTP_TEST=1 allows a temporary trusted-LAN HTTP test only. For real hosting require HTTPS, the correct root CA, a synchronized clock and a device bearer token of at least 32 characters.

See the backend project's HOSTING.md for test/setup steps. No firmware was uploaded. Compile verification was blocked because the installed Windows Xtensa compiler could not launch (Illegal System DLL Relocation), so run PlatformIO Build in your normal VS Code session before any upload.
