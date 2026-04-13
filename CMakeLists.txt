cmake_minimum_required(VERSION 3.14)
project(proxy_wasm_wasmedge_demo CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED PROXY_WASM_CPP_HOST_PATH)
  message(FATAL_ERROR "Set -DPROXY_WASM_CPP_HOST_PATH=<path>")
endif()
if(NOT DEFINED WASMEDGE_INSTALL_PATH)
  set(WASMEDGE_INSTALL_PATH "$ENV{HOME}/.wasmedge")
endif()

set(HOST_ROOT "${PROXY_WASM_CPP_HOST_PATH}")

if(NOT DEFINED PROXY_WASM_CPP_SDK_PATH)
  set(PROXY_WASM_CPP_SDK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/.deps/proxy-wasm-cpp-sdk")
endif()

find_library(WASMEDGE_LIB NAMES wasmedge
  HINTS "${WASMEDGE_INSTALL_PATH}/lib" REQUIRED)
message(STATUS "WasmEdge library : ${WASMEDGE_LIB}")

file(GLOB_RECURSE HOST_SOURCES
  "${HOST_ROOT}/src/*.cc"
)

add_library(proxy_wasm_host STATIC ${HOST_SOURCES})
target_include_directories(proxy_wasm_host PUBLIC
  "${HOST_ROOT}"              # repo root  — for "include/proxy-wasm/…" and "src/…" includes
  "${HOST_ROOT}/include"      # also add include/ so <proxy-wasm/foo.h> works directly
  "${PROXY_WASM_CPP_SDK_PATH}"
  "${WASMEDGE_INSTALL_PATH}/include"
)
target_link_libraries(proxy_wasm_host PUBLIC "${WASMEDGE_LIB}")
target_compile_definitions(proxy_wasm_host PUBLIC WITH_WASMEDGE=1)

add_executable(demo host/main.cc)
target_link_libraries(demo PRIVATE proxy_wasm_host)
target_include_directories(demo PRIVATE
  "${HOST_ROOT}"
  "${HOST_ROOT}/include"
  "${PROXY_WASM_CPP_SDK_PATH}"
  "${WASMEDGE_INSTALL_PATH}/include"
)