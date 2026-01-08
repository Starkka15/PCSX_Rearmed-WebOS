# PCSX-ReARMed for webOS TouchPad - Installation Guide

## Package Information
- **File**: `com.starkka.pcsxrearmed_1.0.0_armv7.ipk`
- **Size**: ~2.0 MB
- **Platform**: HP webOS TouchPad (ARMv7 Cortex-A9 with NEON)

## Features
- **NEON GPU** - Optimized software renderer using ARM NEON SIMD
- **ari64 Dynarec** - Fast ARM dynamic recompiler
- **OpenGL ES 2.0** - Hardware-accelerated display via PDL/SDL
- **Touch Controls** - On-screen button overlays for touchscreen input

## Installation Methods

### Method 1: Using WebOS Quick Install
1. Download and install WebOS Quick Install on your PC
2. Connect your TouchPad via USB
3. Enable Developer Mode on your TouchPad
4. Drag and drop the `.ipk` file into WebOS Quick Install
5. Click "Install" and wait for completion

### Method 2: Using Command Line (novacom)
```bash
# Copy IPK to device
novacom put file:///media/internal/pcsx.ipk < com.starkka.pcsxrearmed_1.0.0_armv7.ipk

# Install via SSH
novacom run file:///usr/bin/ipkg install /media/internal/pcsx.ipk
```

### Method 3: Using Preware
1. Install Preware on your TouchPad
2. Copy the IPK to `/media/internal/` on your device
3. Open Preware -> Manage Feeds -> Install Package
4. Browse to the IPK file and install

## Dependencies
The following libraries are required (provided by webOS):
- libSDL-1.2.so.0 (webOS PDK)
- libGLESv2.so (PowerVR SGX540 driver)
- libpng12.so.0
- libz.so.1

## Configuration
ROMs and BIOS files should be placed in:
- `/media/internal/.pcsx/` (recommended)
- BIOS: `/media/internal/.pcsx/bios/`
- ISOs: `/media/internal/.pcsx/iso/`

## Touch Controls Layout
```
+--------+                    +--------+
|   L1   |                    |   R1   |
+--------+                    +--------+
|   L2   |                    |   R2   |
+--------+                    +--------+

+---+                              +---+
| U |                              | ^ |  (Triangle)
+---+---+---+            +---+---+---+---+
| L |   | R |            | [] |   | O |  (Square/Circle)
+---+---+---+            +---+---+---+---+
| D |                              | X |  (Cross)
+---+                              +---+

        +-------+  +--------+
        | START |  | SELECT |
        +-------+  +--------+
```

## Performance Notes

### Current Status
The emulator runs at approximately **30 FPS effective** due to display
system limitations on the HP TouchPad.

### Technical Analysis (as of 2026-01-08)

**Timing breakdown per 60 emulated frames:**
| Component | Time | Notes |
|-----------|------|-------|
| Texture Upload | 87-565ms | glTexSubImage2D, ~2-9ms/frame |
| Draw Calls | 2-30ms | Shader rendering, <1ms/frame |
| **Buffer Swap** | **3300-3600ms** | **~110ms/swap - BOTTLENECK** |

**Root Cause:**
The HP TouchPad's webOS display system enforces:
1. **Mandatory triple-buffering** at the compositor level
2. **Forced VSync** that cannot be disabled via SDL/EGL
3. **PDL abstraction overhead** in the swap path

The 110ms swap time equals ~6-7 missed VSync intervals at 60Hz.
This is a platform limitation, not an emulation issue.

**Current Workaround:**
Frame skipping is enabled (SWAP_EVERY_N_FRAMES=2) to achieve
~30fps effective display rate while maintaining full emulation speed.

### What's NOT the bottleneck:
- NEON GPU plugin (fastest available software renderer)
- ari64 ARM dynarec (optimal for Cortex-A9)
- Texture uploads (acceptable performance)
- Draw calls (negligible overhead)

### Potential Future Optimizations:
1. Async texture ping-pong (double-buffer textures)
2. PSX-level frameskip sync with display rate
3. Direct EGL/PDL swap interval control (if available)
4. Compositor bypass (requires root/system modifications)

## Troubleshooting

### If the app doesn't launch:
1. **Check logs via SSH**:
   ```bash
   novacom run file:///bin/sh
   tail -f /var/log/messages
   ```

2. **Verify installation**:
   ```bash
   ls -la /usr/palm/applications/com.starkka.pcsxrearmed/
   ```

3. **Test binary directly via SSH**:
   ```bash
   cd /usr/palm/applications/com.starkka.pcsxrearmed/
   ./pcsx --help
   ```

4. **Check library dependencies**:
   ```bash
   ldd /usr/palm/applications/com.starkka.pcsxrearmed/pcsx
   ```

### Performance Issues
- Ensure you're using the **NEON GPU** plugin (default)
- Enable **frameskip** if not already enabled
- Some games may run slower due to CPU-intensive effects

## Building from Source

### Requirements
- webOS PDK (Palm Development Kit)
- ARM cross-compiler (arm-linux-gnueabi-gcc)
- NEON support enabled

### Build Command
```bash
./configure --platform=generic --gpu=neon --sound-drivers=sdl \
            --enable-neon --dynarec=ari64
make
```

See `config.mak` for the full configuration.

## Credits
- PCSX-ReARMed by notaz and contributors
- webOS TouchPad port
- NEON GPU by Exophase

## Version History
- 2026-01-08: Added OpenGL ES 2.0 rendering, performance analysis
- 2026-01-05: Initial webOS TouchPad port with touch controls
