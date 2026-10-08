#!/usr/bin/env python3
"""Minimal client for the Proxor macOS helper, used only by ci-integration.sh (python3 stdlib only).

Wire format: one JSON object per line over the helper's AF_UNIX socket. Replies carry the request
"id"; asynchronous events carry an "event" key and no meaningful id.

Exit codes: 0 ok, 1 failure, 3 connection closed without a reply (uid not allowed),
4 no eligible network service (sysproxy skip signal).
"""
import argparse
import json
import re
import socket
import subprocess
import sys
import time

SOCK = "/var/run/io.github.Ogstra.Proxor.helper.sock"
PROTOCOL = 1
# Same list as machelper.DefaultBypass / MacDefaultProxyBypass.
BYPASS = [
    "127.0.0.1", "localhost", "*.local",
    "169.254.0.0/16", "10.0.0.0/8", "172.16.0.0/12", "192.168.0.0/16", "100.64.0.0/10",
]
NO_SERVICE_MARKERS = ("no network service with a hardware device", "no eligible")


class Closed(Exception):
    """The helper closed the connection (or reset it) before a reply arrived."""


class Conn:
    def __init__(self):
        self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.sock.connect(SOCK)
        self.buf = b""
        self.events = []
        self.next_id = 1

    def close(self):
        try:
            self.sock.close()
        except OSError:
            pass

    def _read_message(self, deadline):
        """Next JSON message, or None if the deadline passes. Raises Closed on EOF/reset."""
        while b"\n" not in self.buf:
            remaining = deadline - time.time()
            if remaining <= 0:
                return None
            self.sock.settimeout(remaining)
            try:
                chunk = self.sock.recv(65536)
            except socket.timeout:
                return None
            except (ConnectionResetError, BrokenPipeError):
                raise Closed()
            if not chunk:
                raise Closed()
            self.buf += chunk
        line, self.buf = self.buf.split(b"\n", 1)
        return json.loads(line)

    def _note_event(self, msg):
        self.events.append(msg)
        if msg.get("event") == "log":
            print("[helper log] " + str(msg.get("line", "")), flush=True)
        else:
            print("[helper event] " + json.dumps(msg), flush=True)

    def request(self, cmd, timeout=30, **fields):
        rid = self.next_id
        self.next_id += 1
        req = {"id": rid, "cmd": cmd}
        req.update(fields)
        self.sock.settimeout(10)
        self.sock.sendall((json.dumps(req) + "\n").encode())
        deadline = time.time() + timeout
        while True:
            msg = self._read_message(deadline)
            if msg is None:
                raise TimeoutError("no reply to %s within %ss" % (cmd, timeout))
            if "event" in msg:
                self._note_event(msg)
                continue
            if msg.get("id") == rid:
                return msg

    def wait_event(self, names, timeout):
        """Wait for one of the named events; returns it or None on timeout."""
        deadline = time.time() + timeout
        while True:
            for i, ev in enumerate(self.events):
                if ev.get("event") in names:
                    return self.events.pop(i)
            msg = self._read_message(deadline)
            if msg is None:
                return None
            if "event" in msg:
                self._note_event(msg)


def connect_and_hello():
    c = Conn()
    reply = c.request("hello", timeout=10, protocol=PROTOCOL)
    if not reply.get("ok") or reply.get("protocol") != PROTOCOL:
        print("hello failed: " + json.dumps(reply), file=sys.stderr)
        sys.exit(1)
    return c, reply


def sh(*argv):
    return subprocess.run(argv, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True).stdout


def test_net_routes():
    return [l for l in sh("netstat", "-rn", "-f", "inet").splitlines() if l.startswith("198.51.100")]


def tun_route():
    """utun name carrying the TEST-NET-2 route, or None."""
    for line in test_net_routes():
        m = re.search(r"\b(utun\d+)\b", line)
        if m:
            return m.group(1)
    return None


def wait_for(pred, timeout, step=0.25):
    end = time.time() + timeout
    while time.time() < end:
        v = pred()
        if v:
            return v
        time.sleep(step)
    return pred()


def norm_proxy(text):
    """Drop `<Kind>Enable : 0` lines: a service that never had a proxy has no such key, and
    networksetup can only set it to 0 (never delete it), so absent and 0 are the same state."""
    return "".join(l for l in text.splitlines(True) if not re.match(r"^\s*\w*Enable\s*:\s*0\s*$", l))


def scutil_proxy():
    return norm_proxy(sh("scutil", "--proxy"))


def shows_proxy(text, port):
    # The Enable keys matter: the helper no longer sends separate `-set*proxystate on` writes, it
    # relies on -setwebproxy/-setsecurewebproxy/-setsocksfirewallproxy turning the proxy on.
    return (re.search(r"HTTPProxy\s*:\s*127\.0\.0\.1\b", text) is not None
            and re.search(r"HTTPPort\s*:\s*%d\b" % port, text) is not None
            and re.search(r"HTTPEnable\s*:\s*1\b", text) is not None
            and re.search(r"HTTPSEnable\s*:\s*1\b", text) is not None
            and re.search(r"SOCKSEnable\s*:\s*1\b", text) is not None)


def no_service(reply):
    err = str(reply.get("error", "")).lower()
    return any(m in err for m in NO_SERVICE_MARKERS)


def cmd_hello(_args):
    try:
        _c, reply = connect_and_hello()
    except (Closed, BrokenPipeError, ConnectionResetError):
        # A peer the helper rejects is dropped before it reads; depending on timing the
        # client notices on the write (broken pipe) or on the read (EOF).
        print("connection closed without a reply", file=sys.stderr)
        return 3
    print(json.dumps(reply))
    return 0


def cmd_status(_args):
    c, _ = connect_and_hello()
    print(json.dumps(c.request("status", timeout=10)))
    return 0


def cmd_tun(args):
    config = open(args.config, encoding="utf-8").read()
    c, _ = connect_and_hello()

    def start_and_wait():
        reply = c.request("tun_start", timeout=30, config=config, socksPort=args.port)
        if not reply.get("ok"):
            print("tun_start refused: " + json.dumps(reply), file=sys.stderr)
            return False
        ev = c.wait_event(("tun_ready", "tun_stopped"), 60)
        if ev is None:
            print("no tun_ready within 60 s", file=sys.stderr)
            return False
        if ev.get("event") != "tun_ready":
            print("tun stopped instead of becoming ready: " + json.dumps(ev), file=sys.stderr)
            return False
        return True

    if not start_and_wait():
        return 1
    utun = wait_for(tun_route, 10)
    if not utun:
        print("no 198.51.100 route via utunN after tun_ready; netstat:\n" + sh("netstat", "-rn", "-f", "inet"),
              file=sys.stderr)
        return 1
    print("TUN_UTUN=" + utun, flush=True)

    reply = c.request("tun_stop", timeout=30)
    if not reply.get("ok"):
        print("tun_stop failed: " + json.dumps(reply), file=sys.stderr)
        return 1
    if not wait_for(lambda: not test_net_routes(), 5):
        print("198.51.100 route still present 5 s after tun_stop:\n" + "\n".join(test_net_routes()),
              file=sys.stderr)
        return 1
    print("route removed after tun_stop", flush=True)

    if not start_and_wait():
        return 1
    if not wait_for(tun_route, 10):
        print("route missing after the second tun_start", file=sys.stderr)
        return 1
    print("second start up; closing the connection abruptly", flush=True)
    c.close()
    return 0


def apply_proxy(c, port):
    """sysproxy_apply; the reply gets "_ms": request sent -> reply received."""
    t0 = time.monotonic()
    reply = c.request("sysproxy_apply", timeout=30, port=port, bypass=BYPASS)
    reply["_ms"] = int((time.monotonic() - t0) * 1000)
    if not reply.get("ok"):
        return reply
    print("applied: %s failed: %s" % (reply.get("applied"), reply.get("failed")), flush=True)
    return reply


def timed_request(c, cmd, **fields):
    """(reply, milliseconds from request sent to reply received)."""
    t0 = time.monotonic()
    reply = c.request(cmd, timeout=30, **fields)
    return reply, int((time.monotonic() - t0) * 1000)


def cmd_sysproxy_apply(args):
    c, _ = connect_and_hello()
    reply = apply_proxy(c, args.port)
    if not reply.get("ok"):
        print("sysproxy_apply failed: " + json.dumps(reply), file=sys.stderr)
        return 4 if no_service(reply) else 1
    print("APPLIED", flush=True)
    if args.hold:
        while True:  # the lease: the shell kills this process to test the cleanup
            time.sleep(3600)
    return 0


def cmd_sysproxy_restore(_args):
    c, _ = connect_and_hello()
    reply = c.request("sysproxy_restore", timeout=30)
    if not reply.get("ok"):
        print("sysproxy_restore failed: " + json.dumps(reply), file=sys.stderr)
        return 1
    print("RESTORED", flush=True)
    return 0


def cmd_sysproxy_cycle(args):
    """The GUI's profile Stop -> Start on ONE connection (lease): apply, restore, apply, restore.
    Prints SYSPROXY_TIMING per round; with --max-ms N round 2 must finish each call within N ms."""
    before = norm_proxy(open(args.before, encoding="utf-8").read())
    c, _ = connect_and_hello()

    def fail(what):
        print("sysproxy-cycle: " + what, file=sys.stderr)
        import difflib
        now = scutil_proxy()
        sys.stderr.write("".join(difflib.unified_diff(
            before.splitlines(True), now.splitlines(True), "before", "now")))
        return 1

    timings = {}
    for round_no in (1, 2):
        reply = apply_proxy(c, args.port)
        if not reply.get("ok"):
            print("sysproxy_apply failed: " + json.dumps(reply), file=sys.stderr)
            return 4 if no_service(reply) else 1
        apply_ms = reply["_ms"]
        services = len(reply.get("applied") or [])
        if not wait_for(lambda: shows_proxy(scutil_proxy(), args.port), 10):
            return fail("round %d: scutil never showed 127.0.0.1:%d with the three proxies enabled"
                        % (round_no, args.port))
        reply, restore_ms = timed_request(c, "sysproxy_restore")
        if not reply.get("ok"):
            return fail("round %d: restore failed: %s" % (round_no, json.dumps(reply)))
        if not wait_for(lambda: scutil_proxy() == before, 10):
            return fail("round %d: scutil did not return to the pre-test state" % round_no)
        timings[round_no] = (apply_ms, restore_ms)
        print("SYSPROXY_TIMING round=%d apply_ms=%d restore_ms=%d services=%d"
              % (round_no, apply_ms, restore_ms, services), flush=True)
        print("cycle round %d ok" % round_no, flush=True)
    if args.max_ms > 0:
        apply_ms, restore_ms = timings[2]
        for what, ms in (("apply", apply_ms), ("restore", restore_ms)):
            if ms > args.max_ms:
                print("sysproxy-cycle: round 2 %s took %d ms, budget %d ms" % (what, ms, args.max_ms),
                      file=sys.stderr)
                return 1
    return 0


def cmd_ns_call_ms(_args):
    """Median wall time of one read-only networksetup call, to scale the timing budget to the runner."""
    samples = []
    for _ in range(5):
        t0 = time.monotonic()
        sh("/usr/sbin/networksetup", "-listallnetworkservices")
        samples.append(int((time.monotonic() - t0) * 1000))
    samples.sort()
    print("NS_CALL_MS=%d" % samples[len(samples) // 2], flush=True)
    return 0


def cmd_pause_resume(args):
    """The GUI's profile Stop -> Start with Tun + System Proxy on ONE connection (the lease):
    apply + tun_start -> PAUSE (restore + tun_stop) -> RESUME (apply + tun_start) -> close."""
    before = norm_proxy(open(args.before, encoding="utf-8").read())
    use_tun, use_proxy = not args.no_tun, not args.no_proxy
    config = open(args.config, encoding="utf-8").read() if use_tun else ""
    c, _ = connect_and_hello()

    def fail(what):
        print("pause-resume: " + what, file=sys.stderr)
        return 1

    def start_tun():
        """tun_start, wait tun_ready and the route; returns milliseconds, or None on failure."""
        t0 = time.monotonic()
        reply = c.request("tun_start", timeout=30, config=config, socksPort=args.port)
        if not reply.get("ok"):
            print("tun_start refused: " + json.dumps(reply), file=sys.stderr)
            return None
        ev = c.wait_event(("tun_ready", "tun_stopped"), 60)
        if ev is None or ev.get("event") != "tun_ready":
            print("no tun_ready (got %s)" % json.dumps(ev), file=sys.stderr)
            return None
        if not wait_for(tun_route, 10):
            print("no 198.51.100 route via utunN after tun_ready", file=sys.stderr)
            return None
        return int((time.monotonic() - t0) * 1000)

    if use_proxy:
        reply = apply_proxy(c, args.proxy_port)
        if not reply.get("ok"):
            print("sysproxy_apply failed: " + json.dumps(reply), file=sys.stderr)
            return 4 if no_service(reply) else 1
    if use_tun and start_tun() is None:
        return 1

    # PAUSE
    restore_ms = tun_stop_ms = 0
    if use_proxy:
        reply, restore_ms = timed_request(c, "sysproxy_restore")
        if not reply.get("ok"):
            return fail("restore failed: " + json.dumps(reply))
    if use_tun:
        reply, tun_stop_ms = timed_request(c, "tun_stop")
        if not reply.get("ok"):
            return fail("tun_stop failed: " + json.dumps(reply))
    print("PAUSE_TIMING restore_ms=%d tun_stop_ms=%d" % (restore_ms, tun_stop_ms), flush=True)
    if use_tun and not wait_for(lambda: not test_net_routes(), 5):
        return fail("198.51.100 route still present 5 s after the pause:\n" + "\n".join(test_net_routes()))
    if use_proxy and not wait_for(lambda: scutil_proxy() == before, 5):
        return fail("scutil did not return to the pre-test state within 5 s of the pause")
    # the connection must still be usable and nothing may be left running
    status = c.request("status", timeout=10)
    if not status.get("ok"):
        return fail("status after the pause: " + json.dumps(status))
    # the helper omits false booleans (json omitempty)
    if status.get("tunRunning", False) or status.get("proxyApplied", False):
        return fail("state leaked after the pause: " + json.dumps(status))
    if use_tun and tun_stop_ms > 3000:
        return fail("tun_stop took %d ms, budget 3000 ms" % tun_stop_ms)
    c.events[:] = []  # nothing from the first run may satisfy the second wait

    # RESUME
    apply_ms = 0
    if use_proxy:
        reply = apply_proxy(c, args.proxy_port)
        if not reply.get("ok"):
            return fail("second sysproxy_apply failed: " + json.dumps(reply))
        apply_ms = reply["_ms"]
    tun_ready_ms = 0
    if use_tun:
        tun_ready_ms = start_tun()
        if tun_ready_ms is None:
            return fail("second tun_start failed")
    print("RESUME_TIMING apply_ms=%d tun_ready_ms=%d" % (apply_ms, tun_ready_ms), flush=True)
    if use_proxy and not wait_for(lambda: shows_proxy(scutil_proxy(), args.proxy_port), 10):
        return fail("scutil never showed 127.0.0.1:%d after the resume" % args.proxy_port)
    print("PAUSE_RESUME_OK", flush=True)
    c.close()  # abrupt: the shell asserts the lease cleanup
    return 0


def cmd_uninstall(_args):
    c, _ = connect_and_hello()
    reply = c.request("uninstall", timeout=30)
    if not reply.get("ok"):
        print("uninstall failed: " + json.dumps(reply), file=sys.stderr)
        return 1
    print("UNINSTALL_ACCEPTED", flush=True)
    return 0


def main():
    p = argparse.ArgumentParser()
    sub = p.add_subparsers(dest="sub", required=True)
    sub.add_parser("hello")
    sub.add_parser("status")
    t = sub.add_parser("tun")
    t.add_argument("--config", required=True)
    t.add_argument("--port", type=int, required=True)
    a = sub.add_parser("sysproxy-apply")
    a.add_argument("--port", type=int, required=True)
    a.add_argument("--hold", action="store_true")
    cyc = sub.add_parser("sysproxy-cycle")
    cyc.add_argument("--port", type=int, required=True)
    cyc.add_argument("--before", required=True)
    cyc.add_argument("--max-ms", type=int, default=0, help="round 2 apply/restore budget; 0 = no check")
    sub.add_parser("ns-call-ms")
    pr = sub.add_parser("pause-resume")
    pr.add_argument("--config", required=True)
    pr.add_argument("--port", type=int, required=True, help="mock SOCKS port for tun_start")
    pr.add_argument("--proxy-port", type=int, required=True)
    pr.add_argument("--before", required=True)
    pr.add_argument("--no-tun", action="store_true")
    pr.add_argument("--no-proxy", action="store_true")
    sub.add_parser("sysproxy-restore")
    sub.add_parser("uninstall")
    args = p.parse_args()
    handler = {
        "hello": cmd_hello,
        "status": cmd_status,
        "tun": cmd_tun,
        "sysproxy-apply": cmd_sysproxy_apply,
        "sysproxy-cycle": cmd_sysproxy_cycle,
        "ns-call-ms": cmd_ns_call_ms,
        "pause-resume": cmd_pause_resume,
        "sysproxy-restore": cmd_sysproxy_restore,
        "uninstall": cmd_uninstall,
    }[args.sub]
    try:
        return handler(args)
    except Closed:
        print("helper closed the connection", file=sys.stderr)
        return 3
    except Exception as e:  # noqa: BLE001 - a CI client: report and fail
        print("%s: %r" % (args.sub, e), file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
