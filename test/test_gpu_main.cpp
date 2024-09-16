#include "test_gpu.hpp"
#include "init.hpp"

namespace gas::test {

GlobalGPUTestState * GlobalGPUTestState::state = nullptr;
GPUAPI * GPUTest::gpuAPI = nullptr;
GPURuntime * GPUTest::gpu = nullptr;
ShaderCompiler * GPUTest::shaderc = nullptr;

void GPUTest::SetUpTestSuite()
{
  GlobalGPUTestState *global_state = GlobalGPUTestState::state;

  gpuAPI = InitSystem::initAPI(global_state->apiSelect, global_state->gpuLib,
                               APIConfig {
    .enableValidation = true,
    .runtimeErrorsAreFatal = true,
  });
  gpu = gpuAPI->createRuntime(global_state->gpuIDX);

  shaderc = global_state->shadercLib.createCompiler();
}

void GPUTest::TearDownTestSuite()
{
  GlobalGPUTestState *global_state = GlobalGPUTestState::state;
  global_state->shadercLib.destroyCompiler(shaderc);
  gpuAPI->destroyRuntime(gpu);
  gpuAPI->shutdown();

  shaderc = nullptr;
  gpu = nullptr;
  gpuAPI = nullptr;
}

class GPUEnvironment : public ::testing::Environment {
public:
  GPUEnvironment(GPUAPISelect api_select = InitSystem::autoSelectAPI(),
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
      .gpuLib = InitSystem::loadAPILib(apiSelect),
      .gpuIDX = gpuIDX,
      .shadercLib = InitSystem::loadShaderCompiler(),
    };
  }

  // Override this to define how to tear down the environment.
  void TearDown() override 
  {
    GlobalGPUTestState *global_state = GlobalGPUTestState::state;

    InitSystem::unloadShaderCompiler(global_state->shadercLib);
    InitSystem::unloadAPILib(global_state->gpuLib);

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

