#include "gas_input.hpp"

namespace gas {

using namespace brt;

void UserInputEvents::merge(const UserInputEvents &o)
{
  for (i32 i = 0; i < (i32)events_.size(); i++) {
    events_[i] |= o.events_[i];
  }

  mouse_scroll_ += o.mouse_scroll_;
}

void UserInputEvents::clear()
{
  zeroN<u32>(events_.data(), events_.size());
  mouse_delta_ = { 0, 0 };
  mouse_scroll_ = { 0, 0 };
}


}
