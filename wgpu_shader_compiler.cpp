#include "wgpu_shader_compiler.hpp"

#include <tint/tint.h>
#include <src/tint/lang/core/ir/module.h>
#include <src/tint/lang/spirv/reader/reader.h>
#include <src/tint/lang/wgsl/writer/writer.h>

#include <cassert>
#include <cstdlib>
#include <vector>

#ifdef gas_dawn_tint_EXPORTS
#define GAS_TINT_VIZ MADRONA_EXPORT
#else
#define GAS_TINT_VIZ MADRONA_IMPORT
#endif

namespace gas::webgpu {

GAS_TINT_VIZ void tintInit()
{
  tint::Initialize();
}

GAS_TINT_VIZ void tintShutdown()
{
  tint::Shutdown();
}

GAS_TINT_VIZ bool tintConvertSPIRVToWGSL(
    void *spirv_bytecode, int64_t num_bytes,
    void *(*alloc_fn)(void *alloc_data, int64_t num_bytes), void *alloc_data,
    void **out_wgsl, int64_t *out_num_bytes, char **out_diagnostics)
{
  assert(num_bytes % 4 == 0);
  std::vector<uint32_t> tint_input(num_bytes / 4);
  memcpy(tint_input.data(), spirv_bytecode, num_bytes);
  
  tint::Program tint_prog = tint::spirv::reader::Read(tint_input);
  
  if (tint_prog.Diagnostics().ContainsErrors()) {
    std::string wgsl_prog_str = tint::Program::printer(tint_prog);

    *out_diagnostics = (char *)alloc_fn(
        alloc_data, (int64_t)wgsl_prog_str.size());
    memcpy(*out_diagnostics, wgsl_prog_str.c_str(), wgsl_prog_str.size());
    return false;
  }
  
  auto tint_wgsl = tint::wgsl::writer::Generate(
      tint_prog, tint::wgsl::writer::Options {});
  
  if (tint_wgsl != tint::Success) {
    std::string wgsl_prog_str = tint::Program::printer(tint_prog);

    *out_diagnostics = (char *)alloc_fn(
        alloc_data, (int64_t)wgsl_prog_str.size());
    memcpy(*out_diagnostics, wgsl_prog_str.c_str(), wgsl_prog_str.size());
    return false;
  }
  
  *out_num_bytes = (int64_t)tint_wgsl->wgsl.size();
  *out_wgsl = alloc_fn(alloc_data, *out_num_bytes);
  *out_diagnostics = nullptr;

  memcpy(*out_wgsl, tint_wgsl->wgsl.data(), *out_num_bytes);

  return true;
}

}
