# 🧠 Uberlogger – ESP32 Development Instructions

Welcome to the development repository for the Uberlogger, a powerful ESP32-based logging device.

This guide provides instructions for setting up your development environment and configuring the project for the ESP32-S2 using **ESP-IDF v5.0.1**.

---

## 🚀 Prerequisites

1. Install **ESP-IDF v5.0.1**:  
   Follow the official Espressif setup guide for your OS:  
   👉 [ESP-IDF v5.0.1 Setup (Linux/macOS)](https://docs.espressif.com/projects/esp-idf/en/v5.0.1/esp32/get-started/linux-macos-setup.html)

2. Clone this repository and open a terminal in the root directory.

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
   wifi_init_softap();
   ESP_ERROR_CHECK(init_fs());
   ESP_ERROR_CHECK(start_rest_server(CONFIG_EXAMPLE_WEB_MOUNT_POINT));
   ```

---

## 🐞 Debugging Panics with GDB Stub

With the above settings, the **GDB Stub** is enabled. To monitor debug output and catch kernel panics:

```bash
idf.py monitor
```

This allows real-time inspection of system crashes and other low-level events.

---

## 📄 Licensing and Legal

### ✅ Software License

The code in this repository is licensed under the [MIT License](LICENSE). You are free to use, modify, and redistribute it — even commercially — as long as proper attribution is provided.

### 🚫 Hardware Notice

The **hardware** design, including schematics, PCB layouts, mechanical components, and industrial design of the **Uberlogger**, is **proprietary**. It is **not covered** under the MIT License and may **not** be reproduced, sold, or modified for commercial use without **explicit written permission** from the author.

---

Happy Hacking! 🛠️
