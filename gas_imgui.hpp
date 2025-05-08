#pragma once

#include "gas.hpp"
#include "gas_input.hpp"
#include <imgui.h>

namespace gas {
namespace ImGuiSystem {

struct UIControl {
  enum Type : u32 {
    None       = 0,
    EnableIME  = 1 << 0,
    DisableIME = 1 << 0,
  };

  Type type;
  brt::Vector2 pos;
  float lineHeight;
};

void init(GPUDevice *gpu,
          GPUQueue tx_queue,
          RasterPassInterface raster_pass_interface,
          const char *shader_dir,
          const char *font_path,
          float font_size);
void shutdown(GPUDevice *gpu);

void reloadFonts(GPUDevice *gpu,
                 GPUQueue gpu_queue,
                 const char *font_path,
                 float font_size);

void newFrame(UserInput &input, UserInputEvents &events,
              u32 window_width, u32 window_height,
              float ui_scale, float delta_t,
              const char *input_text,
              UIControl *out_ui_ctrl);
void render(RasterPassEncoder &enc);

}
}
