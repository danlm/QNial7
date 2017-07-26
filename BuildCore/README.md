# Building the core version of QNial7 

This directory is used to build an executable version of QNial7 that supports the core capabilities.
It is used for platforms that are not in the set of supported platforms described in the QNial7 README.

The directory BuildCore has the following structure:

Entry          | Contents                                   
-------------- | ---------------------------------------- 
README.md      | The file you are currently reading 
build          | The directory used by CMake, initially empty
src            | The core source code and a CmakeLists.txt file

## Requirements

The build process uses CMake and requires that you have installed this. You
can use either the command line version of CMake or the GUI.

Both the CLANG compiler and GCC can be used to build Nial so that you will 
need to have installed one of these compilers. On OSX you should install
*XCode*, on Linux use the appropriate package manager for your distribution.

CLANG appears to generate slightly faster code on Linux.

If you are buiding the Native Windows version on Windows then first read
the section in the *BuildNial* README which indicates which compiler
software is needed to do this and what modifications are needed to the
*cmake* command line.

## Building the core

Use the CMake tool to build  the "nialcore" executable with the following
steps:

1. Ensure that the "build" directory is empty.

2. If you are using the Cmake GUI set up CMake to point at the
   *QNial7/Buildcore/src*  directory as the source directory and  
   *QNial7/BuildCore/build* as the build directory and then use CMake to
   generate the contents of the build directory. 

   If you are using the command line version of Cmake then do the following

   $ cd build

   $ cmake ../src   

3. Change into build, if you are not already there, and execute *make* to build the executable.

   $ make

   The result will be the executable *nialcore* in the build directory.
   Test it by running nialcore interactively with the command

   $ ./nialcore -i   

This will display a header like:

   ```
   Q'Nial V7.0 Open Source Edition Intel x86 64bit Mac OSX May 19 2017
   Copyright (c) NIAL Systems Limited
   clear workspace created
   ```

   and then will be awaiting a *nial* input indented 6 spaces. 

   Type:        

   5 + tell 10   

   which will display

   5 6 7 8 9 10 11 12 13 14   

   Type:   

        bye   

   to end the *nial* interactive session.

4. Copy the nialcore executable to the pkgbldr subdirectory of the BuildNial directory using

   $ cp nialcore ../../BuildNial/pkgbldr

You are now ready to build the full QNial7 using pkgblder in BuildNial. See the README.md in BuildNial for instructions.
