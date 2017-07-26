
# A generic cmake for building Q'Nial versions for Linux and Mac OSX. 
# Both 32 and 64 bit versions on Intel processors.
#

cmake_minimum_required (VERSION 2.8)
project(Nial)

include_directories("${PROJECT_BINARY_DIR}")


#--------------------  Build Options -------------------------

option(USE_INTS32 "Build a 32 bit system" OFF)
option(USE_INTS64 "Build a 64 bit system" ON)
option(USE_FASTMATH "Utilise compiler options to speed up maths" ON)  

# Include package flags 
include("NialPackages.txt" OPTIONAL)


set (OPTFLAGS "-O2")
if (USE_FASTMATH)
  set (OPTFLAGS "${OPTFLAGS} -ffast-math")
endif (USE_FASTMATH)


# ------------------- System Specific -----------------------

set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OPTFLAGS}")

# Linux specific settings
if (CMAKE_SYSTEM_NAME MATCHES "Linux")
  set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DLINUX -DUNIXSYS")
endif (CMAKE_SYSTEM_NAME MATCHES "Linux")

# Cygwin specific settings
if (CMAKE_SYSTEM_NAME MATCHES "CYGWIN")
  set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DLINUX -DUNIXSYS")
endif (CMAKE_SYSTEM_NAME MATCHES "CYGWIN")

# OSX specific flags
if (CMAKE_SYSTEM_NAME MATCHES "Darwin")
  set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DOSX -DOSX_INTEL -DUNIXSYS")
endif (CMAKE_SYSTEM_NAME MATCHES "Darwin")

# Windows specific flags
if (CMAKE_SYSTEM_NAME MATCHES "Windows")
  set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DWINDOWS -DWINNIAL")
endif (CMAKE_SYSTEM_NAME MATCHES "Windows")



# -------------------- Standard sources ---------------------

# Source files for Nial Core
set (NIAL_SRCS absmach.c
          arith.c 
          atops.c 
	  basics.c 
          blders.c
          compare.c
          eval.c
          insel.c
          lib_main.c
          linalg.c
          logicops.c
          main_stu.c
          linenoise.c
          parse.c
          picture.c
          profile.c
          scan.c
          symtab.c
          systemops.c
          trig.c
          trs.c
          unixif.c
	windowsif.c
          utils.c
          wsmanage.c       
	bitops.c
          fileio.c
	)


# ---------------- Standard Libraries --------------------

# Linux specific settings
if (CMAKE_SYSTEM_NAME MATCHES "Linux")
	set (NIAL_LIBS m util dl)
endif (CMAKE_SYSTEM_NAME MATCHES "Linux")

# Cygwin specific settings
if (CMAKE_SYSTEM_NAME MATCHES "CYGWIN")
	set (NIAL_LIBS m util dl)
endif (CMAKE_SYSTEM_NAME MATCHES "CYGWIN")

# OSX specific flags
if (CMAKE_SYSTEM_NAME MATCHES "Darwin")
	set (NIAL_LIBS m util dl)
endif (CMAKE_SYSTEM_NAME MATCHES "Darwin")

# Windows specific flags
if (CMAKE_SYSTEM_NAME MATCHES "Windows")
	set (NIAL_LIBS "")
endif (CMAKE_SYSTEM_NAME MATCHES "Windows")

# ---------------- Extension Options ---------------------

# Compilation options for 32 bit system
if (USE_INTS32)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DINTS32")
endif (USE_INTS32)

# Compilation options for 64 bit system
if (USE_INTS64)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DINTS64")
endif (USE_INTS64)


# ---------------- Core Executable -------------------------


add_executable(nialcore ${NIAL_SRCS})
target_link_libraries(nialcore ${NIAL_LIBS})



