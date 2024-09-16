#include "gas_imgui.hpp"
#include "shader_compiler.hpp"

#include <madrona/crash.hpp>

namespace gas {
namespace {

struct ImGuiBackend {
  WindowManager *wm;

  Sampler bilinearSampler;
  ParamBlockType paramBlockType;
  RasterPassInterface rasterPassInterface;
  RasterShader shader;
};

RasterShader loadShader(GPURuntime *gpu,
                        ShaderCompiler *shaderc,
                        ParamBlockType param_block_type,
                        RasterPassInterface raster_pass)
{
  StackAlloc alloc;
  ShaderCompileResult compiled_shader = shaderc->compileShader(alloc, {
    .path = GAS_SHADER_DIR "imgui.slang",
  });

  if (!compiled_shader.success) {
    FATAL("Failed to compile gas imgui shader: %s\n",
          compiled_shader.diagnostics);
  }

  using enum VertexFormat;
  return gpu->createRasterShader({
    .byteCode = compiled_shader.getByteCodeForBackend(
        gpu->backendShaderByteCodeType()),
    .vertexEntry = "vertMain",
    .fragmentEntry = "fragMain",
    .rasterPass = raster_pass,
    .paramBlockTypes = { param_block_type },
    .vertexBuffers = {{ 
      .stride = sizeof(ImDrawVert), .attributes = {
        { .offset = offsetof(ImDrawVert, pos), .format = Vec2_F32 },
        { .offset = offsetof(ImDrawVert, uv),  .format = Vec2_F32 },
        { .offset = offsetof(ImDrawVert, col), .format = Vec4_UNorm8 },
      }
    }},
    .rasterConfig = {
      .cullMode = CullMode::None,
      .blending = { BlendingConfig::additiveDefault() },
    },
  });
}

}

namespace ImGuiSystem {

void init(WindowManager &wm,
          GPURuntime *gpu,
          ShaderCompiler *shaderc,
          TextureFormat attachment_fmt)
{
  using enum ShaderStage;
  using enum SamplerAddressMode;
  using enum AttachmentLoadMode;

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();

  ImGuiIO &io = ImGui::GetIO();
  if (io.BackendPlatformUserData) {
    FATAL("ImGui already initialized");
  }

  // Setup backend capabilities flags
  ImGuiBackend *bd = new ImGuiBackend {};
  io.BackendPlatformUserData = (void *)bd;
  io.BackendPlatformName = "gas";
  io.BackendRendererName = "gas";
  io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

  bd->wm = &wm;
  bd->bilinearSampler = gpu->createSampler({
    .addressMode = Repeat,
    .anisotropy = 1,
  });

  bd->paramBlockType = gpu->createParamBlockType({
    .uuid = "imgui_param_block"_to_uuid,
    .textures = {
      { .shaderUsage = Fragment },
    },
    .samplers = {
      { .shaderUsage = Fragment },
    },
  });

  bd->rasterPassInterface = gpu->createRasterPassInterface({
    .uuid = "imgui_raster_pass"_to_uuid,
    .colorAttachments = {
      { .format = attachment_fmt, .loadMode = Load },
    },
  });

  bd->shader =
      loadShader(gpu, shaderc, bd->paramBlockType, bd->rasterPassInterface);

  ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
  platform_io.Platform_SetClipboardTextFn =
    [](ImGuiContext *, const char *text)
  {
    // Set clipboard text
    (void)text;
  };

  platform_io.Platform_GetClipboardTextFn =
    [](ImGuiContext *)
  { 
    return "";
  };
}

void shutdown(GPURuntime *gpu)
{
  ImGuiIO &io = ImGui::GetIO();
  auto *bd = (ImGuiBackend *)io.BackendPlatformUserData;

  gpu->destroyRasterShader(bd->shader);
  gpu->destroyRasterPassInterface(bd->rasterPassInterface);
  gpu->destroyParamBlockType(bd->paramBlockType);
  gpu->destroySampler(bd->bilinearSampler);

  delete bd;
  io.BackendRendererName = nullptr;
  io.BackendPlatformName = nullptr;
  io.BackendPlatformUserData = nullptr;
}

}

}
