#!/bin/bash
# Check library dependencies for pcsx binary

echo "=== Checking pcsx binary dependencies ==="
echo ""

if [ -f "pcsx" ]; then
    echo "Binary found: pcsx"
    echo ""

    echo "=== File info ==="
    file pcsx
    echo ""

    echo "=== Required libraries ==="
    arm-linux-gnueabi-readelf -d pcsx | grep NEEDED || echo "readelf not available, trying ldd..."
    echo ""

    # Check specific webOS libs
    echo "=== Checking for webOS PDK libraries ==="
    echo "PDK lib path: /tmp/webos-pdk/device/lib"
    ls -la /tmp/webos-pdk/device/lib/*.so 2>/dev/null | head -10
    echo ""

    echo "=== Binary size ==="
    ls -lh pcsx
else
    echo "ERROR: pcsx binary not found!"
    exit 1
fi
