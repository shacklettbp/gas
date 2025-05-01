#pragma once

#include <memory>

#include "gas.hpp"

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

enum class InputID : u32 {
  MouseLeft, MouseRight, MouseMiddle, Mouse4, Mouse5,
  A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
  K1, K2, K3, K4, K5, K6, K7, K8, K9, K0,
  Shift, Space, BackSpace, Esc, Enter,
  NUM_IDS,
};

class UserInput {
public:
  inline brt::Vector2 mousePosition() const;
  inline brt::Vector2 mouseDelta() const;

  inline bool isDown(InputID id) const;
  inline bool isUp(InputID id) const;

private:
  static constexpr inline u32 NUM_BITFIELDS =
      brt::roundToAlignment((u32)InputID::NUM_IDS, 32_u32);

  brt::Vector2 mouse_pos_;
  brt::Vector2 mouse_delta_;

  std::array<u32, NUM_BITFIELDS> states_;

friend struct UIBackend;
};

class UserInputEvents {
public:
  inline bool downEvent(InputID id) const;
  inline bool upEvent(InputID id) const;

  void merge(const UserInputEvents &o);
  void clear();

  inline brt::Vector2 mouseScroll() const;

private:
  static constexpr inline u32 NUM_BITFIELDS =
      2 * brt::roundToAlignment((u32)InputID::NUM_IDS, 32_u32);

  std::array<u32, NUM_BITFIELDS> events_;
  brt::Vector2 mouse_scroll_;

friend struct UIBackend;
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
