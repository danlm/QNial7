# The Nial Language
![Image](./NialLogo.jpg?raw=true)

# Introduction

The Nial language was developed by Mike Jenkins and Trenchard More in a
collaborative research project supported by Queen's University at Kingston
and IBM Cambridge Scientific Center from 1979 to 1982. Mike's team at 
Queen's designed and implemented a portable C-based interpreter, Q'Nial 
that was initially released in 1983.

The language combines Trenchard More's theory of nested arrays with Mike's 
ideas on how to build an interactive programming system. The goal was to 
combine the strengths of APL array-based programming with implementation 
concepts borrowed from LISP, structured programming ideas from Algol, and
functional programming concepts from FP. The interpreter, originally 
developed for Unix, was small enough to run on the then newly released 
IBM PC and portable enough to execute on IBM mainframes computers. 

Nial Systems limited licensed the interpreter from Queen's University and 
marketed it widely. Mike Jenkins continued to refine both the language and
its implementation. In 2006 Mike released Version 6.3 as an open source 
project to encourage continued development of Nial.

In 2014 Mike started working with John Gibbons to develop a 64-bit version
and to add capabilities that John needed for his work. The decision was 
made to target the open source for Unix-based platforms and release it 
on GitHub.


## Version 7 Q'Nial

This version of Q'Nial is intended for people who want to integrate the
functionality of Nial into projects that can take advantage of its 
powerful array computations for  numeric, symbolic, or data analysis 
problems. The major changes are:

  - the capability to build both 32 and 64 bit versions
  - the removal of multi-platform support and a focus on Unix-based 
    multiprocessor systems
  - a reduced role for workspace management
  - the addition of new capabilites to support mutiprocessor use
  - making interactive mode an explicit choice
  - facilitating the addition of new extensions

## Currently Supported Platforms

-   Linux
-   Darwin/OSX
-   Raspbian
-   MSWindows


## Getting the Source or an Executable Version

You can either use *git* to clone the repository or you can download a zipped 
version of the repository as provided on GitHub.

The repository is organised in a simple directory structure


Entry           | Contents                                   
--------------  | ---------------------------------------- 
README.md       | The file you are currently reading 
binaries        | QNial executables for supported versions
BuildCore       | The directory to build a core version of QNial
BuildNial       | The directory to build a package with various features
CONTRIBUTING.md | How to contribute to the QNial project
docs            | Documentation for QNial and its implementation
examples        | Some examples of Nial code that uses the extensions
LICENSE         | The license for using the open source version of QNial7
NialLogo.jpg    | The Viking ship logo for QNial
Nialroot        | Directory holding a library of Nial code, and a tutorial


## Setting up QNial7 for use on your computer

If your computer is one of the supported platforms then all you need to do 
is the following:

1. Download the QNial7 repository to your $Home directory.

2. Add the *nial* executable for your platform to your $PATH variable. 
   For example, for OSX

   $ export PATH=$PATH:$HOME/QNial7/binaries/OSX

3. Test that the nial executable is working by running it interactively.

   $ nial -i 

   A header of the following form appears:

   Q'Nial V7.0 Open Source Edition Intel x86 64bit Mac OSX Jan 22 2017
   Copyright (c) NIAL Systems Limited
   
   clear workspace created

   The cursor will be indented by 5 spaces.  Test the executable by typing: 

        sum count 100

   The answer

   5050

   will appear at the left margin.

   To terminate the Nial session type:

        bye

   If you are new to using QNial, you can learn more about it by invoking 
   the tutorial library function *teach*. Run nial interactively, then at 
   the prompt type:

        library "teach 

   Enter "intro" at the prompt and follow the instructions.



## Building Nial

The directories BuildCore and BuildNial are used to build versions of
QNial7. If no existing executable is available for the platform you are
using then you need to build the nialcore executable using BuildCore. 
Follow the directions given in the README.md file in BuildCore.

If you are building for a new platform or want to add features to QNial7
that are not in the executable you have then you use directory BuildNial. 
The README.md in BuildNial describes the process for selecting additional features 
already available. There is also an explanation of how a new feature can be 
implemented and added to QNial7.


## Nial Community Links

The following links are for online discussion groups relating to Nial. 

### Discord

There is a discord server for APL inspired languages and it includes a Nial discussion channel

       https://discord.gg/NYxAVx9d

Join in and discuss aspects of Nial that interest you, problems you are having with Nial and
directions you would like to see Nial go.


