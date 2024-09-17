#pragma once

#include "gas_ui.hpp"
#include <imgui.h>

namespace gas {
namespace ImGuiSystem {

void init(WindowManager &wm,
          GPURuntime *gpu,
          GPUQueue tx_queue,
          ShaderCompiler *shaderc,
          RasterPassInterface raster_pass_interface,
          const char *font_path,
          float font_size);
void shutdown(GPURuntime *gpu);

void reloadFonts(GPURuntime *gpu,
                 GPUQueue gpu_queue,
                 const char *font_path,
                 float font_size);

void beginFrame(GPURuntime *gpu);
void endFrame(RasterPassEncoder &enc);

}
}
