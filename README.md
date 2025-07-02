# ESP32 HomeKit Sensor Switch

A HomeKit-compatible smart switch with temperature and humidity sensing capabilities for ESP32, built using the ESP HomeKit SDK.

## Features

- **Smart Switch Control**: Control connected devices through HomeKit
- **Temperature Monitoring**: Real-time temperature readings via DHT22/DHT11 sensor
- **Humidity Monitoring**: Real-time humidity readings via DHT22/DHT11 sensor
- **Hardware Control**: LED indicator and relay output for connected devices
- **HomeKit Integration**: Full Apple HomeKit compatibility
- **Over-the-Air Updates**: Firmware upgrade capability via HomeKit
- **Factory Reset**: Hardware button for network and factory reset

## Hardware Requirements

- ESP32 development board (ESP32, ESP32-S2, ESP32-C3, ESP32-S3, ESP32-C2, ESP32-C6)
- DHT22 (AM2301) or DHT11 temperature/humidity sensor
- LED (optional, for status indication)
- Relay module (for controlling external devices)
- Push button (for reset functionality)

## Pin Configuration

Default GPIO pin assignments (configurable in `main/app_main.c`):

- **GPIO 2**: Status LED
- **GPIO 26**: Relay control output
- **GPIO 23**: DHT sensor data pin
- **GPIO 0**: Reset button (Boot button)

## Quick Start

### Prerequisites

1. Install [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/) (v4.4 or later)
2. Set up ESP-IDF environment variables
3. Install required dependencies

### Building and Flashing

1. **Clone the repository:**
   ```bash
   git clone https://github.com/davidgarc/esp32-homekit-sensor-switch.git
   cd esp32-homekit-sensor-switch
   ```

2. **Set your ESP32 target:**
   ```bash
   idf.py set-target esp32  # or esp32s2, esp32c3, esp32s3, esp32c2, esp32c6
   ```

3. **Configure the project:**
   ```bash
   idf.py menuconfig
   ```
   
   Key configuration options:
   - **App Wi-Fi → Source of Wi-Fi Credentials → Use Hardcoded**
   - **Example Configuration → HomeKit Setup Code**
   - **Example Configuration → Use hard-coded setup code**

4. **Build and flash:**
   ```bash
   export ESPPORT=/dev/tty.SLAB_USBtoUART  # Set your serial port
   idf.py build flash monitor
   ```

### HomeKit Setup

1. **Default Setup Code**: `123-45-678` (configurable in menuconfig)
2. **Setup ID**: `SW01`
3. **Accessory Name**: `Esp-Switch-Sensor`

### Adding to HomeKit

1. Open the **Home** app on your iOS device
2. Tap **Add Accessory**
3. Scan the QR code displayed in the serial monitor, or manually enter setup code `123-45-678`
4. Follow the on-screen instructions to complete pairing

## Project Structure

```
esp32-homekit-sensor-switch/
├── main/                          # Main application code
│   ├── app_main.c                # Main application logic
│   ├── CMakeLists.txt            # Main component build config
│   ├── Kconfig.projbuild         # Project configuration options
│   └── idf_component.yml         # Component dependencies
├── components/                    # Project components
│   ├── homekit/                  # ESP HomeKit SDK components
│   └── common/                   # Common utilities (WiFi, setup)
├── CMakeLists.txt                # Root build configuration
├── partitions_hap.csv            # Flash partition table
├── sdkconfig.defaults*           # Default SDK configurations
├── idf_component.yml             # Root component dependencies
└── README.md                     # This file
```

## Functionality

### Switch Control
- Control connected devices via relay output
- LED status indication
- HomeKit on/off commands control both LED and relay

### Temperature & Humidity Sensing
- Automatic sensor readings every 30 seconds
- Real-time updates to HomeKit
- Configurable sensor type (DHT11/DHT22)

### Reset Functions
- **Network Reset**: Press reset button for 3+ seconds (clears WiFi credentials)
- **Factory Reset**: Press reset button for 10+ seconds (clears all pairings and data)

## Configuration

### GPIO Pin Changes
Modify pin assignments in `main/app_main.c`:

```c
#define LED_GPIO           GPIO_NUM_2   // Status LED
#define RELAY_GPIO         GPIO_NUM_26  // Relay control
#define DHT_GPIO           GPIO_NUM_23  // DHT sensor
#define RESET_GPIO         GPIO_NUM_0   // Reset button
```

### Sensor Type
Change sensor type in `main/app_main.c`:

```c
#define DHT_TYPE           DHT_TYPE_AM2301  // DHT22/AM2301
// or
#define DHT_TYPE           DHT_TYPE_DHT11   // DHT11
```

### Update Interval
Modify sensor reading frequency in `main/app_main.c`:

```c
#define SENSOR_UPDATE_INTERVAL  30  // seconds
```

## Development Commands

### Build & Flash
```bash
# Clean build
idf.py clean

# Build only
idf.py build

# Flash and monitor
idf.py flash monitor

# Monitor only
idf.py monitor
```

### Factory NVS Generation
For production devices with unique setup codes:

```bash
cd tools/factory_nvs_gen/
./factory_nvs_gen.py 11122333 ES32 factory
esptool.py -p $ESPPORT write_flash 0x340000 factory.bin
```

## Troubleshooting

### Common Issues

1. **Build Errors**: Ensure ESP-IDF is properly installed and environment is set up
2. **Flash Errors**: Check serial port and cable connections
3. **WiFi Connection**: Verify credentials in menuconfig
4. **HomeKit Pairing**: Ensure setup code is correct and accessory is discoverable
5. **Sensor Readings**: Check DHT sensor wiring and pull-up resistor

### Reset Procedures

If the device becomes unresponsive:
1. Try network reset (3+ second button press)
2. Try factory reset (10+ second button press)
3. Re-flash firmware if necessary

## License

This project is based on the ESP HomeKit SDK and follows its licensing terms.

## Contributing

1. Fork the repository
2. Create your feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add some amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## Support

For issues and questions:
- Create an issue in this repository
- Check ESP HomeKit SDK documentation
- Review ESP-IDF documentation

## Acknowledgments

- Based on [ESP HomeKit SDK](https://github.com/espressif/esp-homekit-sdk)
- Uses [ESP32-DHT](https://github.com/achimpieters/esp32-dht) component for sensor readings