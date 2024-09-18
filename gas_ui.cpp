#include "gas_ui.hpp"
#include <madrona/crash.hpp>
#include <madrona/heap_array.hpp>

#include <cassert>

#ifdef GAS_USE_SDL
#include <SDL3/SDL.h>

#if defined(SDL_PLATFORM_MACOS)
#include <SDL3/SDL_metal.h>
#endif

#if defined(SDL_PLATFORM_LINUX)
#include "linux.hpp"
#endif

#if defined(SDL_PLATFORM_WIN32)
#include "windows.hpp"
#endif

#endif

#include "init.hpp"

#ifdef GAS_SUPPORT_WEBGPU
#include "wgpu_init.hpp"
#endif

namespace gas {

namespace {

#ifdef GAS_USE_SDL

struct WindowOSData {
  SDL_Window *sdl;
#if defined(SDL_PLATFORM_MACOS)
  SDL_MetalView metalView;
#endif
};

#endif

struct PlatformWindow : public Window {
  WindowOSData os;
};

}

struct UIBackend : public UISystem {
  GPULib *gpuLib;
  GPUAPI *gpuAPI;

  PlatformWindow mainWindow;
#ifdef GAS_USE_SDL
  SDL_WindowID mainWindowID;
#endif

  UserInput userInput;

  inline void shutdown();

  inline Window * createWindow(const char *title,
                               i32 starting_pixel_width,
                               i32 starting_pixel_height);

  inline Window * createMainWindow(const char *title,
                                   i32 starting_pixel_width,
                                   i32 starting_pixel_height);

  inline void destroyWindow(Window *window);
  inline void destroyMainWindow();

  inline bool processEvents();
};

#ifdef GAS_USE_SDL
static inline void checkSDL(bool res, const char *msg)
{
    if (!res) [[unlikely]] {
        FATAL("%s: %s\n", msg, SDL_GetError());
    }
}

static inline SDL_PropertiesID checkSDLProp(
  SDL_PropertiesID res, const char *msg)
{
    if (res == 0) [[unlikely]] {
        FATAL("%s: %s\n", msg, SDL_GetError());
    }

    return res;
}

template <typename T>
static inline T *checkSDLPointer(T *ptr, const char *msg)
{
    if (ptr == nullptr) [[unlikely]] {
        FATAL("%s: %s\n", msg, SDL_GetError());
    }

    return ptr;
}

#define REQ_SDL(expr) checkSDL((expr), #expr)
#define REQ_SDL_PROP(expr) checkSDLProp((expr), #expr)
#define REQ_SDL_PTR(expr) checkSDLPointer((expr), #expr)
#endif

static void initUIBackendAPI()
{
#ifdef GAS_USE_SDL
  REQ_SDL(SDL_Init(SDL_INIT_GAMEPAD | SDL_INIT_VIDEO));
#endif
}

static void cleanupUIBackendAPI()
{
#ifdef GAS_USE_SDL
  SDL_Quit();
#endif
}

UISystem * UISystem::init(const Config &cfg)
{
  initUIBackendAPI();

  GPUAPISelect api_select;
  if (!cfg.desiredGPUAPI.has_value()) {
    api_select = InitSystem::autoSelectAPI();
  } else {
    api_select = *cfg.desiredGPUAPI;
  }

  // FIXME:
  GPULib *gpu_lib = nullptr;

  HeapArray<const char *> api_exts(0);

  GPUAPI *gpu_api = InitSystem::initAPI(api_select, gpu_lib, {
    .enableValidation = cfg.enableValidation,
    .runtimeErrorsAreFatal = cfg.runtimeErrorsAreFatal,
    .enablePresent = true,
    .apiExtensions = api_exts,
  });

  return new UIBackend {
    .gpuLib = gpu_lib,
    .gpuAPI = gpu_api,
    .mainWindow = {},
#ifdef GAS_USE_SDL
    .mainWindowID = {},
#endif
    .userInput = {},
  };
}

void UIBackend::shutdown()
{
  gpuAPI->shutdown();
  InitSystem::unloadAPILib(gpuLib);

  delete this;

  cleanupUIBackendAPI();
}

static void initWindow(PlatformWindow *window_out,
                       GPUAPI *gpu_api,
                       const char *title,
                       i32 starting_pixel_width,
                       i32 starting_pixel_height)
{
#ifdef GAS_USE_SDL
  i32 os_width = starting_pixel_width;
  i32 os_height = starting_pixel_height;

#if defined(SDL_PLATFORM_MACOS)
  assert(os_width % 2 == 0);
  assert(os_height % 2 == 0);

  os_width = os_width / 2;
  os_height = os_height / 2;
#endif

  SDL_WindowFlags window_flags = SDL_WINDOW_HIGH_PIXEL_DENSITY;

#if defined(SDL_PLATFORM_LINUX)
  window_flags |= SDL_WINDOW_VULKAN;
#elif defined(SDL_PLATFORM_MACOS)
  window_flags |= SDL_WINDOW_METAL;
#endif

  WindowOSData os;

  SDL_Window *sdl_hdl = REQ_SDL_PTR(
    SDL_CreateWindow(title, os_width, os_height, window_flags));

  os.sdl = sdl_hdl;

  SDL_PropertiesID window_props =
    REQ_SDL_PROP(SDL_GetWindowProperties(sdl_hdl));
  
  void *os_hdl_ptr;
#if defined(SDL_PLATFORM_LINUX)
  LinuxWindowHandle linux_hdl;

  if (SDL_strcmp(SDL_GetCurrentVideoDriver(), "x11") == 0) {
    void *xdisplay = SDL_GetPointerProperty(
      window_props, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, NULL);
    uint64_t xwindow = SDL_GetNumberProperty(
      window_props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
    if (xdisplay == 0 || xwindow == 0) {
      FATAL("Failed to get X11 window properties from SDL");
    }

    linux_hdl.backend = LinuxWindowHandle::Backend::X11;
    linux_hdl.x11 = {
      .display = xdisplay,
      .window = xwindow,
    };
  } else if (SDL_strcmp(SDL_GetCurrentVideoDriver(), "wayland") == 0) {
    void *display = SDL_GetPointerProperty(
      window_props, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, NULL);
    void *surface = SDL_GetPointerProperty(
      window_props, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, NULL);
    if (!display || !surface) {
      FATAL("Failed to get X11 window properties from SDL");
    }

    linux_hdl.backend = LinuxWindowHandle::Backend::Wayland;
    linux_hdl.wayland = {
      .display = display,
      .surface = surface,
    };
  } else {
    FATAL("Unknown SDL video driver on linux");
  }

  os_hdl_ptr = &linux_hdl;
#elif defined(SDL_PLATFORM_MACOS)
  SDL_MetalView metal_view = REQ_SDL_PTR(SDL_Metal_CreateView(sdl_hdl));
  os.metalView = metal_view;

  void *ca_layer = REQ_SDL_PTR(SDL_Metal_GetLayer(metal_view));

  os_hdl_ptr = ca_layer;
#elif defined(SDL_PLATFORM_WIN32)
  void *hinstance = SDL_GetPointerProperty(SDL_GetWindowProperties(sdl_hdl),
      SDL_PROP_WINDOW_WIN32_INSTANCE_POINTER, NULL);
  void *hwnd = SDL_GetPointerProperty(SDL_GetWindowProperties(sdl_hdl),
      SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
  if (!hinstance || !hwnd) {
    FATAL("Failed to get WIN32 window handles from SDL");
  }

  Win32WindowHandle win32_hdl {
    .hinstance = hinstance,
    .hwnd = hwnd,
  };

  os_hdl_ptr = &win32_hdl;
#else
  static_assert(false, "Unimplemented");
#endif

  REQ_SDL(SDL_SetPointerProperty(window_props, "gas_hdl", window_out));

#endif

  Surface surface = gpu_api->createSurface(
      os_hdl_ptr, starting_pixel_width, starting_pixel_height);

  window_out->pixelWidth = starting_pixel_width;
  window_out->pixelHeight = starting_pixel_height;
  window_out->systemUIScale = 2.f; // FIXME 

  window_out->state = WindowState::IsFocused;

  window_out->surface = surface;
  window_out->os = os;
}

static void cleanupWindow(PlatformWindow *window,
                          GPUAPI *gpu_api)
{
  gpu_api->destroySurface(window->surface);

#ifdef GAS_USE_SDL

#if defined(SDL_PLATFORM_MACOS)
  SDL_Metal_DestroyView(window->os.metalView);
#endif

  SDL_DestroyWindow(window->os.sdl);
#endif
}

Window * UIBackend::createWindow(const char *title,
                                i32 starting_pixel_width,
                                i32 starting_pixel_height)
{
  PlatformWindow *window = new PlatformWindow {};

  initWindow(window, gpuAPI, title,
             starting_pixel_width, starting_pixel_height);

  return window;
}

Window * UIBackend::createMainWindow(const char *title,
                                    i32 starting_pixel_width,
                                    i32 starting_pixel_height)
{
  initWindow(&mainWindow, gpuAPI, title,
             starting_pixel_width, starting_pixel_height);

#ifdef GAS_USE_SDL
  mainWindowID = SDL_GetWindowID(mainWindow.os.sdl);
#endif

  return &mainWindow;
}

void UIBackend::destroyWindow(Window *window)
{
  auto plat_window = static_cast<PlatformWindow *>(window);
  cleanupWindow(plat_window, gpuAPI);
  delete plat_window;
}

void UIBackend::destroyMainWindow()
{
  cleanupWindow(&mainWindow, gpuAPI);
}

bool UIBackend::processEvents()
{
  bool should_quit = false;

  userInput.double_clicks_ = 0;
  utils::zeroN<u8>(userInput.events_.data(), userInput.events_.size());

  auto updateInputEvent =
    [this]
  (InputID id, bool down)
  {
    i32 event_idx = (i32)id / 4;
    i32 event_bit = (i32)id % 4;

    if (down) {
      userInput.events_[event_idx] |= (1 << (2 * event_bit));
    } else {
      userInput.events_[event_idx] |= (1 << (2 * event_bit + 1));
    }
  };

  auto updateInputState =
    [this]
  (InputID id, bool down)
  {
    i32 state_idx = (i32)id / 8;
    i32 state_bit = (i32)id % 8;

    if (down) {
      userInput.states_[state_idx] |= (1 << state_bit);
    } else {
      userInput.states_[state_idx] &= ~(1 << state_bit);
    }
  };

#ifdef GAS_USE_SDL
  auto getPlatformWindow =
    [this]
  (SDL_WindowID window_id) -> PlatformWindow *
  {
    if (window_id == mainWindowID) [[likely]] {
      return &mainWindow;
    } else if (window_id == 0) {
      return nullptr;
    } else {
      SDL_Window *sdl_hdl = REQ_SDL_PTR(SDL_GetWindowFromID(window_id));
      SDL_PropertiesID window_props =
        REQ_SDL_PROP(SDL_GetWindowProperties(sdl_hdl));

      return (PlatformWindow *)REQ_SDL_PTR(SDL_GetPointerProperty(
        window_props, "gas_hdl", nullptr));
    }
  };

  auto keyToInputID =
    []
  (SDL_Keycode key) -> InputID
  {
    switch (key) {
      default:          return InputID::NUM_IDS;
      case SDLK_A:      return InputID::A;
      case SDLK_B:      return InputID::B;
      case SDLK_C:      return InputID::C;
      case SDLK_D:      return InputID::D;
      case SDLK_E:      return InputID::E;
      case SDLK_F:      return InputID::F;
      case SDLK_G:      return InputID::G;
      case SDLK_H:      return InputID::H;
      case SDLK_I:      return InputID::I;
      case SDLK_J:      return InputID::J;
      case SDLK_K:      return InputID::K;
      case SDLK_L:      return InputID::L;
      case SDLK_M:      return InputID::M;
      case SDLK_N:      return InputID::N;
      case SDLK_O:      return InputID::O;
      case SDLK_P:      return InputID::P;
      case SDLK_Q:      return InputID::Q;
      case SDLK_R:      return InputID::S;
      case SDLK_S:      return InputID::S;
      case SDLK_T:      return InputID::T;
      case SDLK_U:      return InputID::U;
      case SDLK_V:      return InputID::V;
      case SDLK_W:      return InputID::W;
      case SDLK_X:      return InputID::X;
      case SDLK_Y:      return InputID::Y;
      case SDLK_Z:      return InputID::Z;
      case SDLK_0:      return InputID::K0;
      case SDLK_1:      return InputID::K1;
      case SDLK_2:      return InputID::K2;
      case SDLK_3:      return InputID::K3;
      case SDLK_4:      return InputID::K4;
      case SDLK_5:      return InputID::K5;
      case SDLK_6:      return InputID::K6;
      case SDLK_7:      return InputID::K7;
      case SDLK_8:      return InputID::K8;
      case SDLK_9:      return InputID::K9;
      case SDLK_SPACE:  return InputID::Space;
      case SDLK_LSHIFT: return InputID::Shift;
      case SDLK_RSHIFT: return InputID::Shift;
    }
  };

  static std::array<SDL_Event, 1024> events;

  SDL_PumpEvents();

  int num_events;
  do {
    num_events = SDL_PeepEvents(events.data(), events.size(),
      SDL_GETEVENT, SDL_EVENT_FIRST, SDL_EVENT_LAST);

    if (num_events < 0) [[unlikely]] {
      FATAL("Error while processing SDL events: %s\n", SDL_GetError());
    }

    for (int i = 0; i < num_events; i++) {
      SDL_Event &e = events[i];

      switch (e.type) {
        default: break;
        case SDL_EVENT_QUIT: {
          should_quit = true;
        } break;
        case SDL_EVENT_MOUSE_MOTION: {
          PlatformWindow *window = getPlatformWindow(e.motion.windowID);
          if (!window) {
            break;
          }

          userInput.mouse_pos_ = { e.motion.x, e.motion.y };
        } break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN: {
          InputID id = InputID((u32)InputID::MouseLeft + e.button.button);
          updateInputState(id, e.button.state == SDL_PRESSED);
          updateInputEvent(id, true);

          if (e.button.clicks > 1) {
            userInput.double_clicks_ |= 1 << (i32)id;
          }
        } break;
        case SDL_EVENT_MOUSE_BUTTON_UP: {
          InputID id = InputID((u32)InputID::MouseLeft + e.button.button);
          updateInputState(id, e.button.state == SDL_PRESSED);
          updateInputEvent(id, false);
        } break;
        case SDL_EVENT_KEY_DOWN: {
          InputID id = keyToInputID(e.key.key);
          if (id == InputID::NUM_IDS) {
            break;
          }
          updateInputState(id, e.button.state == SDL_PRESSED);
          updateInputEvent(id, false);
        } break;
        case SDL_EVENT_KEY_UP: {
          InputID id = keyToInputID(e.key.key);
          if (id == InputID::NUM_IDS) {
            break;
          }
          updateInputState(id, e.button.state == SDL_PRESSED);
          updateInputEvent(id, true);
        } break;
        case SDL_EVENT_WINDOW_FOCUS_GAINED: {
          PlatformWindow *window = getPlatformWindow(e.motion.windowID);
          if (!window) {
            break;
          }

          window->state |= WindowState::IsFocused;
        } break;
        case SDL_EVENT_WINDOW_FOCUS_LOST: {
          PlatformWindow *window = getPlatformWindow(e.motion.windowID);
          if (!window) {
            break;
          }

          window->state &= WindowState(~(u32)WindowState::IsFocused);
        } break;
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED: {
          PlatformWindow *window = getPlatformWindow(e.window.windowID);
          if (!window) {
            break;
          }

          window->state |= WindowState::ShouldClose;
        } break;
      }
    }
  } while (num_events == events.size());

#ifdef SDL_PLATFORM_MACOS
  // macOS reports mouse in half pixel coords for hidpi displays
  userInput.mouse_pos_ *= 2.f;
#endif

#endif

  return should_quit;
}

static inline UIBackend * backend(UISystem *base)
{
  return static_cast<UIBackend *>(base);
}

void UISystem::shutdown() { backend(this)->shutdown(); }

Window * UISystem::createWindow(const char *title,
                                i32 starting_pixel_width,
                                i32 starting_pixel_height)
{
  return backend(this)->createWindow(
      title, starting_pixel_width, starting_pixel_height);
}

Window * UISystem::createMainWindow(const char *title,
                                    i32 starting_pixel_width,
                                    i32 starting_pixel_height)
{
  return backend(this)->createMainWindow(
      title, starting_pixel_width, starting_pixel_height);
}

void UISystem::destroyWindow(Window *window)
{
  return backend(this)->destroyWindow(window);
}

void UISystem::destroyMainWindow()
{
  return backend(this)->destroyMainWindow();
}

Window * UISystem::getMainWindow()
{
  return &backend(this)->mainWindow;
}

bool UISystem::processEvents()
{
  return backend(this)->processEvents();
}

UserInput & UISystem::inputState()
{
  return backend(this)->userInput;
}

GPUAPI * UISystem::gpuAPI()
{
  return backend(this)->gpuAPI;
}

}
