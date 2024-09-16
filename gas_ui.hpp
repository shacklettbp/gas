#pragma once

#include <memory>
#include <madrona/dyn_array.hpp>
#include <madrona/optional.hpp>

#include "gas.hpp"
#include "init.hpp"

namespace gas {

struct Window {
  i32 width;
  i32 height;
  bool shouldClose;
  Surface surface;
};

class WindowManager {
public:
  struct Config {
    bool enableValidation = false;
    bool runtimeErrorsAreFatal = false;
    Optional<GPUAPISelect> desiredGPUAPI =
      Optional<GPUAPISelect>::none();
  };

  static WindowManager init(const Config &cfg);
  void shutdown();

  Window * createWindow(const char *title,
                        i32 width,
                        i32 height);

  Window * createMainWindow(const char *title,
                            i32 width,
                            i32 height);

  void destroyWindow(Window *window);
  void destroyMainWindow();

  bool processEvents();

  GPUAPI * gpuAPI();

private:
  struct Impl;
  Impl *impl_;
  inline WindowManager(Impl *impl);

  friend class WindowHandle;
};

}

#include "gas_ui.inl"
