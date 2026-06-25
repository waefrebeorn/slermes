#!/bin/bash
# build-appimage.sh — Build Slermes AppImage
# Produces: Slermes-x86_64.AppImage
# Usage: ./packaging/appimage/build-appimage.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
ARCH="x86_64"
APP_NAME="Slermes"
OUTPUT="${PROJECT_DIR}/${APP_NAME}-${ARCH}.AppImage"

echo "=== Building Slermes AppImage ==="

# Step 1: Build static binary
echo "[1/4] Building static binary..."
cd "$PROJECT_DIR"
make clean
make static

# Step 2: Create AppDir structure
echo "[2/4] Creating AppDir..."
APPDIR="${PROJECT_DIR}/AppDir"
rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin"
mkdir -p "$APPDIR/usr/share/doc/slermes"
mkdir -p "$APPDIR/usr/share/applications"
mkdir -p "$APPDIR/usr/share/icons/hicolor/256x256/apps"

cp slermes "$APPDIR/usr/bin/"
cp README.md "$APPDIR/usr/share/doc/slermes/"
cp LICENSE "$APPDIR/usr/share/doc/slermes/" 2>/dev/null || true

# Step 3: Create desktop entry
echo "[3/4] Creating desktop entry..."
cat > "$APPDIR/usr/share/applications/slermes.desktop" << 'DESKTOP'
[Desktop Entry]
Name=Slermes
Comment=C Language Hermes Agent
Exec=slermes
Icon=slermes
Type=Application
Categories=Development;Utility;
Terminal=true
DESKTOP

# Copy icon if available
if [ -f "${PROJECT_DIR}/assets/icon.png" ]; then
    cp "${PROJECT_DIR}/assets/icon.png" "$APPDIR/usr/share/icons/hicolor/256x256/apps/slermes.png"
else
    # Create a minimal placeholder icon
    convert -size 256x256 xc:'#2563eb' "$APPDIR/usr/share/icons/hicolor/256x256/apps/slermes.png" 2>/dev/null || \
    touch "$APPDIR/usr/share/icons/hicolor/256x256/apps/slermes.png"
fi

# Step 4: Create AppRun
echo "[4/4] Creating AppRun..."
cat > "$APPDIR/AppRun" << 'EOF'
#!/bin/bash
SELF_DIR="$(dirname "$(readlink -f "$0")")"
export PATH="${SELF_DIR}/usr/bin:${PATH}"
exec "${SELF_DIR}/usr/bin/slermes" "$@"
EOF
chmod +x "$APPDIR/AppRun"

# Step 5: Download appimagetool if needed
if [ ! -f "${PROJECT_DIR}/appimagetool" ]; then
    echo "[5/5] Downloading appimagetool..."
    wget -q "https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-${ARCH}.AppImage" \
        -O "${PROJECT_DIR}/appimagetool"
    chmod +x "${PROJECT_DIR}/appimagetool"
fi

# Step 6: Build AppImage
echo "Building AppImage..."
"${PROJECT_DIR}/appimagetool" "$APPDIR" "$OUTPUT"

echo "=== AppImage built: $OUTPUT ==="
echo "Run with: ./$OUTPUT"
