#pragma once

#include "gas_ui.hpp"
#include <imgui.h>

namespace gas {
namespace ImGuiSystem {

void init(WindowManager &wm,
          GPURuntime *gpu,
          GPUQueue tx_queue,
          ShaderCompiler *shaderc,
          TextureFormat attachment_fmt);
void shutdown(GPURuntime *gpu);

void reloadAssets(GPURuntime *gpu,
                  GPUQueue gpu_queue,
                  CommandEncoder &enc);

void startFrame(GPURuntime *gpu);

}
}
