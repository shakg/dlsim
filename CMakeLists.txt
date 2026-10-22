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
# Fixed at configure time: the GUI and the polkit policy both embed it, so set
# CMAKE_INSTALL_PREFIX when configuring, not with `cmake --install --prefix`.
set(DLSIM_HELPER_INSTALL_PATH "${CMAKE_INSTALL_FULL_LIBEXECDIR}/dlsim/dlsim-helper"
    CACHE STRING "Absolute path the helper is installed to (referenced by the polkit policy)")
# polkitd only reads actions from /usr/share/polkit-1/actions, whatever the
# install prefix. Relative under /usr so packaging tools can stage it.
if(CMAKE_INSTALL_PREFIX STREQUAL "/usr")
    set(_polkit_default "${CMAKE_INSTALL_DATADIR}/polkit-1/actions")
else()
    set(_polkit_default "/usr/share/polkit-1/actions")
endif()
set(DLSIM_POLKIT_ACTIONS_DIR "${_polkit_default}" CACHE PATH "polkit actions directory")

find_package(Qt6 6.4 REQUIRED COMPONENTS Core)
set(CMAKE_AUTOMOC ON)

add_compile_options(-Wall -Wextra -Wpedantic)
add_compile_definitions(DLSIM_VERSION="${PROJECT_VERSION}")

if(DLSIM_BUILD_GUI)
    find_package(Qt6 6.4 REQUIRED COMPONENTS Gui Widgets Qml Quick QuickControls2 Charts)
    add_subdirectory(src) # also adds src/core
else()
    add_subdirectory(src/core)
endif()
add_subdirectory(helper)

if(DLSIM_BUILD_TESTS)
    find_package(Qt6 6.4 REQUIRED COMPONENTS Test)
    enable_testing()
    add_subdirectory(tests)
endif()

# ---------------------------------------------------------------- packaging
# cpack -G DEB  (configure with -DCMAKE_INSTALL_PREFIX=/usr)
set(CPACK_PACKAGE_NAME "dlsim")
set(CPACK_PACKAGE_VENDOR "DLSim")
set(CPACK_PACKAGE_CONTACT "shakg <shakg@users.noreply.github.com>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Radio datalink emulator (tc/netem) with a Qt Quick GUI")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/shakg/dlsim")
set(CPACK_PACKAGING_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")
set(CPACK_STRIP_FILES ON)
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_SECTION "net")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
# QML modules are loaded at run time, so dpkg-shlibdeps cannot see them.
set(CPACK_DEBIAN_PACKAGE_DEPENDS
    "iproute2, pkexec | policykit-1, qt6-qpa-plugins, qml6-module-qtquick, qml6-module-qtquick-controls, \
qml6-module-qtquick-layouts, qml6-module-qtquick-window, qml6-module-qtquick-templates, \
qml6-module-qtqml-workerscript, qml6-module-qtquick-dialogs, qml6-module-qt-labs-folderlistmodel, \
qml6-module-qtcharts")
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "iperf3, jq, iputils-ping, ethtool")
include(CPack)
