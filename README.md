# Plug

A GNOME desktop application for monitoring USB tester devices like the Qianli iBridge A3, built with GTK4 and Adwaita using the GNOME 50 design language.

![GNOME 50 Design](https://img.shields.io/badge/GNOME-50-blue?logo=gnome&logoColor=white)
![GTK4](https://img.shields.io/badge/GTK-4.0-green?logo=gtk&logoColor=white)
![License: GPL-3.0](https://img.shields.io/badge/License-GPL--3.0-yellow)

## Features

- **Real-time Monitoring**: Live voltage, current, and power readings
- **Multiple Device Support**: Extensible architecture for various USB tester devices
- **Data Visualization**: Scrolling charts with historical data
- **Data Logging**: CSV export with configurable logging intervals
- **Modern UI**: GNOME 50 design with Adwaita widgets
- **Statistics**: Min/Max tracking for voltage and current

## Supported Devices

- **Qianli iBridge A3** - Fully supported with reverse-engineered protocol
- **Fnirsi FNB-48** - Fully supported with reverse-engineered protocol
- **Fnirsi C1**, **Fnirsi FNB-48S**, **Fnirsi FNB-58** - Same protocol family as the FNB-48; supported following the reference implementation (not tested against hardware here)
- **POWER-Z KM002C / KM003C** - Supported following the reverse-engineered protocol documentation and the vendor HID demo (ADC polling: voltage, current, temperature, D+/D-)

## Installation

### Dependencies

#### Fedora / RHEL

```bash
# Install build dependencies
sudo dnf install meson gcc gtk4-devel libadwaita-devel

# Optional: For development
sudo dnf install git clang gdb
```

#### Debian / Ubuntu

```bash
# Install build dependencies
sudo apt install meson build-essential libgtk-4-dev libadwaita-1-dev
```

#### Arch Linux

```bash
# Install build dependencies
sudo pacman -S meson gtk4 libadwaita
```

### Building

```bash
# Clone the repository
git clone https://github.com/manuel-rw/plug-monitor.git
cd plug-monitor

# Configure and build
meson setup build --prefix=/usr/local
ninja -C build

# Install (optional)
sudo ninja -C build install
```

### Running

```bash
# Run from build directory
./build/plug

# Or after installation
plug
```

## Usage

1. **Connect your USB tester device** via USB
2. **Launch Plug Monitor**
3. **Select your device** from the device list
4. **Click Connect** to start monitoring
5. View real-time voltage, current, and power readings
6. Switch to the chart view to see historical data

## Device Connection

The application automatically detects connected USB tester devices. Currently supports:

- **Qianli iBridge A3**: Uses serial communication at 115200 baud
- **Fnirsi FNB-48 / C1 / FNB-48S / FNB-58**: USB HID devices, accessed through their PC micro-USB port. All use the same report format; FNB-48 and C1 are polled with `AA 83`, FNB-48S and FNB-58 with `AA 82`
- **POWER-Z KM002C / KM003C**: USB HID devices. Each `GET_DATA/ATT_ADC` request returns one ADC frame (bus voltage in uV, signed current in uA, temperature in 1/128 C, D+/D- in 0.1 mV)

### Manual Device Path

If your device is not auto-detected, you can specify the device path manually:
- Linux: `/dev/ttyUSB0`, `/dev/ttyACM0`, `/dev/hidraw0`, etc.
- Windows: `COM3`, `COM4`, etc.

### Fnirsi FNB-48 Permissions

The Fnirsi and POWER-Z meters are HID devices; on Linux their `/dev/hidraw*`
nodes are only accessible to root by default. The application offers to
install a udev rule (granting access for all supported Fnirsi and POWER-Z
USB IDs) the first time a connect fails with a permission error. To install
it manually:

```shell
sudo tee /etc/udev/rules.d/90-fnb48.rules <<'EOF'
SUBSYSTEM=="hidraw", ATTRS{idVendor}=="0483", ATTRS{idProduct}=="003a", MODE="0660", GROUP="dialout", TAG+="uaccess"
SUBSYSTEM=="hidraw", ATTRS{idVendor}=="0483", ATTRS{idProduct}=="003b", MODE="0660", GROUP="dialout", TAG+="uaccess"
SUBSYSTEM=="hidraw", ATTRS{idVendor}=="2e3c", ATTRS{idProduct}=="0049", MODE="0660", GROUP="dialout", TAG+="uaccess"
SUBSYSTEM=="hidraw", ATTRS{idVendor}=="2e3c", ATTRS{idProduct}=="5558", MODE="0660", GROUP="dialout", TAG+="uaccess"
SUBSYSTEM=="hidraw", ATTRS{idVendor}=="5fc9", ATTRS{idProduct}=="0061", MODE="0660", GROUP="dialout", TAG+="uaccess"
SUBSYSTEM=="hidraw", ATTRS{idVendor}=="5fc9", ATTRS{idProduct}=="0063", MODE="0660", GROUP="dialout", TAG+="uaccess"
EOF
sudo udevadm control --reload && sudo udevadm trigger
```

## Protocol Information

### Qianli iBridge A3

This application implements the reverse-engineered protocol from:

> [Creased/Qianli-iBridge-A3-Monitor](https://github.com/Creased/Qianli-iBridge-A3-Monitor)

### Qianli iBridge A3 Protocol Details

- **Baud Rate**: 115200
- **Data Format**: 8N1
- **Magic Byte**: `0xDA`
- **Packet Structure**: Fixed header with variable-length payload
- **Data Stream**: Model `0x04`, Command `0x05`

### Packet Format

| Offset | Field | Type | Size | Description |
|--------|-------|------|------|-------------|
| 0 | Magic | uint8 | 1 | Constant `0xDA` |
| 1 | Length | uint16_le | 2 | Payload size (Little Endian) |
| 3 | Model | uint8 | 1 | Device Model (e.g., `0x04`) |
| 4 | Command | uint8 | 1 | Command ID |
| 5 | Param 1 | uint8 | 1 | Parameter 1 |
| 6 | Param 2 | uint8 | 1 | Parameter 2 |
| 7 | Checksum | uint8 | 1 | XOR of payload bytes |
| 8 | Payload | bytes | Len | Variable length data |

### Data Stream Payload

| Offset | Field | Type | Size | Scaling |
|--------|-------|------|------|---------|
| 0 | Current | uint32_be | 4 | Value / 10000.0 = Amperes |
| 4 | Voltage | uint32_be | 4 | Value / 1000.0 = Volts |

### Fnirsi FNB-48

The FNB-48 protocol is based on the reverse engineering work of:

> [baryluk/fnirsi-usb-power-data-logger](https://github.com/baryluk/fnirsi-usb-power-data-logger)

The meter communicates over its HID interface with 64-byte reports.

#### FNB-48 Protocol Details

- **USB IDs**: `0483:003a` (STMicroelectronics FNB-48)
- **Transport**: HID reports, 64 bytes each
- **Handshake**: `AA 81`, `AA 82`, `AA 83` requests, each zero-padded to 64 bytes and terminated with a CRC-8 (poly `0x39`, init `0x42`)
- **Keep-alive**: the host must keep sending `AA 83` poll requests or the device stops streaming
- **Data stream**: `AA 04` reports containing 4 samples of 15 bytes each, followed by a CRC-8

#### FNB-48 Sample Format (15 bytes, little-endian)

| Offset | Field | Type | Size | Scaling |
|--------|-------|------|------|---------|
| 0 | Voltage | uint32_le | 4 | Value / 100000.0 = Volts |
| 4 | Current | uint32_le | 4 | Value / 100000.0 = Amperes |
| 8 | D+ voltage | uint16_le | 2 | Value / 1000.0 = Volts |
| 10 | D- voltage | uint16_le | 2 | Value / 1000.0 = Volts |
| 12 | Unknown | uint8 | 1 | - |
| 13 | Temperature | uint16_le | 2 | Value / 10.0 = degrees Celsius |

## Project Structure

```
plug-monitor/
├── meson.build              # Build system configuration
├── README.md               # This file
├── src/                    # Source code
│   ├── main.c              # Application entry point
│   ├── application.c/h     # GTK Application
│   ├── main-window.c/h     # Main application window
│   ├── device-manager.c/h  # Device detection and management
│   ├── device-protocol.c/h # Protocol abstraction
│   ├── qianli-a3.c/h       # Qianli A3 implementation
│   ├── fnirsi-fnb48.c/h    # Fnirsi FNB-48 family implementation
│   ├── powerz-km003c.c/h   # POWER-Z KM002C/KM003C implementation
│   ├── data-display.c/h    # Value display widgets
│   ├── chart-view.c/h      # Chart visualization
│   └── data-logger.c/h     # Data logging
└── data/                   # Resources
    ├── io.github.manuelrw.Plug.gresource.xml
    ├── io.github.manuelrw.Plug.desktop.in
    ├── io.github.manuelrw.Plug.appdata.xml
    ├── css/                # Stylesheets
    │   └── style.css
    └── ui/                 # UI templates
        ├── main-window.ui
        ├── data-display.ui
        ├── chart-view.ui
        ├── device-dialog.ui
        └── preferences-dialog.ui
```

## Configuration

### Preferences

Open the preferences dialog from the menu to configure:
- **Device Settings**: Auto-connect, default device, baud rate
- **Display Settings**: Chart visibility, time range, theme
- **Logging Settings**: Enable/disable logging, log file path, interval

### Command Line

```bash
# Run with specific device
plug /dev/ttyUSB0

# Show help
plug --help
```

## Data Logging

Data is logged to CSV format with the following columns:
- `timestamp`: ISO 8601 timestamp
- `voltage`: Voltage in volts
- `current`: Current in amperes
- `power`: Power in watts

### Example Log File

```csv
timestamp,voltage,current,power
2024-01-15 10:30:00.123,5.025,1.234,6.199
2024-01-15 10:30:00.234,5.018,1.241,6.226
```

## Troubleshooting

### Device Not Detected

1. Check if the device is connected: `ls /dev/tty*`
2. Verify permissions: `sudo usermod -a -G dialout $USER` (then logout/login)
3. Check if the device is working with other tools like `screen` or `minicom`

### Build Issues

1. Ensure all dependencies are installed
2. Clean build directory: `rm -rf build`
3. Reconfigure: `meson setup build`

### Application Crashes

1. Check for error messages in the terminal
2. Run with debug: `G_MESSAGES_DEBUG=all ./build/plug`
3. Report issues on GitHub with logs

## Development

### Building for Development

```bash
# Configure with debug symbols
meson setup build --buildtype=debug

# Build and run
ninja -C build
./build/plug
```

### Adding New Device Support

1. Create a new source file for your device (e.g., `my-device.c`)
2. Implement the `UsbTesterMonitorDeviceProtocol` interface
3. Add device detection in `device-manager.c`
4. Update `meson.build` with your new source files

### Code Style

- Follow GNOME coding style guidelines
- Use GObject for object-oriented patterns
- Keep UI responsive (avoid blocking operations)

## Contributing

Contributions are welcome! Please:

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Submit a pull request

## License

This project is licensed under the **GNU General Public License v3.0 or later**.

See [LICENSE](LICENSE) for the full license text.

## Credits

### Protocol Reverse Engineering

This application uses the reverse-engineered protocol from:

> [Creased/Qianli-iBridge-A3-Monitor](https://github.com/Creased/Qianli-iBridge-A3-Monitor)

Without this groundbreaking work, this application would not have been possible.

### Libraries

- **GTK4**: The GIMP Toolkit (https://gtk.org/)
- **Libadwaita**: GNOME's modern widget library (https://gnome.pages.gitlab.gnome.org/libadwaita/)
- **Meson**: The build system (https://mesonbuild.com/)

### Icons

This application uses standard GNOME icons.

## Contact

- **GitHub**: https://github.com/manuel-rw/plug-monitor
- **Issues**: https://github.com/manuel-rw/plug-monitor/issues
- **Discussions**: https://github.com/manuel-rw/plug-monitor/discussions
