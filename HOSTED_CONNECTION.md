# EDITH hosted connection

The same firmware builds with PlatformIO on Windows and Linux. Start with the operating-system-specific clone, USB-permission and upload instructions in [README.md](README.md).

Private Wi-Fi/cloud settings are in `include/edith_config.h` (gitignored). Create it from `include/edith_config.example.h` on each computer; never copy Windows build caches to Linux. Pins and expressions are in `src/main.cpp`.

`EDITH_CLOUD_ENABLED=0` enables the local HTTP controls. When set to 1, a network task polls the backend and sends validated face commands through a FreeRTOS queue to the display loop. Local HTTP controls are disabled by default in polling mode. `EDITH_ALLOW_HTTP_TEST=1` permits a temporary trusted-LAN HTTP test; real hosting requires HTTPS, the correct root CA, synchronized time and a device bearer token of at least 32 characters.

The backend may run on either computer. Set `EDITH_SERVER_URL` to the backend host's actual LAN IP, not localhost; update it when switching to the laptop. The backend must listen on `0.0.0.0` for LAN access and use the matching device token. Use the backend chat page in polling mode, not the ESP32's old control page.

Verification: the firmware compiled and uploaded successfully from the author's Windows VS Code setup. The real LAN test confirmed online=true, matching desired/reported moods and applied=true. Linux build/upload and hosted HTTPS/hotspot operation remain to be verified. The earlier compiler launch error occurred only inside the agent environment.

The backend repository's HOSTING.md describes credentials, command expiry, delivery acknowledgements and deployment preparation. Do not forward the local ESP32 server directly to the internet.
