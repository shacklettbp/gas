namespace gas {

Vector2 UserInput::mousePosition() const
{
  return mouse_pos_;
}

Vector2 UserInput::mouseDelta() const
{
  return mouse_delta_;
}

bool UserInput::isDown(InputID id) const
{
  i32 id_idx = (i32)id / 32;
  i32 id_bit = (i32)id % 32;
  return (states_[id_idx] & (1 << id_bit)) != 0;
}

bool UserInput::isUp(InputID id) const
{
  i32 id_idx = (i32)id / 32;
  i32 id_bit = (i32)id % 32;
  return (states_[id_idx] & (1 << id_bit)) == 0;
}

bool UserInputEvents::downEvent(InputID id) const
{
  i32 id_idx = (i32)id / 16;
  i32 id_bit = (i32)id % 16;
  return (events_[id_idx] & (1 << (2 * id_bit))) != 0;
}

bool UserInputEvents::upEvent(InputID id) const
{
  i32 id_idx = (i32)id / 16;
  i32 id_bit = (i32)id % 16;
  return (events_[id_idx] & (1 << (2 * id_bit + 1))) != 0;
}

inline WindowState & operator|=(WindowState &a, WindowState b)
{
    a = WindowState(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    return a;
}

inline WindowState operator|(WindowState a, WindowState b)
{
    a |= b;

    return a;
}

inline WindowState & operator&=(WindowState &a, WindowState b)
{
    a = WindowState(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
    return a;
}

inline WindowState operator&(WindowState a, WindowState b)
{
    a &= b;

    return a;
}


}
