#include "gas.hpp"
#include "init.hpp"
#include "shader_compiler.hpp"

#include <gtest/gtest.h>

TEST(GPUTmpInput, SmallTmpData)
{
  using namespace gas;

  GPUAPISelect api_select = InitSystem::autoSelectAPI();
  GPULib *gpu_lib = InitSystem::loadAPILib(api_select);
  GPUAPI *gpu_api = InitSystem::initAPI(api_select, gpu_lib, APIConfig {
    .enableValidation = true,
    .runtimeErrorsAreFatal = true,
  });

  GPURuntime *gpu = gpu_api->createRuntime(0);

  ShaderCompilerLib shaderc_lib = gpu_api->loadShaderCompiler();
  auto backend_bytecode_type = gpu_api->backendShaderByteCodeType();

  StackAlloc shaderc_alloc;
  ShaderByteCode shader_bytecode;
  {
    ShaderCompiler *shaderc = shaderc_lib.createCompiler();

    ShaderCompileResult compile_result =
      shaderc->compileShader(shaderc_alloc, {
        .path = GAS_TEST_DIR "tmp_input.slang",
      });

    if (compile_result.diagnostics.size() != 0) {
      fprintf(stderr, "%s", compile_result.diagnostics.data());
    }

    if (!compile_result.success) {
      FATAL("Shader compilation failed!");
    }

    shader_bytecode =
        compile_result.getByteCodeForBackend(backend_bytecode_type);

    shaderc_lib.destroyCompiler(shaderc);
  }

  GPUQueue main_queue = gpu->getMainQueue();

  gpu_api->processGraphicsEvents();
  gpu->waitUntilReady(main_queue);
  gpu->waitUntilIdle();

  gpu_api->unloadShaderCompiler(shaderc_lib);
  gpu_api->destroyRuntime(gpu);
  gpu_api->shutdown();
  InitSystem::unloadAPILib(gpu_lib);
}
