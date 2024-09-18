#pragma once

#include <memory>
#include <madrona/dyn_array.hpp>
#include <madrona/optional.hpp>

#include "gas.hpp"
#include "init.hpp"

namespace gas {

struct Window {
  i32 pixelWidth;
  i32 pixelHeight;
  f32 systemUIScale;
  bool shouldClose;
  Surface surface;
};

class UISystem {
public:
  struct Config {
    bool enableValidation = false;
    bool runtimeErrorsAreFatal = false;
    Optional<GPUAPISelect> desiredGPUAPI =
      Optional<GPUAPISelect>::none();
  };

  static UISystem init(const Config &cfg);
  void shutdown();

  Window * createWindow(const char *title,
                        i32 starting_pixel_width,
                        i32 starting_pixel_height);

  Window * createMainWindow(const char *title,
                            i32 starting_pixel_width,
                            i32 starting_pixel_height);

  void destroyWindow(Window *window);
  void destroyMainWindow();

  Window * getMainWindow();

  bool processEvents();

  GPUAPI * gpuAPI();

private:
  struct Impl;
  Impl *impl_;
  inline UISystem(Impl *impl);

  friend class WindowHandle;
};

}

#include "gas_ui.inl"
