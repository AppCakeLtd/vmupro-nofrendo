# VMUPro Nofrendo NES Emulator

A Nintendo Entertainment System (NES) emulator for the VMUPro handheld device, based on the Nofrendo emulator core.

## Overview

This project ports the popular Nofrendo NES emulator to run on the VMUPro platform, allowing you to play classic NES games on your VMUPro device. The emulator includes support for multiple mappers and provides smooth gameplay with accurate emulation.

## Features

- Full NES emulation using the Nofrendo core
- Support for multiple mapper types (MMC1, MMC3, UNROM, CNROM, etc.)
- Audio Processing Unit (APU) emulation for authentic sound
- Picture Processing Unit (PPU) emulation for accurate graphics
- Frame rate monitoring and performance statistics
- Built-in ROM browser and launcher
- Save state functionality
- Controller input support via VMUPro buttons

## Supported Mappers

The emulator supports a wide range of NES mappers including:
- Mapper 000 (NROM)
- Mapper 001 (MMC1)
- Mapper 002 (UNROM)
- Mapper 003 (CNROM)
- Mapper 004 (MMC3)
- And many more (see `src/main/nofrendo/mappers/` for complete list)

## Requirements

- VMUPro device
- VMUPro SDK (included as submodule)
- ESP-IDF development framework
- NES ROM files (.nes format)

## Building

### Prerequisites

1. Install ESP-IDF and set up your development environment
2. Clone this repository with submodules:
   ```bash
   git clone --recursive https://github.com/8BitMods/vmupro-nofrendo.git
   cd vmupro-nofrendo/src
   ```

### Build Process

1. Build the project:
   ```bash
   idf.py build
   ```

2. Package the application:
   ```bash
   ./pack.sh
   ```
   Or on Windows:
   ```powershell
   ./pack.ps1
   ```

3. Deploy to VMUPro:
   ```bash
   ./send.sh
   ```
   Or on Windows:
   ```powershell
   ./send.ps1
   ```

## Usage

1. Place your NES ROM files on the VMUPro device
2. Launch the Nofrendo emulator from the VMUPro menu
3. Select a ROM file from the browser
4. Use VMUPro controls to play:
   - D-pad: NES D-pad
   - A/B buttons: NES A/B buttons
   - Start/Select: NES Start/Select

## Project Structure

```
src/
├── main/
│   ├── main.cpp              # Main emulator entry point
│   └── nofrendo/             # Nofrendo emulator core
│       ├── nes/              # NES system components
│       ├── mappers/          # Cartridge mapper implementations
│       └── docs/             # Emulator documentation
├── metadata.json             # VMUPro app metadata
├── icon.bmp                  # Application icon
├── pack.sh/pack.ps1         # Packaging scripts
└── send.sh/send.ps1         # Deployment scripts
```

## Technical Details

- **Platform**: ESP32-based VMUPro
- **Framework**: ESP-IDF with VMUPro SDK
- **Emulator Core**: Nofrendo
- **Display**: VMUPro LCD with custom rendering
- **Audio**: VMUPro audio system
- **Input**: VMUPro button mapping

## Performance

The emulator includes frame rate monitoring and performance statistics:
- Real-time FPS counter
- Frame time averaging
- Performance optimization for VMUPro hardware

## Credits

- **Nofrendo Emulator**: Original NES emulator core
- **8BitMods**: VMUPro porting and optimization
- **VMUPro SDK**: Platform-specific libraries and tools

## License

This project is licensed under the GNU General Public License v2.0. See the [LICENSE](LICENSE) file for details.

The Nofrendo emulator core maintains its original licensing terms. See `src/main/nofrendo/COPYING` for details.

## Contributing

Contributions are welcome! Please feel free to submit issues and pull requests.

## Support

For support and questions:
- Check the VMUPro documentation
- Report issues on the project repository
- Join the 8BitMods community

---

Enjoy playing classic NES games on your VMUPro device!