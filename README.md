# Plug Monitor - USB Tester Monitor

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
./build/usb-tester-monitor

# Or after installation
usb-tester-monitor
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

### Manual Device Path

If your device is not auto-detected, you can specify the device path manually:
- Linux: `/dev/ttyUSB0`, `/dev/ttyACM0`, etc.
- Windows: `COM3`, `COM4`, etc.

## Protocol Information

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
│   ├── data-display.c/h    # Value display widgets
│   ├── chart-view.c/h      # Chart visualization
│   └── data-logger.c/h     # Data logging
└── data/                   # Resources
    ├── org.gnome.PlugMonitor.gresource.xml
    ├── org.gnome.PlugMonitor.desktop.in
    ├── org.gnome.PlugMonitor.appdata.xml
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
usb-tester-monitor /dev/ttyUSB0

# Show help
usb-tester-monitor --help
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
2. Run with debug: `G_MESSAGES_DEBUG=all ./build/usb-tester-monitor`
3. Report issues on GitHub with logs

## Development

### Building for Development

```bash
# Configure with debug symbols
meson setup build --buildtype=debug

# Build and run
ninja -C build
./build/usb-tester-monitor
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
