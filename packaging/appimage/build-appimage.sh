#!/usr/bin/env bash
# Build DLSim-<version>-x86_64.AppImage with linuxdeploy and its Qt plugin.
#
# Usage: packaging/appimage/build-appimage.sh [output-dir]
# Needs the build dependencies from the README plus curl, file and patchelf.
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
out=${1:-$root}
build=${BUILD_DIR:-$root/build-appimage}
tools=${TOOLS_DIR:-$build/tools}
appdir=$build/AppDir
export APPIMAGE_EXTRACT_AND_RUN=1 # run the tool AppImages without FUSE (CI containers)

version=$(sed -n 's/^project(dlsim VERSION \([0-9.]*\).*/\1/p' "$root/CMakeLists.txt")
export LINUXDEPLOY_OUTPUT_VERSION=${DLSIM_VERSION_SUFFIX:+$version-$DLSIM_VERSION_SUFFIX}
export LINUXDEPLOY_OUTPUT_VERSION=${LINUXDEPLOY_OUTPUT_VERSION:-$version}

# ---------------------------------------------------------------- build and stage
cmake -S "$root" -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DDLSIM_BUILD_TESTS=OFF
cmake --build "$build"
rm -rf "$appdir"
DESTDIR="$appdir" cmake --install "$build"
# polkitd never reads policies from inside an AppImage; pkexec uses its generic action.
rm -rf "$appdir/usr/share/polkit-1"

# ---------------------------------------------------------------- tools
mkdir -p "$tools"
fetch() {
    [[ -x $tools/$1 ]] && return
    curl -fsSL -o "$tools/$1" "$2"
    chmod +x "$tools/$1"
}
fetch linuxdeploy-x86_64.AppImage \
    https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
fetch linuxdeploy-plugin-qt-x86_64.AppImage \
    https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage
export PATH="$tools:$PATH"

export QMAKE=${QMAKE:-$(command -v qmake6 || echo /usr/lib/qt6/bin/qmake)}
export QML_SOURCES_PATHS="$root/qml"
# xcb (X11 and XWayland) is always bundled; offscreen allows headless smoke tests.
export EXTRA_PLATFORM_PLUGINS="libqoffscreen.so"

deploy() {
    linuxdeploy-x86_64.AppImage --appdir "$appdir" \
        --executable "$appdir/usr/bin/dlsim" \
        --desktop-file "$appdir/usr/share/applications/dlsim.desktop" \
        --icon-file "$appdir/usr/share/icons/hicolor/scalable/apps/dlsim.svg" \
        --plugin qt "$@"
}
helper=usr/libexec/dlsim/dlsim-helper
deploy --executable "$appdir/$helper"
# linuxdeploy leaves executables outside usr/bin without an RPATH.
patchelf --set-rpath '$ORIGIN/../../lib' "$appdir/$helper"

# Qt's built-in file dialog imports Qt.labs.folderlistmodel from its own
# resources, which qmlimportscanner does not see. Bundle it by hand.
qmlsrc=$("$QMAKE" -query QT_INSTALL_QML)
if [[ ! -d $appdir/usr/qml/Qt/labs/folderlistmodel ]]; then
    mkdir -p "$appdir/usr/qml/Qt/labs"
    cp -a "$qmlsrc/Qt/labs/folderlistmodel" "$appdir/usr/qml/Qt/labs/"
    for lib in $(ldd "$appdir"/usr/qml/Qt/labs/folderlistmodel/*.so | awk '/libQt6LabsFolderListModel/ {print $3}'); do
        cp -L "$lib" "$appdir/usr/lib/"
    done
fi

# The helper runs as root through pkexec, and root cannot read the AppImage's
# FUSE mount. The GUI copies the files listed here to a private directory first.
(
    cd "$appdir"
    ldd "$helper" | grep -q "libQt6Core.so.6 => $appdir/" \
        || { echo "helper does not resolve the bundled QtCore; check its RPATH" >&2; exit 1; }
    echo "$helper"
    ldd "$helper" | awk '{print $3}' | grep "^$appdir/" | xargs realpath --no-symlinks --relative-to=.
) > "$appdir/usr/libexec/dlsim/helper-files.txt"
echo "helper files:"; sed 's/^/  /' "$appdir/usr/libexec/dlsim/helper-files.txt"

# ---------------------------------------------------------------- package
mkdir -p "$out"
(cd "$out" && deploy --output appimage)
ls -l "$out"/DLSim-*.AppImage
