namespace gas {

Vector2 UserInput::mousePosition() const
{
  return mouse_pos_;
}

bool UserInput::isDown(InputID id) const
{
  i32 id_idx = (i32)id / 8;
  i32 id_bit = (i32)id % 8;
  return (states_[id_idx] & (1 << id_bit)) != 0;
}

bool UserInput::isUp(InputID id) const
{
  i32 id_idx = (i32)id / 8;
  i32 id_bit = (i32)id % 8;
  return (states_[id_idx] & (1 << id_bit)) == 0;
}

bool UserInput::downEvent(InputID id) const
{
  i32 id_idx = (i32)id / 4;
  i32 id_bit = (i32)id % 4;
  return (events_[id_idx] & (1 << (2 * id_bit))) != 0;
}

bool UserInput::upEvent(InputID id) const
{
  i32 id_idx = (i32)id / 4;
  i32 id_bit = (i32)id % 4;
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
