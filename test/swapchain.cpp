#include "gui.hpp"

#include <gtest/gtest.h>

TEST(GUI, ManySwapchains)
{
  using namespace gas;
  using namespace gas;

  WindowManager wm = WindowManager::init(WindowManager::Config {
    .enableValidation = true,
  });

  auto processEvents = [&wm]() {
    bool should_exit = wm.processEvents();
    EXPECT_FALSE(should_exit);
    if (should_exit) {
      return;
    }
  };

  processEvents();

  GPUAPI *gpu_api = wm.gpuAPI();

  constexpr i32 num_windows = 32;
  Window * windows[num_windows];
  Swapchain swapchains[num_windows];

  for (i32 i = 0; i < num_windows; i++) {
    windows[i] = wm.createWindow("Labyrinth", 64, 64);
  }

  processEvents();

  GPURuntime *gpu = gpu_api->createRuntime(0, {windows[0]->surface});

  for (i32 i = 0; i < num_windows; i++) {
    SwapchainProperties swapchain_properties;
    swapchains[i] = gpu->createSwapchain(
        windows[i]->surface, &swapchain_properties);
  }

  processEvents();

  for (i32 i = 0; i < num_windows; i++) {
    Swapchain swapchain = swapchains[i];
    auto [_, status] = gpu->acquireSwapchainImage(swapchain);
    EXPECT_EQ(status, SwapchainStatus::Valid);
    gpu->presentSwapchainImage(swapchain);
  }

  processEvents();

  for (i32 i = 0; i < num_windows; i++) {
    gpu->destroySwapchain(swapchains[i]);
  }

  gpu_api->destroyRuntime(gpu);

  processEvents();

  for (i32 i = 0; i < num_windows; i++) {
    wm.destroyWindow(windows[i]);
  }

  wm.shutdown();
}
