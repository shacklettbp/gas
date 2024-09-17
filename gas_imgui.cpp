#include "gas_imgui.hpp"
#include "shader_compiler.hpp"

#include <madrona/crash.hpp>

namespace gas {
namespace {

struct ImGuiBackend {
  WindowManager *wm;

  ParamBlockType paramBlockType;
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
    .path = GAS_IMGUI_SHADER_DIR "imgui.slang",
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
               GPUQueue tx_queue,
               const char *font_path,
               float font_size)
{
  ImGuiIO &io = ImGui::GetIO();
  auto *bd = (ImGuiBackend *)io.BackendPlatformUserData;

  float scale_factor = 2.f;

  float scaled_font_size = font_size * scale_factor;
  io.Fonts->AddFontFromFileTTF(font_path, scaled_font_size);

  auto &style = ImGui::GetStyle();
  style.ScaleAllSizes(scale_factor);

  u8 *atlas_pixels;
  int atlas_width, atlas_height;
  io.Fonts->GetTexDataAsRGBA32(&atlas_pixels, &atlas_width, &atlas_height);

  bd->fontsTexture = gpu->createTexture({
    .format = TextureFormat::RGBA8_UNorm,
    .width = u16(atlas_width),
    .height = u16(atlas_height),
    .initData = { .ptr = atlas_pixels },
  }, tx_queue);

  gpu->waitUntilWorkFinished(tx_queue);

  io.Fonts->ClearTexData();

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

  io.Fonts->ClearFonts();
}

}

namespace ImGuiSystem {

void init(
    WindowManager &wm,
    GPURuntime *gpu,
    GPUQueue tx_queue,
    ShaderCompiler *shaderc,
    RasterPassInterface raster_pass_interface,
    const char *font_path,
    float font_size)
{
  using enum ShaderStage;
  using enum SamplerAddressMode;

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

  bd->shader = loadShader(
      gpu, shaderc, bd->paramBlockType, raster_pass_interface);

  bd->fontSampler = gpu->createSampler({
    .addressMode = Repeat,
    .anisotropy = 1,
  });

  loadFonts(gpu, tx_queue, font_path, font_size);
}

void shutdown(GPURuntime *gpu)
{
  ImGuiIO &io = ImGui::GetIO();
  auto *bd = (ImGuiBackend *)io.BackendPlatformUserData;

  unloadFonts(gpu);
  gpu->destroySampler(bd->fontSampler);
  gpu->destroyRasterShader(bd->shader);
  gpu->destroyParamBlockType(bd->paramBlockType);

  delete bd;
  io.BackendRendererName = nullptr;
  io.BackendPlatformName = nullptr;
  io.BackendPlatformUserData = nullptr;
}

void reloadFonts(GPURuntime *gpu,
                 GPUQueue tx_queue,
                 const char *font_path,
                 float font_size)
{
  unloadFonts(gpu);
  loadFonts(gpu, tx_queue, font_path, font_size);
}

void beginFrame(GPURuntime *gpu)
{
  (void)gpu;
  ImGui::NewFrame();
}

void endFrame(RasterPassEncoder &enc)
{
  ImGuiIO &io = ImGui::GetIO();
  auto *bd = (ImGuiBackend *)io.BackendPlatformUserData;

  ImGui::Render();
  ImDrawData *draw_data = ImGui::GetDrawData();

  struct ImDrawData
  {
    bool                Valid;              // Only valid after Render() is called and before the next NewFrame() is called.
    int                 CmdListsCount;      // Number of ImDrawList* to render (should always be == CmdLists.size)
    int                 TotalIdxCount;      // For convenience, sum of all ImDrawList's IdxBuffer.Size
    int                 TotalVtxCount;      // For convenience, sum of all ImDrawList's VtxBuffer.Size
    ImVector<ImDrawList*> CmdLists;         // Array of ImDrawList* to render. The ImDrawLists are owned by ImGuiContext and only pointed to from here.
    ImVec2              DisplayPos;         // Top-left position of the viewport to render (== top-left of the orthogonal projection matrix to use) (== GetMainViewport()->Pos for the main viewport, == (0.0) in most single-viewport applications)
    ImVec2              DisplaySize;        // Size of the viewport to render (== GetMainViewport()->Size for the main viewport, == io.DisplaySize in most single-viewport applications)
    ImVec2              FramebufferScale;   // Amount of pixels for each unit of DisplaySize. Based on io.DisplayFramebufferScale. Generally (1,1) on normal display, (2,2) on OSX with Retina display.
    ImGuiViewport*      OwnerViewport;      // Viewport carrying the ImDrawData instance, might be of use to the renderer (generally not).

  };

  MappedTmpBuffer tmp_vertices = enc.tmpBuffer(
    sizeof(ImDrawVert) * draw_data->TotalVtxCount + sizeof(ImDrawVert) - 1);

  MappedTmpBuffer tmp_indices = enc.tmpBuffer(
    sizeof(u16) * draw_data->TotalIdxCount);

  u32 cur_vert_offset = utils::divideRoundUp(
      tmp_vertices.offset, sizeof(ImDrawVert));
  u32 cur_idx_offset = 0;
  for (i32 cmd_list_idx = 0; cmd_list_idx < (i32)draw_data->CmdListsCount;
       cmd_list_idx++) {
    ImDrawList *list = draw_data->CmdLists[cmd_list_idx];
    
  }

}

}

}
