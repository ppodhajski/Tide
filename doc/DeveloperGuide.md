Developer Guide {#developer_guide}
============

This document describes the basic structure of the source code and provides
pointers to auxilary documentation.

## Directory Layout

* [CMake/common](https://github.com/Eyescale/CMake#readme): subdirectory
  included using git externals. See below for details.
* tide: Contains the main libraries of the project:
  * core: The core library contains utilities, helpers and shared classes that
          are used by the master and wall applications.
  * master: The master library contains the control GUI and other
            application-control logics.
  * wall: The wall library contains the QML2 rendering engine for the supported
          content types and their synchronization between other wall processes.
* apps: Applications delivered with the project:
  * tide: The main application which launches the master and wall processes
          according to the provided configuration file.
  * TideConverter: A small utility to convert deprecated xml configuration files
                   to json format. It can also be used to convert json
                   configurations between a compact and detailed representation.
  * TideMaster: The master application launched by tide which provides a control
                user interface to open contents and sessions, change options etc.
  * TideWall: The wall application launched for each wall segment launched by
              tide which presents all opened contents.
  * TideForker: An auxilary process launched by tide which forks new processes
                like the Launcher or Webbrowser.
  * Launcher: The application which streams the Qml control panel for browsing
              documents, sessions and launching applications.
  * Webbrowser: The application which streams a Qml WebEngine-based webbrowser.
  * pyramidmaker: A script that generates a TIFF image pyramid from a big
                  source image using ImageMagick's convert tool. %Image pyramids
                  can be loaded and rendered by Tide more efficently.

* tests: Unit tests.
* doc: Doxygen and other documentation.
* examples: Example configuration files, installed under share/Tide.

## CMakeLists

The top-level CMakeLists is relatively simple due to the delegation of
details into the CMake external. It starts with the project setup which
defines the project name and includes the CMake/common using git external.

## CMake

All BBP projects rely on a common
[CMake repository](https://github.com/Eyescale/CMake) which provides
sensible defaults for compilation, documentation and packaging. It is
integrated as a CMake/common subtree as described in the
[Readme](https://github.com/Eyescale/CMake#readme).

## Unit tests

Unit tests are very important. Take a look at the
[coverage report](CoverageReport/index.html).
