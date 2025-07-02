# DHT11 EEPROM Logger With RTC

This Arduino project logs temperature readings using a DHT11 sensor, adds timestamp data from a DS1302 Real-Time Clock (RTC), and stores everything in EEPROM memory.

## 📦 Features

- Logs temperature every 10 seconds
- Timestamp from DS1302 RTC
- Stores data in EEPROM (automatically erases when full)
- Calculates:
  - Minimum temperature
  - Maximum temperature
  - Average temperature every 30 seconds
- Supports serial commands to view saved logs

---

## 🛠 Hardware Required

| Component        | Quantity |
|------------------|----------|
| Arduino Uno/Nano | 1        |
| DHT11 Sensor     | 1        |
| DS1302 RTC Module| 1        |
| Breadboard + Wires| as needed |

## 🚀 Usage

1. Upload the `DHT11_EEPROM_Logger_With_RTC.ino` file to your Arduino using the Arduino IDE.
2. Open Serial Monitor at **9600 baud**.
3. Every 10 seconds, the system will:
   - Log a temperature with timestamp
   - Print current temperature
4. Every 30 seconds, it prints stats (min, max, average).
5. To view all logged history, type: r 
