cmake_minimum_required(VERSION 3.22)
project(dlsim VERSION 0.1.0 DESCRIPTION "Radio datalink emulator using tc/netem" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()

option(DLSIM_BUILD_GUI "Build the Qt Quick GUI" ON)
option(DLSIM_BUILD_TESTS "Build the unit tests" ON)

include(GNUInstallDirs)
set(DLSIM_HELPER_INSTALL_PATH "${CMAKE_INSTALL_FULL_LIBEXECDIR}/dlsim/dlsim-helper"
    CACHE STRING "Absolute path the helper is installed to (referenced by the polkit policy)")
# polkitd only reads actions from this directory, whatever the install prefix.
set(DLSIM_POLKIT_ACTIONS_DIR "/usr/share/polkit-1/actions" CACHE PATH "polkit actions directory")

find_package(Qt6 6.4 REQUIRED COMPONENTS Core)
set(CMAKE_AUTOMOC ON)

add_compile_options(-Wall -Wextra -Wpedantic)
add_compile_definitions(DLSIM_VERSION="${PROJECT_VERSION}")

add_subdirectory(src/core)
add_subdirectory(helper)

if(DLSIM_BUILD_GUI)
    find_package(Qt6 6.4 REQUIRED COMPONENTS Gui Widgets Qml Quick QuickControls2 Charts)
    add_subdirectory(src)
endif()

if(DLSIM_BUILD_TESTS)
    find_package(Qt6 6.4 REQUIRED COMPONENTS Test)
    enable_testing()
    add_subdirectory(tests)
endif()

install(PROGRAMS scripts/selftest.sh DESTINATION ${CMAKE_INSTALL_DATADIR}/dlsim RENAME dlsim-selftest.sh)
