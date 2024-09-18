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
  Shift, Space,
  NUM_IDS,
};

class UserInput {
public:
  inline Vector2 mousePosition() const;

  inline bool isDown(InputID id) const;
  inline bool isUp(InputID id) const;

  inline bool downEvent(InputID id) const;
  inline bool upEvent(InputID id) const;

  inline bool doubleClickEvent(InputID id) const;

private:
  static constexpr inline u32 NUM_BITFIELDS =
      utils::divideRoundUp((u32)InputID::NUM_IDS, 8_u32);

  Vector2 mouse_pos_;
  u8 double_clicks_;

  std::array<u8, NUM_BITFIELDS> states_;
  std::array<u8, NUM_BITFIELDS * 2> events_;

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

  bool processEvents();

  UserInput & inputState();

  GPUAPI * gpuAPI();
};

inline WindowState & operator|=(WindowState &a, WindowState b);
inline WindowState operator|(WindowState a, WindowState b);
inline WindowState & operator&=(WindowState &a, WindowState b);
inline WindowState operator&(WindowState a, WindowState b);

}

#include "gas_ui.inl"
