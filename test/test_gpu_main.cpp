#include "test_gpu.hpp"

namespace gas::test {

GlobalGPUTestState * GlobalGPUTestState::state = nullptr;
GPULib * GPUTest::gpuLib = nullptr;
GPUDevice * GPUTest::gpu = nullptr;
ShaderCompiler * GPUTest::shaderc = nullptr;

void GPUTest::SetUpTestSuite()
{
  GlobalGPUTestState *global_state = GlobalGPUTestState::state;

  gpuLib = GPULib::init(global_state->apiSelect, APIConfig {
    .enableValidation = true,
    .errorsAreFatal = true,
  });
  gpu = gpuLib->createDevice(global_state->gpuIDX);

  shaderc = global_state->shadercLib.createCompiler();
}

void GPUTest::TearDownTestSuite()
{
  GlobalGPUTestState *global_state = GlobalGPUTestState::state;
  global_state->shadercLib.destroyCompiler(shaderc);
  gpuLib->destroyDevice(gpu);
  gpuLib->shutdown();

  shaderc = nullptr;
  gpu = nullptr;
  gpuLib= nullptr;
}

class GPUEnvironment : public ::testing::Environment {
public:
  GPUEnvironment(GPUAPISelect api_select = GPULib::autoSelectAPI(),
                 i32 gpu_idx = 0)
    : apiSelect(api_select),
      gpuIDX(gpu_idx)
  {}

  ~GPUEnvironment() override {}

  // Override this to define how to set up the environment.
  void SetUp() override
  {
    GlobalGPUTestState::state = new GlobalGPUTestState {
      .apiSelect = apiSelect,
      .gpuIDX = gpuIDX,
      .shadercLib = {},
    };
    GlobalGPUTestState::state->shadercLib.load();
  }

  // Override this to define how to tear down the environment.
  void TearDown() override 
  {
    GlobalGPUTestState *global_state = GlobalGPUTestState::state;

    global_state->shadercLib.unload();

    delete global_state;
    GlobalGPUTestState::state = nullptr;
  }

  GPUAPISelect apiSelect;
  i32 gpuIDX;
};

}

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);

  testing::AddGlobalTestEnvironment(new gas::test::GPUEnvironment {});

  return RUN_ALL_TESTS();
}

