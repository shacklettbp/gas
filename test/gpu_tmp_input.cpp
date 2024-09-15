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

  auto backend_bytecode_type = gpu_api->backendShaderByteCodeType();

  StackAlloc shaderc_alloc;
  ShaderByteCode shader_bytecode;
  {
    ShaderCompilerLib shaderc_lib = gpu_api->loadShaderCompiler();
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
    gpu_api->unloadShaderCompiler(shaderc_lib);
  }

  GPURuntime *gpu = gpu_api->createRuntime(0);
  GPUQueue main_queue = gpu->getMainQueue();

  RasterPassInterface rp_iface = gpu->createRasterPassInterface({
    .uuid = "rp"_to_uuid,
    .colorAttachments = {
      { .format = TextureFormat::RGBA8_SRGB },
    },
  });

  RasterShader shader = gpu->createRasterShader({
    .byteCode = shader_bytecode,
    .vertexEntry = "vertMain",
    .fragmentEntry = "fragMain",
    .rasterPass = { rp_iface },
    .numPerDrawBytes = sizeof(Vector3),
  });
  shaderc_alloc.release();

  Texture attachment0 = gpu->createTexture({
    .format = TextureFormat::RGBA8_SRGB,
    .width = 16,
    .height = 16,
    .usage = TextureUsage::ColorAttachment | TextureUsage::CopySrc,
  });

  Texture attachment1 = gpu->createTexture({
    .format = TextureFormat::RGBA8_SRGB,
    .width = 16,
    .height = 16,
    .usage = TextureUsage::ColorAttachment | TextureUsage::CopySrc,
  });

  RasterPass rp0 = gpu->createRasterPass({
    .interface = rp_iface,
    .colorAttachments = { attachment0 },
  });

  RasterPass rp1 = gpu->createRasterPass({
    .interface = rp_iface,
    .colorAttachments = { attachment1 },
  });

  CommandEncoder enc = gpu->createCommandEncoder(main_queue);
  for (i32 i = 0; i < 16; i++) {
    gpu_api->processGraphicsEvents();
    gpu->waitUntilReady(main_queue);

    enc.beginEncoding();

    {
      RasterPassEncoder raster_enc = enc.beginRasterPass(rp0);
      raster_enc.setShader(shader);
      raster_enc.drawData(Vector3 { 1, 1, 0 });
      raster_enc.draw(0, 1);
      enc.endRasterPass(raster_enc);
    }

    {
      RasterPassEncoder raster_enc = enc.beginRasterPass(rp1);
      raster_enc.setShader(shader);
      raster_enc.drawData(Vector3 { 1, 0, 1 });
      raster_enc.draw(0, 1);
      enc.endRasterPass(raster_enc);
    }

    enc.endEncoding();
  }

  gpu->destroyCommandEncoder(enc);

  gpu_api->processGraphicsEvents();
  gpu->waitUntilReady(main_queue);
  gpu->waitUntilIdle();

  gpu_api->destroyRuntime(gpu);
  gpu_api->shutdown();
  InitSystem::unloadAPILib(gpu_lib);
}
