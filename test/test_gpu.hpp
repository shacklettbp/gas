#pragma once

#include "gas.hpp"
#include "shader_compiler.hpp"

#include <gtest/gtest.h>

namespace gas::test {

struct GlobalGPUTestState {
  GPUAPISelect apiSelect; 
  i32 gpuIDX;
  ShaderCompilerLib shadercLib;

  static GlobalGPUTestState *state;
};

class GPUTest : public ::testing::Test {
public:
  static GPULib * gpuLib;
  static GPUDevice * gpu;
  static ShaderCompiler * shaderc;

protected:
  static void SetUpTestSuite();
  static void TearDownTestSuite();
};

}
