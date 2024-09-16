#include "init.hpp"

#ifdef GAS_SUPPORT_WEBGPU
#include "wgpu_init.hpp"
#endif

#include <madrona/macros.hpp>

#include <cassert>

#if defined(MADRONA_LINUX) or defined(MADRONA_MACOS)
#include <dlfcn.h>
#elif defined(MADRONA_WINDOWS)
#include "windows.hpp"
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

ShaderCompilerLib loadShaderCompiler()
{
#if defined(MADRONA_WINDOWS)
  const char *lib_name = "gas_shader_compiler.dll";

  void *handle = LoadLibraryExA(
      lib_name, nullptr, LOAD_LIBRARY_SEARCH_APPLICATION_DIR);
  if (!handle) {
    FATAL("Failed to load shader compiler library: %u", GetLastError());
  }

  auto startup_fn = (void (*)())GetProcAddress(
      handle, "gasStartupShaderCompilerLib");

  if (!startup_fn) {
    FATAL("Failed to find startup function in shader compiler library: %u",
          GetLastError());
  }

  startup_fn();

  auto create_fn = (ShaderCompiler * (*)())GetProcAddress(
      handle, "gasCreateShaderCompiler");
  auto destroy_fn = (void (*)(ShaderCompiler *))GetProcAddress(
      lib, "gasDestroyShaderCompiler");

  if (!create_fn || !destroy_fn) {
    FATAL("Failed to find create / destroy functions in shader compiler library: %u",
          GetLastError());
  }

  // Return the handle and the create function
  return { handle, create_fn, destroy_fn };
#elif defined(MADRONA_LINUX) or defined(MADRONA_MACOS)
#ifdef MADRONA_LINUX
  const char *lib_name = "libgas_shader_compiler.so";
#else
  const char *lib_name = "libgas_shader_compiler.dylib";
#endif
  void *lib = dlopen(lib_name, RTLD_NOW | RTLD_LOCAL);
  if (!lib) {
    FATAL("Failed to load shader compiler library: %s", dlerror());
  }

  auto startup_fn = (void (*)())dlsym(
      lib, "gasStartupShaderCompilerLib");

  if (!startup_fn) {
    FATAL("Failed to find startup function in shader compiler library: %s",
          dlerror());
  }

  startup_fn();

  auto create_fn = (ShaderCompiler * (*)())dlsym(
      lib, "gasCreateShaderCompiler");
  auto destroy_fn = (void (*)(ShaderCompiler *))dlsym(
      lib, "gasDestroyShaderCompiler");
  if (!create_fn || !destroy_fn) {
    FATAL("Failed to find create /destroy functions in shader compiler library: %s",
          dlerror());
  }

  return { lib, create_fn, destroy_fn };
#else 
  FATAL("Shader compiler not supported");
#endif
}

void unloadShaderCompiler(ShaderCompilerLib compiler_lib)
{
#if defined(MADRONA_WINDOWS)
  auto shutdown_fn = (void (*)())GetProcAddress(
      compiler_lib.hdl, "gasShutdownShaderCompilerLib");
  if (!shutdown_fn) {
    FATAL("Failed to shutdown shader compiler: %u", GetLastError());
  }

  shutdown_fn();
  if (!FreeLibrary(compiler_lib.hdl)) {
    FATAL("Failed to unload shader compiler library: %u", GetLastError());
  }
#elif defined(MADRONA_LINUX) or defined(MADRONA_MACOS)
  auto shutdown_fn = (void (*)())dlsym(
      compiler_lib.hdl, "gasShutdownShaderCompilerLib");
  if (!shutdown_fn) {
    FATAL("Failed to shutdown shader compiler: %s", dlerror());
  }

  shutdown_fn();
  dlclose(compiler_lib.hdl);
#else
  (void)compiler_lib;
  FATAL("Shader compiler not supported");
#endif
}

}

}
