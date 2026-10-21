#!/usr/bin/env bash
# DLSim self-test. Builds a lab out of network namespaces and veth pairs,
# drives dlsim-helper directly (no GUI, no pkexec) and checks the effect of
# every preset with ping and iperf3.
#
#   dlsim-gnd (192.0.2.20) gnd0 <-veth-> dut-g ┐
#                                               ├ dlsim-dut: br-dlsim, impairment
#   dlsim-air (192.0.2.10) air0 <-veth-> dut-a ┘
#
# Usage: sudo scripts/selftest.sh [path/to/dlsim-helper] [path/to/presets.json]
# Needs: root, iproute2 with sch_netem/ifb/cls_flower/act_mirred, iperf3, jq, ping.
set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
HELPER=${1:-${DLSIM_HELPER:-$here/../build/helper/dlsim-helper}}
PRESETS=${2:-$here/../presets/presets.json}
NS_GND=dlsim-gnd NS_AIR=dlsim-air NS_DUT=dlsim-dut
GND_IP=192.0.2.20 AIR_IP=192.0.2.10
PINGS=${PINGS:-60}
IPERF_SECONDS=${IPERF_SECONDS:-4}

pass=0 failcount=0
TMP=$(mktemp -d)

red() { printf '\033[31m%s\033[0m\n' "$*"; }
green() { printf '\033[32m%s\033[0m\n' "$*"; }
ok() { green "  PASS $*"; pass=$((pass + 1)); }
bad() { red "  FAIL $*"; failcount=$((failcount + 1)); }
check() { # check <description> <command...>
    local d=$1; shift
    if "$@" >/dev/null 2>&1; then ok "$d"; else bad "$d"; fi
}
die() { red "ERROR: $*" >&2; exit 2; }
jqtest() { jq -e "$1" <<<"$2" >/dev/null; }           # jqtest <filter> <json>
absent() { ! "$@" >/dev/null 2>&1; }                   # absent <command...>
grepcmd() { local pat=$1; shift; "$@" 2>/dev/null | grep -q -- "$pat"; } # grepcmd <pattern> <command...>

in_gnd() { ip netns exec "$NS_GND" "$@"; }
in_air() { ip netns exec "$NS_AIR" "$@"; }
in_dut() { ip netns exec "$NS_DUT" "$@"; }

cleanup() {
    stop_helper || true
    [[ -f $TMP/iperf.pid ]] && kill "$(cat "$TMP/iperf.pid")" 2>/dev/null || true
    for ns in "$NS_GND" "$NS_AIR" "$NS_DUT"; do ip netns del "$ns" 2>/dev/null || true; done
    rm -rf "$TMP"
}
trap cleanup EXIT

# ------------------------------------------------------------------ prerequisites
[[ $EUID -eq 0 ]] || die "run as root (sudo $0)"
[[ -x $HELPER ]] || die "helper not found: $HELPER (build first, or pass its path)"
[[ -f $PRESETS ]] || die "presets not found: $PRESETS"
for t in ip tc jq ping iperf3; do command -v "$t" >/dev/null || die "missing tool: $t"; done

# ------------------------------------------------------------------ lab
build_lab() {
    for ns in "$NS_GND" "$NS_AIR" "$NS_DUT"; do
        ip netns del "$ns" 2>/dev/null || true
        ip netns add "$ns"
        ip -n "$ns" link set lo up
    done
    ip link add gnd0 netns "$NS_GND" type veth peer name dut-g netns "$NS_DUT"
    ip link add air0 netns "$NS_AIR" type veth peer name dut-a netns "$NS_DUT"
    ip -n "$NS_GND" addr add "$GND_IP/24" dev gnd0
    ip -n "$NS_AIR" addr add "$AIR_IP/24" dev air0
    ip -n "$NS_GND" link set gnd0 up
    ip -n "$NS_AIR" link set air0 up
    ip -n "$NS_DUT" link set dut-g up
    ip -n "$NS_DUT" link set dut-a up
}

# ------------------------------------------------------------------ helper I/O
REQ_ID=0
start_helper() { # start_helper <netns>
    coproc HELPER_PROC { exec ip netns exec "$1" "$HELPER" 2>"$TMP/helper.err"; }
    HELPER_PID=$HELPER_PROC_PID
    HOUT=${HELPER_PROC[0]} HIN=${HELPER_PROC[1]}
    IFS= read -r -t 30 READY <&"$HOUT" || die "helper did not start: $(cat "$TMP/helper.err")"
    [[ $(jq -r .event <<<"$READY") == ready ]] || die "unexpected first line: $READY"
}
close_helper_fds() {
    exec {HIN}>&- {HOUT}<&-
    HIN= HOUT=
}
stop_helper() { # closes stdin; the helper cleans up and exits
    [[ -n ${HIN:-} ]] || return 0
    close_helper_fds
    wait "$HELPER_PID" 2>/dev/null || true
}
kill_helper() { # simulates a crash: no cleanup
    kill -9 "$HELPER_PID" 2>/dev/null || true
    close_helper_fds
    wait "$HELPER_PID" 2>/dev/null || true
}
req() { # req <op> [params-json] -> prints the response; returns 1 if not ok
    REQ_ID=$((REQ_ID + 1))
    local params=${2:-'{}'} line
    jq -cn --argjson id "$REQ_ID" --arg op "$1" --argjson p "$params" '{id:$id, op:$op, params:$p}' >&"$HIN"
    IFS= read -r -t 60 line <&"$HOUT" || { red "no response to $1" >&2; return 1; }
    if [[ $(jq -r .ok <<<"$line") != true ]]; then
        red "  $1 failed: $(jq -r .error <<<"$line")" >&2
        return 1
    fi
    printf '%s\n' "$line"
}
reqq() { req "$@" >/dev/null; }

# ------------------------------------------------------------------ measurements
# ping_stats <netns> <dst> <count> -> "loss_pct avg_ms" (avg = -1 when nothing came back)
ping_stats() {
    local out loss avg
    out=$(ip netns exec "$1" ping -n -q -c "$3" -i 0.05 -W 3 "$2" 2>&1 || true)
    loss=$(sed -n 's/.* \([0-9.]*\)% packet loss.*/\1/p' <<<"$out")
    avg=$(sed -n 's#^rtt [^=]*= [0-9.]*/\([0-9.]*\)/.*#\1#p' <<<"$out")
    echo "${loss:-100} ${avg:--1}"
}

# udp_rate <client-ns> <server-ip> <kbit> <reverse:0|1> -> received kbit/s
udp_rate() {
    local rev=() json
    [[ $4 == 1 ]] && rev=(-R)
    json=$(timeout $((IPERF_SECONDS + 30)) ip netns exec "$1" iperf3 -c "$2" -u -b "${3}k" -l 1200 \
        -t "$IPERF_SECONDS" --connect-timeout 15000 -J "${rev[@]}" 2>/dev/null) || { echo 0; return; }
    jq -r '(.end.sum.bits_per_second * (1 - .end.sum.lost_percent / 100) / 1000) | floor' <<<"$json"
}

fcmp() { awk "BEGIN { exit !($*) }"; } # fcmp "1.5 < 2"

profile_json() { jq -c --arg n "$1" --arg d "$2" '.[] | select(.name == $n) | .[$d]' "$PRESETS"; }
pget() { jq -r --arg k "$2" '.[$k] // 0' <<<"$1"; }

expected_loss_pct() { # rough steady-state loss of one direction
    local p=$1 model
    model=$(jq -r '.lossModel // "random"' <<<"$p")
    if [[ $model == gemodel ]]; then
        jq -r '(.geP // 0) as $p | (.geR // 100) as $r | (.geBadLossPct // 100) as $h | (.geGoodLossPct // 0) as $k
               | if $p == 0 then 0 else ($p / ($p + $r)) * $h + ($r / ($p + $r)) * $k end' <<<"$p"
    else
        pget "$p" lossPct
    fi
}

bridge_config() { # bridge_config <uplink-json> <downlink-json> [classes-json] [otherMode]
    jq -cn --argjson up "$1" --argjson down "$2" --argjson cls "${3:-[]}" --arg other "${4:-link}" \
        '{config: {mode: "bridge", groundIface: "dut-g", airIface: "dut-a", uplink: $up, downlink: $down,
                   classes: $cls, otherMode: $other}}'
}

# ------------------------------------------------------------------ tests
echo "== lab setup"
build_lab
in_dut tc qdisc add dev lo root netem delay 1ms 2>/dev/null || die "netem is not available (modprobe sch_netem)"
in_dut tc qdisc del dev lo root

echo "== bridge mode"
start_helper "$NS_DUT"
check "setup_bridge" reqq setup_bridge '{"groundIface":"dut-g","airIface":"dut-a"}'
check "bridge br-dlsim exists" in_dut ip link show br-dlsim
in_air iperf3 -s -D --pidfile "$TMP/iperf.pid" >/dev/null
sleep 0.5

mapfile -t NAMES < <(jq -r '.[].name' "$PRESETS")
for name in "${NAMES[@]}"; do
    echo "-- preset: $name"
    up=$(profile_json "$name" uplink)
    down=$(profile_json "$name" downlink)
    if ! reqq apply_profile "$(bridge_config "$up" "$down")"; then
        bad "apply $name"
        continue
    fi
    ok "apply $name"
    check "netem on dut-a (uplink)" grepcmd 'netem 20:' in_dut tc qdisc show dev dut-a
    check "netem on dut-g (downlink)" grepcmd 'netem 20:' in_dut tc qdisc show dev dut-g

    rtt_expected=$(jq -rn --argjson u "$up" --argjson d "$down" '($u.delayMs // 0) + ($d.delayMs // 0)')
    jitter=$(jq -rn --argjson u "$up" --argjson d "$down" '($u.jitterMs // 0) + ($d.jitterMs // 0)')
    lu=$(expected_loss_pct "$up"); ld=$(expected_loss_pct "$down")
    loss_expected=$(awk -v a="$lu" -v b="$ld" 'BEGIN { printf "%.2f", 100 * (1 - (1 - a / 100) * (1 - b / 100)) }')
    read -r loss avg < <(ping_stats "$NS_GND" "$AIR_IP" "$PINGS")
    echo "     ping: loss ${loss}% (expected ~${loss_expected}%), avg rtt ${avg} ms (expected ~${rtt_expected} ms ± jitter ${jitter})"
    if fcmp "$loss < 100"; then
        check "rtt >= 0.7 x expected" fcmp "$avg >= 0.7 * $rtt_expected - 1"
        check "rtt <= expected + margin" fcmp "$avg <= 1.5 * $rtt_expected + 4 * $jitter + 20"
    else
        bad "ping got nothing through"
    fi
    if fcmp "$loss_expected == 0"; then
        check "no loss" fcmp "$loss == 0"
    else
        check "loss within bounds" fcmp "$loss <= 3 * $loss_expected + 15"
    fi

    for dir in uplink downlink; do
        p=$up rev=0
        [[ $dir == downlink ]] && p=$down rev=1
        rate=$(pget "$p" rateKbit)
        [[ $rate == 0 ]] && continue
        l=$(expected_loss_pct "$p")
        got=$(udp_rate "$NS_GND" "$AIR_IP" $((rate * 3 / 2)) $rev)
        echo "     $dir UDP: offered $((rate * 3 / 2)) kbit/s, received $got kbit/s (rate $rate, loss ~$l%)"
        check "$dir rate <= 1.1 x limit" fcmp "$got <= 1.1 * $rate"
        if fcmp "$l < 5"; then
            check "$dir rate >= 0.5 x limit" fcmp "$got >= 0.5 * $rate"
        fi
    done
done

echo "-- cut / restore"
reqq apply_profile "$(bridge_config '{"delayMs":5}' '{"delayMs":5}')"
check "cut" reqq cut
read -r loss avg < <(ping_stats "$NS_GND" "$AIR_IP" 10)
check "100% loss while cut (got $loss%)" fcmp "$loss == 100"
check "restore" reqq restore
read -r loss avg < <(ping_stats "$NS_GND" "$AIR_IP" 10)
check "link back after restore (loss $loss%)" fcmp "$loss < 30"

echo "-- traffic classes: ICMP excluded stays clean during a cut"
cls='[{"name":"ICMP","proto":"icmp","mode":"excluded"}]'
reqq apply_profile "$(bridge_config '{"delayMs":40}' '{"delayMs":40}' "$cls")"
read -r loss avg < <(ping_stats "$NS_GND" "$AIR_IP" 10)
check "excluded ICMP has no delay (avg $avg ms)" fcmp "$avg < 20"
reqq cut
read -r loss avg < <(ping_stats "$NS_GND" "$AIR_IP" 10)
check "excluded ICMP passes during cut (loss $loss%)" fcmp "$loss < 30"
reqq restore

echo "-- read_stats"
stats=$(req read_stats)
check "stats are active" jqtest '.result.active == true' "$stats"
check "uplink total has packets" jqtest '.result.uplink.total.packets > 0' "$stats"

echo "-- reset"
check "reset" reqq reset
check "bridge removed" absent in_dut ip link show br-dlsim
check "no DLSim qdisc on dut-a" absent grepcmd ' 1d: ' in_dut tc qdisc show dev dut-a
stop_helper

echo "== crash leftovers"
start_helper "$NS_DUT"
reqq setup_bridge '{"groundIface":"dut-g","airIface":"dut-a"}'
reqq apply_profile "$(bridge_config '{"delayMs":10}' '{"delayMs":10}')"
kill_helper
check "leftover bridge present after crash" in_dut ip link show br-dlsim
start_helper "$NS_DUT"
check "restarted helper reports cleanup" jqtest '(.cleaned | length) > 0' "$READY"
check "leftover bridge removed" absent in_dut ip link show br-dlsim
check "leftover qdisc removed" absent grepcmd ' 1d: ' in_dut tc qdisc show dev dut-a
stop_helper

echo "== host mode (helper on the ground side, IFB for ingress)"
ip -n "$NS_DUT" link add br-plain type bridge
ip -n "$NS_DUT" link set dut-g master br-plain
ip -n "$NS_DUT" link set dut-a master br-plain
ip -n "$NS_DUT" link set br-plain up
start_helper "$NS_GND"
host_cfg=$(jq -cn '{config: {mode: "host", hostIface: "gnd0", otherMode: "excluded",
    uplink: {delayMs: 50}, downlink: {delayMs: 30},
    classes: [{name: "to vehicle", proto: "icmp", ip: "192.0.2.10", mode: "link"}]}}')
check "apply host profile" reqq apply_profile "$host_cfg"
check "ifb-dlsim exists" in_gnd ip link show ifb-dlsim
check "ingress redirect filter" grepcmd mirred in_gnd tc filter show dev gnd0 parent ffff:
read -r loss avg < <(ping_stats "$NS_GND" "$AIR_IP" 20)
check "rtt includes egress + ingress delay (avg $avg ms, expected ~80)" fcmp "$avg >= 70 && $avg <= 110"
check "cut (host)" reqq cut
read -r loss avg < <(ping_stats "$NS_GND" "$AIR_IP" 10)
check "100% loss while cut (host, got $loss%)" fcmp "$loss == 100"
reqq restore
stop_helper
check "IFB removed when the helper exits" absent in_gnd ip link show ifb-dlsim
check "ingress qdisc removed" absent grepcmd ingress in_gnd tc qdisc show dev gnd0
check "root qdisc removed" absent grepcmd ' 1d: ' in_gnd tc qdisc show dev gnd0

echo
if [[ $failcount -eq 0 ]]; then
    green "selftest: $pass checks passed"
else
    red "selftest: $failcount of $((pass + failcount)) checks failed"
fi
exit $((failcount > 0))
