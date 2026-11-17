# DLSim

[![CI](https://github.com/shakg/dlsim/actions/workflows/ci.yml/badge.svg)](https://github.com/shakg/dlsim/actions/workflows/ci.yml)

DLSim is a Linux desktop app for testing software over unreliable radio links.
It uses `tc`/netem to add latency, jitter, packet loss, bandwidth limits,
duplication, reordering, corruption and outages without disconnecting the
Ethernet link.

Uplink and downlink are controlled separately. Traffic can share the main link
profile, use its own profile, or bypass impairment entirely.

## Features

- Simple controls for latency, variation, loss and bandwidth
- Advanced netem controls for both link directions
- Random and bursty loss models
- One-click and timed link outages
- Reusable presets and multi-step scenarios
- Traffic rules for TCP, UDP, MAVLink, RTSP/RTP, SSH, HTTP and WebSocket
- Live throughput, drop and queue charts
- TCP recovery monitoring with RTO, retransmit and backoff details
- HTML run reports and CSV exports
- Bridge mode for a dedicated test box and host mode for a single PC

## Requirements

DLSim is tested on Ubuntu 24.04 with Qt 6.4. It needs Linux traffic control,
`pkexec`, and either one or two Ethernet interfaces depending on the mode.

Install the build and runtime dependencies:

```bash
sudo apt install \
  build-essential cmake ninja-build libgl-dev \
  qt6-base-dev qt6-declarative-dev qt6-charts-dev \
  qml6-module-qtquick qml6-module-qtquick-controls \
  qml6-module-qtquick-layouts qml6-module-qtquick-window \
  qml6-module-qtquick-templates qml6-module-qtqml-workerscript \
  qml6-module-qtquick-dialogs qml6-module-qt-labs-folderlistmodel \
  qml6-module-qtcharts qml6-module-qt5compat-graphicaleffects \
  qt6-qpa-plugins iproute2 pkexec polkitd
```

## Install

Build and install from source:

```bash
git clone https://github.com/shakg/dlsim.git
cd dlsim
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build
ctest --test-dir build --output-on-failure
sudo cmake --install build
```

The install prefix must be set while configuring because the GUI and polkit
policy embed the helper path.

To build a Debian package instead:

```bash
(cd build && cpack -G DEB)
sudo apt install ./build/dlsim_*.deb
```

To build an AppImage:

```bash
sudo apt install patchelf file qt6-base-dev-tools
packaging/appimage/build-appimage.sh dist-out
```

The AppImage bundles Qt, but the host still needs `iproute2` and `pkexec`.

## Usage

Start the installed app:

```bash
dlsim
```

Or run it from the build directory:

```bash
./build/src/dlsim
```

On first run:

1. Open **Setup** and choose bridge or host mode.
2. Select the Ethernet interface or interfaces.
3. Add traffic rules if only specific flows should be impaired.
4. Click **Start** and approve the `pkexec` prompt.
5. Open **Link** and select a preset or edit the conditions.

Use **CUT LINK** to simulate a complete outage. The Ethernet carrier stays up;
packets are dropped by netem.

### Bridge mode

Use bridge mode when the DLSim PC sits between the ground system and the radio
or vehicle. It needs two Ethernet interfaces.

```text
Ground PC -- [ground NIC] == br-dlsim == [air NIC] -- Radio / vehicle
                 downlink                 uplink
```

Stop NetworkManager from managing both bridge ports before starting:

```bash
nmcli device set enp1s0 managed no
nmcli device set enp2s0 managed no
```

Replace the interface names with your own. Both endpoints keep their physical
link while DLSim impairs each direction independently.

### Host mode

Use host mode when DLSim runs on an endpoint and only one interface is
available. Outgoing traffic is impaired on the interface; incoming traffic is
redirected through `ifb-dlsim`.

Other traffic is excluded by default so management traffic such as SSH stays
usable.

## Link profiles and traffic rules

The **Link** page has two editors:

- **Simple**: latency, jitter, random or bursty loss, and bandwidth
- **Advanced**: correlation, distributions, Gilbert-Elliott loss, queue size,
  duplication, reordering and corruption

Traffic rules match IPv4 packets by protocol, endpoint and port. First match
wins. A rule can use the shared link profile, a custom profile, or no
impairment. WebSocket rules match TCP by IP and port; DLSim does not inspect
the HTTP upgrade.

Built-in presets include clean, line-of-sight, long-range, fringe,
intermittent and congested links. User data is stored under:

```text
~/.config/dlsim/presets/
~/.config/dlsim/scenarios/
~/.config/dlsim/reports/
~/.config/dlsim/dlsim.conf
```

## Scenarios and monitoring

A scenario combines timed steps:

- Hold a preset
- Fade gradually to another preset
- Drop the link
- Generate random dropouts

The **Monitor** page shows per-direction throughput, packet rate, drops and
queue backlog. Its TCP monitor reads `ss -tnoi` and reports retransmit timers,
RTO, backoff, possible half-open connections and recovery time after an
outage.

When a scenario stops or finishes, DLSim writes a self-contained HTML report
with the setup, timeline, outages, TCP recovery times and charts.

## Development

Common commands are available through [just](https://github.com/casey/just):

```bash
just build
just run
just test
just ui          # run without the privileged helper
```

The unit tests do not need root. The integration self-test does:

```bash
sudo apt install iperf3 jq iputils-ping
sudo scripts/selftest.sh
```

It creates temporary network namespaces and verifies bridge mode, host mode,
loss, delay, rate limits, traffic exclusions, outages and cleanup.

## How it works

The GUI runs as the current user and starts `dlsim-helper` through `pkexec`.
The helper validates requests, invokes `tc`, `ip` and `ss` without a shell, and
removes its qdiscs, IFB and bridge when the GUI exits. Profile-only changes use
`tc qdisc change`; structural changes rebuild the affected qdisc tree.

## Known limitations

- Traffic rules are IPv4 only. IPv6, ARP and other frames follow the default
  traffic setting.
- Netem acts on egress, so host-mode ingress requires an IFB device.
- NIC offloads can turn many packets into one large packet. For accurate loss
  tests, disable them with `ethtool -K <iface> tso off gso off gro off`.
- Jitter can reorder packets, and explicit reordering needs a non-zero delay.
- A cut drops newly queued packets; packets already waiting in a delay queue
  may still arrive.
- Docker may enable bridge netfilter. On a dedicated bridge-mode box, check
  `net.bridge.bridge-nf-call-iptables` if forwarding is unexpectedly blocked.

## Repository layout

```text
src/core/   Link model, validation, tc plans and parsers
src/        GUI backend
qml/        Qt Quick interface
helper/     Privileged helper and polkit policy
presets/    Built-in presets, scenarios and templates
tests/      Unit tests
scripts/    Root integration self-test
packaging/  AppImage builder
```
