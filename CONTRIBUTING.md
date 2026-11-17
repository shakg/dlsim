# Contributing to DLSim

Contributions are welcome. By participating, you agree to follow the
[Code of Conduct](CODE_OF_CONDUCT.md).

## Development setup

DLSim is tested on Ubuntu 24.04 with Qt 6.4. Install the build and runtime
dependencies:

```bash
sudo apt install \
  build-essential cmake ninja-build libgl-dev file patchelf dpkg-dev \
  qt6-base-dev qt6-base-dev-tools qt6-declarative-dev qt6-charts-dev \
  qml6-module-qtquick qml6-module-qtquick-controls \
  qml6-module-qtquick-layouts qml6-module-qtquick-window \
  qml6-module-qtquick-templates qml6-module-qtqml-workerscript \
  qml6-module-qtquick-dialogs qml6-module-qt-labs-folderlistmodel \
  qml6-module-qtcharts qml6-module-qt5compat-graphicaleffects \
  qt6-qpa-plugins iproute2 pkexec polkitd
```

Clone and build:

```bash
git clone https://github.com/shakg/dlsim.git
cd dlsim
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

Run the app from the build directory:

```bash
./build/src/dlsim
```

The GUI starts `build/helper/dlsim-helper` through `pkexec`. To work on the UI
without starting the privileged helper:

```bash
DLSIM_NO_HELPER=1 ./build/src/dlsim
```

If [just](https://github.com/casey/just) is installed, the common commands are:

```bash
just build
just run
just test
just ui
```

## Tests

Unit tests do not require root:

```bash
ctest --test-dir build --output-on-failure
```

The integration self-test needs root, netem and three additional packages:

```bash
sudo apt install iperf3 jq iputils-ping
sudo scripts/selftest.sh
```

It creates temporary network namespaces and tests bridge mode, host mode,
delay, loss, rate limits, traffic exclusions, outages and cleanup.

## Packaging

Configure release packages with an `/usr` prefix. The prefix must be set at
configure time because the GUI and polkit policy embed the helper path.

### Debian package

```bash
cmake -S . -B build-release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build-release
(cd build-release && cpack -G DEB)
```

### AppImage

```bash
packaging/appimage/build-appimage.sh dist-out
```

The script downloads linuxdeploy tools into `build-appimage/tools` and writes
the AppImage to `dist-out`.

## Pull requests

1. Fork the repository and branch from `dev`.
2. Keep each pull request focused on one change.
3. Add or update tests when behavior changes.
4. Run the unit tests and relevant integration tests.
5. Update user or contributor documentation when needed.
6. Open the pull request against `dev` and explain the change and how it was
   tested.

## CI/CD

GitHub Actions uses `.github/workflows/ci.yml`:

- Pull requests run the build, unit tests, package builds, package smoke tests
  and the privileged network-namespace self-test.
- Pushes to `main` run the same checks and upload `.deb`, AppImage and
  screenshot artifacts to the workflow run.
- Tags matching `v*` run all checks, then create a GitHub release with the
  `.deb` and AppImage attached.
- `workflow_dispatch` allows a maintainer to run the workflow manually.

The release job runs only after both the build and self-test jobs pass.

## Creating a release

Releases use semantic versions such as `v0.2.0`. The tag must exactly match
the version in the top-level `CMakeLists.txt`; CI rejects mismatches.

1. Merge the release changes into `main`.
2. Change `project(dlsim VERSION X.Y.Z ...)` in `CMakeLists.txt`.
3. Build and test the release locally.
4. Commit and push the version change.
5. Create and push an annotated tag:

```bash
git switch main
git pull --ff-only origin main

# Edit CMakeLists.txt and set VERSION to 0.2.0.
cmake -S . -B build-release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build-release
ctest --test-dir build-release --output-on-failure

git add CMakeLists.txt
git commit -m "chore: release v0.2.0"
git push origin main
git tag -a v0.2.0 -m "v0.2.0"
git push origin v0.2.0
```

The tag starts the release workflow. After all jobs pass, GitHub Actions
creates the release, generates release notes and uploads both packages. Check
the release page and install each asset before announcing it.

Do not reuse a published version or tag. If a released build needs a fix,
increment the patch version and create a new release.
