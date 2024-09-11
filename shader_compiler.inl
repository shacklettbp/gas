namespace gas {

Span<const ParamBlockTypeInit>
  ShaderParamBlockReflectionResult::getParamBlocksForBackend(
    ShaderByteCodeType bytecode_type)
{
  switch (bytecode_type) {
    case ShaderByteCodeType::SPIRV: return spirv;
    case ShaderByteCodeType::MTLLib: return mtl;
    case ShaderByteCodeType::DXIL: return dxil;
    case ShaderByteCodeType::WGSL: return wgsl;
    default: MADRONA_UNREACHABLE();
  }
}

ShaderByteCode ShaderCompileResult::getByteCodeForBackend(
    ShaderByteCodeType bytecode_type)
{
  switch (bytecode_type) {
    case ShaderByteCodeType::SPIRV: return spirv;
    case ShaderByteCodeType::MTLLib: return mtl;
    case ShaderByteCodeType::DXIL: return dxil;
    case ShaderByteCodeType::WGSL: return wgsl;
    default: MADRONA_UNREACHABLE();
  }
}

}
