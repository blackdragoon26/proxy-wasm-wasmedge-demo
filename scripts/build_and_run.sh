#!/usr/bin/env bash
# scripts/build_and_run.sh — proxy-wasm + WasmEdge demo builder
set -euo pipefail

DEMO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEPS_DIR="${DEMO_ROOT}/.deps"
BUILD_DIR="${DEMO_ROOT}/build"
WASMEDGE_VERSION="0.14.1"
# Ensure build tools are in the path (e.g. for Homebrew on macOS)
export PATH="/opt/homebrew/bin:/usr/local/bin:${PATH}"

mkdir -p "${DEPS_DIR}" "${BUILD_DIR}"

section() { echo; echo "══════════════════════════════════════════════"; echo "  $*"; echo "══════════════════════════════════════════════"; }

# Detect platform — wasi-sdk uses "macos"/"linux", not "darwin"
ARCH="$(uname -m)"
case "$(uname -s)" in
  Darwin) HOST_OS="macos" ;;
  Linux)  HOST_OS="linux" ;;
  *) echo "Unsupported OS"; exit 1 ;;
esac
echo "Platform: ${ARCH}-${HOST_OS}"

# ── 1. WasmEdge ───────────────────────────────────────────────────────────────
section "1/5  Installing WasmEdge ${WASMEDGE_VERSION}"
if [[ ! -d "${HOME}/.wasmedge/lib" ]]; then
  curl -sSfL https://raw.githubusercontent.com/WasmEdge/WasmEdge/master/utils/install.sh \
    | bash -s -- --version "${WASMEDGE_VERSION}"
else
  echo "Already installed — skipping."
fi
WASMEDGE_PREFIX="${HOME}/.wasmedge"

# ── 2. wasi-sdk (latest release, auto-detected) ───────────────────────────────
section "2/5  Setting up wasi-sdk"
WASI_SDK_DIR="${DEPS_DIR}/wasi-sdk"
if [[ ! -d "${WASI_SDK_DIR}" ]]; then
  # Fetch the tag of the latest wasi-sdk release (e.g. "wasi-sdk-25")
  LATEST_TAG="$(curl -sSfL \
    https://api.github.com/repos/WebAssembly/wasi-sdk/releases/latest \
    | grep '"tag_name"' | head -1 | sed 's/.*"tag_name": *"\([^"]*\)".*/\1/')"
  # Tag is like "wasi-sdk-25"; version number is everything after the last dash
  VERSION="${LATEST_TAG##*-}"
  ARCHIVE="wasi-sdk-${VERSION}.0-${ARCH}-${HOST_OS}.tar.gz"
  URL="https://github.com/WebAssembly/wasi-sdk/releases/download/${LATEST_TAG}/${ARCHIVE}"
  echo "Latest tag : ${LATEST_TAG}"
  echo "Downloading: ${URL}"
  curl -sSfL "${URL}" -o "/tmp/${ARCHIVE}"
  mkdir -p "${WASI_SDK_DIR}"
  tar -xzf "/tmp/${ARCHIVE}" -C "${WASI_SDK_DIR}" --strip-components=1
  rm "/tmp/${ARCHIVE}"
else
  echo "Already present — skipping."
fi

WASI_TOOLCHAIN="${WASI_SDK_DIR}/share/cmake/wasi-sdk-p1.cmake"
[[ -f "${WASI_TOOLCHAIN}" ]] || { echo "ERROR: toolchain not found at ${WASI_TOOLCHAIN}"; exit 1; }

# ── 3. Clone proxy-wasm repos ────────────────────────────────────────────────
section "3/5  Cloning proxy-wasm repos"
clone_or_skip() {
  [[ -d "$2/.git" ]] && { echo "$(basename $2) already cloned — skipping."; return; }
  git clone --depth=1 --branch="$3" "$1" "$2"
}
clone_or_skip https://github.com/proxy-wasm/proxy-wasm-cpp-sdk  "${DEPS_DIR}/proxy-wasm-cpp-sdk"  main
clone_or_skip https://github.com/proxy-wasm/proxy-wasm-cpp-host "${DEPS_DIR}/proxy-wasm-cpp-host" main

# ── 4. Build Wasm plugin ──────────────────────────────────────────────────────
section "4/5  Building Wasm plugin (wasm32-wasi)"
PLUGIN_BUILD="${BUILD_DIR}/plugin"
rm -rf "${PLUGIN_BUILD}" # Ensure clean rebuild
cmake -S "${DEMO_ROOT}/plugin" -B "${PLUGIN_BUILD}" \
  -DCMAKE_TOOLCHAIN_FILE="${WASI_TOOLCHAIN}" \
  -DPROXY_WASM_CPP_SDK_PATH="${DEPS_DIR}/proxy-wasm-cpp-sdk" \
  -DCMAKE_BUILD_TYPE=Release -G Ninja
cmake --build "${PLUGIN_BUILD}" --parallel
WASM_FILE="${PLUGIN_BUILD}/add_header_filter.wasm"
echo "Built: ${WASM_FILE} ($(wc -c < "${WASM_FILE}") bytes)"

# ── 5. Build host runner ──────────────────────────────────────────────────────
section "5/5  Building host runner (WasmEdge backend)"
HOST_BUILD="${BUILD_DIR}/host"
rm -rf "${HOST_BUILD}" # Ensure clean rebuild

# Help CMake find OpenSSL on macOS
EXTRA_CMAKE_ARGS=""
if [[ "${HOST_OS}" == "macos" ]]; then
  if [[ -d "/opt/homebrew/opt/openssl@3" ]]; then
    EXTRA_CMAKE_ARGS="-DOPENSSL_ROOT_DIR=/opt/homebrew/opt/openssl@3"
  elif [[ -d "/usr/local/opt/openssl@3" ]]; then
    EXTRA_CMAKE_ARGS="-DOPENSSL_ROOT_DIR=/usr/local/opt/openssl@3"
  fi
fi

cmake -S "${DEMO_ROOT}/host" -B "${HOST_BUILD}" \
  -DPROXY_WASM_CPP_HOST_PATH="${DEPS_DIR}/proxy-wasm-cpp-host" \
  -DPROXY_WASM_CPP_SDK_PATH="${DEPS_DIR}/proxy-wasm-cpp-sdk" \
  -DWASMEDGE_INSTALL_PATH="${WASMEDGE_PREFIX}" \
  -DCMAKE_BUILD_TYPE=Release -G Ninja ${EXTRA_CMAKE_ARGS}
cmake --build "${HOST_BUILD}" --parallel

# ── Run ───────────────────────────────────────────────────────────────────────
section "Running demo"
[[ "${HOST_OS}" == "macos" ]] \
  && export DYLD_LIBRARY_PATH="${WASMEDGE_PREFIX}/lib:${DYLD_LIBRARY_PATH:-}" \
  || export LD_LIBRARY_PATH="${WASMEDGE_PREFIX}/lib:${LD_LIBRARY_PATH:-}"

"${HOST_BUILD}/demo" "${WASM_FILE}"
