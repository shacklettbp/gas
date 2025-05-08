namespace gas {

brt::Vector2 UserInput::mousePosition() const
{
  return mouse_pos_;
}

bool UserInput::isDown(InputID id) const
{
  brt::i32 id_idx = (brt::i32)id / 32;
  brt::i32 id_bit = (brt::i32)id % 32;
  return (states_[id_idx] & (1 << id_bit)) != 0;
}

bool UserInput::isUp(InputID id) const
{
  brt::i32 id_idx = (brt::i32)id / 32;
  brt::i32 id_bit = (brt::i32)id % 32;
  return (states_[id_idx] & (1 << id_bit)) == 0;
}

void UserInput::setMousePosition(brt::Vector2 p)
{
  mouse_pos_ = p;
}

void UserInput::setDown(InputID id)
{
  brt::i32 state_idx = (brt::i32)id / 32;
  brt::i32 state_bit = (brt::i32)id % 32;

  states_[state_idx] |= (1 << state_bit);
}

void UserInput::setUp(InputID id)
{
  brt::i32 state_idx = (brt::i32)id / 32;
  brt::i32 state_bit = (brt::i32)id % 32;

  states_[state_idx] &= ~(1 << state_bit);
}

bool UserInputEvents::downEvent(InputID id) const
{
  brt::i32 id_idx = (brt::i32)id / 16;
  brt::i32 id_bit = (brt::i32)id % 16;
  return (events_[id_idx] & (1 << (2 * id_bit))) != 0;
}

bool UserInputEvents::upEvent(InputID id) const
{
  brt::i32 id_idx = (brt::i32)id / 16;
  brt::i32 id_bit = (brt::i32)id % 16;
  return (events_[id_idx] & (1 << (2 * id_bit + 1))) != 0;
}

brt::Vector2 UserInputEvents::mouseDelta() const
{
  return mouse_delta_;
}

brt::Vector2 UserInputEvents::mouseScroll() const
{
  return mouse_scroll_;
}

void UserInputEvents::recordDownEvent(InputID id)
{
  brt::i32 event_idx = (brt::i32)id / 16;
  brt::i32 event_bit = (brt::i32)id % 16;
  events_[event_idx] |= (1 << (2 * event_bit));
}

void UserInputEvents::recordUpEvent(InputID id)
{
  brt::i32 event_idx = (brt::i32)id / 16;
  brt::i32 event_bit = (brt::i32)id % 16;
  events_[event_idx] |= (1 << (2 * event_bit + 1));
}

void UserInputEvents::updateMouseDelta(brt::Vector2 d)
{
  mouse_delta_ += d;
}

void UserInputEvents::updateMouseScroll(brt::Vector2 d)
{
  mouse_scroll_ += d;
}

}
