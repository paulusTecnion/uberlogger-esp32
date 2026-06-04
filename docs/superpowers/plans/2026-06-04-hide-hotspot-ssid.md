# Hide Hotspot SSID Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a user-configurable option to hide the SoftAP (hotspot) SSID broadcast on the ESP32, exposed as a checkbox in the web config UI.

**Architecture:** A new persisted `uint8_t wifi_ssid_ap_hidden` setting is threaded through the two JSON layers (the lowercase persisted `/spiffs/settings.json` in `settings.c`, and the UPPERCASE REST API in `rest_server.c`), applied at boot to ESP-IDF's built-in `wifi_ap_config_t.ssid_hidden` field. Default is `0` (visible), and the flag takes effect on reset, mirroring the existing Wi-Fi channel setting.

**Tech Stack:** ESP-IDF (C), cJSON, jQuery frontend. No unit-test harness exists — verification is `./build.sh -n` (compile) plus on-device manual checks. Frequent commits per task.

**Spec:** `docs/superpowers/specs/2026-06-04-hide-hotspot-ssid-design.md`

---

## File Structure

- `main/settings.h` — add struct field + getter/setter declarations
- `main/settings.c` — default, getter/setter, persisted JSON serialize + parse, log
- `main/wifi.c` — apply `ap.ssid_hidden` in `wifi_start()` and `wifi_update_ap()`
- `main/rest_server.c` — emit `WIFI_SSID_HIDDEN` in getConfig, parse it in setConfig
- `front/www/html/config.html` — checkbox + warning note in "Hotspot settings"
- `front/www/js/config.js` — include `WIFI_SSID_HIDDEN` in the setConfig payload
- `front/DefaultConfig.json` — add `"WIFI_SSID_HIDDEN": false`

---

## Task 1: Storage layer — persisted setting

**Files:**
- Modify: `main/settings.h` (struct ~line 194, declarations ~line 299)
- Modify: `main/settings.c` (default ~line 327, getter/setter ~line 647, parse ~line 926, serialize ~line 1232, print ~line 1147)

- [ ] **Step 1: Add the struct field**

In `main/settings.h`, inside `struct Settings_t` (the CURRENT struct, NOT `Settings_old_t`), add the field at the end of the security settings block. Find:

```c
	/* Security settings */
	char wifi_password_ap[MAX_WIFI_AP_PASSW_LEN]; // password for the device's own WiFi AP hotspot
	char web_password[MAX_WEB_PASSWORD_LEN];       // password for web UI HTTP Basic Auth
};
```

Replace with:

```c
	/* Security settings */
	char wifi_password_ap[MAX_WIFI_AP_PASSW_LEN]; // password for the device's own WiFi AP hotspot
	char web_password[MAX_WEB_PASSWORD_LEN];       // password for web UI HTTP Basic Auth
	uint8_t wifi_ssid_ap_hidden;                   // 0 = SoftAP SSID broadcast (default), 1 = hidden
};
```

> Do NOT touch `struct Settings_old_t` — its byte layout must stay frozen for binary migration.

- [ ] **Step 2: Add getter/setter declarations**

In `main/settings.h`, find:

```c
uint8_t settings_get_wifi_channel();
esp_err_t settings_set_wifi_channel(uint8_t channel);
```

Add immediately after:

```c
uint8_t settings_get_wifi_ssid_ap_hidden();
esp_err_t settings_set_wifi_ssid_ap_hidden(uint8_t hidden);
```

- [ ] **Step 3: Initialize the default**

In `main/settings.c`, in `settings_set_default()`, find:

```c
    _inSettings->wifi_mode = WIFI_MODE_AP;
    _inSettings->wifi_channel = 1;
```

Add immediately after:

```c
    _inSettings->wifi_ssid_ap_hidden = 0; // SoftAP SSID visible by default
```

- [ ] **Step 4: Implement getter/setter**

In `main/settings.c`, find the `settings_set_wifi_channel` function:

```c
esp_err_t settings_set_wifi_channel(uint8_t channel)
{
    if (channel > 0 && channel < 14)
    {
        _settings.wifi_channel = channel;
        return ESP_OK;
    } else {
        return ESP_FAIL;
    }
}
```

Add immediately after it:

```c
uint8_t settings_get_wifi_ssid_ap_hidden()
{
    return _settings.wifi_ssid_ap_hidden;
}

esp_err_t settings_set_wifi_ssid_ap_hidden(uint8_t hidden)
{
    _settings.wifi_ssid_ap_hidden = hidden ? 1 : 0;
    return ESP_OK;
}
```

- [ ] **Step 5: Parse from persisted JSON**

In `main/settings.c`, in `settings_load_json`, find:

```c
    const cJSON* wifi_channel = cJSON_GetObjectItemCaseSensitive(root, "wifi_channel");
    if (cJSON_IsNumber(wifi_channel)) {
        _settings.wifi_channel = wifi_channel->valueint;
    }
```

Add immediately after:

```c
    const cJSON* wifi_ssid_ap_hidden = cJSON_GetObjectItemCaseSensitive(root, "wifi_ssid_ap_hidden");
    if (cJSON_IsNumber(wifi_ssid_ap_hidden)) {
        _settings.wifi_ssid_ap_hidden = wifi_ssid_ap_hidden->valueint ? 1 : 0;
    }
```

> Missing key is fine — the default from Step 3 (`0`) is already set before load runs, so old `settings.json` files upgrade cleanly.

- [ ] **Step 6: Serialize to persisted JSON**

In `main/settings.c`, find (around line 1232):

```c
    cJSON_AddNumberToObject(root, "wifi_channel", _settings.wifi_channel);
    cJSON_AddNumberToObject(root, "wifi_mode", _settings.wifi_mode);
```

Add immediately after:

```c
    cJSON_AddNumberToObject(root, "wifi_ssid_ap_hidden", _settings.wifi_ssid_ap_hidden);
```

- [ ] **Step 7: Log the value (consistency with other Wi-Fi fields)**

In `main/settings.c`, in `settings_print()`, find:

```c
    ESP_LOGI(TAG_SETTINGS, "Wifi channel %d", _settings.wifi_channel);
```

Add immediately after:

```c
    ESP_LOGI(TAG_SETTINGS, "Wifi AP SSID hidden %d", _settings.wifi_ssid_ap_hidden);
```

- [ ] **Step 8: Build to verify it compiles**

Run: `./build.sh -n`
Expected: build completes, prints `Output file ota_main.bin and ota_filesystem.bin.` with no compile errors.

- [ ] **Step 9: Commit**

```bash
git add main/settings.h main/settings.c
git commit -m "feat(settings): add persisted wifi_ssid_ap_hidden option"
```

---

## Task 2: Wi-Fi apply — set ap.ssid_hidden

**Files:**
- Modify: `main/wifi.c` (`wifi_start()` AP block ~line 363, `wifi_update_ap()` AP block ~line 428)

- [ ] **Step 1: Apply in `wifi_start()`**

In `main/wifi.c`, in `wifi_start()`, find:

```c
    // always default to Uberlogger
    strcpy((char*)wifi_config.ap.ssid, settings_get_wifi_ssid_ap());
    wifi_config.ap.ssid_len = strlen((const char*)(wifi_config.ap.ssid));
    strcpy((char*)wifi_config.ap.password, settings_get_wifi_password_ap());
```

Add immediately after:

```c
    wifi_config.ap.ssid_hidden = settings_get_wifi_ssid_ap_hidden();
```

- [ ] **Step 2: Apply in `wifi_update_ap()`**

In `main/wifi.c`, in `wifi_update_ap()`, find:

```c
    strcpy((char*)wifi_config.ap.ssid, settings_get_wifi_ssid_ap());
    wifi_config.ap.ssid_len = strlen((const char*)(wifi_config.ap.ssid));
    strcpy((char*)wifi_config.ap.password, settings_get_wifi_password_ap());

    if (strlen((const char*)(wifi_config.ap.password)) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    return esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
```

Replace with:

```c
    strcpy((char*)wifi_config.ap.ssid, settings_get_wifi_ssid_ap());
    wifi_config.ap.ssid_len = strlen((const char*)(wifi_config.ap.ssid));
    strcpy((char*)wifi_config.ap.password, settings_get_wifi_password_ap());
    wifi_config.ap.ssid_hidden = settings_get_wifi_ssid_ap_hidden();

    if (strlen((const char*)(wifi_config.ap.password)) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    return esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
```

- [ ] **Step 3: Build to verify it compiles**

Run: `./build.sh -n`
Expected: build completes with no compile errors.

- [ ] **Step 4: Commit**

```bash
git add main/wifi.c
git commit -m "feat(wifi): apply ssid_hidden to SoftAP config"
```

---

## Task 3: REST API — getConfig / setConfig

**Files:**
- Modify: `main/rest_server.c` (getConfig ~line 550, setConfig ~line 1077)

- [ ] **Step 1: Emit the value in getConfig**

In `main/rest_server.c`, in `logger_getConfig_handler` (the response builder), find:

```c
    cJSON_AddNumberToObject(root, "WIFI_CHANNEL", settings->wifi_channel);
```

Add immediately after:

```c
    cJSON_AddBoolToObject(root, "WIFI_SSID_HIDDEN", settings->wifi_ssid_ap_hidden);
```

> Use `AddBoolToObject` so the frontend checkbox (a `json-as-bool` control) round-trips as a JS boolean, matching `WIFI_PASSWORD_AP_SET` and the channel-enable toggles.

- [ ] **Step 2: Parse the value in setConfig**

In `main/rest_server.c`, in `logger_setConfig_handler`, find the end of the `WIFI_CHANNEL` block:

```c
        if (oldSettings.wifi_channel != settings_get_wifi_channel())
        {
            ap_update_required = true;
        }
    }
```

Add immediately after that closing brace:

```c
    item = cJSON_GetObjectItemCaseSensitive(settings_in, "WIFI_SSID_HIDDEN");
    if (item != NULL)
    {
        // Takes effect on next reset (mirrors the channel "press reset after
        // saving" guidance), so we deliberately do NOT set ap_update_required.
        settings_set_wifi_ssid_ap_hidden(cJSON_IsTrue(item) ? 1 : 0);
    }
```

- [ ] **Step 3: Build to verify it compiles**

Run: `./build.sh -n`
Expected: build completes with no compile errors.

- [ ] **Step 4: Commit**

```bash
git add main/rest_server.c
git commit -m "feat(rest): expose WIFI_SSID_HIDDEN in get/setConfig"
```

---

## Task 4: Frontend — checkbox, payload, default

**Files:**
- Modify: `front/www/html/config.html` ("Hotspot settings" group, after the Wi-Fi channel note ~line 502)
- Modify: `front/www/js/config.js` (setConfig payload object, near `WIFI_CHANNEL` ~line 471)
- Modify: `front/DefaultConfig.json`

- [ ] **Step 1: Add the checkbox + warning note in config.html**

In `front/www/html/config.html`, find (inside the `wifi_configuration` group):

```html
        <p class="note" style="margin-top:0"><i><b>Note:</b> Press the reset button manually after saving this setting.</i></p>
```

Add immediately after:

```html
        <!-- Hide SSID toggle -->
        <div class="field-inline" style="margin-top:12px;">
          <input type="checkbox" name="WIFI_SSID_HIDDEN" id="WIFI_SSID_HIDDEN" value="1" class="json-as-bool" />
          <label for="WIFI_SSID_HIDDEN">Hide hotspot network name (SSID)</label>
        </div>
        <p class="note" style="margin-top:0"><i>When enabled, the hotspot won't appear in Wi-Fi network lists &mdash; you'll need to enter the network name manually to connect. Press the reset button after saving.</i></p>
```

- [ ] **Step 2: Include the field in the setConfig payload**

In `front/www/js/config.js`, in `setConfig()`, find:

```js
    WIFI_CHANNEL: input["WIFI_CHANNEL"],
```

Add immediately after:

```js
    WIFI_SSID_HIDDEN: input["WIFI_SSID_HIDDEN"],
```

> Loading needs no change: `parseConfig` already calls `populateFields("#configuration", data)`, which will check the box when getConfig returns `WIFI_SSID_HIDDEN: true`.

- [ ] **Step 3: Add the default to DefaultConfig.json**

In `front/DefaultConfig.json`, find:

```json
	"WIFI_SSID": "wifi network name",
	"WIFI_PASSWORD": "wifi password",
```

Add immediately after:

```json
	"WIFI_SSID_HIDDEN": false,
```

- [ ] **Step 4: Build to verify the filesystem image builds**

Run: `./build.sh -n`
Expected: build completes, including the `www.bin` / `ota_filesystem.bin` step, with no errors.

- [ ] **Step 5: Commit**

```bash
git add front/www/html/config.html front/www/js/config.js front/DefaultConfig.json
git commit -m "feat(ui): add hide-hotspot-SSID toggle to config page"
```

---

## Task 5: On-device manual verification

**Files:** none (flash + observe)

- [ ] **Step 1: Flash the device**

Run: `./build.sh` (flashes to `/dev/ttyACM0`)
Expected: build + flash succeed; device reboots.

- [ ] **Step 2: Round-trip the toggle**

1. Open the config page, go to "Hotspot settings".
2. Check "Hide hotspot network name (SSID)", click Save.
3. Reload the config page.
   Expected: the checkbox is still checked (value persisted via getConfig).
4. Uncheck it, Save, reload.
   Expected: checkbox is unchecked.

- [ ] **Step 3: Verify broadcast behavior**

1. With the toggle ON and saved, press the device reset button.
2. From a phone/laptop, scan for Wi-Fi networks.
   Expected: the `Uberlogger-XXXX` hotspot does NOT appear in the list.
3. Manually add a network with the exact SSID and connect.
   Expected: connection succeeds.
4. Turn the toggle OFF, save, reset, scan again.
   Expected: the hotspot appears in the scan list normally.

- [ ] **Step 4: Verify upgrade safety (optional, if a pre-update device is available)**

Flash onto a device that already has a `settings.json` lacking the key.
Expected: device boots with the hotspot visible (default `0`), no settings corruption, serial log shows `Wifi AP SSID hidden 0`.

---

## Notes for the implementer

- There is no automated test suite; "build to verify" means a clean `./build.sh -n` compile. Treat any compiler warning/error in the touched files as a failing step.
- The two JSON layers are independent and both required: lowercase `wifi_ssid_ap_hidden` (persisted file, Task 1) and UPPERCASE `WIFI_SSID_HIDDEN` (REST API, Task 3). Do not collapse them.
- `wifi_ap_config_t.ssid_hidden` is a standard ESP-IDF field — no struct definition or include change is needed.
