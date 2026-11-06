# 📶 EDITH connection setup

Upload the updated firmware **once**. After that, use EDITH's setup page to change Wi-Fi and the Python backend address without uploading again.

## 🔌 Open setup

1. Power EDITH normally and let it finish starting.
2. Hold the board's **BOOT** button for **3 seconds**, then release it. **Do not hold BOOT while resetting/powering on**; that enters the ESP flashing mode instead.
3. The OLED shows an **EDITH-Setup-XXXX** network, a temporary **KEY**, and an IP address.
4. Join that network from a phone or computer using the displayed key. Choose **stay connected / use without internet** if prompted.
5. Open **http://192.168.4.1** in a browser (use the address shown on the OLED). Type `http://` explicitly; this is a local setup page.

Setup opens **only when you hold BOOT**. It never opens automatically, even with no Wi-Fi configuration or no connection. It closes after **5 minutes**; hold BOOT again to reopen it. The temporary key changes each time.

Boards with a different setup button pin can define `EDITH_SETUP_BUTTON_PIN`; `-1` disables setup activation. The Serial `SETUP` shortcut has been removed so only the button opens setup.

## 💾 Save connection settings

- **Wi-Fi name:** your home network or mobile hotspot name, matching spelling and case.
- **Wi-Fi password:** enter it for a new network. Leave blank to retain a saved network's password. Select **network has no password** only for an intentionally open network.
- **Python backend address:** for example `http://192.168.100.108:8000`. Use your laptop/PC's current network address, **not** the ESP address, `127.0.0.1`, emulator-only `10.0.2.2`, or LM Studio's port `1234`.
- **Device token:** usually leave blank. If switching to a backend with a different token, enter its private `EDITH_DEVICE_TOKEN`; this is **not** the app login password.

Tap **Save & restart EDITH**. Your three most recently saved networks are remembered; EDITH tries the newest first and cycles through them if unavailable. You can save two backend URL/token pairs. EDITH stays with the working pair and switches after three failed polls. Both backends must be yours and configured for EDITH; each URL uses only its own token.

Set the **same backend address in the Android app's Settings**. Changing the ESP's settings does not change the Android app automatically.

Settings survive a restart and normal firmware upload. The private `include/edith_config.h` provides defaults only until setup settings are saved. Editing those defaults afterward does not replace saved settings—use setup to update them. Erasing all ESP flash removes saved settings.

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

## 🔒 Local setup and HTTPS

The setup page is available only through EDITH's temporary password-protected Wi-Fi, not through its normal LAN address. Each form has a session token, and saved Wi-Fi passwords/device tokens are never displayed by the page. Credentials are saved in ordinary ESP NVS; this development firmware does **not** enable encrypted flash. Do not publish private config files.

Local HTTP remains controlled by `EDITH_ALLOW_HTTP_TEST`. HTTPS continues to validate certificates with `EDITH_ROOT_CA`; the portal never disables TLS verification. A future host with a different trust root may require updating that root certificate in firmware. This portal changes connection settings; it does not enable cloud mode or replace certificate provisioning.

## ✅ First hardware check

- Open setup using BOOT while EDITH is already running.
- Confirm invalid addresses are rejected and a blank password retains the current saved password.
- Save the current network/address, restart, and confirm the OLED comes back online.
- Save your hotspot as a second network, restore the hotspot, and confirm EDITH reconnects.
- Change the backend to the laptop, update Android Settings, and send a face command.
- Open Sleep: check the clearer clock, second markers, clipped city label, weather panel and alarm footer. A running timer or due alarm still takes priority.

The Android preview is a drawing of the same layout, not a live framebuffer stream. Rebuild/install the Android app to update that preview.

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
