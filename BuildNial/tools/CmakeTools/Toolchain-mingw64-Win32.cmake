# Set the system name for the target operating system
SET(CMAKE_SYSTEM_NAME "Windows")

# Compilers etc
SET(CMAKE_C_COMPILER i686-w64-mingw32-gcc)
SET(CMAKE_CXX_COMPILER i686-w64-mingw32-gcc)
SET(CMAKE_RC_COMPILER i686-w64-mingw32-windres)

SET(CMAKE_FIND_ROOT_PATH /usr/i686-w64-mingw32 /home/johng/mingw/install)

# Change default behaviour of FIND_XXX() commands
SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

