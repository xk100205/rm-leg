#!/usr/bin/env python3
"""Keyboard teleop for phase-1 wheel test.

The board must already be in the phase-1 firmware. This script never asks
the firmware to balance or to move a leg.

Line protocol, one command per line, 115200 8N1 on UART5 (PC12 TX, PB12 RX):

    SEL 5|6     select a wheel. SEL 1..4 is sent and then ESTOP; firmware
                rejects the leg selector and does not run RS01.
    SPD <dps>   speed magnitude in degrees per second
    DIR -1|0|1  direction. Firmware applies the right-wheel sign.
    TELEOP      enter TELEOP from TEST only
    CLEAR       explicit exit from ESTOP or FAULT back to TEST
    ESTOP       latch a stop

Keys:
    1..4   SEL n and ESTOP (legs stay off)
    5 / 6  select left / right wheel
    W / S  speed up / down by SPD_STEP_DPS, clamped to SPD_MAX_DPS
    A / D  direction -1 / +1. While one is held, resend every 50 ms.
            Releasing the last direction key sends ESTOP.
    space  DIR 0 and stop resending. The firmware deadman latches if no
            further line arrives within 100 ms.
    Esc    ESTOP immediately
    C      CLEAR
    T      TELEOP

Motion lines are accepted by the firmware only after TELEOP is already
active, so hold the direction key (the 50 ms resend) rather than tapping
once. After ESTOP, press C, then T, then the direction key again.

Placeholders matched to the firmware header: step 30 dps, max 360 dps.
"""

from __future__ import annotations

import argparse
import os
import select
import struct
import sys
import time

SPD_STEP_DPS = 30.0
SPD_MAX_DPS = 360.0
RESEND_S = 0.05

# q is 8 bytes on Linux and Windows. ll is 8 only where long is 8.
EVENT_FMT = "qqHHi"
EV_KEY = 0x01

# Linux input key codes.
KEY_NAMES = {
    1: "esc",
    2: "1",
    3: "2",
    4: "3",
    5: "4",
    6: "5",
    7: "6",
    17: "w",
    20: "t",
    30: "a",
    31: "s",
    32: "d",
    46: "c",
    57: "space",
}


def format_spd(spd: float) -> str:
    rounded = round(spd)
    if abs(spd - rounded) < 1e-6:
        return str(int(rounded))
    return f"{spd:.1f}"


class KeyboardTeleop:
    """Pure key-to-line mapping. `now` is seconds from an arbitrary clock."""

    def __init__(self) -> None:
        self.sel: int | None = None
        self.spd = 0.0
        self.held: set[str] = set()
        self.last_tx: float | None = None

    def direction(self) -> int | None:
        left = "a" in self.held
        right = "d" in self.held
        if left and right:
            return 0
        if left:
            return -1
        if right:
            return 1
        return None

    def motion_lines(self) -> list[str]:
        if self.sel not in (5, 6):
            return []
        direction = self.direction()
        if direction is None:
            return []
        return [
            f"SEL {self.sel}",
            f"SPD {format_spd(self.spd)}",
            f"DIR {direction}",
        ]

    def handle(self, key: str, pressed: bool, now: float) -> list[str]:
        if key in ("1", "2", "3", "4") and pressed:
            self.sel = int(key)
            self.held.discard("a")
            self.held.discard("d")
            self.last_tx = None
            return [f"SEL {self.sel}", "ESTOP"]

        if key in ("5", "6") and pressed:
            self.sel = int(key)
            lines = [f"SEL {self.sel}"]
            extra = self._refresh(now)
            return lines + extra

        if key == "w" and pressed:
            self.spd = min(SPD_MAX_DPS, self.spd + SPD_STEP_DPS)
            return self._with_motion(f"SPD {format_spd(self.spd)}", now)

        if key == "s" and pressed:
            self.spd = max(0.0, self.spd - SPD_STEP_DPS)
            return self._with_motion(f"SPD {format_spd(self.spd)}", now)

        if key in ("a", "d") and pressed:
            self.held.add(key)
            self.last_tx = now
            return self.motion_lines()

        if key in ("a", "d") and not pressed:
            if key not in self.held:
                return []
            self.held.discard(key)
            if self.direction() is None:
                self.last_tx = None
                return ["ESTOP"]
            self.last_tx = now
            return self.motion_lines()

        if key == "space" and pressed:
            self.held.discard("a")
            self.held.discard("d")
            self.last_tx = None
            return ["DIR 0"]

        if key == "esc" and pressed:
            self.held.discard("a")
            self.held.discard("d")
            self.last_tx = None
            return ["ESTOP"]

        if key == "c" and pressed:
            return ["CLEAR"]

        if key == "t" and pressed:
            return ["TELEOP"]

        return []

    def poll(self, now: float) -> list[str]:
        if self.direction() is None or self.sel not in (5, 6):
            return []
        if self.last_tx is None or now - self.last_tx >= RESEND_S:
            self.last_tx = now
            return self.motion_lines()
        return []

    def _refresh(self, now: float) -> list[str]:
        if self.direction() is None:
            return []
        self.last_tx = now
        # SEL was already emitted by the caller.
        lines = self.motion_lines()
        return lines[1:] if lines else []

    def _with_motion(self, first: str, now: float) -> list[str]:
        if self.direction() is None or self.sel not in (5, 6):
            return [first]
        self.last_tx = now
        rest = self.motion_lines()
        if rest and rest[1] == first:
            return rest
        return [first] + rest


def _self_test() -> int:
    failures = 0

    def check(cond: bool, message: str) -> None:
        nonlocal failures
        if not cond:
            print(f"FAIL {message}", file=sys.stderr)
            failures += 1

    tele = KeyboardTeleop()
    check(tele.handle("5", True, 0.0) == ["SEL 5"], "select 5")
    check(tele.handle("t", True, 0.0) == ["TELEOP"], "teleop")
    check(tele.handle("w", True, 0.0) == ["SPD 30"], "speed up")
    check(
        tele.handle("d", True, 0.0) == ["SEL 5", "SPD 30", "DIR 1"],
        "direction press",
    )
    check(tele.poll(0.049) == [], "no early resend")
    check(
        tele.poll(0.05) == ["SEL 5", "SPD 30", "DIR 1"],
        "50 ms resend",
    )
    check(
        tele.handle("w", True, 0.05) == ["SEL 5", "SPD 60", "DIR 1"],
        "speed change while a direction is held",
    )
    check(tele.handle("d", False, 0.06) == ["ESTOP"], "release estop")
    check(tele.poll(1.0) == [], "no resend after release")

    tele = KeyboardTeleop()
    tele.handle("6", True, 0.0)
    tele.handle("w", True, 0.0)
    check(
        tele.handle("a", True, 0.0) == ["SEL 6", "SPD 30", "DIR -1"],
        "a is negative",
    )
    check(
        tele.handle("d", True, 0.0) == ["SEL 6", "SPD 30", "DIR 0"],
        "both directions hold zero",
    )
    check(
        tele.handle("a", False, 0.0) == ["SEL 6", "SPD 30", "DIR 1"],
        "remaining direction",
    )
    check(tele.handle("d", False, 0.0) == ["ESTOP"], "last release")

    tele = KeyboardTeleop()
    tele.handle("5", True, 0.0)
    tele.handle("d", True, 0.0)
    check(tele.handle("2", True, 0.0) == ["SEL 2", "ESTOP"], "leg key stops")
    check(tele.poll(1.0) == [], "leg key cancels resend")
    check(tele.handle("d", False, 1.1) == [], "dropped hold does not double estop")

    tele = KeyboardTeleop()
    tele.handle("5", True, 0.0)
    for _ in range(20):
        tele.handle("w", True, 0.0)
    check(tele.spd == SPD_MAX_DPS, "speed clamp")
    check(tele.handle("s", True, 0.0) == [f"SPD {int(SPD_MAX_DPS - SPD_STEP_DPS)}"], "speed down")

    tele = KeyboardTeleop()
    tele.handle("5", True, 0.0)
    tele.handle("w", True, 0.0)
    tele.handle("d", True, 0.0)
    check(tele.handle("space", True, 0.2) == ["DIR 0"], "space is dir 0")
    check(tele.poll(1.0) == [], "space stops resend")
    check(tele.handle("esc", True, 1.0) == ["ESTOP"], "esc")
    check(tele.handle("c", True, 1.0) == ["CLEAR"], "clear")

    tele = KeyboardTeleop()
    check(tele.handle("d", True, 0.0) == [], "no motion before a wheel is selected")
    check(tele.handle("d", False, 0.0) == ["ESTOP"], "release still estops")

    if failures:
        print(f"{failures} keyboard check(s) failed", file=sys.stderr)
        return 1
    print("keyboard teleop checks passed")
    return 0


def _ev_name(fd: int) -> str:
    # EVIOCGNAME(256) = _IOC(_IOC_READ, 'E', 0x06, 256)
    request = (2 << 30) | (256 << 16) | (ord("E") << 8) | 0x06
    buf = bytearray(256)
    import fcntl

    fcntl.ioctl(fd, request, buf)
    return buf.split(b"\x00", 1)[0].decode("utf-8", "replace")


def list_devices() -> int:
    print("input devices:")
    for name in sorted(os.listdir("/dev/input")):
        if not name.startswith("event"):
            continue
        path = os.path.join("/dev/input", name)
        try:
            fd = os.open(path, os.O_RDONLY | os.O_NONBLOCK)
        except OSError as exc:
            print(f"  {path}: {exc}")
            continue
        try:
            print(f"  {path}: {_ev_name(fd)}")
        except OSError as exc:
            print(f"  {path}: {exc}")
        finally:
            os.close(fd)
    return 0


def open_serial(path: str) -> int:
    import termios

    fd = os.open(path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    attrs = termios.tcgetattr(fd)
    attrs[0] &= ~(
        termios.IGNBRK
        | termios.BRKINT
        | termios.PARMRK
        | termios.ISTRIP
        | termios.INLCR
        | termios.IGNCR
        | termios.ICRNL
        | termios.IXON
    )
    attrs[1] &= ~termios.OPOST
    attrs[2] &= ~(termios.CSIZE | termios.PARENB)
    attrs[2] |= termios.CS8 | termios.CREAD | termios.CLOCAL
    attrs[3] &= ~(termios.ICANON | termios.ECHO | termios.ECHOE | termios.ISIG)
    attrs[6][termios.VMIN] = 0
    attrs[6][termios.VTIME] = 0
    attrs[4] = termios.B115200
    attrs[5] = termios.B115200
    termios.tcsetattr(fd, termios.TCSANOW, attrs)
    termios.tcflush(fd, termios.TCIOFLUSH)
    return fd


class EventReader:
    def __init__(self, fd: int) -> None:
        self.fd = fd
        self.buf = b""
        self.size = struct.calcsize(EVENT_FMT)

    def read(self) -> list[tuple[int, int, int]]:
        try:
            chunk = os.read(self.fd, 1024)
        except BlockingIOError:
            chunk = b""
        self.buf += chunk
        out = []
        while len(self.buf) >= self.size:
            _sec, _usec, typ, code, value = struct.unpack_from(EVENT_FMT, self.buf)
            self.buf = self.buf[self.size :]
            out.append((typ, code, value))
        return out


def write_all(fd: int, payload: bytes) -> None:
    view = payload
    while view:
        n = os.write(fd, view)
        if n <= 0:
            raise OSError("short UART write")
        view = view[n:]


def emit(lines: list[str], serial_fd: int | None, verbose: bool) -> None:
    if not lines:
        return
    payload = "".join(f"{line}\n" for line in lines).encode("ascii")
    if serial_fd is not None:
        write_all(serial_fd, payload)
    if serial_fd is None or verbose:
        sys.stdout.write(payload.decode("ascii"))
        sys.stdout.flush()


def live(device: str, port: str | None, verbose: bool) -> int:
    if struct.calcsize(EVENT_FMT) != 24:
        print("this host's input_event is not the 64-bit 24-byte layout", file=sys.stderr)
        return 1
    kbd = os.open(device, os.O_RDONLY | os.O_NONBLOCK)
    serial_fd = open_serial(port) if port else None
    reader = EventReader(kbd)
    tele = KeyboardTeleop()
    print(
        "phase-1 wheel teleop. C clears a latch, T enters TELEOP, "
        "hold A/D to drive, release or Esc stops. Ctrl-C sends ESTOP and exits.",
        file=sys.stderr,
    )
    try:
        while True:
            now = time.monotonic()
            readable, _, _ = select.select([kbd], [], [], 0.01)
            if readable:
                for typ, code, value in reader.read():
                    if typ != EV_KEY or value == 2:
                        continue
                    name = KEY_NAMES.get(code)
                    if name is None:
                        continue
                    emit(tele.handle(name, value == 1, now), serial_fd, verbose)
            emit(tele.poll(time.monotonic()), serial_fd, verbose)
    except KeyboardInterrupt:
        emit(["ESTOP"], serial_fd, True)
        print("\nESTOP sent", file=sys.stderr)
        return 0
    finally:
        os.close(kbd)
        if serial_fd is not None:
            os.close(serial_fd)


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description="Phase-1 wheel keyboard teleop")
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--list-devices", action="store_true")
    parser.add_argument("--device", help="keyboard event node, e.g. /dev/input/event3")
    parser.add_argument("--port", help="UART tty, e.g. /dev/ttyUSB0. Omit for a dry run.")
    parser.add_argument("--verbose", action="store_true", help="print lines while writing the UART")
    args = parser.parse_args(argv)
    if args.self_test:
        return _self_test()
    if args.list_devices:
        return list_devices()
    if not args.device:
        parser.error("--device is required (try --list-devices)")
    return live(args.device, args.port, args.verbose or args.port is None)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
