// plugin/plugin.cc
//
// proxy-wasm filter using the raw ABI headers (no protobuf dependency).
// Uses the exact types defined in proxy_wasm_enums.h / proxy_wasm_common.h.
//
// What it does:
//   - Logs on every HTTP request.
//   - Adds "x-powered-by: WasmEdge" to every HTTP response.

#include <cstdint>
#include <cstring>
#include <string_view>

#include "proxy_wasm_common.h" // WasmResult, WasmHeaderMapType
#include "proxy_wasm_enums.h" // LogLevel, FilterHeadersStatus, FilterDataStatus
#include "proxy_wasm_externs.h" // proxy_log, proxy_add_header_map_value, …

static void log_info(std::string_view msg) {
  proxy_log(LogLevel::info, msg.data(), msg.size());
}

extern "C" {

__attribute__((export_name("proxy_abi_version_0_2_0"))) void
proxy_abi_version_0_2_0() {}

__attribute__((export_name("proxy_on_vm_start"))) uint32_t
proxy_on_vm_start(uint32_t, uint32_t) {
  log_info("[AddHeaderFilter] VM started on WasmEdge runtime.");
  return 1;
}

__attribute__((export_name("proxy_on_configure"))) uint32_t
proxy_on_configure(uint32_t, uint32_t) {
  log_info("[AddHeaderFilter] Plugin configured.");
  return 1;
}

__attribute__((export_name("proxy_on_context_create"))) void
proxy_on_context_create(uint32_t, uint32_t) {}

__attribute__((export_name("proxy_on_request_headers"))) FilterHeadersStatus
proxy_on_request_headers(uint32_t, uint32_t, uint32_t) {
  log_info("[AddHeaderFilter] Incoming HTTP request — passing through.");
  return FilterHeadersStatus::Continue;
}

__attribute__((export_name("proxy_on_response_headers"))) FilterHeadersStatus
proxy_on_response_headers(uint32_t, uint32_t, uint32_t) {
  constexpr std::string_view key = "x-powered-by";
  constexpr std::string_view value = "WasmEdge";
  proxy_add_header_map_value(
      WasmHeaderMapType::ResponseHeaders, // correct enum name
      key.data(), key.size(), value.data(), value.size());
  log_info(
      "[AddHeaderFilter] Injected 'x-powered-by: WasmEdge' into response.");
  return FilterHeadersStatus::Continue;
}

__attribute__((export_name("proxy_on_request_body"))) FilterDataStatus
proxy_on_request_body(uint32_t, uint32_t, uint32_t) {
  return FilterDataStatus::Continue;
}

__attribute__((export_name("proxy_on_response_body"))) FilterDataStatus
proxy_on_response_body(uint32_t, uint32_t, uint32_t) {
  return FilterDataStatus::Continue;
}

__attribute__((export_name("proxy_on_done"))) uint32_t proxy_on_done(uint32_t) {
  return 1;
}

__attribute__((export_name("proxy_on_log"))) void proxy_on_log(uint32_t) {}

__attribute__((export_name("proxy_on_delete"))) void proxy_on_delete(uint32_t) {
}

} // extern "C"