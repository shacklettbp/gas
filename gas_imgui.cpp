#include "gas_imgui.hpp"
#include "shader_compiler.hpp"

#include <madrona/crash.hpp>

namespace gas {
namespace {

struct ImGuiBackend {
  WindowManager *wm;

  ParamBlockType paramBlockType;
  RasterPassInterface rasterPassInterface;
  RasterShader shader;
  Sampler fontSampler;
  Texture fontsTexture;
  ParamBlock fontsParamBlock;
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

void loadFonts(GPURuntime *gpu,
               GPUQueue tx_queue)
{
  ImGuiIO &io = ImGui::GetIO();
  auto *bd = (ImGuiBackend *)io.BackendPlatformUserData;

  u8 *atlas_pixels;
  int atlas_width, atlas_height;
  io.Fonts->GetTexDataAsRGBA32(&atlas_pixels, &atlas_width, &atlas_height);

  bd->fontsTexture = gpu->createTexture({
    .format = TextureFormat::RGBA8_UNorm,
    .width = u16(atlas_width),
    .height = u16(atlas_height),
    .initData = { .ptr = atlas_pixels },
  }, tx_queue);

  bd->fontsParamBlock = gpu->createParamBlock({
    .typeID = bd->paramBlockType,
    .textures = { bd->fontsTexture },
    .samplers = { bd->fontSampler },
  });

  // Store our identifier
  io.Fonts->SetTexID((ImTextureID)(u64)bd->fontsParamBlock.uint());
}

void unloadFonts(GPURuntime *gpu)
{
  ImGuiIO &io = ImGui::GetIO();
  auto *bd = (ImGuiBackend *)io.BackendPlatformUserData;

  gpu->destroyParamBlock(bd->fontsParamBlock);
  gpu->destroyTexture(bd->fontsTexture);
}

}

namespace ImGuiSystem {

void init(WindowManager &wm,
          GPURuntime *gpu,
          GPUQueue tx_queue,
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

  bd->wm = &wm;
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

  bd->shader = loadShader(
      gpu, shaderc, bd->paramBlockType, bd->rasterPassInterface);

  bd->fontSampler = gpu->createSampler({
    .addressMode = Repeat,
    .anisotropy = 1,
  });

  loadFonts(gpu, tx_queue);
}

void reloadAssets(GPURuntime *gpu, GPUQueue tx_queue)
{
  unloadFonts(gpu);
  loadFonts(gpu, tx_queue);
}

void shutdown(GPURuntime *gpu)
{
  ImGuiIO &io = ImGui::GetIO();
  auto *bd = (ImGuiBackend *)io.BackendPlatformUserData;

  unloadFonts(gpu);
  gpu->destroySampler(bd->fontSampler);
  gpu->destroyRasterShader(bd->shader);
  gpu->destroyRasterPassInterface(bd->rasterPassInterface);
  gpu->destroyParamBlockType(bd->paramBlockType);

  delete bd;
  io.BackendRendererName = nullptr;
  io.BackendPlatformName = nullptr;
  io.BackendPlatformUserData = nullptr;
}

}

}
