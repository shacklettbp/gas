#include "gas.hpp"
#include "backend_common.hpp"

#include "wgpu_init.hpp"

#include <brt/sync.hpp>

#include <dlfcn.h>

#if defined(BRT_OS_LINUX) or defined(BRT_OS_MACOS)
#include <dlfcn.h>
#elif defined(BRT_OS_WINDOWS)
#include "windows.hpp"
#endif

namespace gas {

using namespace brt;

GPUAPISelect GPULib::autoSelectAPI()
{
  return GPUAPISelect::WebGPU;
}

GPULib * GPULib::init(GPUAPISelect select,
                      const APIConfig &cfg)
{
  switch (select) {
  case GPUAPISelect::None: {
    FATAL("Invalid GPU API selected");
  } break;
  case GPUAPISelect::Vulkan: {
    FATAL("Not implemented");
  } break;
  case GPUAPISelect::Metal: {
    FATAL("Not implemented");
  } break;
  case GPUAPISelect::WebGPU: {
    return webgpu::initWebGPU(cfg);
  } break;
  default: {
    BRT_UNREACHABLE();
  } break;
  }
}

void ShaderCompilerLib::load()
{
#if defined(BRT_OS_WINDOWS)
  const char *lib_name = "gas_shader_compiler.dll";

  hdl = LoadLibraryExA(
      lib_name, nullptr, LOAD_LIBRARY_SEARCH_APPLICATION_DIR);
  if (!hdl) {
    FATAL("Failed to load shader compiler library: %u", GetLastError());
  }

  auto startup_fn = (void (*)())GetProcAddress(
      hdl, "gasStartupShaderCompilerLib");

  if (!startup_fn) {
    FATAL("Failed to find startup function in shader compiler library: %u",
          GetLastError());
  }

  startup_fn();

  createCompiler = (ShaderCompiler * (*)())GetProcAddress(
      hdl, "gasCreateShaderCompiler");
  destroyCompiler = (void (*)(ShaderCompiler *))GetProcAddress(
      hdl, "gasDestroyShaderCompiler");

  if (!createCompiler || !destroyCompiler) {
    FATAL("Failed to find create / destroy functions in shader compiler library: %u",
          GetLastError());
  }
#elif defined(BRT_OS_LINUX) or defined(BRT_OS_MACOS)
#ifdef BRT_OS_LINUX
  const char *lib_name = "libgas_shader_compiler.so";
#else
  const char *lib_name = "libgas_shader_compiler.dylib";
#endif
  hdl = dlopen(lib_name, RTLD_NOW | RTLD_LOCAL);
  if (!hdl) {
    FATAL("Failed to load shader compiler library: %s", dlerror());
  }

  auto startup_fn = (void (*)())dlsym(
      hdl, "gasStartupShaderCompilerLib");

  if (!startup_fn) {
    FATAL("Failed to find startup function in shader compiler library: %s",
          dlerror());
  }

  startup_fn();

  createCompiler = (ShaderCompiler * (*)())dlsym(
      hdl, "gasCreateShaderCompiler");
  destroyCompiler = (void (*)(ShaderCompiler *))dlsym(
      hdl, "gasDestroyShaderCompiler");
  if (!createCompiler || !destroyCompiler) {
    FATAL("Failed to find create /destroy functions in shader compiler library: %s",
          dlerror());
  }
#else 
  FATAL("Shader compiler not supported");
#endif
}

void ShaderCompilerLib::unload()
{
#if defined(BRT_OS_WINDOWS)
  auto shutdown_fn = (void (*)())GetProcAddress(
      hdl, "gasShutdownShaderCompilerLib");
  if (!shutdown_fn) {
    FATAL("Failed to shutdown shader compiler: %u", GetLastError());
  }

  shutdown_fn();
  if (!FreeLibrary(hdl)) {
    FATAL("Failed to unload shader compiler library: %u", GetLastError());
  }
#elif defined(BRT_OS_LINUX) or defined(BRT_OS_MACOS)
  auto shutdown_fn = (void (*)())dlsym(
      hdl, "gasShutdownShaderCompilerLib");
  if (!shutdown_fn) {
    FATAL("Failed to shutdown shader compiler: %s", dlerror());
  }

  shutdown_fn();
  dlclose(hdl);
#else
  FATAL("Shader compiler not supported");
#endif
}

ResourceUUIDMap::ResourceUUIDMap()
{
  for (i32 bucket_idx = 0; bucket_idx < NUM_BUCKETS; bucket_idx++) {
    Bucket &bucket = buckets_[bucket_idx];
    for (i32 i = 0; i < BUCKET_SIZE; i++) {
      bucket.hashes[i] = 0;
    }
  }
}

i32 ResourceUUIDMap::lookup(UUID uuid)
{
  auto [key1, key2, bucket1, bucket2] = hash(uuid);

  i32 result = NOT_FOUND;

  BRT_UNROLL
  for (i32 i = 0; i < BUCKET_SIZE; i++) {
    u64 hash1 = bucket1->hashes[i];
    u64 hash2 = bucket2->hashes[i];

    u16 row1 = bucket1->rows[i];
    u16 row2 = bucket2->rows[i];

    if (hash1 == key1) {
      result = row1;
    } else if (hash2 == key2) {
      result = row2;
    }
  }

  return result;
}

void ResourceUUIDMap::insert(UUID uuid, u16 row)
{
  auto [key1, key2, bucket1, bucket2] = hash(uuid);

  i32 num_empty1 = 0;
  i32 num_empty2 = 0;

  i32 free1 = -1;
  i32 free2 = -1;

  bool duplicate = false;

  BRT_UNROLL
  for (i32 i = 0; i < BUCKET_SIZE; i++) {
    u64 hash1 = bucket1->hashes[i];
    u64 hash2 = bucket2->hashes[i];

    if (hash1 == key1 || hash2 == key2) {
      duplicate = true;
    }

    if (hash1 == 0) {
      free1 = i;
      num_empty1 += 1;
    }

    if (hash2 == 0) {
      free2 = i;
      num_empty2 += 1;
    }
  }

  // Note that this isn't necessarily a duplicate UUID, since we only compare 
  // the XOR of each half of the UUIDs.
  if (duplicate) [[unlikely]] {
    FATAL("ResourceUUIDMap: Failed to insert UUID (%llu %llu). Duplicate UUID hash in same bucket.",
        uuid[0], uuid[1]);
  }

  if (num_empty1 > num_empty2) {
    bucket1->hashes[free1] = key1;
    bucket1->rows[free1] = row;
  } else if (num_empty2 > 0) {
    bucket2->hashes[free2] = key2;
    bucket2->rows[free2] = row;
  } else [[unlikely]] {
    FATAL("ResourceUUIDMap: Failed to insert UUID (%llu %llu). Both buckets full.",
        uuid[0], uuid[1]);
  }
}

void ResourceUUIDMap::remove(UUID uuid)
{
  auto [key1, key2, bucket1, bucket2] = hash(uuid);

  i32 num_found = 0;
  BRT_UNROLL
  for (i32 i = 0; i < BUCKET_SIZE; i++) {
    u64 hash1 = bucket1->hashes[i];
    u64 hash2 = bucket2->hashes[i];

    if (hash1 == key1) {
      bucket1->hashes[i] = 0;
      num_found += 1;
    }
    
    if (hash2 == key2) {
      bucket2->hashes[i] = 0;
      num_found += 1;
    }
  }

  if (num_found != 1) [[unlikely]] {
    FATAL("ResourceUUIDMap: Failed to remove UUID (%llu %llu). Removed %d entries.",
        uuid[0], uuid[1], num_found);
  }
}

ResourceUUIDMap::Hash ResourceUUIDMap::hash(UUID uuid)
{
  u64 key1 = uuid[0];
  u64 key2 = uuid[1];

  u64 bucket_idx1 = key1 % NUM_BUCKETS;
  u64 bucket_idx2 = key2 % NUM_BUCKETS;

  return {
    .key1 = key1,
    .key2 = key2,
    .bucket1 = &buckets_[bucket_idx1],
    .bucket2 = &buckets_[bucket_idx2],
  };
}

ErrorStatus GPUDevice::currentErrorStatus()
{
  auto *backend_common = static_cast<BackendCommon *>(this);
  AtomicU32Ref err_atomic(backend_common->errorStatus);
  return (ErrorStatus)err_atomic.load<sync::relaxed>();
}

FrontendCommands * GPUDevice::allocCommandBlock()
{
  auto cmds = (FrontendCommands *)malloc(sizeof(FrontendCommands));
  cmds->next = nullptr;

  return cmds;
}

void GPUDevice::deallocCommandBlocks(FrontendCommands *cmds)
{
  while (cmds != nullptr) {
    FrontendCommands *next = cmds->next;
    free(cmds);
    cmds = next;
  }
}

BackendCommon::BackendCommon(bool errors_are_fatal)
  : GPUDevice(),
    paramBlockTypeIDs(),
    rasterPassInterfaceIDs(),
    errorStatus((u32)ErrorStatus::None),
    errorsAreFatal(errors_are_fatal)
{}

void BackendCommon::reportError(ErrorStatus error)
{
  AtomicU32Ref err_atomic(errorStatus);
  err_atomic.fetch_or<sync::relaxed>(static_cast<u32>(error));

  if (errorsAreFatal) {
    const char *err_str = nullptr;
    switch (error) {
      case ErrorStatus::TableFull: {
        err_str = "Resource table is full";
      } break;
      case ErrorStatus::OutOfMemory: {
        err_str = "Out of GPU memory";
      } break;
      default: {
        err_str = "Unknown error!";
      } break;
    }

    FATAL("GAS runtime error encountered: %s", err_str);
  }
}

}
