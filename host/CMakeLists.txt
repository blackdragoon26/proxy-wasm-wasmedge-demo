cmake_minimum_required(VERSION 3.14)
project(proxy_wasm_wasmedge_demo CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Required paths (pass via -D on the cmake command line) ────────────────────
#   PROXY_WASM_CPP_HOST_PATH  – root of the proxy-wasm-cpp-host clone
#   WASMEDGE_INSTALL_PATH     – WasmEdge install prefix (default: ~/.wasmedge)

if(NOT DEFINED PROXY_WASM_CPP_HOST_PATH)
  message(FATAL_ERROR
    "Set PROXY_WASM_CPP_HOST_PATH to your proxy-wasm-cpp-host source tree.\n"
    "  cmake -DPROXY_WASM_CPP_HOST_PATH=... .")
endif()

if(NOT DEFINED PROXY_WASM_CPP_SDK_PATH)
  set(PROXY_WASM_CPP_SDK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../.deps/proxy-wasm-cpp-sdk")
endif()

if(NOT DEFINED WASMEDGE_INSTALL_PATH)
  set(WASMEDGE_INSTALL_PATH "$ENV{HOME}/.wasmedge")
endif()

set(HOST_ROOT "${PROXY_WASM_CPP_HOST_PATH}")

# ── WasmEdge shared library ───────────────────────────────────────────────────
find_library(WASMEDGE_LIB
  NAMES wasmedge
  HINTS "${WASMEDGE_INSTALL_PATH}/lib"
  REQUIRED)

message(STATUS "WasmEdge library : ${WASMEDGE_LIB}")

# ── OpenSSL ───────────────────────────────────────────────────────────────────
find_package(OpenSSL REQUIRED)
message(STATUS "OpenSSL include : ${OPENSSL_INCLUDE_DIR}")
message(STATUS "OpenSSL library : ${OPENSSL_LIBRARIES}")

# ── Abseil (required by proxy-wasm-cpp-host for absl::cleanup) ───────────────
if(NOT DEFINED ABSL_ROOT_DIR)
  # Try common Homebrew locations first
  if(EXISTS "/opt/homebrew/opt/abseil")
    set(ABSL_ROOT_DIR "/opt/homebrew/opt/abseil")
  elseif(EXISTS "/usr/local/opt/abseil")
    set(ABSL_ROOT_DIR "/usr/local/opt/abseil")
  endif()
endif()
if(DEFINED ABSL_ROOT_DIR)
  list(PREPEND CMAKE_PREFIX_PATH "${ABSL_ROOT_DIR}")
endif()
find_package(absl REQUIRED)
message(STATUS "Abseil include : ${ABSL_ROOT_DIR}/include")

# ── proxy-wasm-cpp-host sources ───────────────────────────────────────────────
# The host library is built as part of the main project here so that we can
# enable the WasmEdge backend.  In a real project you would add the host as a
# CMake subdirectory or consume its installed targets.

set(HOST_SOURCES
  # Core base library
  "${HOST_ROOT}/src/bytecode_util.cc"
  "${HOST_ROOT}/src/context.cc"
  "${HOST_ROOT}/src/exports.cc"
  "${HOST_ROOT}/src/hash.cc"
  "${HOST_ROOT}/src/pairs_util.cc"
  "${HOST_ROOT}/src/shared_data.cc"
  "${HOST_ROOT}/src/shared_queue.cc"
  "${HOST_ROOT}/src/signature_util.cc"
  "${HOST_ROOT}/src/vm_id_handle.cc"
  "${HOST_ROOT}/src/wasm.cc"
  # Null VM (needed for NullIntegration base class)
  "${HOST_ROOT}/src/null/null.cc"
  "${HOST_ROOT}/src/null/null_plugin.cc"
  "${HOST_ROOT}/src/null/null_vm.cc"
  # WasmEdge runtime backend (the whole point of the demo)
  "${HOST_ROOT}/src/wasmedge/wasmedge.cc"
)

add_library(proxy_wasm_host STATIC ${HOST_SOURCES})

target_include_directories(proxy_wasm_host PUBLIC
  "${HOST_ROOT}"
  "${HOST_ROOT}/include"
  "${PROXY_WASM_CPP_SDK_PATH}"
  "${WASMEDGE_INSTALL_PATH}/include"
  "${OPENSSL_INCLUDE_DIR}"
)
target_link_libraries(proxy_wasm_host PUBLIC
  "${WASMEDGE_LIB}"
  OpenSSL::SSL
  OpenSSL::Crypto
  absl::cleanup
  absl::base
  absl::strings
)
target_compile_definitions(proxy_wasm_host PUBLIC WITH_WASMEDGE=1)

# ── Demo executable ───────────────────────────────────────────────────────────
add_executable(demo main.cc)
target_link_libraries(demo PRIVATE proxy_wasm_host)
target_include_directories(demo PRIVATE
  "${HOST_ROOT}"
  "${HOST_ROOT}/include"
  "${PROXY_WASM_CPP_SDK_PATH}"
  "${WASMEDGE_INSTALL_PATH}/include"
  "${OPENSSL_INCLUDE_DIR}"
)
