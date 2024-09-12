#pragma once

namespace gas {

struct Win32WindowHandle {
  void *hinstance;
  void *hwnd;
};

}

extern "C" {

extern void * LoadLibraryEx(const char *, void *, uint32_t);
extern int FreeLibrary(void *);
extern void * GetProcAddress(void *, const char *);
extern uint32_t GetLastError();

constexpr inline u32 LOAD_LIBRARY_SEARCH_APPLICATION_DIR = 0x00000200;


}
