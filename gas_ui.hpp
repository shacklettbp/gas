#pragma once

#include <memory>
#include <madrona/dyn_array.hpp>
#include <madrona/optional.hpp>

#include "gas.hpp"
#include "init.hpp"

namespace gas {

enum class WindowState : u32 {
  None        = 0,
  ShouldClose = 1 << 0,
  IsFocused   = 1 << 1,
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
  Shift, Space, BackSpace,
  NUM_IDS,
};

class UserInput {
public:
  inline Vector2 mousePosition() const;
  inline Vector2 mouseDelta() const;

  inline bool isDown(InputID id) const;
  inline bool isUp(InputID id) const;

private:
  static constexpr inline u32 NUM_BITFIELDS =
      utils::divideRoundUp((u32)InputID::NUM_IDS, 32_u32);

  Vector2 mouse_pos_;
  Vector2 mouse_delta_;

  std::array<u32, NUM_BITFIELDS> states_;

friend struct UIBackend;
};

class UserInputEvents {
public:
  inline bool downEvent(InputID id) const;
  inline bool upEvent(InputID id) const;

  void merge(const UserInputEvents &o);
  void clear();

private:
  static constexpr inline u32 NUM_BITFIELDS =
      2 * utils::divideRoundUp((u32)InputID::NUM_IDS, 32_u32);

  std::array<u32, NUM_BITFIELDS> events_;

friend struct UIBackend;
};

class UISystem {
public:
  struct Config {
    bool enableValidation = false;
    bool runtimeErrorsAreFatal = false;
    Optional<GPUAPISelect> desiredGPUAPI =
      Optional<GPUAPISelect>::none();
  };

  static UISystem * init(const Config &cfg);
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

  void enableRawMouseInput(Window *window);
  void disableRawMouseInput(Window *window);

  void beginTextEntry(Window *window, Vector2 pos, float line_height);
  void endTextEntry(Window *window);

  bool processEvents();

  UserInput & inputState();
  UserInputEvents & inputEvents();
  const char * inputText();

  GPUAPI * gpuAPI();
};

inline WindowState & operator|=(WindowState &a, WindowState b);
inline WindowState operator|(WindowState a, WindowState b);
inline WindowState & operator&=(WindowState &a, WindowState b);
inline WindowState operator&(WindowState a, WindowState b);

}

#include "gas_ui.inl"
