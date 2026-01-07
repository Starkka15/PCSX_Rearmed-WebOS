#!/bin/bash
# Build PCSX-ReARMed for webOS TouchPad

set -e

# Linaro toolchain setup (use symlink to avoid spaces in path)
LINARO_PATH="/opt/linaro-gcc"
export PATH="$LINARO_PATH/bin:$PATH"

# webOS SDK paths
# Create symlink to avoid issues with spaces in paths
ln -sfn "/mnt/c/RE3 to WebOS Project Files/HP webOS/PDK" /tmp/webos-pdk
WEBOS_PDK="/tmp/webos-pdk"

# Temporarily move problematic PDK libraries that conflict with system libs
mv -f "$WEBOS_PDK/device/lib/libdl.so" "$WEBOS_PDK/device/lib/libdl.so.backup" 2>/dev/null || true

# Cross-compilation setup with Linaro GCC 4.8
export CROSS_COMPILE=arm-linux-gnueabi-
export CC="$LINARO_PATH/bin/arm-linux-gnueabi-gcc"
export CXX="$LINARO_PATH/bin/arm-linux-gnueabi-g++"
export AR="$LINARO_PATH/bin/arm-linux-gnueabi-ar"
export AS="$LINARO_PATH/bin/arm-linux-gnueabi-as"
# Copy sdl-config to a path without spaces
cp sdl-config /tmp/sdl-config
chmod +x /tmp/sdl-config
export SDL_CONFIG="/tmp/sdl-config"

# Compiler flags for Cortex-A9 + NEON
export CFLAGS="-march=armv7-a -mfpu=neon -mfloat-abi=softfp -O3 -DWEBOS_TOUCHPAD -I$WEBOS_PDK/include -I$WEBOS_PDK/include/SDL"
export LDFLAGS="-L$WEBOS_PDK/device/lib -Wl,--exclude-libs,libdl.so"
export LDLIBS="-lSDL -lGLESv2 -lpthread -lm -lpdl"

echo "=== Configuring PCSX-ReARMed for webOS TouchPad ==="
bash configure \
    --platform=generic \
    --gpu=neon \
    --sound-drivers=sdl \
    --enable-neon \
    --dynarec=ari64

echo ""
echo "=== Building PCSX-ReARMed ==="
make clean
make -j$(nproc)

echo ""
echo "=== Build Complete! ==="
ls -lh pcsx
file pcsx
