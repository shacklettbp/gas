#pragma once

#include "gas_ui.hpp"
#include <imgui.h>

namespace gas {
namespace ImGuiSystem {

void init(WindowManager &wm,
          GPURuntime *gpu,
          ShaderCompiler *shaderc,
          TextureFormat attachment_fmt);
void reloadAssets(GPURuntime *gpu);
void shutdown(GPURuntime *gpu);

void startFrame(GPURuntime *gpu);

}
}
