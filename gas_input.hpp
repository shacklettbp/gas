#pragma once

#include <brt/types.hpp>
#include <brt/math.hpp>
#include <brt/utils.hpp>

#include <array>

namespace gas {

enum class InputID : brt::u32 {
  MouseLeft, MouseRight, MouseMiddle, Mouse4, Mouse5,
  A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
  K1, K2, K3, K4, K5, K6, K7, K8, K9, K0,
  Shift, Space, BackSpace, Esc, Enter,
  NUM_IDS,
};

class UserInput {
public:
  inline brt::Vector2 mousePosition() const;

  inline bool isDown(InputID id) const;
  inline bool isUp(InputID id) const;

  inline void setMousePosition(brt::Vector2 p);
  inline void setDown(InputID id);
  inline void setUp(InputID id);

private:
  static constexpr inline brt::u32 NUM_BITFIELDS =
      brt::roundToAlignment((brt::u32)InputID::NUM_IDS, (brt::u32)32);

  brt::Vector2 mouse_pos_;

  std::array<brt::u32, NUM_BITFIELDS> states_;
};

class UserInputEvents {
public:
  inline bool downEvent(InputID id) const;
  inline bool upEvent(InputID id) const;

  inline brt::Vector2 mouseDelta() const;
  inline brt::Vector2 mouseScroll() const;

  inline void recordDownEvent(InputID id);
  inline void recordUpEvent(InputID id);

  inline void updateMouseDelta(brt::Vector2 d);
  inline void updateMouseScroll(brt::Vector2 d);

  void merge(const UserInputEvents &o);
  void clear();

private:
  static constexpr inline brt::u32 NUM_BITFIELDS =
      2 * brt::roundToAlignment((brt::u32)InputID::NUM_IDS, (brt::u32)32);

  std::array<brt::u32, NUM_BITFIELDS> events_;
  brt::Vector2 mouse_delta_;
  brt::Vector2 mouse_scroll_;
};

}

#include "gas_input.inl"
