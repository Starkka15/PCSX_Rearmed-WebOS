# PCSX-ReARMed for webOS (HP TouchPad)

A port of PCSX-ReARMed PlayStation 1 emulator to HP webOS TouchPad.

Based on [notaz/pcsx_rearmed](https://github.com/notaz/pcsx_rearmed).

## Features

- **Full PlayStation 1 Emulation** - Runs most PS1 games
- **On-Screen Touch Controls** - D-pad, face buttons, triggers, Start/Select
- **Multitouch Support** - PDL multitouch for simultaneous button presses
- **YUV Overlay Rendering** - Hardware-accelerated video output
- **ARM NEON Optimization** - Optimized for Cortex-A9 (Snapdragon APQ8060)

## Performance

Performance is approximately **40-50 FPS** on HP TouchPad, depending on the game. Some less demanding games may run at full speed (60 FPS), while more complex games may require frameskip.

## Requirements

- HP TouchPad with webOS 3.0.5
- webOS PDK installed for building
- Linaro GCC 4.8 cross-compiler (arm-linux-gnueabi)

## Building

### Prerequisites

1. Install the webOS PDK
2. Install Linaro GCC 4.8 arm-linux-gnueabi toolchain to `/opt/linaro-gcc/`

### Build Commands

```bash
# Build the emulator
./build-webos.sh

# Create IPK package
./build-ipk.sh
```

The build will create:
- `pcsx` - The emulator binary
- `com.starkka.pcsxrearmed_VERSION_armv7.ipk` - Installable package

## Installation

### Method 1: Manual Installation (Recommended)

```bash
# Copy IPK to device
novacom put file:///media/internal/pcsx.ipk < com.starkka.pcsxrearmed_*.ipk

# SSH to device and extract
ssh root@touchpad
cd /media/cryptofs/apps
ar x /media/internal/pcsx.ipk data.tar.gz
tar xzf data.tar.gz
rm data.tar.gz

# Rescan applications
luna-send -n 1 palm://com.palm.applicationManager/rescan {}
```

### Method 2: Using palm-install

```bash
palm-install com.starkka.pcsxrearmed_*.ipk
```

## Usage

1. Place PS1 BIOS files in `/media/internal/.pcsx/bios/`
   - Required: `scph1001.bin` (or other valid BIOS)
2. Place game ISOs/BINs in `/media/internal/.pcsx/`
3. Launch PCSX ReARMed from the app launcher
4. Select "Run BIOS" or browse for a game

### Touch Controls

The on-screen controls are laid out as follows:

```
[L1] [L2]                    [R2] [R1]

       [UP]              [TRIANGLE]
   [LEFT][RIGHT]      [SQUARE][CIRCLE]
      [DOWN]             [CROSS]

         [START] [SELECT]
```

Touch controls support multitouch - you can press multiple buttons simultaneously.

## Configuration

Configuration files are stored in `/media/internal/.pcsx/`:
- `pcsx.cfg` - Main emulator configuration
- `bios/` - BIOS files directory
- `memcards/` - Memory card saves
- `sstates/` - Save states

## Changes from Original PCSX-ReARMed

This port includes the following modifications:

1. **webOS Touch Input System** (`frontend/in_webos_touch.c/h`)
   - On-screen touch controls with visual overlay
   - PDL multitouch support via `PDL_SetTouchAggression(PDL_AGGRESSION_MORETOUCHES)`
   - Touch zones for all PS1 controller buttons

2. **SDL Platform Layer** (`frontend/plat_sdl.c`, `frontend/libpicofe/plat_sdl.c`)
   - Software surface mode (SDL_SWSURFACE) for webOS compatibility
   - YUV overlay rendering support
   - Fixed 1024x768 resolution for TouchPad display
   - Touch overlay drawing integration

3. **SDL Input Modifications** (`frontend/libpicofe/in_sdl.c`)
   - Touch event forwarding with finger ID tracking
   - Integration with webOS touch input driver

4. **Build System**
   - Linaro GCC 4.8 cross-compilation support
   - webOS PDK integration
   - IPK package generation

## Credits

- **PCSX-ReARMed** - notaz and contributors
- **Original PCSX** - PCSX Team
- **webOS Port** - Starkka

## License

This project is licensed under the GNU General Public License v2.0 - see the original PCSX-ReARMed license for details.

## Links

- [Original PCSX-ReARMed](https://github.com/notaz/pcsx_rearmed)
- [webOS Homebrew Community](https://www.webos-ports.org/)
