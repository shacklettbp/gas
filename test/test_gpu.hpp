#pragma once

#include "gas.hpp"
#include "init.hpp"
#include "shader_compiler.hpp"

#include <gtest/gtest.h>

namespace gas::test {

struct GlobalGPUTestState {
  GPUAPISelect apiSelect; 
  GPULib *gpuLib;
  i32 gpuIDX;
  ShaderCompilerLib shadercLib;

  static GlobalGPUTestState *state;
};

class GPUTest : public ::testing::Test {
public:
  static GPUAPI * gpuAPI;
  static GPURuntime * gpu;
  static ShaderCompiler * shaderc;

protected:
  static void SetUpTestSuite();
  static void TearDownTestSuite();
};

}
