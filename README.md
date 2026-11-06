# 🤖 EDITH Middleware

Cross-platform PlatformIO project for the ESP32-S3 firmware of EDITH. Drives a 128x64 SH1106 OLED with eleven expressions, blinking and smooth dissolve transitions. Supports local HTTP control and outward authenticated polling of the companion backend.

### ✨ Living OLED

- Speech animates the mouth and raises/tilts the eyebrows; idle brows move gently too.
- Sleep includes drifting Zs. Thinking has moving dots. Weather has drifting clouds, falling rain and a sun sparkle; long city names scroll slowly.
- Timer and stopwatch screens have moving edge accents. The sleep dashboard and setup page have quiet scanning accents.
- Every 25–70 seconds, a short glitch reveals an evil grin for one second before restoring the current screen. This is a temporary visual effect, not a new selectable mood. Timer/stopwatch timekeeping, speech and device acknowledgements continue underneath.
- Due alarms, completed timers and setup credentials are protected from the glitch. Alerts remain readable.

Build and upload from Windows 11 or Linux, including CachyOS/KDE. PlatformIO downloads the compiler for the host operating system; the firmware still runs on the same ESP32. No Windows drive letters or COM ports are hardcoded in the project.

## 🔧 Hardware

- ESP32-S3 board configured in `platformio.ini`
- SH1106 128x64 I2C OLED
- SDA: GPIO 8; SCL: GPIO 9

## 📶 Change Wi-Fi or backend without reflashing

After one firmware update, hold **BOOT for 3 seconds while EDITH is running**. Join the `EDITH-Setup-XXXX` Wi-Fi using the temporary key on the OLED, then open **http://192.168.4.1**. Enter your Wi-Fi and Python backend address, save, and EDITH restarts. It remembers up to three networks and the backend address across restarts. Setup opens **only** by holding BOOT; it never appears automatically on startup or Wi-Fi failure.

Read the [📘 setup and mobile-hotspot guide](SETUP.md) for Windows/Linux commands, password handling, recovery and a first hardware check. The Android app's backend URL is changed separately. Holding BOOT during power-on/reset enters flashing mode, so activate setup **after** EDITH has started.

Match your board's flash/PSRAM specifications to `platformio.ini`; different ESP32-S3 boards have different memory. Build, upload and local polling have been verified on Windows with the ESP32. Linux build/upload still needs hardware verification.

## 📱 Use EDITH with your laptop and phone hotspot — simple steps

**EDITH's setup Wi-Fi is only for entering settings. Once finished, the laptop and EDITH both connect to your phone hotspot. The laptop then has internet access for Gemini.**

### 1. Connect the laptop to your phone

Turn on mobile data and your phone's **Wi-Fi hotspot**. Select **2.4 GHz** in the hotspot settings if there is a band option. Note the hotspot's name and password.

On the laptop, click the Wi-Fi icon, choose your phone's hotspot name, enter its password, and connect. Open a website to check that the laptop has internet.

### 2. Find the laptop's IP address

**Windows:** press **Win + R**, type `cmd`, and press Enter. In the window, type:

```powershell
ipconfig
```

Find **Wireless LAN adapter Wi-Fi**, then its **IPv4 Address** line. For example:

```text
Wireless LAN adapter Wi-Fi:
   IPv4 Address . . . . . . . . . . : 192.168.43.120
   Default Gateway . . . . . . . . : 192.168.43.1
```

Write down **192.168.43.120** in this example. Yours will probably be different—use yours! Do **not** use Default Gateway, a VPN adapter, or an address from EDITH's setup Wi-Fi.

**Linux / CachyOS KDE:** open **Konsole** and type:

```bash
ip -4 address
```

Find your connected Wi-Fi interface (often named `wlan0` or starting with `wlp`). Its `inet` line might show `192.168.43.120/24`. Write down `192.168.43.120`, leaving off `/24`. Ignore `lo`, `127.0.0.1`, VPN and virtual interfaces.

### 3. Start the Python backend on the laptop

Open the backend project in VS Code, open its terminal, and run:

```bash
uv run python -m uvicorn app:app --host 0.0.0.0 --port 8000 --workers 1
```

Leave that terminal running. Keep your working Gemini API settings in the backend's private `.env`. If using LM Studio instead, keep LM Studio and its model server running too.

The backend address is **`http://` + your laptop's IPv4 address + `:8000`**. With the example above, it is:

```text
http://192.168.43.120:8000
```

Open `http://YOUR-LAPTOP-IP:8000/healthz` in the laptop's browser to check that the backend responds. Connections from EDITH also need incoming TCP port **8000** allowed through the laptop firewall on your trusted hotspot network.

### 4. Tell EDITH which network and backend to use

After the updated firmware has been uploaded once, power EDITH normally. **While it is already running**, hold its **BOOT** button for **3 seconds**, then release. Do not hold BOOT during power-on/reset.

The OLED shows an **EDITH-Setup-XXXX** network, a temporary **KEY**, and a setup IP. On the laptop, temporarily disconnect from your phone's hotspot and join **EDITH-Setup-XXXX** using that key. Internet access will stop briefly; this is expected.

Open the IP shown on the OLED in your browser—normally **http://192.168.4.1**—and fill in:

| Field | What to enter |
| --- | --- |
| Wi-Fi name | Your **phone hotspot's name**, not EDITH-Setup |
| Wi-Fi password | Your **phone hotspot's password**, not the temporary OLED key |
| Python backend address | `http://YOUR-LAPTOP-IP:8000`, using the address you wrote down in step 2 |
| Device token | Leave blank to keep your existing working token |

Click **Save & restart EDITH**. You do not need to upload firmware again when changing these settings.

### 5. Reconnect the laptop to your phone hotspot

After saving, select your **phone hotspot** from the laptop's Wi-Fi list and reconnect. EDITH will join that hotspot too. Your laptop now has internet access again, so the backend can call Gemini.

Run `ipconfig` (Windows) or `ip -4 address` (Linux) again. If the laptop's Wi-Fi IPv4 address changed, repeat step 4 with the new address.

You can also configure EDITH from another device while the laptop stays on the hotspot. If you use the phone providing the hotspot to join EDITH-Setup, the phone may turn its hotspot off; turn it back on after saving and reconnect the laptop.

### 6. Connect the Android app

In EDITH Android, open **Settings**, enter that same backend address (`http://YOUR-LAPTOP-IP:8000`), enter your existing web username/password, and tap **Save & connect**. Try changing a face from Controls.

The final arrangement is:

```text
Phone hotspot supplies internet
  ├── Laptop: Python backend → Gemini API
  └── ESP: connects to the laptop's backend
```

The laptop must stay awake with the backend running. Some hotspots block devices from talking to one another; if the app/ESP cannot connect, test `http://YOUR-LAPTOP-IP:8000/healthz` from another connected device and check the laptop firewall and hotspot's device-isolation setting. A check on the laptop alone does not prove other devices can reach it.

The firmware must use polling mode (`EDITH_CLOUD_ENABLED=1`) and allow local HTTP (`EDITH_ALLOW_HTTP_TEST=1`); both are already enabled in the author's working setup. The ESP's device token must match the backend's `EDITH_DEVICE_TOKEN`.

## 🚀 Quick start

Already using EDITH? See [📶 Change Wi-Fi or backend without reflashing](#-change-wi-fi-or-backend-without-reflashing) above. Your existing private config remains the first-boot default.

**You need:** [Git](https://git-scm.com/downloads), [VS Code](https://code.visualstudio.com/) with the [PlatformIO IDE extension](https://platformio.org/install/ide?install=vscode), an ESP32-S3 and a data-capable USB cable.

### 1. Clone the project — Windows or Linux

Run these commands wherever you want to keep the project:

```bash
git clone https://github.com/QZainSami/edith-middleware.git
cd edith-middleware
```

Open the cloned folder in VS Code. PlatformIO will detect `platformio.ini` and download the tools it needs.

### 2. Create your settings file

**Windows PowerShell:**

```powershell
Copy-Item include/edith_config.example.h include/edith_config.h
```

**Linux / CachyOS:**

```bash
cp include/edith_config.example.h include/edith_config.h
```

Do this once. Keep an existing `include/edith_config.h` when updating.

### 3. Enter your Wi-Fi details

Edit `include/edith_config.h` and set `WIFI_SSID` and `WIFI_PASSWORD`. For the first local HTTP test, keep:

```cpp
#define EDITH_CLOUD_ENABLED 0
#define EDITH_ALLOW_HTTP_TEST 0
```

🔒 This file is ignored by Git. Never commit Wi-Fi passwords or device tokens.

### 4. Build and upload

Connect the ESP32. In PlatformIO, click **Build**, then **Upload**, then **Serial Monitor** (115200 baud). The monitor shows the device's IP address.

Prefer the terminal? Use the commands below in a PlatformIO terminal.

## ⌨️ Terminal commands — Windows and Linux

From this repository root:

```text
pio run
pio device list
pio run --target upload
pio device monitor --baud 115200
```

The first build downloads the platform, host compiler and U8g2 library. Upload only to your intended connected board. Close any other serial monitor first; Ctrl+C exits the CLI monitor. If PlatformIO cannot auto-detect the port, use the actual port from `pio device list`:

**Windows example:**

```powershell
pio run --target upload --upload-port COM5
pio device monitor --port COM5 --baud 115200
```

**Linux example:**

```bash
pio run --target upload --upload-port /dev/ttyACM0
pio device monitor --port /dev/ttyACM0 --baud 115200
```

These are examples, not fixed port assignments; Linux may use `/dev/ttyUSB0`. If the board needs download mode, hold BOOT while starting upload and follow your board's reset procedure. Use a data-capable USB cable.

In VS Code, open this repository folder so PlatformIO finds `platformio.ini`, then use its Build and Upload actions. The same source files and project configuration are used on both systems.

## 🎭 Control the faces

The serial monitor reports the device's IP. Open that IP in your browser:

- `GET /`: device control page.
- `GET /status`: current mood and animation status.
- `GET /mood?name=happy`: start a face transition.

Supported moods: `normal`, `happy`, `sad`, `angry`, `sleepy`, `curious`, `thinking`, `uwu`, `shy`, `blush`, `unknown`.

Local HTTP controls have no authentication and are for trusted LAN use only. Never expose the ESP32's local web server directly to the internet.

## 📡 Connect to the backend with polling

Set these values in your private `include/edith_config.h` to match the backend:

```cpp
#define EDITH_CLOUD_ENABLED 1
#define EDITH_ALLOW_HTTP_TEST 1
static const char *EDITH_SERVER_URL = "http://YOUR_BACKEND_LAN_IP:8000";
static const char *EDITH_DEVICE_TOKEN = "YOUR_MATCHING_DEVICE_TOKEN";
```

Edit the existing declarations rather than adding duplicate ones. The backend must use `DEVICE_TRANSPORT=poll` with the same device token (at least 32 characters), and listen on `0.0.0.0:8000`. Use the backend computer's reachable LAN IP. On Linux find it with `ip -4 address` / `ip route`; on Windows use `ipconfig`. `127.0.0.1` on the ESP32 refers to the ESP32 itself, not the laptop or home PC. Moving the backend to Linux may change its IP, so rebuild/upload after updating this URL.

The local ESP32 web server is disabled by default in cloud/polling mode. Use the backend's chat page instead. Network requests run in a background task; rendering stays in the main loop. Acknowledgements are reported after transitions complete. Latest-command delivery, expiry and reconnect backoff are implemented. Local polling has been verified on Windows with the ESP32; hosted HTTPS/hotspot use is not yet verified.

For real hosting, set `EDITH_ALLOW_HTTP_TEST=0`, use an HTTPS origin, install the correct trusted root CA in `EDITH_ROOT_CA`, and synchronize the clock. Never replace certificate verification with an insecure bypass. See [HOSTED_CONNECTION.md](HOSTED_CONNECTION.md) and the backend's HOSTING.md.

## 🐧 Linux USB access

<details>
<summary>Board detected but upload says permission denied? Start here.</summary>

If PlatformIO cannot access the serial device, install the official PlatformIO udev rules:

```bash
curl -fsSLo /tmp/99-platformio-udev.rules https://raw.githubusercontent.com/platformio/platformio-core/develop/platformio/assets/system/99-platformio-udev.rules
sudo install -m 644 /tmp/99-platformio-udev.rules /etc/udev/rules.d/99-platformio-udev.rules
sudo udevadm control --reload-rules
sudo udevadm trigger
```

Unplug and reconnect the board. Use `pio device list` to identify it. If permission errors remain, check the actual port's group, for example:

```bash
ls -l /dev/ttyACM0
id
```

CachyOS/Arch commonly uses `uucp`. Only if the device is owned by that group, add your user:

```bash
sudo usermod -aG uucp "$USER"
```

Log out of KDE and back in for group changes to take effect. If the device is owned by a different group, use that group instead. Do not use `sudo pio` or make all serial devices world-writable. See the official [PlatformIO permissions guide](https://docs.platformio.org/en/stable/core/installation/udev-rules.html).

</details>

## 🛠️ Standalone PlatformIO CLI (optional)

<details>
<summary>Use PlatformIO without the VS Code extension</summary>

Install Git, VS Code and its PlatformIO IDE extension. The extension manages PlatformIO Core and provides Build, Upload and Serial Monitor buttons on Windows and Linux. If using that extension, its PlatformIO terminal already provides `pio`; skip the standalone CLI installation below.

For a standalone terminal workflow, install uv, then install PlatformIO into a separate tool environment. Do not install it into the backend environment or system Python.

### Windows — PowerShell

Install Git for Windows from https://git-scm.com/downloads/win. If uv is missing:

```powershell
powershell -ExecutionPolicy ByPass -c "irm https://astral.sh/uv/install.ps1 | iex"
```

Reopen PowerShell, then:

```powershell
uv tool install --python 3.11 platformio
uv tool update-shell
```

Reopen the terminal again and check `pio --version`. If uv is already installed, skip its installer. No administrator terminal is needed for PlatformIO itself.

### CachyOS / Arch Linux — bash or zsh

```bash
sudo pacman -Syu --needed git curl
curl -LsSf https://astral.sh/uv/install.sh | sh
```

Reopen the terminal, then:

```bash
uv tool install --python 3.11 platformio
uv tool update-shell
```

Reopen the terminal again and check `pio --version`. Do not run PlatformIO as root. Alternatively use the PlatformIO IDE extension's managed terminal instead of installing a second copy.

</details>

## 💻 Work on another computer

Before switching computers, commit and push source changes. On the other computer:

```text
git pull --ff-only
pio run
```

Resolve local edits before pulling; do not force-reset work. Private configuration files stay local and are not cloned. PlatformIO regenerates the operating system's build files. If build output becomes stale, use `pio run --target clean` and build again.

## 🗺️ Planned features and related projects

Touch reactions, sound cues, wake word "EDITH", speech input/output, a companion app and a speaking-mouth animation are planned, not implemented here.

- Backend: [QZainSami/edith-backend](https://github.com/QZainSami/edith-backend)
- Inspiration: [SathiyaSenpai/deskbot](https://github.com/SathiyaSenpai/deskbot)
- [uv installation](https://docs.astral.sh/uv/getting-started/installation/)
- [uv tool environments](https://docs.astral.sh/uv/concepts/tools/)

## 📄 License

[MIT](LICENSE), copyright 2026 QZainSami. Dependencies retain their own licenses.

## 💤 Clock dashboard, timer and mouth animation

Upload this firmware together with the updated backend. Existing Wi-Fi, device token and server URL settings stay the same. Polling uses `/api/device/poll-v2`; local mode adds `POST /companion`. Older backend versions do not provide these features.

Use the backend Control Center to switch between **Sleep / dashboard** and **Wake / face**, save a city/time zone, and start or dismiss a timer. Sleep is a connected dashboard, not power-saving deep sleep. The display shows date/time, weather icon, temperature, humidity and city; stale weather has an asterisk. Missing weather uses placeholders. Time advances locally from the latest backend synchronization.

Timers take over the OLED with large countdown digits, a slim progress bar and a slow orbiting dot, without clock/weather panels. They run locally through temporary network loss and show **TIME UP** until dismissed. After a power cycle, reconnect to recover the backend's saved timer. There is no buzzer or speaker alarm yet. Chat replies animate the mouth for an estimated duration; this is a visual speaking effect, with real audio synchronization planned later. Face transitions and blinking are preserved.

Typed backend commands include `EDITH sleep`, `EDITH wake up`, `set a timer for 5 minutes` and `cancel timer`. Weather location and clock offset are configured in the browser, not in firmware. Pakistan uses an offset of 300 minutes. No new hardware is required for these display features.

The dashboard and animation changes still require a successful local build/upload and physical OLED verification. The development environment's installed compiler failed to launch before C++ compilation.

## ⚡ Glitch boot and time tools

The OLED now has a short (about 1.4 second) robotic scan-line boot animation. New stopwatch and alarm displays use the existing network link; no new libraries or wiring are needed. Upload this firmware and restart the matching updated backend before using them.

Start/resume, pause/reset a stopwatch, or set/dismiss a one-time alarm from Android or the backend Control Center. Stopwatch elapsed time and alarm deadlines are restored from the backend after reconnecting. Both advance locally during a temporary network outage while the ESP32 stays powered. Stopwatch limit is 99:59:59.99. A due alarm overrides the other displays until dismissed; a timer overrides the selected stopwatch, and the stopwatch can keep running in the background when another screen is selected.

Alarms are visual only; speaker output is not implemented. Power loss needs a backend reconnection to restore state. Due alarms stay visible until dismissed in the app/web controls. The optional TOOLS1 extension adds six fields to the original v2 snapshot, while this firmware also accepts old 16-field snapshots.

## 📺 Ambient OLED glitches
During normal operation, a cosmetic glitch occurs every 25–70 seconds, including an evil-face appearance lasting one second. Shifted scan bands and a few noise pixels affect only the transmitted frame; the clean buffer is restored immediately. It adds no delays and leaves face/timer state intact. Due alarms and finished timer alerts suppress the effect. Adjust GLITCH_MIN_INTERVAL_MS and GLITCH_MAX_INTERVAL_MS in src/main.cpp to change the frequency.

## 🏠 Two backends + idle faces

Hold BOOT and open setup. Keep your home backend in **Python backend address** and add the laptop/university backend under **Second backend address**, with that backend's own **Second device token**. Save once. Existing saved Wi-Fi/primary settings are migrated automatically to the new format.

EDITH first tries the primary backend. After three failed polls (network error, failed authentication, or invalid response), it tries the other valid pair. It stays on whichever is working, without constantly switching back. Both servers must be reachable from the current network; two addresses do not make an offline home PC accessible from university. If both are reachable, this is availability-based fallback, not location detection. The Android app still needs the address of the backend you intend to use. Timers/settings on separate backends are not synchronized.

Prefer the setup page for an already configured ESP. For first-boot defaults in your private `include/edith_config.h`, leave your existing primary constants and add:

```cpp
#define EDITH_SECOND_BACKEND_ENABLED 1
static const char *EDITH_SERVER_URL_2 = "http://YOUR-LAPTOP-IP:8000";
static const char *EDITH_DEVICE_TOKEN_2 = "YOUR_SECOND_BACKENDS_DEVICE_TOKEN";
```

Do not duplicate the primary constant names. Each token must match its own backend's private `EDITH_DEVICE_TOKEN`. Saved portal settings take precedence over header defaults; set the second pair in the portal if settings were already saved in the new format. Local HTTP still requires `EDITH_ALLOW_HTTP_TEST=1`. Both HTTPS hosts must be trusted by `EDITH_ROOT_CA` (a PEM bundle can contain multiple roots); certificate verification remains enabled and redirects are disabled.

When left alone after startup, EDITH dissolves to the next of its eleven faces about every **8 seconds**. An app face/chat command pauses the idle rotation for **60 seconds**. Speech, dashboards, stopwatches, active/completed timers and due alarms take priority. The idle rotation never opens setup. The evil face remains a temporary glitch effect rather than a selectable mood.
