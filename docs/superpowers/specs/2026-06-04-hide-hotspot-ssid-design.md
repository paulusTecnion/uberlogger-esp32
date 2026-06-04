# Hide Hotspot SSID — Design

**Date:** 2026-06-04
**Status:** Approved (design)

## Goal

Add a user-configurable option to hide the SoftAP (hotspot) SSID broadcast. When
enabled, the UberLogger's own Wi-Fi hotspot stops broadcasting its network name,
so it will not appear in device Wi-Fi scan lists. Users must then enter the SSID
manually to connect. Default is **off** (SSID visible), preserving current behavior.

## Decisions

- **Apply timing:** takes effect **on reset**, mirroring the existing Wi-Fi
  channel setting. The value is persisted and applied at boot in `wifi_start()`.
  No live re-apply (`wifi_update_ap()`) path is added for this flag.
- **UX:** an italic warning note is shown next to the toggle, plus reuse of the
  existing "Press the reset button manually after saving this setting" guidance.

## Architecture — four layers

The system has **two distinct JSON layers** that both carry settings, and they
must not be confused:

1. **Persisted settings file** (`/spiffs/settings.json`) — handled in
   `main/settings.c`, uses **lowercase** keys (e.g. `wifi_channel`). This is how
   a setting survives reboot.
2. **REST API** (`/ajax/getConfig`, `/ajax/setConfig`) — handled in
   `main/rest_server.c`, uses **UPPERCASE** keys (e.g. `WIFI_CHANNEL`). This is
   the frontend round-trip.

ESP-IDF's `wifi_ap_config_t` already exposes a `.ssid_hidden` field (0 = visible,
1 = hidden), so the firmware work is plumbing, not protocol design.

### Layer 1 — Storage (`main/settings.h`, `main/settings.c`)

- Add `uint8_t wifi_ssid_ap_hidden;` to `struct Settings_t` (the current struct).
  **Do NOT** add it to `struct Settings_old_t` — that legacy binary blob layout
  must stay byte-frozen for migration to keep working.
- `settings_set_default()`: initialize `wifi_ssid_ap_hidden = 0`.
- Getter/setter: `uint8_t settings_get_wifi_ssid_ap_hidden(void)` and
  `esp_err_t settings_set_wifi_ssid_ap_hidden(uint8_t value)`.
- Persisted JSON **serialize** (`settings_persist_settings` / cJSON build,
  near the other `cJSON_AddNumberToObject(root, "wifi_channel", ...)`):
  add `cJSON_AddNumberToObject(root, "wifi_ssid_ap_hidden", _settings.wifi_ssid_ap_hidden);`
- Persisted JSON **parse** (`settings_load_json`, near the `wifi_channel` parse):
  read `"wifi_ssid_ap_hidden"` as a number into `_settings.wifi_ssid_ap_hidden`.
- Optionally log it in `settings_print()` alongside the other Wi-Fi fields.

### Layer 2 — Wi-Fi apply (`main/wifi.c`)

- In the AP config block of `wifi_start()` (and `wifi_update_ap()` for
  consistency), set:
  `wifi_config.ap.ssid_hidden = settings_get_wifi_ssid_ap_hidden();`
- No new live-restart trigger is wired — the flag is read fresh on each boot.

### Layer 3 — REST API (`main/rest_server.c`)

- **getConfig** (`logger_getConfig_handler`, near
  `cJSON_AddNumberToObject(root, "WIFI_CHANNEL", ...)`):
  add `cJSON_AddNumberToObject(root, "WIFI_SSID_HIDDEN", settings->wifi_ssid_ap_hidden);`
- **setConfig** (`logger_setConfig_handler`, near the `WIFI_CHANNEL` parse):
  read `"WIFI_SSID_HIDDEN"`, call `settings_set_wifi_ssid_ap_hidden(item->valueint)`.
  Because timing is "on reset", do **not** set `ap_update_required` for this flag.

### Layer 4 — Frontend (`front/www/html/config.html`, `front/www/js/config.js`, `front/DefaultConfig.json`)

- **config.html:** in the "Hotspot settings" group, add a checkbox
  `id="WIFI_SSID_HIDDEN" name="WIFI_SSID_HIDDEN"` (class consistent with other
  boolean/number inputs so `populateFields` and the save path treat it as a
  number — 0/1). Add an italic note: *"When enabled, the hotspot won't appear in
  Wi-Fi network lists — you'll need to enter the network name manually to
  connect."*
- **config.js:** include `WIFI_SSID_HIDDEN: input["WIFI_SSID_HIDDEN"]` (as a
  number, 0/1) in the `setConfig()` payload object. Loading is handled by the
  existing `populateFields("#configuration", data)` since getConfig now returns
  the `WIFI_SSID_HIDDEN` key.
- **DefaultConfig.json:** add `"WIFI_SSID_HIDDEN": 0` so "Load defaults" works.

## Data flow

Checkbox → setConfig (UPPERCASE `WIFI_SSID_HIDDEN`) → rest_server parse →
`settings_set_wifi_ssid_ap_hidden` → `settings_persist_settings` writes lowercase
`wifi_ssid_ap_hidden` to `/spiffs/settings.json`. On next boot, `wifi_start()`
reads it and sets `ap.ssid_hidden`. Display is the reverse: getConfig emits
`WIFI_SSID_HIDDEN`, the checkbox is populated from it.

## Migration / compatibility

No special migration code and no `SETTINGS_FORMAT_VERSION` bump required:

- Current format is JSON, parsed key-by-key. `settings_set_default()` runs before
  any load and sets the field to `0`; `settings_load_json()` overrides only keys
  present in the file, so an old `settings.json` lacking the key keeps `0`.
- The legacy binary path (`settings_migrate`) never references the new field, so
  it also keeps the default `0`.

## Error handling

- `settings_set_wifi_ssid_ap_hidden` validates/normalizes input to 0 or 1.
- setConfig treats a missing `WIFI_SSID_HIDDEN` key as "no change" (consistent
  with other optional keys) — it does not error.

## Testing

- Build firmware via `build.sh` — must compile cleanly.
- Round-trip: enable the toggle, save, reload the config page → checkbox is
  checked; disable, save, reload → unchecked.
- Behavioral: after a reset with the toggle on, the hotspot does not appear in a
  Wi-Fi scan, yet is still joinable by manually entering the SSID. With the
  toggle off, the hotspot broadcasts normally.
- Upgrade safety: a device with a pre-existing `settings.json` (no key) boots
  with the hotspot visible (default 0).
