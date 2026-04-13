cmake_minimum_required(VERSION 3.14)
project(proxy_wasm_add_header_plugin CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED PROXY_WASM_CPP_SDK_PATH)
  message(FATAL_ERROR "Set -DPROXY_WASM_CPP_SDK_PATH=<path>")
endif()

# ── wasi-sdk 25+ (Clang 22) fix ───────────────────────────────────────────────
# Newer wasi-sdk releases place C++ stdlib headers under a target-specific
# subdirectory of the sysroot (wasm32-wasip1/c++/v1/) rather than a flat
# include path.  Derive the SDK prefix from the toolchain file and wire up:
#   1. --sysroot pointing to share/wasi-sysroot
#   2. The target-specific C++ include directory
if(DEFINED CMAKE_TOOLCHAIN_FILE)
  # Toolchain file lives at <wasi-sdk>/share/cmake/wasi-sdk-p1.cmake
  get_filename_component(_WASI_CMAKE_DIR "${CMAKE_TOOLCHAIN_FILE}" DIRECTORY)
  get_filename_component(WASI_SDK_PREFIX "${_WASI_CMAKE_DIR}/../.." ABSOLUTE)
  set(WASI_SYSROOT "${WASI_SDK_PREFIX}/share/wasi-sysroot")
  if(EXISTS "${WASI_SYSROOT}")
    set(WASI_CXX_INCLUDE "${WASI_SYSROOT}/include/wasm32-wasip1/c++/v1")
    add_compile_options(--sysroot=${WASI_SYSROOT})
    if(EXISTS "${WASI_CXX_INCLUDE}")
      include_directories(SYSTEM "${WASI_CXX_INCLUDE}")
      message(STATUS "wasi-sdk sysroot  : ${WASI_SYSROOT}")
      message(STATUS "wasi-sdk C++ hdrs : ${WASI_CXX_INCLUDE}")
    endif()
  endif()
endif()
# ──────────────────────────────────────────────────────────────────────────────

add_executable(plugin plugin.cc)
target_include_directories(plugin PRIVATE "${PROXY_WASM_CPP_SDK_PATH}")

set_target_properties(plugin PROPERTIES
  OUTPUT_NAME "add_header_filter"
  SUFFIX      ".wasm"
  # -mexec-model=reactor : builds a WASI reactor instead of command (no main() required)
  # --export-all         : export all our ABI callbacks to the host
  # --export=malloc      : explicitly export malloc from wasi-libc since proxy-wasm relies on it
  # --export=free        : explicitly export free
  # --allow-undefined    : host imports resolved at runtime
  LINK_FLAGS  "-mexec-model=reactor -Wl,--export-all -Wl,--export=malloc -Wl,--export=free -Wl,--allow-undefined"
)