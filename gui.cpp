#include "gui.hpp"
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

struct WindowManager::Impl {
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

static void initWindowManagerAPI()
{
#ifdef GAS_USE_SDL
  REQ_SDL(SDL_Init(SDL_INIT_GAMEPAD | SDL_INIT_VIDEO));
#endif
}

static void cleanupWindowManagerAPI()
{
#ifdef GAS_USE_SDL
  SDL_Quit();
#endif
}

WindowManager::WindowManager(Impl *impl)
  : impl_(impl)
{}

WindowManager WindowManager::init(const Config &cfg)
{
  initWindowManagerAPI();

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

  return WindowManager(new Impl {
    .gpuLib = gpu_lib,
    .gpuAPI = gpu_api,
    .mainWindow = {},
#ifdef GAS_USE_SDL
    .mainWindowID = {},
#endif
  });
}

void WindowManager::shutdown()
{
  impl_->gpuAPI->shutdown();
  InitSystem::unloadAPILib(impl_->gpuLib);

  delete impl_;

  cleanupWindowManagerAPI();
}

static void initWindow(PlatformWindow *window_out,
                       GPUAPI *gpu_api,
                       const char *title,
                       i32 width,
                       i32 height)
{
#ifdef GAS_USE_SDL
#if defined(SDL_PLATFORM_MACOS)
  assert(width % 2 == 0);
  assert(height % 2 == 0);

  width = width / 2;
  height = height / 2;
#endif

  SDL_WindowFlags window_flags = SDL_WINDOW_HIGH_PIXEL_DENSITY;

#if defined(SDL_PLATFORM_LINUX)
  window_flags |= SDL_WINDOW_VULKAN;
#elif defined(SDL_PLATFORM_MACOS)
  window_flags |= SDL_WINDOW_METAL;
#endif

  WindowOSData os;

  SDL_Window *sdl_hdl = REQ_SDL_PTR(
    SDL_CreateWindow(title, width, height, window_flags));

  os.sdl = sdl_hdl;

  SDL_PropertiesID window_props =
    REQ_SDL_PROP(SDL_GetWindowProperties(sdl_hdl));
  
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

  Surface surface = gpu_api->createSurface(&linux_hdl, width, height);
#elif defined(SDL_PLATFORM_MACOS)
  SDL_MetalView metal_view = REQ_SDL_PTR(SDL_Metal_CreateView(sdl_hdl));
  os.metalView = metal_view;

  void *ca_layer = REQ_SDL_PTR(SDL_Metal_GetLayer(metal_view));

  Surface surface =
    gpu_api->createSurface(ca_layer, width * 2, height * 2);
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

  Surface surface =
    gpu_api->createSurface(&win32_hdl, width, height);
#else
  static_assert(false, "Unimplemented");
#endif

  REQ_SDL(SDL_SetPointerProperty(window_props, "gas_hdl", window_out));

#endif

  window_out->width = width;
  window_out->height = height;
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

Window * WindowManager::createWindow(const char *title,
                                     i32 width,
                                     i32 height)
{
  PlatformWindow *window = new PlatformWindow {};

  initWindow(window, impl_->gpuAPI, title, width, height);

  return window;
}

Window * WindowManager::createMainWindow(const char *title,
                                         i32 width,
                                         i32 height)
{
  initWindow(&impl_->mainWindow, impl_->gpuAPI, title, width, height);

#ifdef GAS_USE_SDL
  impl_->mainWindowID = SDL_GetWindowID(impl_->mainWindow.os.sdl);
#endif

  return &impl_->mainWindow;
}

void WindowManager::destroyWindow(Window *window)
{
  auto plat_window = static_cast<PlatformWindow *>(window);
  cleanupWindow(plat_window, impl_->gpuAPI);
  delete plat_window;
}

void WindowManager::destroyMainWindow()
{
  cleanupWindow(&impl_->mainWindow, impl_->gpuAPI);
}

bool WindowManager::processEvents()
{
  bool should_quit = false;

#ifdef GAS_USE_SDL
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

        PlatformWindow *window;
        if (window_id == impl_->mainWindowID) {
          window = &impl_->mainWindow;
        } else {
          SDL_Window *sdl_hdl = REQ_SDL_PTR(SDL_GetWindowFromID(window_id));
          SDL_PropertiesID window_props =
            REQ_SDL_PROP(SDL_GetWindowProperties(sdl_hdl));

          window = (PlatformWindow *)REQ_SDL_PTR(SDL_GetPointerProperty(
            window_props, "gas_hdl", nullptr));
        }

        window->shouldClose = true;
      } break;
      }
    }
  } while (num_events == events.size());
#endif

  impl_->gpuAPI->processGraphicsEvents();

  return should_quit;
}

GPUAPI * WindowManager::gpuAPI()
{
  return impl_->gpuAPI;
}

}
