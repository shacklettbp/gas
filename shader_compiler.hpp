#pragma once

#include <madrona/macros.hpp>

#include "namespace.hpp"
#include "gas.hpp"

namespace gas {

struct ShaderMacroDefinition {
  const char *name;
  const char *value;
};

struct ShaderCompileArgs {
  const char *path;
  const char *str = nullptr; 
  Span<const char *const> includeDirs = {};
  Span<const ShaderMacroDefinition> macroDefinitions = {};

  static inline constexpr std::array<ShaderByteCodeType, 4> allTargets {
    ShaderByteCodeType::SPIRV,
    ShaderByteCodeType::MTLLib,
    ShaderByteCodeType::DXIL,
    ShaderByteCodeType::WGSL,
  };
  Span<const ShaderByteCodeType> targets = allTargets;
};

struct ShaderParamBlockReflectionResult {
  Span<const ParamBlockTypeInit> spirv;
  Span<const ParamBlockTypeInit> mtl;
  Span<const ParamBlockTypeInit> dxil;
  Span<const ParamBlockTypeInit> wgsl;

  Span<const char> diagnostics;
  bool success;

  inline Span<const ParamBlockTypeInit> getParamBlocksForBackend(
      ShaderByteCodeType bytecode_type);
};

struct ShaderCompileResult {
  ShaderByteCode spirv;
  ShaderByteCode mtl;
  ShaderByteCode dxil;
  ShaderByteCode wgsl;

  Span<const char> diagnostics;
  bool success;

  inline ShaderByteCode getByteCodeForBackend(
      ShaderByteCodeType bytecode_type);
};

class ShaderCompiler {
public:
  virtual ~ShaderCompiler() = 0;

  virtual ShaderParamBlockReflectionResult paramBlockReflection(
      StackAlloc &alloc, ShaderCompileArgs args) = 0;

  virtual ShaderCompileResult compileShader(
      StackAlloc &alloc, ShaderCompileArgs args) = 0;
};

}

extern "C" {

#ifdef gas_shader_compiler_EXPORTS
#define GAS_SHADER_COMPILER_VIS MADRONA_EXPORT
#else
#define GAS_SHADER_COMPILER_VIS MADRONA_IMPORT
#endif
GAS_SHADER_COMPILER_VIS ::gas::ShaderCompiler *
    gasCreateShaderCompiler();

GAS_SHADER_COMPILER_VIS void gasStartupShaderCompilerLib();
GAS_SHADER_COMPILER_VIS void gasShutdownShaderCompilerLib();

#undef GAS_SHADER_COMPILER_VIS

}

#include "shader_compiler.inl"
