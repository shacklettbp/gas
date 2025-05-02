#pragma once

#include "gas_ui.hpp"
#include <imgui.h>

namespace gas {
namespace ImGuiSystem {

void init(UISystem *ui_sys,
          GPUDevice *gpu,
          GPUQueue tx_queue,
          ShaderCompiler *shaderc,
          RasterPassInterface raster_pass_interface,
          const char *font_path,
          float font_size);
void shutdown(GPUDevice *gpu);

void reloadFonts(GPUDevice *gpu,
                 GPUQueue gpu_queue,
                 const char *font_path,
                 float font_size);

void newFrame(UISystem *ui_sys, float ui_scale, float delta_t);
void render(RasterPassEncoder &enc);

}
}
