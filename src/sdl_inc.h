/* Two layouts for the same header:
   Homebrew's sdl2-config adds .../include/SDL2, so the file is <SDL.h>.
   MinGW on Windows installs it as <SDL2/SDL.h> on the normal include path. */
#if defined(__has_include)
#  if __has_include(<SDL.h>)
#    include <SDL.h>
#  elif __has_include(<SDL2/SDL.h>)
#    include <SDL2/SDL.h>
#  else
#    include <SDL.h>
#  endif
#else
#  include <SDL.h>
#endif
