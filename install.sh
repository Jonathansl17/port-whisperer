#!/bin/sh
set -e

REPO="Jonathansl17/port-whisperer"
INSTALL_DIR="/usr/local/bin"
BINARY_NAME="ports"

# Detect OS
OS="$(uname -s)"
case "$OS" in
    Linux)  OS="linux" ;;
    Darwin) OS="darwin" ;;
    *)
        echo "Error: Unsupported operating system: $OS"
        exit 1
        ;;
esac

# Detect architecture
ARCH="$(uname -m)"
case "$ARCH" in
    x86_64|amd64)  ARCH="x86_64" ;;
    aarch64|arm64) ARCH="aarch64" ;;
    *)
        echo "Error: Unsupported architecture: $ARCH"
        exit 1
        ;;
esac

ASSET_NAME="ports-${OS}-${ARCH}"

echo ""
echo "  🔊 Port Whisperer Installer"
echo "  ─────────────────────────────"
echo ""
echo "  OS:   $OS"
echo "  Arch: $ARCH"
echo ""

# Get latest release tag
echo "  Fetching latest release..."
TAG=$(curl -sfL "https://api.github.com/repos/${REPO}/releases/latest" | grep '"tag_name"' | head -1 | sed 's/.*"tag_name": *"\([^"]*\)".*/\1/')

if [ -z "$TAG" ]; then
    echo "  Error: Could not determine latest release."
    exit 1
fi

echo "  Version: $TAG"

# Download binary
URL="https://github.com/${REPO}/releases/download/${TAG}/${ASSET_NAME}"
echo "  Downloading ${ASSET_NAME}..."

TMPFILE=$(mktemp)
HTTP_CODE=$(curl -sfL -w '%{http_code}' -o "$TMPFILE" "$URL")

if [ "$HTTP_CODE" != "200" ]; then
    rm -f "$TMPFILE"
    echo "  Error: Failed to download binary (HTTP $HTTP_CODE)"
    echo "  URL: $URL"
    exit 1
fi

# Install
chmod +x "$TMPFILE"

if [ -w "$INSTALL_DIR" ]; then
    mv "$TMPFILE" "${INSTALL_DIR}/${BINARY_NAME}"
    ln -sf "${INSTALL_DIR}/${BINARY_NAME}" "${INSTALL_DIR}/whoisonport"
else
    echo "  Installing to ${INSTALL_DIR} (requires sudo)..."
    sudo mv "$TMPFILE" "${INSTALL_DIR}/${BINARY_NAME}"
    sudo ln -sf "${INSTALL_DIR}/${BINARY_NAME}" "${INSTALL_DIR}/whoisonport"
fi

echo ""
echo "  ✓ Installed port-whisperer $TAG to ${INSTALL_DIR}/${BINARY_NAME}"
echo "  ✓ Symlink created: whoisonport -> ports"
echo ""
echo "  Try it: ports"
echo ""
