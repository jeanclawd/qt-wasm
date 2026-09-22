#!/usr/bin/env bash
set -euxo pipefail

# ---------------------------------------------------------------------------
# Native Qt6 host tools (moc/rcc/uic/qmake6) live under BUILD_PREFIX.
# ---------------------------------------------------------------------------
export QT_HOST_PATH="${BUILD_PREFIX}"

# ---------------------------------------------------------------------------
# Qt6's QtPublicWasmToolchainHelpers.cmake reads $EMSDK/.emscripten to discover
# the LLVM / binaryen paths. emscripten-forge ships emscripten as a conda
# package and does *not* create that file, so synthesise one. This mirrors the
# workaround in emscripten-forge's own qt6 and qt-calculator recipes.
# ---------------------------------------------------------------------------
export EMSDK="${EMSCRIPTEN_FORGE_EMSDK_DIR}"
export EMSDK_NODE="$(command -v node)"

if [ ! -f "${EMSDK}/.emscripten" ]; then
    cat > "${EMSDK}/.emscripten" <<EOF
LLVM_ROOT = '${EMSDK}/upstream/bin'
BINARYEN_ROOT = '${EMSDK}/upstream'
EMSCRIPTEN_ROOT = 'upstream/emscripten'
NODE_JS = '${EMSDK_NODE}'
COMPILER_ENGINE = NODE_JS
JS_ENGINES = [NODE_JS]
EOF
fi

mkdir -p "${SRC_DIR}/build"
cd "${SRC_DIR}/build"

cmake -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_FIND_ROOT_PATH="${PREFIX}" \
    -DCMAKE_PREFIX_PATH="${PREFIX}" \
    -DQT_HOST_PATH="${BUILD_PREFIX}" \
    "${SRC_DIR}"

ninja -j "${CPU_COUNT:-2}"

# ---------------------------------------------------------------------------
# Install the four wasm artifacts into a stable share/<name>/ path, which is
# the layout the emscripten-forge qtapp runner expects:
#   share/<appname>/<appname>.{wasm,js,html} + qtloader.js
# ---------------------------------------------------------------------------
INSTALL_DIR="${PREFIX}/share/qt-wasm-demo"
mkdir -p "${INSTALL_DIR}"
cp qt-wasm-demo.wasm qt-wasm-demo.js qt-wasm-demo.html qtloader.js "${INSTALL_DIR}/"

ls -la "${INSTALL_DIR}"
