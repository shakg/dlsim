# DLSim — radio datalink emulator

DLSim is a Qt 6 / QML desktop app that makes an Ethernet link behave like a
radio datalink. It adds delay, jitter, loss (random or bursty), rate limits,
duplication, reordering, corruption and full outages, separately for
**uplink** (ground → air) and **downlink** (air → ground). The Ethernet links
stay up the whole time: packets are delayed, dropped or throttled, never
unplugged.

It was built to reproduce one specific problem: after a radio fade, UDP video
resumes at once but MAVLink-over-TCP telemetry (vehicle 192.0.2.10:4555 ↔
mavlink-router on 192.0.2.20) stays stuck. The socket monitor shows RTO, backoff,
retransmits and the TCP timer live, so you can see the backoff or half-open
connection happen.

It uses only standard tools: `tc` (netem, prio, flower, matchall/mirred),
`ip` (bridge, ifb) and `ss`. No kernel modules beyond what Ubuntu ships.

## Contents

- [Operating modes](#operating-modes)
- [Packages (Ubuntu 24.04)](#packages-ubuntu-2404)
- [Build, test, install](#build-test-install)
- [Packages: .deb and AppImage](#packages-deb-and-appimage)
- [Running](#running)
- [Using it](#using-it)
- [Reproducing the TCP telemetry stall](#reproducing-the-tcp-telemetry-stall)
- [Presets and scenarios](#presets-and-scenarios)
- [How it works](#how-it-works)
- [Limitations (netem and friends)](#limitations-netem-and-friends)
- [Repository layout](#repository-layout)

## Operating modes

**Bridge mode (primary).** The DLSim PC sits between vehicle and ground with
two NICs. DLSim creates `br-dlsim` from them and impairs each port's egress:

```
 ground PC ── [ground NIC] ═ br-dlsim ═ [air NIC] ── radio/converter (192.0.2.10)
                  ▲ downlink netem          ▲ uplink netem
```

Both endpoints keep a physical link and see nothing but a bad radio.

**Host mode.** DLSim runs on the ground PC itself, on one NIC. Uplink is
impaired on the NIC's egress. Downlink is impaired on ingress: an `ingress`
qdisc redirects every incoming packet to `ifb-dlsim`, whose egress carries
the downlink netem. Traffic classes limit impairment to the vehicle, so SSH
and everything else stay clean ("Other traffic: Excluded" is the default in
this mode).

## Packages (Ubuntu 24.04)

Verified against the noble archive (Qt 6.4.2):

```sh
sudo apt install \
    build-essential cmake ninja-build libgl-dev \
    qt6-base-dev qt6-declarative-dev qt6-charts-dev \
    qml6-module-qtquick qml6-module-qtquick-controls qml6-module-qtquick-layouts \
    qml6-module-qtquick-window qml6-module-qtquick-templates \
    qml6-module-qtqml-workerscript qml6-module-qtquick-dialogs \
    qml6-module-qt-labs-folderlistmodel qml6-module-qtcharts \
    qt6-qpa-plugins \
    iproute2 pkexec polkitd
# self-test only:
sudo apt install iperf3 jq iputils-ping
```

`qml6-module-qt-labs-folderlistmodel` is used by Qt's built-in file dialog
when no native dialog is available.

## Build, test, install

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build
ctest --test-dir build --output-on-failure      # unit tests, no root needed
```

The unit tests cover `TcCommandBuilder` (every netem option combination,
plans for both modes, filters, the minimal diff, cut, teardown and leftover
cleanup), the `tc -j` / `ss` parsers, traffic class editing and the preset
store's error reporting.

Install system-wide:

```sh
sudo cmake --install build
```

This installs:

| File | Path |
| --- | --- |
| GUI | `/usr/bin/dlsim` |
| helper | `/usr/libexec/dlsim/dlsim-helper` |
| polkit policy | `/usr/share/polkit-1/actions/org.dlsim.helper.policy` |
| desktop entry | `/usr/share/applications/dlsim.desktop` |

The helper path is fixed when you configure: the GUI and the policy's
`exec.path` both embed it. Set the prefix with `-DCMAKE_INSTALL_PREFIX` at
configure time, not with `cmake --install --prefix`. polkitd only reads
policies from `/usr/share/polkit-1/actions`, so the policy goes there
whatever the prefix.

### Self-test (needs root and a kernel with netem)

```sh
sudo scripts/selftest.sh                 # uses build/helper/dlsim-helper and presets/presets.json
sudo scripts/selftest.sh /usr/libexec/dlsim/dlsim-helper
```

It builds three network namespaces joined by two veth pairs (`dlsim-gnd`
192.0.2.20, `dlsim-air` 192.0.2.10, `dlsim-dut` in the middle), drives the
helper directly over stdin/stdout, and for every preset:

- applies it in bridge mode and checks that netem is on both ports;
- pings ground → air and compares RTT and loss with the preset;
- sends UDP with iperf3 at 1.5× the rate limit in each direction and checks
  the received rate stays under the limit.

It then checks CUT/RESTORE, an excluded class passing during a cut,
`read_stats`, `reset`, cleanup of leftovers after a `kill -9` of the helper,
and host mode with the IFB (RTT = egress + ingress delay, cleanup on exit).
Tune with `PINGS=…` and `IPERF_SECONDS=…`. Namespaces are deleted on exit.

## Packages: .deb and AppImage

CI (`.github/workflows/ci.yml`) builds and tests on every push and pull
request, then produces both packages as workflow artifacts. It also runs
`selftest.sh` on the runner. Pushing a tag `vX.Y.Z` (matching the version in
`CMakeLists.txt`) attaches both to a GitHub release.

**.deb** (Ubuntu 24.04):

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build
(cd build && cpack -G DEB)
sudo apt install ./build/dlsim_*.deb     # pulls in the QML modules, iproute2, pkexec
```

**AppImage**:

```sh
sudo apt install patchelf file qt6-base-dev-tools   # plus the build deps above
packaging/appimage/build-appimage.sh [output-dir]   # → DLSim-<version>-x86_64.AppImage
```

The script uses linuxdeploy and linuxdeploy-plugin-qt (downloaded to
`build-appimage/tools`). The AppImage bundles Qt, so it needs no Qt packages,
but the host still needs `iproute2` and `pkexec`, and a glibc at least as new
as Ubuntu 24.04's. Two differences from the .deb:

- No polkit policy is installed, so pkexec asks through its generic
  "run a program as root" prompt.
- Root cannot read an AppImage's FUSE mount, so the GUI copies the helper and
  the libraries it needs (listed in `usr/libexec/dlsim/helper-files.txt`) to a
  private directory under `$XDG_RUNTIME_DIR`, runs it from there with pkexec,
  and deletes it on exit.

It bundles the `xcb` (X11 and XWayland) and `offscreen` platform plugins.

## Running

```sh
dlsim                 # installed
./build/src/dlsim     # from the build tree
```

The GUI runs as your user. At start it launches `dlsim-helper` through
`pkexec`, which asks for your password once. From a build tree the GUI uses
`build/helper/dlsim-helper`; pkexec then uses its generic "run a program as
root" action because that path is not in the policy.

Environment variables:

| Variable | Effect |
| --- | --- |
| `DLSIM_HELPER=/path/to/dlsim-helper` | helper to start |
| `DLSIM_NO_PKEXEC=1` | start the helper directly (useful under `sudo -E`) |
| `DLSIM_NO_HELPER=1` | do not start the helper (UI only) |
| `DLSIM_SCREENSHOT=<dir>` | save a PNG of every page and quit (development aid) |

If the helper dies, a red banner shows the reason with a **Start helper**
button. A new helper removes whatever the old one left behind.

Errors stay on screen until dismissed and say what to do: e.g. a missing
`netem` module shows the `apt`/`modprobe` command with a **Copy command**
button, and the raw `tc` output is under **Details**. The bell in the header
lists every error and warning of the session.

Before using bridge mode, stop NetworkManager from managing the two NICs
(`nmcli device set enp1s0 managed no`), or it may reconfigure them.

## Using it

The window has a header, five tabs, a status bar and a collapsible bottom
panel. It fits 1366×768.

**Header.** Helper status, a state badge (Off / Running / Link cut /
Scenario step N of M / Fading), the interfaces and the active preset. On the
right: **LIVE** (edits apply at once), the one **Start / Stop** button for
the whole app, a **⋮** menu with **Reset all…** (removes every qdisc, filter,
IFB and bridge DLSim created, after a confirmation), the notification bell and
the big **CUT LINK / RESTORE** button (100% loss both ways, immediately).
RESTORE clears every cut source (manual, timed, scenario, dropout).

**Home.** The default tab once the link is configured. A diagram
`Ground ⇄ DLSim ⇄ Vehicle` (or `This PC ⇄ Vehicle` in host mode) with one arrow
per direction, labelled with the current delay, loss and rate and the live
throughput and drops; the labels follow fades and scenarios. Below: the
current condition (**Change…** opens Link), **Timed outage** (cut for N
seconds, then restore) and the scenario's Play/Pause.

**Setup.** Pick bridge or host mode and the interface(s). The interface list
shows, per port and refreshed every second without root, whether a cable is
in (carrier), the speed and the live rx/tx rate; virtual interfaces are hidden
unless **Show all** is on. **Identify…** next to each NIC asks you to unplug
the cable going to that side and picks the port that loses its link. Home
shows each port's cable state under Ground and Vehicle. Problems are shown
next to the field that causes them, and Start stays disabled until they are
fixed. DLSim warns before you impair an interface that carries the default
route or an SSH session, before enslaving an interface with addresses to the
bridge, and when NetworkManager manages a bridge port. **Start** creates the
bridge (bridge mode) and applies the profiles; **Stop** removes them. The
right side holds the **traffic classes**, added with **Add traffic ▾** from templates (MAVLink
TCP/UDP, RTSP/RTP video, SSH, HTTP, custom; `presets/traffic.json`) that ask
for the endpoint and fill protocol and port. Each row reads as a sentence
("Telemetry · TCP to/from Vehicle:4555 · uses link profile"); **Edit** opens
its fields. **Endpoints** (left) name the devices, e.g. *Vehicle* = 192.0.2.10;
changing an endpoint's IP changes every class that uses it. A class is
protocol + IP (or endpoint) + port, each using the
link profile, a custom profile (own uplink/downlink netem), or excluded. A
class matches packets *to or from* IP:port, so one class covers both
directions. First match wins. "Other traffic" uses the link profile or is
excluded.

**Link.** A plain-language summary of the link ("≈ 80 ms round trip · about
1 in 100 packets lost · max 2 Mbit/s") and two editor modes (the choice is
remembered). **Simple** (default): latency, variation, packet loss (None /
Occasional / Bursty fades with amount and burst length) and a bandwidth cap,
with **Same for both directions** on by default. Bursty loss maps to
Gilbert-Elliott with `r = 100 / burst`, `1-h = 100%`, `1-k = 0` and `p` chosen
for the average loss. Fields simple mode can't show are kept and listed.
**Advanced**: uplink and downlink editors with every netem field: delay, jitter, correlation and
distribution (uniform, normal, pareto, paretonormal); loss (random with
correlation, or Gilbert-Elliott `p`, `r`, `1-h`, `1-k`); rate (∞ = unlimited)
and queue limit (auto = sized from delay × rate); duplicate, reorder (with
correlation and gap) and corrupt. Presets are cards on the left: name, description and delay · loss · rate per
direction. Click a card to apply it. The active card is highlighted; after an
edit it shows *modified* with **Save as new…** and **Revert**. Each card's …
menu has Rename, Export and Delete (user presets). With **Apply edits immediately (live)** on, every edit is sent
within 150 ms; otherwise an **Unapplied changes · Apply · Discard** bar
appears under the header.

**Scenario.** A timeline of steps: *Hold condition* (switch to a preset),
*Gradually change to* (ramp from the current profiles to a preset over the
step's duration), *Drop link* (100% loss) and *Random dropouts* (outages with
exponentially distributed gaps and lengths, a Poisson process, around a mean
interval and mean length, laid over the current condition). Play, pause, stop
and loop, with the current step, step time and total time shown. New steps
use the previous step's preset, else the active one.

The **timeline** shows one block per step, its width proportional to the
duration (outages red, fades as a gradient, dropouts amber), and under it a
graph of delay and loss over time computed with the same interpolation the
fades use. Drag a block to move it, drag its right edge to change the
duration (1 s steps, Shift for 0.1 s), click it to select its row in the step
list below. A playhead shows the position and loop count; while paused, click
the graph to jump there.

**New…** starts from a goal: *Does my app recover after outages?* (outage
lengths, recovery time), *Fly out to the edge of range and back* (start and
edge condition, fade and hold time), *Unstable link* (base condition, dropout
frequency and length, total time), *Congested downlink* (bitrate cap,
duration) or *Blank*. Each shows its description, step count and total time
before creating; the result is a normal scenario to edit and save.
Templates are data: `presets/templates.json`, plus your own in
`~/.config/dlsim/templates/*.json` (same format: `params`, `steps` with
`$param` values and `forEach` over a list, optional derived `presets`).

The manual **Gradual change** (current → preset over T seconds at 1–10 Hz,
with **Fade** and **Fade back**) and **Random dropouts** switch are quick actions on Home.

**Monitor.** 1 Hz statistics from `tc -s -j qdisc show` per direction:
throughput, packets/s, drops/s, backlog, total drops and overlimits, with live
charts. Below, the **TCP socket monitor**: pick the peer from your TCP traffic
classes (or *Other…* to type IP and port) and press Start. Each connection
gets a plain status line: *Healthy · RTT 42 ms*, *Stuck · waiting on
retransmit timer, next try in 12.3 s (backoff 5)*, *Stuck · the peer's receive
window is zero*, *Possibly half-open · ESTAB, no data acknowledged for 40 s*,
*Idle*, *Connecting*, *Listening*, and after a RESTORE or outage end
*Recovered 23.4 s after link was restored* (time from the restore to the
first data acknowledged after it; also logged). The rules are in
`src/core/SocketHealth.h`. It polls `ss -tnoi` for `dst IP dport = :PORT` and `src IP sport = :PORT`
at 1 Hz. **Details** shows the raw state, Recv-Q/Send-Q, rto, rtt, retransmits, backoff and the
timer (`on` = retransmit, `keepalive`, `persist`) with its countdown. The chart
plots RTO, backoff and total retransmits of one socket over time. Statistics
and socket history export to CSV.

**Bottom panel.** Closed by default; open it with **Log & commands** in the
status bar (a red count shows new errors). Its state and height are
remembered. *Log*: a timestamped list of every applied change, outage
start/end, scenario step, socket state or backoff change, and helper message
(exports to CSV). *Commands*: the exact `tc`/`ip` commands — **Pending** (the
minimal diff the next apply will run, and whether that is a `tc qdisc change`
or a rebuild), **Full state**
(everything needed to build the current configuration from scratch) and
**Executed** (what the helper ran, with exit codes and stderr).

**Keyboard.** Space: CUT / RESTORE (not while typing; Esc leaves a text
field). Ctrl+Enter: Start, or Apply. Ctrl+P: play/pause the scenario; Ctrl+.:
stop it. Ctrl+1…5: tabs. F1: the list of shortcuts.

Hover a disabled control to see why it is disabled.

## Reproducing the TCP telemetry stall

1. Bridge mode between the converter and the ground PC, or host mode on the
   ground PC with a class *MAVLink TCP 4555 → 192.0.2.10*.
2. Load **Good LOS** and start.
3. Run DLSim on the ground PC (host mode) to see the socket: Monitor →
   peer `192.0.2.10 : 4555` → Start. In bridge mode the socket lives on the
   ground PC, not on the DLSim box; run `ss -tnoi dst 192.0.2.10` there.
4. Play the **Outage ladder** scenario (3 s, 10 s, 30 s, 90 s outages) or use
   timed outages.

What to look for:

- During an outage with unacked data, `timer:(on,…)` runs, `backoff` climbs
  and `rto` doubles (200 ms … up to 120 s). After RESTORE nothing is sent
  until the timer fires, so telemetry stays stuck for up to one RTO after the
  radio is back. Video (UDP) has no such timer.
- With no data in flight the socket may sit in `keepalive` or no timer at
  all. If the converter dropped its side meanwhile (reboot, own timeout), the
  ground socket stays `ESTAB` (half-open) until the next send fails.
- Retransmits stop after `net.ipv4.tcp_retries2` (default 15, roughly 15
  minutes), then the socket closes.

The log records each backoff change with RTO and retransmit count; export it
together with the socket CSV for a timeline.

## Presets and scenarios

Built-in presets: **Wired/clean**, **Good LOS**, **Long range**, **Fringe**,
**Intermittent**, **Congested** (downlink limited to a 4 Mbit/s video bitrate
with a deep queue). Built-in scenarios: **Outage ladder**, **Fade out and
back**, **Flight loop** (looping).

User files live in `~/.config/dlsim/`:

```
~/.config/dlsim/presets/<name>.json
~/.config/dlsim/scenarios/<name>.json
~/.config/dlsim/dlsim.conf          # last configuration, auto-apply setting
```

Preset format (missing fields take defaults):

```json
{
  "name": "My link",
  "description": "…",
  "uplink":   { "delayMs": 40, "jitterMs": 10, "delayCorrPct": 25, "distribution": "normal",
                "lossModel": "random", "lossPct": 1, "lossCorrPct": 25,
                "rateKbit": 500, "limitPkts": 0,
                "duplicatePct": 0, "reorderPct": 0, "reorderCorrPct": 0, "reorderGap": 0,
                "corruptPct": 0 },
  "downlink": { "lossModel": "gemodel", "geP": 2, "geR": 20, "geBadLossPct": 80, "geGoodLossPct": 0.5 }
}
```

Scenario format. `type` is `preset` (Hold condition), `fade` (Gradually
change to), `outage` (Drop link) or `dropouts` (Random dropouts); `preset`
names a preset. A `dropouts` step takes `meanIntervalS` and `meanDurationS`
(defaults 30 and 3) and an optional `seed`: with a seed the step produces the
same dropouts on every run.

```json
{ "name": "My test", "loop": false,
  "steps": [ { "type": "preset", "preset": "Good LOS", "durationS": 20 },
             { "type": "fade", "preset": "Fringe", "durationS": 30 },
             { "type": "outage", "durationS": 10 },
             { "type": "dropouts", "durationS": 120, "meanIntervalS": 15, "meanDurationS": 2, "seed": 7 } ] }
```

## How it works

### Privilege split

```
 dlsim (GUI, your user)  ──stdin/stdout, JSON lines──▶  pkexec dlsim-helper (root)
                                                          └─ tc / ip / ss via QProcess argv
```

The helper accepts only `apply_profile`, `cut`, `restore`, `reset`,
`read_stats`, `read_sockets`, `setup_bridge` and `teardown`. It validates
every parameter: interface names against `ip -j link`, numeric ranges, IPv4
addresses, ports. It runs tools from fixed system directories with argument
lists, never through a shell. It cleans everything up when its stdin closes
(the GUI exited or crashed) and on SIGINT, SIGTERM or SIGHUP. At start it
removes leftovers of an earlier crash and reports them in its `ready` event.

```
→ {"id":7,"op":"apply_profile","params":{"config":{…}}}
← {"id":7,"op":"apply_profile","ok":true,"result":{"level":"change","cut":false},"commands":[{"cmd":"tc qdisc change …","ok":true,"exitCode":0,"stderr":""}]}
```

### Kernel objects

On every impaired device (fixed names and handles, so leftovers can be found):

```
root 1d: prio bands N priomap <Other band ×16>
  1d:1 → 10: pfifo           excluded traffic
  1d:2 → 20: netem <link>    the direction's link profile
  1d:3 → 30: netem <custom>  one per class with its own profile (up to 14)
  filters: flower on 1d:, pref 10+2i (dst ip/port) and 11+2i (src ip/port)
```

Host mode adds `ifb-dlsim`, an `ingress` qdisc on the NIC and a `matchall`
filter with pref 4919 that redirects to the IFB. Bridge mode adds `br-dlsim`.

### Minimal diff

`TcCommandBuilder` turns a configuration into a plan (devices, leaves,
filters) and diffs it against the applied plan:

| What changed | Commands |
| --- | --- |
| profile values only (including CUT, fades) | `tc qdisc change … netem …` on the affected leaves |
| class matches or "Other" mode | `tc qdisc change … prio priomap …`, `tc filter del` + `add` |
| number of custom classes, interfaces, mode | full rebuild: `del root`, then `replace`/`add` |

The GUI computes the same diff for the Commands preview. Fades push
`tc qdisc change` at 1–10 Hz; if the helper is still busy, updates are
coalesced so only the newest values are sent.

## Limitations (netem and friends)

These are properties of the kernel tools, not bugs in DLSim; the UI and
defaults work around them where possible.

- **netem only acts on egress.** Downlink in host mode therefore goes through
  an IFB device. Bridge mode avoids the IFB by using each port's egress.
- **Rate plus delay needs a big enough queue.** netem's `limit` counts every
  packet it holds, including those waiting out the delay. A limit smaller than
  delay × rate drops packets that look like radio loss. DLSim auto-sizes it:
  `ceil(1.5 × rate × (delay + 2 × jitter) / 1500 B)`, at least 100 packets with
  a rate limit and 1000 without (1 Gbit/s assumed when unlimited), at most
  1,000,000. The limit counts packets, not bytes: small MAVLink frames fill it
  as fast as full-size video packets. Set it by hand to model a specific radio
  buffer (bufferbloat).
- **Rate is per netem instance.** All classes on the link profile share one
  netem, one queue and one rate. A class with a custom profile gets its own
  netem and its own, independent rate. Bands are served in strict priority
  (excluded, then link, then custom classes).
- **Rate counts whole frames** (Ethernet header included), so iperf payload
  throughput is a few percent below the configured rate.
- **Jitter reorders packets.** netem sorts its queue by send time, so a
  packet with less jitter overtakes an earlier one. TCP then sees reordering
  and duplicate ACKs. Keep jitter below the packet spacing, or combine it with
  a rate limit.
- **Reorder needs delay.** `reorder` sends some packets immediately and
  delays the rest, so it does nothing when delay is 0.
- **Random-loss correlation is weak.** netem's correlated random loss does
  not produce real bursts. Use the Gilbert-Elliott model for fading.
- **`tc qdisc change` keeps old reorder/corrupt/rate values** when they are
  omitted. DLSim always sends `reorder 0% corrupt 0% rate 0bit` in a change to
  clear them.
- **CUT drops new packets only.** Packets already in netem's delay line are
  still delivered, so the cut becomes total after at most one delay.
- **Offloads coarsen the impairment.** With TSO/GSO/GRO on, netem can see
  64 KB super-packets: one "lost packet" is a whole burst, and the queue limit
  holds far more bytes. For realistic per-packet behaviour, disable them on
  the impaired NICs: `sudo ethtool -K enp1s0 tso off gso off gro off`.
- **Filters are IPv4 only** (`protocol ip` + flower). IPv6, ARP and other
  non-IP frames follow the "Other traffic" setting. In bridge mode with Other
  on the link profile, ARP is delayed and cut too, as on a real radio.
- **Bridged traffic and firewalls.** If `br_netfilter` is loaded (Docker does
  this), bridged frames pass through iptables and may be dropped by a
  `FORWARD DROP` policy. Check `sysctl net.bridge.bridge-nf-call-iptables`;
  set it to 0 on a dedicated DLSim box.
- **Counters restart on a rebuild**, so the charts show one zero sample when
  the qdisc structure changes.
- If an apply fails halfway, the helper removes the impairment entirely (after
  one full-rebuild retry) rather than leave a half-built tree.

## Repository layout

```
CMakeLists.txt
src/core/        Model (profiles, classes, JSON, validation), TcCommandBuilder, parsers — QtCore only
src/             GUI backend: HelperClient, LinkController, ScenarioEngine, StatsPoller,
                 SocketMonitor, PresetStore, SystemInfo, EventLog, main.cpp
qml/             Qt Quick UI (dark theme)
helper/          dlsim-helper and the polkit policy template
presets/         built-in presets and scenarios (compiled into the GUI)
tests/           QtTest unit tests (no root)
scripts/         selftest.sh (root, namespaces, ping/iperf3)
packaging/       AppImage build script
dist/            desktop entry and icon
.github/         CI: build, tests, .deb, AppImage, self-test, releases
```
