
# SMART CLASSROOM MONITORING SYSTEM
## Hardware Components and Wiring Configuration

### 1. List of Hardware Components

1. ESP32 Development Board
2. MFRC522 RFID Reader
3. RFID Cards
4. 20x4 LCD Display with I2C Module
5. 5V Relay Module
6. 12V Magnetic Door Lock
7. 12V DC Power Adapter
8. LM2596 Adjustable Buck Converter
9. Red LED
10. Green LED
11. Exit Push Button
12. DC Power Jack Terminal
13. Connecting Wires
14. Project Casing

### 2. Wiring Configuration

| Component | Connection |
|---|---|
| ESP32 | Main controller |
| MFRC522 | 3.3V, GND, SPI connections to ESP32 |
| 20x4 LCD I2C | 5V, GND, SDA and SCL |
| Relay Module | IN connected to GPIO32 |
| Red LED | ESP32 GPIO output |
| Green LED | ESP32 GPIO output |
| Exit Button | ESP32 digital input |
| LM2596 | DC voltage regulation |
| Magnetic Door Lock | 12V DC supply through relay contacts |

### 3. Magnetic Door Lock Wiring

1. DC Power Jack (+12V) → Relay COM
2. Relay NC → Magnetic Door Lock (+)
3. Magnetic Door Lock (-) → DC Power Jack (-12V)
4. Relay NO → Not Connected

### 4. System Operation

The ESP32 acts as the main controller.
The RFID reader detects RFID cards.
The LCD displays system information.
The relay controls the magnetic door lock.
The exit button provides manual exit operation.
The red and green LEDs indicate system status.
