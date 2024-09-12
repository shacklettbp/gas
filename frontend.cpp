#include "gui.hpp"
#include "shader_compiler.hpp"

namespace gas {



}

int main(int argc, char *argv[])
{
  using namespace gas;
  using namespace gas;

  WindowManager wm = WindowManager::init(WindowManager::Config {
    .enableValidation = true,
    .runtimeErrorsAreFatal = true,
  });

  {
    bool should_exit = wm.processEvents();
    if (should_exit) {
      return 0;
    }
  }

  GPUAPI *gpu_api = wm.gpuAPI();

  Window *window = wm.createMainWindow("Labyrinth", 1920, 1080);
  
  ShaderCompilerLib shaderc_lib = gpu_api->loadShaderCompiler();
  auto backend_bytecode_type = gpu_api->backendShaderByteCodeType();

  StackAlloc shaderc_alloc;
  ShaderByteCode shader_bytecode;
  {
    ShaderCompiler *shaderc = shaderc_lib.createCompiler();

    ShaderCompileResult compile_result =
      shaderc->compileShader(shaderc_alloc, {
        .path = GAS_SRC_DIR "basic.slang",
      });

    if (compile_result.diagnostics.size() != 0) {
      fprintf(stderr, "%s", compile_result.diagnostics.data());
    }

    if (!compile_result.success) {
      FATAL("Shader compilation failed!");
    }

    shader_bytecode =
        compile_result.getByteCodeForBackend(backend_bytecode_type);

    delete shaderc;
  }

  GPURuntime *gpu = gpu_api->createRuntime(0, {window->surface});

  SwapchainProperties swapchain_properties;
  Swapchain swapchain = gpu->createSwapchain(
      window->surface, &swapchain_properties);

  Buffer uniform_buf;
  {
    Vector4 init { 1, 1, 0.5, 1 };
    uniform_buf = gpu->createBuffer({
        .numBytes = 16, 
        .usage = BufferUsage::ShaderUniform,
        .initData = { .ptr = &init },
      });
  }

  ParamBlockType global_param_blk_type = gpu->createParamBlockType({
    .uuid = "global_param_blk"_to_uuid,
    .buffers = {
      { .type = BufferBindingType::Uniform },
    },
  });

  Sampler clamp_sampler = gpu->createSampler({
    .addressMode = SamplerAddressMode::Clamp,
  });

  ParamBlock global_param_blk = gpu->createParamBlock({
    .typeID = global_param_blk_type,
    .buffers = {
      { uniform_buf },
    },
  });

  RasterPassInterface onscreen_pass_iface = gpu->createRasterPassInterface({
    .uuid = "test_pass"_to_uuid,
    .depthAttachment = { .format = TextureFormat::None },
    .colorAttachments = {
      { .format = swapchain_properties.format },
    },
  });

  RasterShader shader = gpu->createRasterShader({
    .byteCode = shader_bytecode,
    .vertexEntry = "vertMain",
    .fragmentEntry = "fragMain",
    .rasterPass = onscreen_pass_iface,
    .paramBlockTypes = { global_param_blk_type },
  });

  shaderc_alloc.release();

  RasterPass onscreen_pass = gpu->createRasterPass({
    .interface = onscreen_pass_iface,
    .colorAttachments = { swapchain.proxyAttachment() },
  });

  StackAlloc cmd_alloc;
  while (true) {
    {
      bool should_exit = wm.processEvents();
      if (should_exit || window->shouldClose) {
        break;
      }
    }

    auto [swapchain_tex, swapchain_status] =
      gpu->acquireSwapchainImage(swapchain);
    assert(swapchain_status == SwapchainStatus::Valid);

    auto cmd_alloc_frame = cmd_alloc.push();
    CommandEncoder enc = gpu->createCommandEncoder(cmd_alloc);
    {
      RasterPassEncoder raster_enc = enc.beginRasterPass(onscreen_pass);

      raster_enc.setShader(shader);
      raster_enc.setParamBlock(0, global_param_blk);
      raster_enc.draw(0, 1);

      enc.endRasterPass(raster_enc);
    }

    gpu->submit(enc);

    cmd_alloc.pop(cmd_alloc_frame);

    gpu->presentSwapchainImage(swapchain);
  }

  gpu->waitForIdle();

  gpu->destroyRasterPass(onscreen_pass);

  gpu->destroyRasterPassInterface(onscreen_pass_iface);

  gpu->destroyParamBlock(global_param_blk);

  gpu->destroySampler(clamp_sampler);

  gpu->destroyParamBlockType(global_param_blk_type);

  gpu->destroyBuffer(uniform_buf);

  gpu->destroyRasterShader(shader);

  gpu->destroySwapchain(swapchain);

  gpu_api->destroyRuntime(gpu);

  gpu_api->unloadShaderCompiler(shaderc_lib);

  wm.destroyMainWindow();
  wm.shutdown();
}
