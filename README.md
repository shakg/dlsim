# DLSim

[![CI](https://github.com/shakg/dlsim/actions/workflows/ci.yml/badge.svg)](https://github.com/shakg/dlsim/actions/workflows/ci.yml)
[![Release](https://github.com/shakg/dlsim/actions/workflows/release.yml/badge.svg)](https://github.com/shakg/dlsim/actions/workflows/release.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

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

## Install

Download the latest package from
[GitHub Releases](https://github.com/shakg/dlsim/releases/latest).

### Debian / Ubuntu

```bash
sudo apt install ./dlsim_*.deb
```

The package installs DLSim, its desktop entry, the privileged helper and the
polkit policy. `apt` installs the required Qt and system packages.

### AppImage

```bash
sudo apt install iproute2 pkexec
chmod +x DLSim-*-x86_64.AppImage
./DLSim-*-x86_64.AppImage
```

The AppImage bundles Qt. It is built for x86-64 Linux and requires glibc 2.39
or newer, as provided by Ubuntu 24.04.

## Usage

Start the installed app:

```bash
dlsim
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

## Contributing

Build instructions, tests, packaging, CI/CD and the release process are in
[CONTRIBUTING.md](CONTRIBUTING.md). By participating, you agree to follow the
[Code of Conduct](CODE_OF_CONDUCT.md).

## License

[MIT](LICENSE)
