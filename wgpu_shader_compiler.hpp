#pragma once
#include <cstdint>

#include <madrona/macros.hpp>

#ifdef gas_dawn_tint_EXPORTS
#define GAS_TINT_VIZ MADRONA_EXPORT
#else
#define GAS_TINT_VIZ MADRONA_IMPORT
#endif

namespace gas::webgpu {

enum class TintConvertStatus {
  Success,
  SPIRVConvertError,
  WGSLOutputError,
};

GAS_TINT_VIZ void tintInit();
GAS_TINT_VIZ void tintShutdown();

GAS_TINT_VIZ TintConvertStatus tintConvertSPIRVToWGSL(
    void *spirv_bytecode, int64_t num_bytes,
    void *(*alloc_fn)(void *alloc_data, int64_t num_bytes), void *alloc_data,
    char **out_wgsl, int64_t *out_num_bytes, char **out_diagnostics);
                        
}

#undef GAS_TINT_VIZ
