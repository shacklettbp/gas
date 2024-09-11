set(GAS_USE_SDL FALSE)
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(GAS_USE_SDL TRUE)
endif()
set(GAS_USE_SDL TRUE) # FIXME

set(GAS_USE_SDL "${GAS_USE_SDL}" PARENT_SCOPE)

if (GAS_USE_SDL)
  include(setup-sdl.cmake)
  set(SDL_BUNDLED_DIR "${SDL_BUNDLED_DIR}" PARENT_SCOPE)
endif()


set(GAS_USE_DAWN TRUE)

set(GAS_USE_DAWN "${GAS_USE_DAWN}" PARENT_SCOPE)

if (GAS_USE_DAWN)
  include(setup-dawn.cmake)
  set(DAWN_BUNDLED_DIR "${DAWN_BUNDLED_DIR}" PARENT_SCOPE)
endif()

include(setup-slang.cmake)
set(SLANG_BUNDLED_DIR "${SLANG_BUNDLED_DIR}" PARENT_SCOPE)
