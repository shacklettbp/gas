#pragma once

#include "gas.hpp"
#include "gas_input.hpp"

namespace gas {

enum class WindowState : u32 {
  None        = 0,
  ShouldClose = 1 << 0,
  IsFocused   = 1 << 1,
};

enum class WindowInitFlags : u32 {
  None       = 0,
  Resizable  = 1 << 0,
  Fullscreen = 1 << 1,
};

struct Window {
  i32 pixelWidth;
  i32 pixelHeight;
  f32 systemUIScale;

  WindowState state;

  Surface surface;
};

class UISystem {
public:
  struct Config {
    bool enableValidation = false;
    bool debugPipelineCompilation = false;
    bool errorsAreFatal = false;
    GPUAPISelect desiredGPULib = GPUAPISelect::None;
  };

  static UISystem * init(const Config &cfg);
  void shutdown();

  Window * createWindow(const char *title,
                        i32 starting_pixel_width,
                        i32 starting_pixel_height,
                        WindowInitFlags flags = WindowInitFlags::None);

  Window * createMainWindow(const char *title,
                            i32 starting_pixel_width,
                            i32 starting_pixel_height,
                            WindowInitFlags flags = WindowInitFlags::None);

  void destroyWindow(Window *window);
  void destroyMainWindow();

  Window * getMainWindow();

  void enableRawMouseInput(Window *window);
  void disableRawMouseInput(Window *window);

  void beginTextEntry(Window *window, brt::Vector2 pos, f32 line_height);
  void endTextEntry(Window *window);

  bool processEvents();

  UserInput & inputState();
  UserInputEvents & inputEvents();
  const char * inputText();

  GPULib * gpuLib();
};

inline WindowState & operator|=(WindowState &a, WindowState b);
inline WindowState operator|(WindowState a, WindowState b);
inline WindowState & operator&=(WindowState &a, WindowState b);
inline WindowState operator&(WindowState a, WindowState b);

inline WindowInitFlags & operator|=(WindowInitFlags &a, WindowInitFlags b);
inline WindowInitFlags operator|(WindowInitFlags a, WindowInitFlags b);
inline WindowInitFlags & operator&=(WindowInitFlags &a, WindowInitFlags b);
inline WindowInitFlags operator&(WindowInitFlags a, WindowInitFlags b);

}

#include "gas_ui.inl"
