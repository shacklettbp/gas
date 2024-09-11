#include "init.hpp"

#include <cassert>

#ifdef GAS_SUPPORT_WEBGPU
#include "wgpu_init.hpp"
#endif

namespace gas {

namespace InitSystem {

GPUAPISelect autoSelectAPI()
{
  return GPUAPISelect::WebGPU;
}

GPULib * loadAPILib(GPUAPISelect api_select)
{
  switch (api_select) {
  case GPUAPISelect::WebGPU: {
    return webgpu::loadWebGPULib();
  } break;
  default: {
    MADRONA_UNREACHABLE();
  } break;
  }
}

void unloadAPILib(GPULib *lib)
{
  delete lib;
}

GPUAPI * initAPI(GPUAPISelect chosen_api,
                 GPULib *lib,
                 const APIConfig &cfg)
{
  switch (chosen_api) {
  case GPUAPISelect::WebGPU: {
    return webgpu::initWebGPU(lib, cfg);
  } break;
  default: {
    MADRONA_UNREACHABLE();
  } break;
  }
}

}

}
