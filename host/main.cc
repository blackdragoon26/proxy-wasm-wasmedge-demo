// host/main.cc
//
// Minimal proxy-wasm host that demonstrates WasmEdge as the runtime backend.
//
// It:
//   1. Creates a WasmEdge VM through proxy-wasm-cpp-host's factory.
//   2. Loads the compiled add_header_filter.wasm plugin.
//   3. Simulates an HTTP exchange (request headers → response headers).
//   4. Prints the resulting response headers so you can see "x-powered-by:
//   WasmEdge".

#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

// proxy-wasm-cpp-host public headers
#include "include/proxy-wasm/context.h"
#include "include/proxy-wasm/wasm.h"

// WasmEdge runtime factory (added by PR #193)
#include "include/proxy-wasm/wasmedge.h"

// ────────────────────────────────────────────────────────────────────────────
// Helpers
// ────────────────────────────────────────────────────────────────────────────

namespace {

using HeaderMap = std::vector<std::pair<std::string, std::string>>;

std::string readFile(const std::string &path) {
  std::ifstream f(path, std::ios::binary);
  if (!f) {
    std::cerr << "ERROR: Cannot open '" << path << "'\n";
    std::exit(1);
  }
  std::ostringstream buf;
  buf << f.rdbuf();
  return buf.str();
}

void printHeaders(const std::string &label, const HeaderMap &hdrs) {
  std::cout << "\n── " << label << " ──\n";
  for (auto &[k, v] : hdrs) {
    std::cout << "  " << k << ": " << v << "\n";
  }
}

// ── DemoContext
// ───────────────────────────────────────────────────────────────
//
// A concrete ContextBase that:
//   • stores simulated request and response header maps
//   • implements the subset of HeaderInterface actually called by a typical
//     HTTP filter (add/get/getSize/getPairs)
//   • prints plugin log lines to stdout

class DemoContext : public proxy_wasm::ContextBase {
public:
  DemoContext(proxy_wasm::WasmBase *wasm,
              const std::shared_ptr<proxy_wasm::PluginBase> &plugin)
      : proxy_wasm::ContextBase(wasm, plugin) {}

  // ── HeaderInterface overrides ─────────────────────────────────────────────

  proxy_wasm::WasmResult addHeaderMapValue(proxy_wasm::WasmHeaderMapType type,
                                           std::string_view key,
                                           std::string_view value) override {
    headerMap(type).emplace_back(key, value);
    return proxy_wasm::WasmResult::Ok;
  }

  proxy_wasm::WasmResult getHeaderMapValue(proxy_wasm::WasmHeaderMapType type,
                                           std::string_view key,
                                           std::string_view *result) override {
    for (auto &[k, v] : headerMap(type)) {
      if (k == key) {
        *result = v;
        return proxy_wasm::WasmResult::Ok;
      }
    }
    *result = {};
    return proxy_wasm::WasmResult::Ok;
  }

  proxy_wasm::WasmResult getHeaderMapPairs(proxy_wasm::WasmHeaderMapType type,
                                           proxy_wasm::Pairs *result) override {
    auto &map = headerMap(type);
    result->clear();
    for (auto &[k, v] : map)
      result->emplace_back(k, v);
    return proxy_wasm::WasmResult::Ok;
  }

  proxy_wasm::WasmResult
  setHeaderMapPairs(proxy_wasm::WasmHeaderMapType type,
                    const proxy_wasm::Pairs &pairs) override {
    auto &map = headerMap(type);
    map.clear();
    for (auto &[k, v] : pairs)
      map.emplace_back(k, v);
    return proxy_wasm::WasmResult::Ok;
  }

  proxy_wasm::WasmResult
  removeHeaderMapValue(proxy_wasm::WasmHeaderMapType type,
                       std::string_view key) override {
    auto &map = headerMap(type);
    map.erase(std::remove_if(map.begin(), map.end(),
                             [&](const auto &p) { return p.first == key; }),
              map.end());
    return proxy_wasm::WasmResult::Ok;
  }

  proxy_wasm::WasmResult
  replaceHeaderMapValue(proxy_wasm::WasmHeaderMapType type,
                        std::string_view key, std::string_view value) override {
    for (auto &[k, v] : headerMap(type)) {
      if (k == key) {
        v = value;
        return proxy_wasm::WasmResult::Ok;
      }
    }
    headerMap(type).emplace_back(key, value);
    return proxy_wasm::WasmResult::Ok;
  }

  proxy_wasm::WasmResult getHeaderMapSize(proxy_wasm::WasmHeaderMapType type,
                                          uint32_t *result) override {
    *result = static_cast<uint32_t>(headerMap(type).size());
    return proxy_wasm::WasmResult::Ok;
  }

  // ── GeneralInterface partial overrides ───────────────────────────────────

  proxy_wasm::WasmResult log(uint32_t level,
                             std::string_view message) override {
    const char *lvl = "INFO";
    if (level == 0)
      lvl = "TRACE";
    else if (level == 1)
      lvl = "DEBUG";
    else if (level == 3)
      lvl = "WARN";
    else if (level == 4)
      lvl = "ERROR";
    std::cout << "[" << lvl << "] " << message << "\n";
    return proxy_wasm::WasmResult::Ok;
  }

  uint64_t getCurrentTimeNanoseconds() override {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
  }

  // ── Header storage (public for the driver) ────────────────────────────────
  HeaderMap request_headers;
  HeaderMap response_headers;

private:
  HeaderMap &headerMap(proxy_wasm::WasmHeaderMapType type) {
    if (type == proxy_wasm::WasmHeaderMapType::RequestHeaders)
      return request_headers;
    return response_headers;
  }
};

// ── DemoWasmVmIntegration
// ──────────────────────────────────────────────────────
class DemoWasmVmIntegration : public proxy_wasm::WasmVmIntegration {
public:
  DemoWasmVmIntegration *clone() override {
    return new DemoWasmVmIntegration();
  }
  proxy_wasm::LogLevel getLogLevel() override {
    return proxy_wasm::LogLevel::trace;
  }
  void error(std::string_view message) override {
    std::cerr << "[WasmVm Error] " << message << std::endl;
  }
  void trace(std::string_view message) override {
    std::cout << "[WasmVm Trace] " << message << std::endl;
  }
  bool getNullVmFunction(std::string_view /*function_name*/,
                         bool /*returns_word*/, int /*number_of_arguments*/,
                         proxy_wasm::NullPlugin * /*plugin*/,
                         void * /*ptr_to_function_return*/) override {
    return false;
  }
};

// ── DemoWasm ─────────────────────────────────────────────────────────────────
//
// Extends WasmBase only to supply our DemoContext factory and VM integration.

class DemoWasm : public proxy_wasm::WasmBase {
public:
  DemoWasm(std::unique_ptr<proxy_wasm::WasmVm> vm, std::string_view vm_id)
      : proxy_wasm::WasmBase(std::move(vm), vm_id,
                             /*vm_configuration=*/"",
                             /*vm_key=*/"",
                             /*envs=*/{},
                             /*allowed_capabilities=*/{}) {
    wasm_vm()->integration().reset(new DemoWasmVmIntegration());
  }

  proxy_wasm::ContextBase *createRootContext(
      const std::shared_ptr<proxy_wasm::PluginBase> &plugin) override {
    auto *ctx = new DemoContext(this, plugin);
    demo_ctx_ = ctx;
    return ctx;
  }

  proxy_wasm::ContextBase *createContext(
      const std::shared_ptr<proxy_wasm::PluginBase> &plugin) override {
    return new DemoContext(this, plugin);
  }

  DemoContext *demo_ctx() const { return demo_ctx_; }

private:
  DemoContext *demo_ctx_{nullptr};
};

// ── DemoWasmHandle
// ────────────────────────────────────────────────────────────

class DemoWasmHandle : public proxy_wasm::WasmHandleBase {
public:
  explicit DemoWasmHandle(std::shared_ptr<proxy_wasm::WasmBase> wasm)
      : proxy_wasm::WasmHandleBase(std::move(wasm)) {}
};

} // anonymous namespace

// ────────────────────────────────────────────────────────────────────────────
// main
// ────────────────────────────────────────────────────────────────────────────

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <path/to/add_header_filter.wasm>\n";
    return 1;
  }
  const std::string wasm_path = argv[1];

  std::cout << "==============================================\n";
  std::cout << " proxy-wasm + WasmEdge demo\n";
  std::cout << "==============================================\n\n";

  // 1. Read the compiled Wasm plugin from disk.
  std::string wasm_bytes = readFile(wasm_path);
  std::cout << "Loaded plugin: " << wasm_path << " (" << wasm_bytes.size()
            << " bytes)\n";

  // 2. Construct our DemoWasm host instance backed by WasmEdge.
  const std::string vm_id = "demo_vm";
  auto demo_wasm =
      std::make_shared<DemoWasm>(proxy_wasm::createWasmEdgeVm(), vm_id);
  std::cout << "Runtime:       WasmEdge ("
            << demo_wasm->wasm_vm()->getEngineName() << ")\n";

  // 3. Load and initialise the Wasm module.
  if (!demo_wasm->load(wasm_bytes, /*allow_precompiled=*/false)) {
    std::cerr << "ERROR: Failed to load Wasm module.\n";
    return 1;
  }
  if (!demo_wasm->initialize()) {
    std::cerr << "ERROR: Failed to initialize Wasm module.\n";
    return 1;
  }

  // 4. Create a PluginBase that carries metadata the Wasm plugin can query.
  const std::string root_id = "add_header_filter";
  auto plugin = std::make_shared<proxy_wasm::PluginBase>(
      /*name=*/"add_header_filter",
      /*root_id=*/root_id,
      /*vm_id=*/vm_id,
      /*engine=*/"wasmedge",
      /*plugin_configuration=*/"",
      /*fail_open=*/false,
      /*key=*/"");

  // 5. Start the root context (calls the plugin's proxy_on_vm_start /
  // proxy_on_configure).
  auto *root_ctx = demo_wasm->start(plugin);
  if (!root_ctx) {
    std::cerr << "ERROR: Failed to start root context.\n";
    return 1;
  }
  demo_wasm->configure(root_ctx, plugin);
  std::cout << "Root context created (root_id=\"" << root_id << "\")\n";

  // ── Simulate HTTP request / response ────────────────────────────────────

  // Grab the root context so we can seed the header maps.
  auto *ctx = demo_wasm->demo_ctx();
  ctx->request_headers = {
      {":method", "GET"},
      {":path", "/api/hello"},
      {":authority", "example.com"},
      {"user-agent", "curl/7.88.1"},
      {"accept", "*/*"},
  };
  ctx->response_headers = {
      {":status", "200"},
      {"content-type", "application/json"},
      {"content-length", "27"},
  };

  printHeaders("Request headers (before filter)", ctx->request_headers);
  printHeaders("Response headers (before filter)", ctx->response_headers);

  // 6. Run the HTTP filter lifecycle on the root context.
  //    (For proper stream contexts use WasmBase::createContext + callOnThread;
  //     running directly on the root context is sufficient for this demo.)
  std::cout << "\n── Running onRequestHeaders ──\n";
  root_ctx->onRequestHeaders(static_cast<uint32_t>(ctx->request_headers.size()),
                             /*end_of_stream=*/false);

  std::cout << "\n── Running onResponseHeaders ──\n";
  root_ctx->onResponseHeaders(
      static_cast<uint32_t>(ctx->response_headers.size()),
      /*end_of_stream=*/false);

  printHeaders("Response headers (after filter)", ctx->response_headers);

  std::cout << "\n==============================================\n";
  std::cout << " Demo complete!  Check 'x-powered-by: WasmEdge' above.\n";
  std::cout << "==============================================\n";

  return 0;
}
