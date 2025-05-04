#pragma once

#include "namespace.hpp"
#include "gas.hpp"

#include <brt/span.hpp>
#include <brt/stack_alloc.hpp>

namespace gas {

struct ShaderMacroDefinition {
  const char *name;
  const char *value;
};

struct ShaderCompileArgs {
  const char *path;
  const char *str = nullptr; 
  brt::Span<const char *const> includeDirs = {};
  brt::Span<const ShaderMacroDefinition> macroDefinitions = {};

  static inline constexpr auto allTargets = std::to_array({
    ShaderByteCodeType::SPIRV,
    ShaderByteCodeType::WGSL,
  });
  brt::Span<const ShaderByteCodeType> targets = allTargets;
};

struct ShaderParamBlockReflectionResult {
  brt::Span<const ParamBlockTypeInit> spirv;
  brt::Span<const ParamBlockTypeInit> mtl;
  brt::Span<const ParamBlockTypeInit> dxil;
  brt::Span<const ParamBlockTypeInit> wgsl;

  brt::Span<const char> diagnostics;
  bool success;

  inline brt::Span<const ParamBlockTypeInit> getParamBlocksForBackend(
      ShaderByteCodeType bytecode_type);
};

struct ShaderCompileResult {
  ShaderByteCode spirv;
  ShaderByteCode mtl;
  ShaderByteCode dxil;
  ShaderByteCode wgsl;

  brt::Span<const char> diagnostics;
  brt::Span<const char *> dependencies;
  bool success;

  inline ShaderByteCode getByteCodeForBackend(
      ShaderByteCodeType bytecode_type);
};

class ShaderCompiler {
public:
  virtual ~ShaderCompiler() = 0;

  virtual ShaderParamBlockReflectionResult paramBlockReflection(
      brt::StackAlloc &alloc, ShaderCompileArgs args) = 0;

  virtual ShaderCompileResult compileShader(
      brt::StackAlloc &alloc, ShaderCompileArgs args) = 0;
};

}

extern "C" {

#ifdef gas_shader_compiler_EXPORTS
#define GAS_SHADER_COMPILER_VIS BRT_EXPORT
#else
#define GAS_SHADER_COMPILER_VIS BRT_IMPORT
#endif
GAS_SHADER_COMPILER_VIS ::gas::ShaderCompiler *
    gasCreateShaderCompiler();

GAS_SHADER_COMPILER_VIS void
    gasDestroyShaderCompiler(::gas::ShaderCompiler *);

GAS_SHADER_COMPILER_VIS void gasStartupShaderCompilerLib();
GAS_SHADER_COMPILER_VIS void gasShutdownShaderCompilerLib();

#undef GAS_SHADER_COMPILER_VIS

}

#include "shader_compiler.inl"
