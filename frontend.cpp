#include "gas_ui.hpp"
#include "gas_imgui.hpp"
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
  
  ShaderCompilerLib shaderc_lib = InitSystem::loadShaderCompiler();
  auto backend_bytecode_type = gpu_api->backendShaderByteCodeType();

  StackAlloc shaderc_alloc;
  ShaderByteCode shader_bytecode;
  ShaderCompiler *shaderc = shaderc_lib.createCompiler();
  {
    ShaderCompileResult compile_result =
      shaderc->compileShader(shaderc_alloc, {
        .path = GAS_SHADER_DIR "basic.slang",
      });

    if (compile_result.diagnostics.size() != 0) {
      fprintf(stderr, "%s", compile_result.diagnostics.data());
    }

    if (!compile_result.success) {
      FATAL("Shader compilation failed!");
    }

    shader_bytecode =
        compile_result.getByteCodeForBackend(backend_bytecode_type);
  }

  GPURuntime *gpu = gpu_api->createRuntime(0, {window->surface});

  SwapchainProperties swapchain_properties;
  Swapchain swapchain = gpu->createSwapchain(
      window->surface, &swapchain_properties);

  GPUQueue main_queue = gpu->getMainQueue();
  ImGuiSystem::init(wm, gpu, main_queue, shaderc, swapchain_properties.format);

  shaderc_lib.destroyCompiler(shaderc);

  Buffer uniform_buf;
  {
    Vector4 init { 1, 1, 1, 1 };
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
    .numPerDrawBytes = sizeof(Vector3),
  });

  shaderc_alloc.release();

  RasterPass onscreen_pass = gpu->createRasterPass({
    .interface = onscreen_pass_iface,
    .colorAttachments = { swapchain.proxyAttachment() },
  });

  u32 frame_num = 0;

  CommandEncoder enc = gpu->createCommandEncoder(main_queue);
  while (true) {
    {
      bool should_exit = wm.processEvents();
      if (should_exit || window->shouldClose) {
        break;
      }
    }

    gpu->waitUntilReady(main_queue);

    auto [swapchain_tex, swapchain_status] =
      gpu->acquireSwapchainImage(swapchain);
    assert(swapchain_status == SwapchainStatus::Valid);

    enc.beginEncoding();
    {
      RasterPassEncoder raster_enc = enc.beginRasterPass(onscreen_pass);

      raster_enc.setShader(shader);
      raster_enc.setParamBlock(0, global_param_blk);

      raster_enc.drawData(Vector3 { 1, 0, 0 });
      raster_enc.draw(0, 1);

      raster_enc.drawData(Vector3 { 1, sinf(math::toRadians(frame_num)), 0 });
      raster_enc.draw(0, 1);

      enc.endRasterPass(raster_enc);
    }
    enc.endEncoding();
    gpu->submit(main_queue, enc);

    gpu->presentSwapchainImage(swapchain);

    frame_num += 1;
  }

  gpu->waitUntilReady(main_queue);

  gpu->waitUntilIdle();

  gpu->destroyCommandEncoder(enc);

  gpu->destroyRasterPass(onscreen_pass);

  gpu->destroyRasterPassInterface(onscreen_pass_iface);

  gpu->destroyParamBlock(global_param_blk);

  gpu->destroySampler(clamp_sampler);

  gpu->destroyParamBlockType(global_param_blk_type);

  gpu->destroyBuffer(uniform_buf);

  gpu->destroyRasterShader(shader);

  ImGuiSystem::shutdown(gpu);

  gpu->destroySwapchain(swapchain);

  gpu_api->destroyRuntime(gpu);

  InitSystem::unloadShaderCompiler(shaderc_lib);

  wm.destroyMainWindow();
  wm.shutdown();
}
