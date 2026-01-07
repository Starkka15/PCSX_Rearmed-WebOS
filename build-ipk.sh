#!/bin/bash
# Build script for creating webOS IPK package

set -e

VERSION="1.0.7-multitouch"
PACKAGE_NAME="com.starkka.pcsxrearmed_${VERSION}_armv7.ipk"
APP_DIR="ipkg/usr/palm/applications/com.starkka.pcsxrearmed"

echo "=== Building webOS IPK Package ==="

# Ensure binary is up to date
if [ ! -f "pcsx" ]; then
    echo "Error: pcsx binary not found. Run build-webos.sh first."
    exit 1
fi

# Copy binary to IPK structure
echo "Copying binary to IPK structure..."
cp pcsx "${APP_DIR}/"

# Copy plugins
echo "Copying plugins..."
mkdir -p "${APP_DIR}/plugins"
cp plugins/spunull/spunull.so "${APP_DIR}/plugins/" 2>/dev/null || true
cp plugins/dfxvideo/gpu_peops.so "${APP_DIR}/plugins/" 2>/dev/null || true
cp plugins/gpu_unai/gpu_unai.so "${APP_DIR}/plugins/" 2>/dev/null || true

# Create IPK package
echo "Creating IPK package..."
cd ipkg

# Create debian-binary
echo '2.0' > debian-binary

# Create control and data archives (with root ownership for webOS)
tar --format=ustar --owner=0 --group=0 -czf control.tar.gz -C . CONTROL
tar --format=ustar --owner=0 --group=0 -czf data.tar.gz -C . usr

# Create IPK using ar
rm -f "../${PACKAGE_NAME}"
ar r "../${PACKAGE_NAME}" debian-binary control.tar.gz data.tar.gz

# Cleanup temp files
rm debian-binary control.tar.gz data.tar.gz

cd ..

echo "=== Package created: ${PACKAGE_NAME} ==="
ls -lh "${PACKAGE_NAME}"

echo ""
echo "To install on device:"
echo "  palm-install ${PACKAGE_NAME}"
echo "  -or-"
echo "  novacom put file:///media/internal/${PACKAGE_NAME} < ${PACKAGE_NAME}"
echo "  novacom run file:///usr/bin/ipkg -- install /media/internal/${PACKAGE_NAME}"
