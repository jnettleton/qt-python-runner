cmake_minimum_required(VERSION 3.25)

# The single version source. Everything else (About box, --version,
# setApplicationVersion, tests) reads core/version.h.in via configure_file.
project(python-runner VERSION 0.1.0 LANGUAGES CXX)

# Names live here once; version.h.in and the .rc VERSIONINFO both read them.
set(PYRUN_GUI_NAME "Python Runner")
set(PYRUN_CLI_NAME "pyrun")
set(PYRUN_ORGANIZATION "NCC")
set(PYRUN_COMPANY "NCC (Nettleton Computer Consulting)")

if(WIN32)
  enable_language(RC)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(PyrunWarnings)
include(PyrunVersionInfo)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Test)
qt_standard_project_setup()
include(PyrunDeploy)

enable_testing()

add_subdirectory(core)
add_subdirectory(cli)
add_subdirectory(gui)
add_subdirectory(tests)
