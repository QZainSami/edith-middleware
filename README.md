# EDITH Middleware

ESP32-S3 firmware for EDITH, an expressive AI desktop companion. Drives a 128x64 SH1106 OLED with eight faces, blinking and smooth dissolve transitions. Works with the EDITH Python backend and optionally receives commands through an outward authenticated connection.

## Hardware

- ESP32-S3 board configured by platformio.ini
- SH1106 128x64 I2C OLED
- SDA: GPIO 8; SCL: GPIO 9

Check your own board's flash/PSRAM specifications against platformio.ini before building. The existing configuration targets the author's board; not every ESP32-S3 devkit has the same memory.

## Setup

Install PlatformIO for VS Code, then open this repository as a project.

1. Copy `include/edith_config.example.h` to `include/edith_config.h`.
2. Set your Wi-Fi SSID and password in the private configuration file.
3. Keep `EDITH_CLOUD_ENABLED=0` for the first test.
4. Build, then upload to your connected ESP32. Open the serial monitor at 115200 baud to find its IP.

```powershell
Copy-Item include/edith_config.example.h include/edith_config.h
pio run
pio run --target upload
pio device monitor
```

Do not commit `include/edith_config.h`. It is excluded by .gitignore.

## Local control

- `GET /`: device control page.
- `GET /status`: current mood and animation status.
- `GET /mood?name=happy`: start a face transition.

Supported moods: `normal`, `happy`, `sad`, `angry`, `sleepy`, `curious`, `thinking`, `uwu`.

Local HTTP controls are for trusted LAN use and have no authentication. They are disabled by default when cloud mode is enabled. Never expose the local ESP32 web server directly to the internet.

## Hosted connection

The prototype supports outward polling of the backend over verified HTTPS, a device bearer token, bounded network timeouts and retry backoff. Network work runs separately from OLED animation. Cloud mode remains disabled until configured. See [HOSTED_CONNECTION.md](HOSTED_CONNECTION.md).

Cloud integration has not yet been verified on physical hardware. The author's automated build attempt was blocked by a Windows compiler launch error; build and test on your own system before uploading.

## Planned features

Touch reactions, sound cues, wake word "EDITH", speech input/output, a companion app and a speaking mouth animation. These require additional hardware/software and are not implemented here.

## Related project

Companion backend: [QZainSami/edith-backend](https://github.com/QZainSami/edith-backend).

Project inspiration: [SathiyaSenpai/deskbot](https://github.com/SathiyaSenpai/deskbot).

## License

[MIT](LICENSE), copyright 2026 QZainSami. Dependencies remain subject to their own licenses.
