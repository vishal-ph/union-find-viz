#!/bin/bash
# Build the WebAssembly module from C++ sources
# Requires: emsdk installed and activated
# Usage: cd web && ./build.sh

set -e

EMSDK_DIR="${EMSDK_DIR:-$HOME/Documents/dev/emsdk}"

if [ -f "$EMSDK_DIR/emsdk_env.sh" ]; then
    source "$EMSDK_DIR/emsdk_env.sh" 2>/dev/null
fi

# Check emcc is available
if ! command -v emcc &> /dev/null; then
    echo "Error: emcc not found. Install Emscripten SDK first."
    echo "  git clone https://github.com/emscripten-core/emsdk.git"
    echo "  cd emsdk && ./emsdk install latest && ./emsdk activate latest"
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"

echo "Compiling C++ to WebAssembly..."
emcc \
    "$ROOT_DIR/cpp/src/wasm_bindings.cpp" \
    "$ROOT_DIR/cpp/src/dem_parser.cpp" \
    "$ROOT_DIR/cpp/src/union_find.cpp" \
    "$ROOT_DIR/cpp/src/decoder_stepper.cpp" \
    -I "$ROOT_DIR/cpp/src" \
    -o "$SCRIPT_DIR/decoder.js" \
    -s WASM=1 \
    -s MODULARIZE=1 \
    -s EXPORT_NAME="DecoderModule" \
    -s ALLOW_MEMORY_GROWTH=1 \
    -s ENVIRONMENT=web,node \
    --bind \
    -O2 \
    -std=c++17

echo "Build complete: decoder.js + decoder.wasm"
echo ""
echo "To serve: cd web && python3 -m http.server 8000"
echo "Open: http://localhost:8000"
