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

GPUAPI * initAPI(GPUAPISelect select, GPULib *lib, const APIConfig &cfg);

}

}
