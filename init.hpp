#pragma once

#include "namespace.hpp"
#include "gas.hpp"

#include <madrona/span.hpp>

namespace gas {

enum class GPUAPISelect : u32 {
  Vulkan,
  Metal,
  WebGPU,
};

namespace InitSystem {

GPUAPISelect autoSelectAPI();
GPULib * loadAPILib(GPUAPISelect select);
void unloadAPILib(GPULib *lib);

ShaderCompilerLib loadShaderCompiler();
// Note: Currently, the slang dynamic library doesn't seem to get
// unloaded properly by this, meaning its not safe to unload and reload
// the shader compiler library in the same process.
void unloadShaderCompiler(ShaderCompilerLib compiler_lib);

GPUAPI * initAPI(GPUAPISelect select, GPULib *lib, const APIConfig &cfg);

}

}
