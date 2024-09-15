#include "shader_compiler.hpp"

#include <madrona/stack_alloc.hpp>

#include <slang.h>

#ifdef GAS_SUPPORT_WEBGPU
#include "wgpu_shader_compiler.hpp"
#endif

using namespace slang;

#define REQ_SLANG(expr) \
    ::gas::checkSlang((expr), __FILE__, __LINE__,\
                                  MADRONA_COMPILER_FUNCTION_NAME)

namespace gas {

ShaderCompiler::~ShaderCompiler() = default;

namespace {

struct RequestConfig {
  int spirvIDX;
  int mtlIDX;
  int dxilIDX;
  bool wgslOut;
};

class CompilerBackend final : public ShaderCompiler {
public:
  IGlobalSession *globalSession;
  ISession *session;

  static inline CompilerBackend * init();
  ~CompilerBackend() final;
  
  ShaderParamBlockReflectionResult paramBlockReflection(
    StackAlloc &alloc, ShaderCompileArgs args) final;

  ShaderCompileResult compileShader(
    StackAlloc &alloc, ShaderCompileArgs args) final;

private:
  RequestConfig setupRequest(ICompileRequest *request,
                             const ShaderCompileArgs &args);
};

inline void checkSlang(SlangResult res,
                              const char *file,
                              int line,
                              const char *funcname)
{
  if (SLANG_SUCCEEDED(res)) {
    return;
  }

  fatal(file, line, funcname, "Slang error");
}

CompilerBackend * CompilerBackend::init()
{
  IGlobalSession *global_session;
  REQ_SLANG(createGlobalSession(&global_session));

  auto opts = std::to_array<CompilerOptionEntry>({
    { 
      .name = CompilerOptionName::VulkanUseEntryPointName,
      .value = {.intValue0 = 1 },
    },
    //{ 
    //  .name = CompilerOptionName::GLSLForceScalarLayout,
    //  .value = {.intValue0 = 1 },
    //},
  });

  SessionDesc session_desc {
    .compilerOptionEntries = opts.data(),
    .compilerOptionEntryCount = (uint32_t)opts.size(),
  };

  ISession *session;
  REQ_SLANG(global_session->createSession(session_desc, &session));

  auto out = new CompilerBackend();
  out->globalSession = global_session;
  out->session = session;

  return out;
}

CompilerBackend::~CompilerBackend()
{
  session->release();
  globalSession->release();
}

RequestConfig CompilerBackend::setupRequest(
    ICompileRequest *request, const ShaderCompileArgs &args)
{
  int spirv_tgt = -1;
  int mtl_tgt = -1;
  int dxil_tgt = -1;

  bool wgsl_out = false;
  {
    SlangProfileID sm_6_1 = globalSession->findProfile("sm_6_2");
    SlangProfileID sm_6_5 = globalSession->findProfile("sm_6_5");
    for (ShaderByteCodeType bytecode_type : args.targets) {
      switch (bytecode_type) {
        case ShaderByteCodeType::WGSL: wgsl_out = true;
        case ShaderByteCodeType::SPIRV: {
          if (spirv_tgt != -1) {
            break;
          }

          spirv_tgt = request->addCodeGenTarget(SLANG_SPIRV);
          request->setTargetProfile(spirv_tgt, sm_6_1);
          request->setTargetFlags(spirv_tgt, 
              SLANG_TARGET_FLAG_GENERATE_WHOLE_PROGRAM |
              SLANG_TARGET_FLAG_GENERATE_SPIRV_DIRECTLY);
          request->addTargetCapability(spirv_tgt, 
              globalSession->findCapability("spirv_1_3"));
        } break;
        case ShaderByteCodeType::MTLLib: {
          if (mtl_tgt != -1) {
            break;
          }

          mtl_tgt = request->addCodeGenTarget(SLANG_METAL);
          request->setTargetProfile(mtl_tgt, sm_6_1);
          request->setTargetFlags(mtl_tgt, 
              SLANG_TARGET_FLAG_GENERATE_WHOLE_PROGRAM);
          request->addTargetCapability(mtl_tgt, 
              globalSession->findCapability("metallib_2_4"));
        } break;
        case ShaderByteCodeType::DXIL: {
          if (dxil_tgt != -1) {
            break;
          }

          dxil_tgt = request->addCodeGenTarget(SLANG_DXIL);
          request->setTargetProfile(dxil_tgt, sm_6_5);
          request->setTargetFlags(dxil_tgt,
              SLANG_TARGET_FLAG_GENERATE_WHOLE_PROGRAM);
        } break;
      }
    }
  }

  for (const char *include_dir : args.includeDirs) {
    request->addSearchPath(include_dir);
  }

  for (const ShaderMacroDefinition &macro_defn : args.macroDefinitions) {
    request->addPreprocessorDefine(macro_defn.name, macro_defn.value);
  }

  int translation_unit_id = request->addTranslationUnit(
      SLANG_SOURCE_LANGUAGE_SLANG, nullptr);
    
  if (args.str) {
    request->addTranslationUnitSourceString(
        translation_unit_id, args.path, args.str);
  } else {
    request->addTranslationUnitSourceFile(
        translation_unit_id, args.path);
  } 

  return RequestConfig {
    .spirvIDX = spirv_tgt,
    .mtlIDX = mtl_tgt,
    .dxilIDX = dxil_tgt,
    .wgslOut = wgsl_out,
  };
}

Span<const char> copyOutDiagnostics(StackAlloc &alloc,
                                    const char *diagnostics)
{
  size_t diagnostics_len = strlen(diagnostics);
  if (diagnostics_len == 0) {
    return { nullptr, 0 };
  }

  size_t num_bytes = diagnostics_len + 1;
  char *diagnostics_out = alloc.allocN<char>(num_bytes);
  memcpy(diagnostics_out, diagnostics, num_bytes);
  return { diagnostics_out, (CountT)num_bytes };
}

[[maybe_unused]] const char * debugSlangParameterCategoryName(
    ParameterCategory category)
{
#define CASE_STR(e) case e: return MADRONA_STRINGIFY(e)

  switch (category) {
    CASE_STR(None);
    CASE_STR(Mixed);
    CASE_STR(ConstantBuffer);
    CASE_STR(ShaderResource);
    CASE_STR(UnorderedAccess);
    CASE_STR(VaryingInput);
    CASE_STR(VaryingOutput);
    CASE_STR(SamplerState);
    CASE_STR(Uniform);
    CASE_STR(DescriptorTableSlot);
    CASE_STR(SpecializationConstant);
    CASE_STR(PushConstantBuffer);
    CASE_STR(RegisterSpace);
    CASE_STR(GenericResource);

    CASE_STR(RayPayload);
    CASE_STR(HitAttributes);
    CASE_STR(CallablePayload);

    CASE_STR(ShaderRecord);

    CASE_STR(ExistentialTypeParam);
    CASE_STR(ExistentialObjectParam);

    CASE_STR(SubElementRegisterSpace);

    CASE_STR(InputAttachmentIndex);

    CASE_STR(MetalArgumentBufferElement);
    CASE_STR(MetalAttribute);
    CASE_STR(MetalPayload);

    default: return nullptr;
  }
#undef CASE_STR
}

#if 0
Span<const ParamBlockTypeInit> reflectParameterBlocksForTarget(
    StackAlloc &alloc, IComponentType *program, int tgt_idx)
{
  ShaderReflection *reflection = program->getLayout(tgt_idx);

  i32 param_count = (i32)reflection->getParameterCount();

  ParamBlockTypeInit *out = alloc.allocN<ParamBlockTypeInit>(param_count);

  for (i32 param_idx = 0; param_idx < param_count; param_idx++) {
    VariableLayoutReflection *param =
        reflection->getParameterByIndex(param_idx);

    size_t param_binding_idx = param->getBindingSpace() +
        param->getOffset(SLANG_PARAMETER_CATEGORY_SUB_ELEMENT_REGISTER_SPACE);
    i32 param_start_offset = param->getBindingIndex();
    assert(param_start_offset == 0);

    printf("%s: %d %d\n", param->getName(),
           param_binding_idx, param_start_offset);

    TypeLayoutReflection *param_block_type_layout = param->getTypeLayout();
    if (param_block_type_layout->getKind() !=
          TypeReflection::Kind::ParameterBlock) {
      FATAL("Top level parameters must be ParameterBlock in shader");
    }

    TypeLayoutReflection *inner_type_layout =
      param_block_type_layout->getElementTypeLayout();

    if (inner_type_layout->getKind() != TypeReflection::Kind::Struct) {
      FATAL("ParameterBlock type must be struct");
    }

    i32 num_fields = (i32)inner_type_layout->getFieldCount();
    for(i32 field_idx = 0; field_idx < num_fields; field_idx++)
    {
    	VariableLayoutReflection *field =
          inner_type_layout->getFieldByIndex(field_idx);

      ParameterCategory field_category = field->getCategory();
      if (field_category == DescriptorTableSlot) {
        TypeLayoutReflection *descriptor_type_layout =
          field->getTypeLayout()->getElementTypeLayout();
        field_category = descriptor_type_layout->getParameterCategory();
      }

      size_t field_binding_idx = field->getBindingSpace();
      if (field_binding_idx != param_binding_idx) {
        FATAL("ParameterBlock fields must not spill outside single group");
      }

      i32 field_offset = field->getBindingIndex();
      printf("  %s %s: %d\n",
        debugSlangParameterCategoryName(field_category),
        field->getName(), param_start_offset + field_offset);
    }
  }

  return { out, param_count };
}
#endif

ShaderParamBlockReflectionResult CompilerBackend::paramBlockReflection(
    StackAlloc &alloc, ShaderCompileArgs args)
{
  (void)alloc;
  (void)args;
  return {};

#if 0
  ICompileRequest *request;
  REQ_SLANG(session->createCompileRequest(&request));

  RequestConfig request_cfg = setupRequest(request, args);

  SlangResult slang_result = request->compile();
  ShaderParamBlockReflectionResult out;
  out.spirv = { nullptr, 0 };
  out.mtl = { nullptr, 0 };
  out.dxil = { nullptr, 0 };
  out.wgsl = { nullptr, 0 };

  out.diagnostics = copyOutDiagnostics(alloc, request->getDiagnosticOutput());

  if (!SLANG_SUCCEEDED(slang_result)) {
    out.success = false;
    return out;
  }
  out.success = true;

  IComponentType *program;
  REQ_SLANG(request->getProgram(&program));

  if (request_cfg.spirvIDX != -1) {
    out.spirv = reflectParameterBlocksForTarget(
        alloc, program, request_cfg.spirvIDX);
  }

  if (request_cfg.mtlIDX != -1) {
    out.mtl = reflectParameterBlocksForTarget(
        alloc, program, request_cfg.mtlIDX);
  }

  if (request_cfg.dxilIDX != -1) {
    out.dxil = reflectParameterBlocksForTarget(
        alloc, program, request_cfg.dxilIDX);
  }

  if (request_cfg.wgslOut) {
    out.wgsl = out.spirv;
  }
    
  program->release();
  request->release();

  return out;
#endif
}

ShaderCompileResult CompilerBackend::compileShader(
    StackAlloc &alloc, ShaderCompileArgs args)
{
  ICompileRequest *request;
  REQ_SLANG(session->createCompileRequest(&request));

  RequestConfig request_cfg = setupRequest(request, args);

  SlangResult slang_result = request->compile();
  ShaderCompileResult out;
  out.spirv = { nullptr, 0 };
  out.mtl = { nullptr, 0 };
  out.dxil = { nullptr, 0 };
  out.wgsl = { nullptr, 0 };
  out.diagnostics = copyOutDiagnostics(alloc, request->getDiagnosticOutput());

  if (!SLANG_SUCCEEDED(slang_result)) {
    out.success = false;
    return out;
  }
  out.success = true;

  auto copyOutTargetCode = [](
      StackAlloc &alloc,
      ICompileRequest *request,
      int tgt_idx)
    -> ShaderByteCode
  {
    IBlob *blob;
    REQ_SLANG(request->getTargetCodeBlob(tgt_idx, &blob));

    i64 num_bytes = (i64)blob->getBufferSize();
    void *dst = alloc.allocN<char>(num_bytes);
    memcpy(dst, blob->getBufferPointer(), num_bytes);
    blob->release();

    return { dst, num_bytes };
  };

  if (request_cfg.spirvIDX != -1) {
    out.spirv = copyOutTargetCode(alloc, request, request_cfg.spirvIDX);
  }

  if (request_cfg.mtlIDX != -1) {
    out.mtl = copyOutTargetCode(alloc, request, request_cfg.mtlIDX);
  }

  if (request_cfg.dxilIDX != -1) {
    out.dxil = copyOutTargetCode(alloc, request, request_cfg.dxilIDX);
  }

  if (request_cfg.wgslOut) {
    assert(request_cfg.spirvIDX != -1);

    auto alloc_wrapper = [](void *to_stack_alloc, i64 num_bytes) {
      StackAlloc &alloc = *(StackAlloc *)to_stack_alloc;
      return alloc.alloc(num_bytes, 1);
    };

    char *wgsl_out;
    i64 num_wgsl_bytes;
    char *wgsl_diagnostics;
    webgpu::TintConvertStatus status = webgpu::tintConvertSPIRVToWGSL(
        out.spirv.data, out.spirv.numBytes,
        alloc_wrapper, &alloc, 
        &wgsl_out, &num_wgsl_bytes, &wgsl_diagnostics);

    if (status != webgpu::TintConvertStatus::Success) {
      out.success = false;
      // FIXME
      fprintf(stderr, "%s\n", wgsl_diagnostics);
    } else {
      out.wgsl = { wgsl_out, num_wgsl_bytes};
    }
  }

  request->release();

  return out;
}

}
}

extern "C" {

#ifdef gas_shader_compiler_EXPORTS
#define GAS_SHADER_COMPILER_VIS MADRONA_EXPORT
#else
#define GAS_SHADER_COMPILER_VIS MADRONA_IMPORT
#endif

GAS_SHADER_COMPILER_VIS ::gas::ShaderCompiler * gasCreateShaderCompiler()
{
  return ::gas::CompilerBackend::init();
}

GAS_SHADER_COMPILER_VIS void
    gasDestroyShaderCompiler(::gas::ShaderCompiler *shaderc)
{
  delete shaderc;
}

GAS_SHADER_COMPILER_VIS void gasStartupShaderCompilerLib()
{
#ifdef GAS_SUPPORT_WEBGPU
  ::gas::webgpu::tintInit();
#endif
}

GAS_SHADER_COMPILER_VIS void gasShutdownShaderCompilerLib()
{
  slang::shutdown();

#ifdef GAS_SUPPORT_WEBGPU
  ::gas::webgpu::tintShutdown();
#endif
}

#undef GAS_SHADER_COMPILER_VIS

}
