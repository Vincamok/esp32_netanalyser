#!/usr/bin/env bash
# Compile les deux profils PlatformIO et copie les binaires dans web-flasher/bins/
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BINS_DIR="$SCRIPT_DIR/bins"

BOOT_APP0="$HOME/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin"

build_profile() {
  local env="$1"
  local dest="$BINS_DIR/$env"

  echo "==> Building $env ..."
  (cd "$PROJECT_DIR" && pio run -e "$env")

  mkdir -p "$dest"
  cp "$PROJECT_DIR/.pio/build/$env/bootloader.bin" "$dest/"
  cp "$PROJECT_DIR/.pio/build/$env/partitions.bin" "$dest/"
  cp "$PROJECT_DIR/.pio/build/$env/firmware.bin"   "$dest/"

  if [ -f "$BOOT_APP0" ]; then
    cp "$BOOT_APP0" "$dest/"
  else
    echo "WARN: boot_app0.bin introuvable à $BOOT_APP0 — à copier manuellement."
  fi

  echo "    => Binaires copiés dans $dest"
}

build_profile "lilygo-t-display-s3"
build_profile "lilygo-t-display-s3-dp83848"

echo ""
echo "Done. Servez le dossier web-flasher/ avec un serveur HTTP puis ouvrez index.html."
echo "  Exemple : cd web-flasher && python3 -m http.server 8080"
