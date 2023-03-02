# Instructions to develop for the ESP32 using this repo

- First, make you sure have installed the correct ESP-IDF version: 4.4.4 release version (NOT 4.4 release branch!)

For some reason, the sdkconfig does not contain all settings used for the ESP32-S2, so we first need to configure it.

- Execute `idf.py menuconfig` from the esp-idf command prompt
- Go to `Component config`

Set the following items:
- Component config
    - ESP32S2-specific:
        - CPU frequency = 240 MHz
    - ESP System settings
        - Panic handler behaviour = GDBStub on panic
        - Channel for console output = USB CDC

Optionally, if you want to turn off the webserver to compile faster, you can set:

- Webserver settings
    - Website deploy mode = Deploy website to host (JTAG is needed)

Then, in main.c you'll need to comment out the next lines: 
`wifi_init_softap();`
`ESP_ERROR_CHECK(init_fs());`
`ESP_ERROR_CHECK(start_rest_server(CONFIG_EXAMPLE_WEB_MOUNT_POINT));`