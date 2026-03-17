#pragma once

#include <Core/LogsLevel.h>
#include <Interpreters/WebAssembly/WasmEngine.h>

namespace DB::WebAssembly {

/// WasmEdgeRuntime is a specific implementation of WasmModule using the WasmEdge runtime.
class WasmEdgeRuntime final : public IWasmEngine {
 public:
  explicit WasmEdgeRuntime();

  std::unique_ptr<WasmModule> compileModule(std::string_view wasm_code) const override;
  static void setLogLevel(LogsLevel level);
};

}  // namespace DB::WebAssembly
