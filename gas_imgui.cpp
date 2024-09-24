#include "gas_imgui.hpp"
#include "shader_compiler.hpp"

#include <madrona/crash.hpp>

namespace gas {
namespace {

struct VertexTransform {
  Vector2 scale;
  Vector2 translation;
};

struct ImGuiBackend {
  ParamBlockType paramBlockType;
  RasterShader shader;
  Sampler fontSampler;
  Texture fontsTexture;
  ParamBlock fontsParamBlock;

  i32 fbWidth;
  i32 fbHeight;
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
    .numPerDrawBytes = sizeof(VertexTransform),
    .vertexBuffers = {{ 
      .stride = sizeof(ImDrawVert), .attributes = {
        { .offset = offsetof(ImDrawVert, pos), .format = Vec2_F32 },
        { .offset = offsetof(ImDrawVert, uv),  .format = Vec2_F32 },
        { .offset = offsetof(ImDrawVert, col), .format = Vec4_UNorm8 },
      }
    }},
    .rasterConfig = {
      .depthCompare = DepthCompare::Disabled,
      .writeDepth = false,
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

  // FIXME: make this an input
  float ui_scale = 2.f;

  io.Fonts->Clear();
  ImFontConfig font_cfg;
  font_cfg.RasterizerDensity = ui_scale;
  io.Fonts->AddFontFromFileTTF(font_path, font_size, &font_cfg);

  //auto &style = ImGui::GetStyle();
  //style.ScaleAllSizes(1.f);

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

void init(UISystem *ui_sys,
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

void newFrame(UISystem *ui_sys, float ui_scale, float delta_t)
{
  float pixels_to_ui = 1.f / ui_scale;

  UserInput &cur_input = ui_sys->inputState();
  UserInputEvents &events = ui_sys->inputEvents();
  Window *window = ui_sys->getMainWindow();

  ImGuiIO &io = ImGui::GetIO();
  auto *bd = (ImGuiBackend *)io.BackendPlatformUserData;
  bd->fbWidth = window->pixelWidth;
  bd->fbHeight = window->pixelHeight;

  io.DisplaySize = ImVec2(window->pixelWidth * pixels_to_ui,
                          window->pixelHeight * pixels_to_ui);
  io.DisplayFramebufferScale = ImVec2(ui_scale, ui_scale);

  io.DeltaTime = delta_t;

  Vector2 mouse_pos = cur_input.mousePosition();

  io.AddMousePosEvent(mouse_pos.x * pixels_to_ui,
                      mouse_pos.y * pixels_to_ui);

  for (int i = 0; i < 5; i++) {
    InputID id = InputID((u32)InputID::MouseLeft + (u32)i);

    if (cur_input.isDown(id)) {
      if (events.upEvent(id)) {
        io.AddMouseButtonEvent(i, false);
        io.AddMouseButtonEvent(i, true);
      } else if (events.downEvent(id)) {
        io.AddMouseButtonEvent(i, true);
      }
    } else {
      if (events.downEvent(id)) {
        io.AddMouseButtonEvent(i, true);
        io.AddMouseButtonEvent(i, false);
      } else if (events.upEvent(id)) {
        io.AddMouseButtonEvent(i, false);
      }
    }
  }

  io.AddFocusEvent((window->state & WindowState::IsFocused) != 
                   WindowState::None);

  ImGui::NewFrame();
}

void render(RasterPassEncoder &enc)
{
  ImGuiIO &io = ImGui::GetIO();
  auto *bd = (ImGuiBackend *)io.BackendPlatformUserData;

  ImGui::Render();
  ImDrawData *draw_data = ImGui::GetDrawData();

  MappedTmpBuffer tmp_vertices = enc.tmpBuffer(
    sizeof(ImDrawVert) * draw_data->TotalVtxCount + sizeof(ImDrawVert) - 1);

  MappedTmpBuffer tmp_indices = enc.tmpBuffer(
    sizeof(ImDrawIdx) * draw_data->TotalIdxCount);

  u32 base_draw_vert_offset = utils::divideRoundUp(
      tmp_vertices.offset, (u32)sizeof(ImDrawVert));
  u32 base_draw_idx_offset = tmp_indices.offset / sizeof(ImDrawIdx);

  ImDrawVert *tmp_verts_out;
  {
    u32 alignment_offset =
        base_draw_vert_offset * sizeof(ImDrawVert) - tmp_vertices.offset;

    tmp_verts_out = (ImDrawVert *)(tmp_vertices.ptr + alignment_offset);
  }

  ImDrawIdx *tmp_idxs_out = (ImDrawIdx *)tmp_indices.ptr;

  enc.setShader(bd->shader);
  enc.setParamBlock(0, bd->fontsParamBlock);
  enc.setVertexBuffer(0, tmp_vertices.buffer);
  enc.setIndexBufferU16(tmp_indices.buffer);

  Vector2 clip_offset {
    draw_data->DisplayPos.x,
    draw_data->DisplayPos.y,
  };

  Vector2 clip_scale {
    draw_data->FramebufferScale.x,
    draw_data->FramebufferScale.y,
  };

  { // Setup VertexTransform Dynamic Uniform
    Vector2 vert_scale {
      2.f / draw_data->DisplaySize.x,
      -2.f / draw_data->DisplaySize.y,
    };

    Vector2 vert_translation {
      -1.f - clip_offset.x * vert_scale.x,
      1.f + clip_offset.y * vert_scale.y,
    };

    enc.drawData(VertexTransform { vert_scale, vert_translation });
  }

  for (i32 cmd_list_idx = 0; cmd_list_idx < (i32)draw_data->CmdListsCount;
       cmd_list_idx++) {
    ImDrawList *list = draw_data->CmdLists[cmd_list_idx];

    i32 num_list_vertices = list->VtxBuffer.Size;
    ImDrawVert *vert_buffer = list->VtxBuffer.Data;
    memcpy(tmp_verts_out, vert_buffer, sizeof(ImDrawVert) * num_list_vertices);

    i32 num_list_idxs = list->IdxBuffer.Size;
    ImDrawIdx *idx_buffer = list->IdxBuffer.Data;
    memcpy(tmp_idxs_out, idx_buffer, sizeof(ImDrawIdx) * num_list_idxs);

    i32 num_cmds = list->CmdBuffer.Size;
    ImDrawCmd *cmds = list->CmdBuffer.Data;

    for (i32 i = 0; i < num_cmds; i++) {
      ImDrawCmd cmd = cmds[i];

      // Project scissor/clipping rectangles into framebuffer space
      i32 clip_min_x = std::max(
          i32(clip_scale.x * (cmd.ClipRect.x - clip_offset.x)), 0_i32);
      i32 clip_min_y = std::max(
          i32(clip_scale.y * (cmd.ClipRect.y - clip_offset.y)), 0_i32);

      i32 clip_max_x = std::min(
          i32(clip_scale.x * (cmd.ClipRect.z - clip_offset.x)), bd->fbWidth);
      i32 clip_max_y = std::min(
          i32(clip_scale.y * (cmd.ClipRect.w - clip_offset.y)), bd->fbHeight);

      if (clip_max_x <= clip_min_x || clip_max_y <= clip_min_y) {
        continue;
      }

      enc.setDrawScissors(clip_min_x, clip_min_y,
          clip_max_x - clip_min_x, clip_max_y - clip_min_y);

      enc.drawIndexed(base_draw_vert_offset + cmd.VtxOffset, 
                      base_draw_idx_offset + cmd.IdxOffset,
                      cmd.ElemCount / 3);
    }

    tmp_verts_out += num_list_vertices;
    tmp_idxs_out += num_list_idxs;
    base_draw_vert_offset += num_list_vertices;
    base_draw_idx_offset += num_list_idxs;
  }
}

}

}
