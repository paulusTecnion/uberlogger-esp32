# 🧠 Uberlogger – ESP32 Development Instructions

![Uberlogger UL01](https://github.com/user-attachments/assets/0152f741-67ca-4d7e-8d23-f9b1f567e315)

Welcome to the  repository for the [Uberlogger](https://www.uberlogger.com], a simple yet powerful ESP32-based & STM32 WiFi enabled logging device. The logger can log 8 analog channels and 6 digital channels at maximum 250 Hz at 16 or 12 bits to CSV or raw data file on an SD card and is fully stand-alone. The analog channels can be set as either NTC inputs or normal analog inputs with input ranges varying from -10V to +10V DC or from -60V to + 60V DC. There is a web UI interface to configure the device and see live data and an [API](https://docs.uberlogger.com/api) to retrieve data using your own scripts or devices.

The software of this device was not open source, since I spent a lot of time on it and I'm still selling the device. 50% of the profits go to EDELAC, a school for the poorest children in the city Xela of Guatemala. Through this product, I am helping them a bit with getting education and you, the end user, with logging your data. You can find more information about the school [here](https://edelac.org/). 

I decided to make this open source, since I have little time to make more features alone by myself, and next to that I notice there are hobbyists and research people who might be interested in using or adjusting the code to their needs. Since I value innovation and education, it made sense to make this open source. The source is what it is, with all the things that are nice and not so nice :-) I'm not at full-time programmer, so I was also learning along the way. What makes it a challenge to debug is that it needs to be stable over long periods over time and it uses 2 chips, from which the documentation was not always completely right. I don't have time to give support for new features you might want to add, but you are free to adjust or contribute and do whatever you want with it. 

This repo is the software for the ESP32-S2 chip. The repo for the STM32G032 can be found [here](https://github.com/paulusTecnion/uberlogger-stm32). 

## 💖 Support our cause

If you buy the Uberlogger, you help the children in Guatemala. Ordering can be done at our partner [Eleshop](https://eleshop.eu/uberloggerul01.html) for best and fast international shipments. As written below, the code is open source, but the hardware is not. Sorry. 

## 📄 Licensing and Legal

### ✅ Software License

The code in this repository is licensed under the [MIT License](LICENSE). You are free to use, modify, and redistribute it — even commercially — as long as proper attribution is provided.

### 🚫 Hardware Notice

The **hardware** design, including schematics, PCB layouts, mechanical components, and industrial design of the **Uberlogger**, is **proprietary**. It is **not covered** under the MIT License and may **not** be reproduced, sold, or modified for commercial use without **explicit written permission** from the author. 

## Features that might be interesting

- NTP sync: I tried to integrate this, but when there is a request to do an NTP sync while it also wants to send data while logging, the ESP32 stack may run into issues. My architecture is not build for this, so you may have to refactor this or hack your way to make it work (but the latter is generally not recommended with this device)
- Add MQTT / Home-assistant support . Doing this in parallel with existing polling method / webserver might put stress on resources, but I haven't done much research on it
- Showing MAC addresses of the WiFi ESP32 hotspot and client controllers in the web UI.
- Increase log speed
- Stream live data instead of polling. Comes with challenges to make it work on all devices, though. Polling is just easy and straight forward.
- Reduce JSON size for settings / live data
- Increase possiblities of low-power energy => very valuable! But lot of work (lots of testing mainly)
- Add unit tests where you can...
- Add password for logging in
- Your fantastic feature....


---

This guide provides instructions for setting up your development environment and configuring the project for the ESP32-S2 using **ESP-IDF v5.2.x**.

---

## 🚀 Prerequisites

1. Install **ESP-IDF v5.2.x**:  
   Follow the official Espressif setup guide for your OS:  
   👉 [ESP-IDF v5.2.x Setup (Windows)](https://docs.espressif.com/projects/esp-idf/en/release-v5.2/esp32/get-started/windows-setup.html)
   👉 [ESP-IDF v5.2.x Setup (Mac/Linux)](https://docs.espressif.com/projects/esp-idf/en/release-v5.2/esp32/get-started/linux-macos-setup.html)

3. Clone this repository and open a terminal in the root directory.

---

## ⚙️ Post-Clone Configuration

Sometimes the `sdkconfig` file lacks some important ESP32-S2 settings. After cloning:

1. Open the **ESP-IDF Console**
2. Run:
   ```bash
   idf.py menuconfig
   ```
3. Go to:
   ```
   Component config → ESP32S2-specific
   ```
4. Ensure the following values are set:

- **Component Config**
  - **ESP32S2-specific**
    - `CPU frequency` = `240 MHz`
  - **ESP System Settings**
    - `Panic handler behaviour` = `GDBStub on panic`
    - `Channel for console output` = `USB CDC`
5. Putting Uberlogger in programming mode

 - Turn on the Uberlogger
 - Then hold the "mode" and "reset" button simultaneously and release the "reset" button. A new COM port should pop on your PC.
 - Use ```idf.py build flash``` to build and flash the Uberlogger or add the ```-p COMPORT``` parameter where COMPORT is the COM port name/location, to flash directly without using auto-detect.
---



## 🧪 Optional: Disable Webserver (for faster build)

If you don't need the webserver during development:

1. Navigate to:
   ```
   Webserver settings → Website deploy mode
   ```
2. Set to:

   ```
   Deploy website to host (JTAG is needed)
   ```

3. In `main.c`, **comment out** the following lines:
   ```c
  
   ESP_ERROR_CHECK(init_fs());
   ...
   
    if (settings_get_boot_reason() ==0)
    {
        wifi_init();
    }

   ...
   if (settings_get_boot_reason() ==0)
    {
        wifi_start();
    }
   ...
    if (settings_get_boot_reason() == 0)
    {
        ESP_ERROR_CHECK(start_rest_server(CONFIG_EXAMPLE_WEB_MOUNT_POINT));
    }
  
   ```

---

## 🐞 Debugging Panics with GDB Stub

With the above settings, the **GDB Stub** is enabled. To monitor debug output and catch kernel panics:

```bash
idf.py monitor
```

This allows real-time inspection of system crashes and other low-level events.

---



Happy Hacking! 🛠️
