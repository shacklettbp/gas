#pragma once
#include <gas/gas.hpp>

namespace gas {

enum class ImGuiShaderID : brt::u32 {
  Render = 0,
};

struct ImGuiShaders : gas::CompiledShadersBlob {
  inline gas::ShaderByteCode getByteCode(ImGuiShaderID id) const
  {
    return gas::CompiledShadersBlob::getByteCode((brt::u32)id);
  }
};

}
