#include "gas_ui.hpp"

#include <gtest/gtest.h>

TEST(UI, ManySwapchains)
{
  using namespace gas;
  using namespace gas;

  UISystem *ui_sys = UISystem::init(UISystem::Config {
    .enableValidation = true,
  });

  auto processEvents = [ui_sys]() {
    bool should_exit = ui_sys->processEvents();
    EXPECT_FALSE(should_exit);
    if (should_exit) {
      return;
    }
  };

  processEvents();

  GPUAPI *gpu_api = ui_sys->gpuAPI();

  constexpr i32 num_windows = 32;
  Window * windows[num_windows];
  Swapchain swapchains[num_windows];

  for (i32 i = 0; i < num_windows; i++) {
    windows[i] = ui_sys->createWindow("Labyrinth", 64, 64);
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
    ui_sys->destroyWindow(windows[i]);
  }

  ui_sys->shutdown();
}
