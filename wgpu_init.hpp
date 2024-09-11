#pragma once

#include "gas.hpp"

namespace gas::webgpu {

GPULib * loadWebGPULib();
GPUAPI * initWebGPU(GPULib *lib, const APIConfig &cfg);

}
