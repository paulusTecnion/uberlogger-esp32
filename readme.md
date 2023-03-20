# Instructions to develop for the ESP32 using this repo

First, make you sure have installed the correct ESP-IDF version: 5.0.1 release version. In Linux you will have to use these instructions:

https://docs.espressif.com/projects/esp-idf/en/v5.0.1/esp32/get-started/linux-macos-setup.html

For some reason, the sdkconfig does not always contain all settings used for the ESP32-S2, so we first need to check the configuration after you have downloaded the repository on your pc.

- Open the ESP-IDF console
- Execute `idf.py menuconfig` from the esp-idf command prompt
- Go to `Component config`

Make sure the following items are set if not done already:
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
- `wifi_init_softap();`
- `ESP_ERROR_CHECK(init_fs());`
- `ESP_ERROR_CHECK(start_rest_server(CONFIG_EXAMPLE_WEB_MOUNT_POINT));`

With the settings above, the GDB Stub is enabled, which allows you to dig a little better at potential issues when a kernel panic happens. In order to make use of this, it's best to use `idf.py monitor` to see the debug output of the ESP and catching kernel panics.