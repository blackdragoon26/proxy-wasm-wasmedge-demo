# WasmEdge Proxy-Wasm Demo

This repository provides a self-contained demonstration of [WasmEdge](https://github.com/WasmEdge/WasmEdge) acting as the runtime backend for the [proxy-wasm-cpp-host](https://github.com/proxy-wasm/proxy-wasm-cpp-host) library.

The integration showcased here was introduced in `proxy-wasm-cpp-host` PR [#193](https://github.com/proxy-wasm/proxy-wasm-cpp-host/pull/193).

## Architecture

The demonstration is composed of two primary layers:

1. **Host Process:** A standalone native C++ application utilizing `proxy-wasm-cpp-host`. The host initializes a WasmEdge virtual machine and fakes an HTTP transaction.
2. **WebAssembly Plugin:** A lightweight Wasm plugin built against the `proxy-wasm-cpp-sdk` targeting the `wasm32-wasi` architecture. When the host invokes the proxy-wasm lifecycle methods, this plugin securely mutates the response headers from within the sandbox.

## Component Overview

- **Plugin (`plugin/plugin.cc`):** Implements a WebAssembly filter that hooks into the HTTP response phase and injects a custom `x-powered-by: WasmEdge` header.
- **Host (`host/main.cc`):** Compiles natively and relies on WasmEdge to load the `.wasm` binary. It pushes a mock HTTP request through the plugin and prints the headers before and after modification to verify functionality.

## Prerequisites
For both [Quick Start](#quick-start) and [Manual Build](#manual-build), ensure your system has the required build tools such as CMake, Ninja, and Clang/GCC installed and the Abseil C++ library.

**macOS (Homebrew):**
```bash
brew install cmake ninja abseil
```

**Linux (Ubuntu/Debian):**
```bash
sudo apt update
sudo apt install cmake ninja-build libabsl-dev
```

## Quick Start

For systems running Linux or macOS, a comprehensive build pipeline is provided. The script automatically handles downloading required SDKs, cross-compiling the WebAssembly module, compiling the host natively, and executing the integrated binary.

### Build and Run
Once the dependencies are installed, clone the repository and execute the pipeline:

```bash
git clone https://github.com/blackdragoon26/wasmedge_proxy_wasm_demo.git
cd wasmedge_proxy_wasm_demo
chmod +x scripts/build_and_run.sh
./scripts/build_and_run.sh
```

## Manual Build

If you prefer to configure the build environment independently, please adhere to the following sequence:

### 1. Install WasmEdge

Target the specified minimum supported version:

```bash
curl -sSfL https://raw.githubusercontent.com/WasmEdge/WasmEdge/master/utils/install.sh | bash -s -- --version 0.14.1
source ~/.wasmedge/env
```

### 2. Fetch Proxy-Wasm Dependencies

Clone the upstream SDK and host libraries without commit histories to minimize disk footprint:

```bash
git clone --depth=1 https://github.com/proxy-wasm/proxy-wasm-cpp-sdk .deps/proxy-wasm-cpp-sdk
git clone --depth=1 https://github.com/proxy-wasm/proxy-wasm-cpp-host .deps/proxy-wasm-cpp-host
```

### 3. Compile the WebAssembly Plugin

You will require the `wasi-sdk` installed locally to target `wasm32-wasi`.

```bash
cmake -S plugin -B build/plugin \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/wasi-sdk/share/cmake/wasi-sdk.cmake \
  -DPROXY_WASM_CPP_SDK_PATH=$(pwd)/.deps/proxy-wasm-cpp-sdk \
  -DCMAKE_BUILD_TYPE=Release -G Ninja
cmake --build build/plugin
```

### 4. Compile the Native Host

The native host links against WasmEdge. Make sure `absl` and `openssl` are available in your system path.

```bash
cmake -S host -B build/host \
  -DPROXY_WASM_CPP_HOST_PATH=$(pwd)/.deps/proxy-wasm-cpp-host \
  -DWASMEDGE_INSTALL_PATH=$HOME/.wasmedge \
  -DCMAKE_BUILD_TYPE=Release -G Ninja
cmake --build build/host
```

### 5. Execution

Supply the generated WebAssembly binary directly to the host runner:

```bash
LD_LIBRARY_PATH=$HOME/.wasmedge/lib ./build/host/demo ./build/plugin/add_header_filter.wasm
```
## Expected Execution Trace

Upon successful execution, the host will load the WebAssembly filter and simulate network traffic. The output will explicitly indicate that the plugin injected the correct header:

```text
Loaded plugin: .../add_header_filter.wasm
Runtime:       WasmEdge (wasmedge)

── Request headers (before filter) ──
  :method: GET
  :path: /api/hello

── Response headers (before filter) ──
  :status: 200

── Running onResponseHeaders ──
[INFO] [AddHeaderFilter] Injected 'x-powered-by: WasmEdge' into response.

── Response headers (after filter) ──
  :status: 200
  x-powered-by: WasmEdge
```

## References

- [WasmEdge Runtime](https://github.com/WasmEdge/WasmEdge)
- [Proxy-Wasm Host Environment PR #193](https://github.com/proxy-wasm/proxy-wasm-cpp-host/pull/193)
- [Proxy-Wasm ABI Specification](https://github.com/proxy-wasm/spec)
