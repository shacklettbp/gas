#pragma once

namespace gas {

struct LinuxWindowHandle {
  enum class Backend {
    X11,
    Wayland,
  };

  struct X11 {
    void *display;
    uint64_t window;
  };

  struct Wayland {
    void *display;
    void *surface;
  };

  Backend backend;
  union {
    X11 x11;
    Wayland wayland;
  };
};

}
