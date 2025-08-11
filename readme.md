# 🧠 Uberlogger – ESP32 Development Instructions

Welcome to the development repository for the Uberlogger, a simple yet powerful ESP32-based logging device. 

The software of this device was not open source, since I spent a lot of time on it and I'm still selling the device. 50% of the profits go to EDELAC, a school for the poorest children in the city Xela of Guatemala. Through this product, I helping them a bit with getting education and you, the end user, with logging your data. You can find more information about the school [here](https://edelac.org/). 

I decided to make this open source, since I have little time to make more features alone by myself, and next to that I notice there are hobbyists and research people who might be interested in using or adjusting the code to their needs. Since I value innovation and education, it made sense to make this open source. 

## 💖 Support our cause

If you buy the Uberlogger, you help the children in Guatemala. Ordering can be done at our partner [Eleshop](https://eleshop.eu/uberloggerul01.html). As written below, the code is open source, but the hardware is not. Sorry. 

## 📄 Licensing and Legal

### ✅ Software License

The code in this repository is licensed under the [MIT License](LICENSE). You are free to use, modify, and redistribute it — even commercially — as long as proper attribution is provided.

### 🚫 Hardware Notice

The **hardware** design, including schematics, PCB layouts, mechanical components, and industrial design of the **Uberlogger**, is **proprietary**. It is **not covered** under the MIT License and may **not** be reproduced, sold, or modified for commercial use without **explicit written permission** from the author. 

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
