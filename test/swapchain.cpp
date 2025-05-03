#include "gas_ui.hpp"

#include <gtest/gtest.h>

TEST(UI, ManySwapchains)
{
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

  GPULib *gpu_lib = ui_sys->gpuLib();

  constexpr i32 num_windows = 32;
  Window * windows[num_windows];
  Swapchain swapchains[num_windows];

  for (i32 i = 0; i < num_windows; i++) {
    windows[i] = ui_sys->createWindow("Labyrinth", 64, 64);
  }

  processEvents();

  GPUDevice *gpu = gpu_lib->createDevice(0, {windows[0]->surface});

  for (i32 i = 0; i < num_windows; i++) {
    SwapchainProperties swapchain_properties;
    swapchains[i] = gpu->createSwapchain(
        windows[i]->surface, { SwapchainFormat::SDR_SRGB }, &swapchain_properties);
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
    ui_sys->destroyWindow(windows[i]);
  }

  gpu_lib->destroyDevice(gpu);
  processEvents();

  ui_sys->shutdown();
}
