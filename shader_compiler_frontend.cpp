#include "shader_compiler.hpp"

#include <brt/err.hpp>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

using namespace brt;
using namespace gas;

static constexpr std::array<ShaderByteCodeType, 4> all_bytecode_types = {
  ShaderByteCodeType::SPIRV,
  ShaderByteCodeType::WGSL,
  ShaderByteCodeType::MTLLib,
  ShaderByteCodeType::DXIL,
};

static void compile(char *out_filename,
                    char *dep_out_filename,
                    char *src_filename,
                    Span<char *> arg_strs)
{
  (void)arg_strs;

  std::ofstream out(out_filename, std::ios::binary);
  if (!out.is_open()) {
    fprintf(stderr, "Failed to open output file: %s\n", out_filename);
    exit(EXIT_FAILURE);
  }

  std::ofstream dep_out(dep_out_filename, std::ios::binary);
  if (!dep_out.is_open()) {
    fprintf(stderr, "Failed to open dependency output file: %s\n", dep_out_filename);
    exit(EXIT_FAILURE);
  }

  StackAlloc alloc;

  ShaderCompiler *compiler = gasCreateShaderCompiler();

  ShaderCompileArgs args = {
    .path = src_filename,
  };

  ShaderCompileResult result = compiler->compileShader(alloc, args);

  if (!result.success) {
    fprintf(stderr, "Shader compilation failed!\n%.*s",
      (int)result.diagnostics.size(), result.diagnostics.data());
    exit(EXIT_FAILURE);
  }

  dep_out << out_filename << ": \\\n";
  for (i32 i = 0; i < result.dependencies.size(); i++) {
    dep_out << "    " << result.dependencies[i];
    dep_out << "\\\n";
  }

  for (ShaderByteCodeType bytecode_type : all_bytecode_types) {
    ShaderByteCode bytecode = result.getByteCodeForBackend(bytecode_type);
    chk(bytecode.numBytes <= (u32)-1);
    u32 num_bytes = (u32)bytecode.numBytes;
    out.write((char *)&num_bytes, sizeof(u32));
    out.write((char *)bytecode.data, bytecode.numBytes);
  }

  gasDestroyShaderCompiler(compiler);
}

static void link(char *shader_enum,
                 char *shader_class,
                 char *cpp_namespace,
                 char *compiled_out_base_dir,
                 char *compiled_out_name,
                 char *reload_info_filename,
                 char *hpp_out_filename,
                 char *metadata_filename)
{
  std::ifstream metadata_in(metadata_filename);
  if (!metadata_in.is_open()) {
    fprintf(stderr, "Failed to open metadata input file: %s\n", metadata_filename);
    exit(EXIT_FAILURE);
  }

  StackAlloc alloc {};

  std::ofstream spirv_out(
      std::filesystem::path(compiled_out_base_dir) / "spirv" / compiled_out_name, std::ios::binary);
  std::ofstream wgsl_out(
      std::filesystem::path(compiled_out_base_dir) / "wgsl" / compiled_out_name, std::ios::binary);
  std::ofstream mtl_out(
      std::filesystem::path(compiled_out_base_dir) / "mtl" / compiled_out_name, std::ios::binary);
  std::ofstream dxil_out(
      std::filesystem::path(compiled_out_base_dir) / "dxil" / compiled_out_name, std::ios::binary);

  std::ofstream reload_info_out(reload_info_filename, std::ios::binary);
  if (!reload_info_out.is_open()) {
    fprintf(stderr, "Failed to open reload info output file: %s\n", reload_info_filename);
    exit(EXIT_FAILURE);
  }

  const char **shader_names= nullptr;
  const char **shader_paths = nullptr;
  const char **object_paths = nullptr;
  const char **args = nullptr;
  u32 num_shaders = 0;
  u32 num_args = 0;

  { 
    std::string line;

    // Read number of shaders
    metadata_in >> num_shaders;
    metadata_in.ignore(); // Skip newline

    shader_names = alloc.allocN<const char *>(num_shaders);
    for (u32 i = 0; i < num_shaders; i++) {
      std::getline(metadata_in, line);
      char *name = alloc.allocN<char>(line.length() + 1);
      memcpy(name, line.c_str(), line.length() + 1);
      shader_names[i] = name;
    }

    shader_paths = alloc.allocN<const char *>(num_shaders);
    for (u32 i = 0; i < num_shaders; i++) {
      std::getline(metadata_in, line);
      char *path = alloc.allocN<char>(line.length() + 1);
      memcpy(path, line.c_str(), line.length() + 1);
      shader_paths[i] = path;
    }

    object_paths = alloc.allocN<const char *>(num_shaders);
    for (u32 i = 0; i < num_shaders; i++) {
      std::getline(metadata_in, line);
      char *path = alloc.allocN<char>(line.length() + 1);
      memcpy(path, line.c_str(), line.length() + 1);
      object_paths[i] = path;
    }

    // Process ARGS section
    metadata_in >> num_args;
    metadata_in.ignore(); // Skip newline

    args = alloc.allocN<const char *>(num_args);
    for (u32 i = 0; i < num_args; i++) {
      std::getline(metadata_in, line);
      char *arg = alloc.allocN<char>(line.length() + 1);
      memcpy(arg, line.c_str(), line.length() + 1);
      args[i] = arg;
    }
  }

  struct ShaderInfo {
    char *bytecode;
    u32 numBytes;
    u32 offset;
  };

  Span<ShaderInfo> spirv_bytecodes = { alloc.allocN<ShaderInfo>(num_shaders), num_shaders };
  Span<ShaderInfo> wgsl_bytecodes = { alloc.allocN<ShaderInfo>(num_shaders), num_shaders };
  Span<ShaderInfo> mtl_bytecodes = { alloc.allocN<ShaderInfo>(num_shaders), num_shaders };
  Span<ShaderInfo> dxil_bytecodes = { alloc.allocN<ShaderInfo>(num_shaders), num_shaders };

  u32 spirv_offset = 0;
  u32 wgsl_offset = 0;
  u32 mtl_offset = 0;
  u32 dxil_offset = 0;

  for (u32 i = 0; i < num_shaders; i++) {
    std::ifstream object_in(object_paths[i], std::ios::binary);
    if (!object_in.is_open()) {
      fprintf(stderr, "Failed to open object input file: %s\n", object_paths[i]);
      exit(EXIT_FAILURE);
    }

    u32 spirv_size;
    object_in.read((char *)&spirv_size, sizeof(u32));
    spirv_bytecodes[i] = {
      .bytecode = alloc.allocN<char>(spirv_size),
      .numBytes = spirv_size,
      .offset = spirv_offset
    };
    object_in.read(spirv_bytecodes[i].bytecode, spirv_size);
    spirv_offset += spirv_size;

    u32 wgsl_size;
    object_in.read((char *)&wgsl_size, sizeof(u32));
    wgsl_bytecodes[i] = {
      .bytecode = alloc.allocN<char>(wgsl_size),
      .numBytes = wgsl_size,
      .offset = wgsl_offset
    };
    object_in.read(wgsl_bytecodes[i].bytecode, wgsl_size);
    wgsl_offset += wgsl_size;

    u32 mtl_size;
    object_in.read((char *)&mtl_size, sizeof(u32));
    mtl_bytecodes[i] = {
      .bytecode = alloc.allocN<char>(mtl_size),
      .numBytes = mtl_size,
      .offset = mtl_offset
    };
    object_in.read(mtl_bytecodes[i].bytecode, mtl_size);
    mtl_offset += mtl_size;

    u32 dxil_size;
    object_in.read((char *)&dxil_size, sizeof(u32));
    dxil_bytecodes[i] = {
      .bytecode = alloc.allocN<char>(dxil_size),
      .numBytes = dxil_size,
      .offset = dxil_offset
    };
    object_in.read(dxil_bytecodes[i].bytecode, dxil_size);
    dxil_offset += dxil_size;
  }
  
  // Write number of shaders
  spirv_out.write((char *)&num_shaders, sizeof(u32));
  wgsl_out.write((char *)&num_shaders, sizeof(u32));
  mtl_out.write((char *)&num_shaders, sizeof(u32));
  dxil_out.write((char *)&num_shaders, sizeof(u32));

  // Write offset/size pairs for each shader
  for (u32 i = 0; i < num_shaders; i++) {
    spirv_out.write((char *)&spirv_bytecodes[i].offset, sizeof(u32));
    spirv_out.write((char *)&spirv_bytecodes[i].numBytes, sizeof(u32));

    wgsl_out.write((char *)&wgsl_bytecodes[i].offset, sizeof(u32));
    wgsl_out.write((char *)&wgsl_bytecodes[i].numBytes, sizeof(u32));

    mtl_out.write((char *)&mtl_bytecodes[i].offset, sizeof(u32));
    mtl_out.write((char *)&mtl_bytecodes[i].numBytes, sizeof(u32));

    dxil_out.write((char *)&dxil_bytecodes[i].offset, sizeof(u32));
    dxil_out.write((char *)&dxil_bytecodes[i].numBytes, sizeof(u32));
  }

  // Write all bytecode data
  for (u32 i = 0; i < num_shaders; i++) {
    spirv_out.write(spirv_bytecodes[i].bytecode, spirv_bytecodes[i].numBytes);
    wgsl_out.write(wgsl_bytecodes[i].bytecode, wgsl_bytecodes[i].numBytes);
    mtl_out.write(mtl_bytecodes[i].bytecode, mtl_bytecodes[i].numBytes);
    dxil_out.write(dxil_bytecodes[i].bytecode, dxil_bytecodes[i].numBytes);
  }

  // Write reload info file
  reload_info_out.write((char *)&num_args, sizeof(u32));
  for (u32 i = 0; i < num_args; i++) {
    reload_info_out.write(args[i], strlen(args[i]) + 1);
  }

  reload_info_out.write((char *)&num_shaders, sizeof(u32));
  
  // Calculate and write offsets for each shader source path
  u32 current_offset = 0;
  for (u32 i = 0; i < num_shaders; i++) {
    reload_info_out.write((char *)&current_offset, sizeof(u32));
    current_offset += strlen(shader_paths[i]) + 1;
  }

  // Write the shader source paths
  for (u32 i = 0; i < num_shaders; i++) {
    reload_info_out.write(shader_paths[i], strlen(shader_paths[i]) + 1);
  }

  // Write header file for C++ interface.
  // We write to a string first and then check if the file has changed to
  // avoid unnecessary C++ rebuilds
  //
  std::stringstream hpp_out_ss;

  hpp_out_ss << "#pragma once\n";
  hpp_out_ss << "#include <gas/gas.hpp>\n\n";

  hpp_out_ss << "namespace " << cpp_namespace << " {\n\n";
  hpp_out_ss << "enum class " << shader_enum << " : brt::u32 {\n";
  for (u32 i = 0; i < num_shaders; i++) {
    hpp_out_ss << "  " << shader_names[i] << " = " << i << ",\n";
  }
  hpp_out_ss << "};\n\n";

  hpp_out_ss << "struct " << shader_class << " : gas::CompiledShadersBlob {\n";
  hpp_out_ss << "  inline gas::ShaderByteCode getByteCode(" << shader_enum << " id) const\n";
  hpp_out_ss << "  {\n";
  hpp_out_ss << "    return gas::CompiledShadersBlob::getByteCode((brt::u32)id);\n";
  hpp_out_ss << "  }\n";
  hpp_out_ss << "};\n\n";
  hpp_out_ss << "}\n";

  std::string new_hpp_out_contents = hpp_out_ss.str();

  bool should_write_hpp = false;
  {
    std::ifstream hpp_in(hpp_out_filename, std::ios::binary);
    if (!hpp_in.is_open()) {
      should_write_hpp = true;
    } else {
      hpp_in.seekg(0, std::ios::end);
      u64 num_bytes = hpp_in.tellg();
      hpp_in.seekg(0, std::ios::beg);

      std::vector<char> existing_header(num_bytes + 1);
      hpp_in.read(existing_header.data(), num_bytes);
      existing_header[num_bytes] = 0;

      if (strcmp(new_hpp_out_contents.c_str(), existing_header.data())) {
        should_write_hpp = true;
      }
    }
  }

  if (should_write_hpp) {
    std::ofstream hpp_out(hpp_out_filename, std::ios::binary);
    if (!hpp_out.is_open()) {
      fprintf(stderr, "Failed to open hpp output file: %s\n", hpp_out_filename);
      exit(EXIT_FAILURE);
    }

    hpp_out.write(new_hpp_out_contents.data(), new_hpp_out_contents.size());
  }
}

int main(int argc, char *argv[])
{
  auto usage_err = [argv]() {
    fprintf(stderr, "Usage: %s MODE ARGS...\n", argv[0]);
    exit(EXIT_FAILURE);
  };

  if (argc < 2) {
    usage_err();
  }

  i32 arg_offset = 1;

  char *mode = argv[arg_offset++];

  if (!strcmp(mode, "compile")) {
    auto compile_usage_err = [argv]() {
      fprintf(stderr, "Usage: %s compile OUTPUT DEP_OUT INPUT [ARGS]\n", argv[0]);
      exit(EXIT_FAILURE);
    };

    if (argc < 5) {
      compile_usage_err();
    }

    char *output = argv[arg_offset++];
    char *dep_out = argv[arg_offset++];
    char *input = argv[arg_offset++];

    Span<char *> arg_strs(argv + arg_offset, argc - arg_offset);
    compile(output, dep_out, input, arg_strs);
  } else if (!strcmp(mode, "link")) {
    auto link_usage_err = [argv]() {
      fprintf(stderr, "Usage: %s link SHADER_ENUM SHADER_CLASS CPP_NAMESPACE COMPILED_OUT_PREFIX RELOAD_INFO_OUT HPP_OUT METADATA_FILE\n", argv[0]);
      exit(EXIT_FAILURE);
    };

    if (argc < 10) {
      link_usage_err();
    }

    char *shader_enum = argv[arg_offset++];
    char *shader_class = argv[arg_offset++];
    char *cpp_namespace = argv[arg_offset++];
    char *compiled_out_base_dir = argv[arg_offset++];
    char *compiled_out_name = argv[arg_offset++];
    char *reload_info_out = argv[arg_offset++];
    char *hpp_out = argv[arg_offset++];
    char *metadata_file = argv[arg_offset++];

    link(shader_enum, shader_class, cpp_namespace, compiled_out_base_dir, compiled_out_name, reload_info_out, hpp_out, metadata_file);
  } else {
    fprintf(stderr, "%s: unknown mode: %s\n", argv[0], mode);
    exit(EXIT_FAILURE);
  }

  return 0;
}
