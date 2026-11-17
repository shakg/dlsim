set shell := ["bash", "-cu"]

build_dir := "build"
build_type := "Debug"

default: build

configure:
    cmake -S . -B {{build_dir}} -G Ninja -DCMAKE_BUILD_TYPE={{build_type}}

build: configure
    cmake --build {{build_dir}}

run: build
    ./{{build_dir}}/src/dlsim

# Run the interface without pkexec or the Linux traffic-control helper.
ui: build
    DLSIM_NO_HELPER=1 ./{{build_dir}}/src/dlsim

test: build
    ctest --test-dir {{build_dir}} --output-on-failure

# Capture every page for visual review without starting the helper.
screenshots output="screenshots": build
    mkdir -p "{{output}}"
    DLSIM_NO_HELPER=1 DLSIM_SCREENSHOT="{{output}}" ./{{build_dir}}/src/dlsim

clean:
    cmake --build {{build_dir}} --target clean
