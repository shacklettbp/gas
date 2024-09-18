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

struct UISystem::Impl {
  GPULib *gpuLib;
  GPUAPI *gpuAPI;

  PlatformWindow mainWindow;
#ifdef GAS_USE_SDL
  SDL_WindowID mainWindowID;
#endif
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

static void initUISystemAPI()
{
#ifdef GAS_USE_SDL
  REQ_SDL(SDL_Init(SDL_INIT_GAMEPAD | SDL_INIT_VIDEO));
#endif
}

static void cleanupUISystemAPI()
{
#ifdef GAS_USE_SDL
  SDL_Quit();
#endif
}

UISystem::UISystem(Impl *impl)
  : impl_(impl)
{}

UISystem UISystem::init(const Config &cfg)
{
  initUISystemAPI();

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

  return UISystem(new Impl {
    .gpuLib = gpu_lib,
    .gpuAPI = gpu_api,
    .mainWindow = {},
#ifdef GAS_USE_SDL
    .mainWindowID = {},
#endif
  });
}

void UISystem::shutdown()
{
  impl_->gpuAPI->shutdown();
  InitSystem::unloadAPILib(impl_->gpuLib);

  delete impl_;

  cleanupUISystemAPI();
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

  // FIXME
  window_out->mousePos = { -FLT_MAX, -FLT_MAX };
  window_out->leftMousePressed = false;
  window_out->rightMousePressed = false;

  window_out->shouldClose = false;
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

Window * UISystem::createWindow(const char *title,
                                i32 starting_pixel_width,
                                i32 starting_pixel_height)
{
  PlatformWindow *window = new PlatformWindow {};

  initWindow(window, impl_->gpuAPI, title,
             starting_pixel_width, starting_pixel_height);

  return window;
}

Window * UISystem::createMainWindow(const char *title,
                                    i32 starting_pixel_width,
                                    i32 starting_pixel_height)
{
  initWindow(&impl_->mainWindow, impl_->gpuAPI, title,
             starting_pixel_width, starting_pixel_height);

#ifdef GAS_USE_SDL
  impl_->mainWindowID = SDL_GetWindowID(impl_->mainWindow.os.sdl);
#endif

  return &impl_->mainWindow;
}

void UISystem::destroyWindow(Window *window)
{
  auto plat_window = static_cast<PlatformWindow *>(window);
  cleanupWindow(plat_window, impl_->gpuAPI);
  delete plat_window;
}

void UISystem::destroyMainWindow()
{
  cleanupWindow(&impl_->mainWindow, impl_->gpuAPI);
}

Window * UISystem::getMainWindow()
{
  return &impl_->mainWindow;
}

bool UISystem::processEvents()
{
  bool should_quit = false;

#ifdef GAS_USE_SDL
  auto getPlatformWindow =
    [this]
  (SDL_WindowID window_id) -> PlatformWindow *
  {
    PlatformWindow *window;
    if (window_id == impl_->mainWindowID) [[likely]] {
      window = &impl_->mainWindow;
    } else {
      SDL_Window *sdl_hdl = REQ_SDL_PTR(SDL_GetWindowFromID(window_id));
      SDL_PropertiesID window_props =
        REQ_SDL_PROP(SDL_GetWindowProperties(sdl_hdl));

      window = (PlatformWindow *)REQ_SDL_PTR(SDL_GetPointerProperty(
        window_props, "gas_hdl", nullptr));
    }

    return window;
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
      SDL_Event &event = events[i];

      switch (event.type) {
      default: break;
      case SDL_EVENT_QUIT: {
        should_quit = true;
      } break;
      case SDL_EVENT_WINDOW_CLOSE_REQUESTED: {
        SDL_WindowID window_id = event.window.windowID;

        PlatformWindow *window = getPlatformWindow(window_id);
        window->shouldClose = true;
      } break;
      }
    }
  } while (num_events == events.size());

  // FIXME:
  {
    float mouse_x, mouse_y;
    u32 button_mask = SDL_GetMouseState(&mouse_x, &mouse_y);

    impl_->mainWindow.mousePos = { mouse_x, mouse_y };
    impl_->mainWindow.leftMousePressed =
        (button_mask & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
    impl_->mainWindow.rightMousePressed =
        (button_mask & SDL_BUTTON(SDL_BUTTON_RIGHT)) != 0;
  }

#endif

  return should_quit;
}

GPUAPI * UISystem::gpuAPI()
{
  return impl_->gpuAPI;
}

}
