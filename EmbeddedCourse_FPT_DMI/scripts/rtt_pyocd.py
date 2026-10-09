#!/usr/bin/env python3
"""Read the SEGGER RTT up channel 0 of the STM32H573 through pyocd (ST-LINK, no OpenOCD).

Used by rtt.sh when OpenOCD has no STM32H5 target file. It does not halt the target.

  rtt_pyocd.py [--seconds N] [--reset] [--all] [--send TEXT]
    --seconds N  stop after N seconds (default: until Ctrl+C)
    --reset      reset the target first, so the whole boot log is captured
    --all        also print what is already in the buffer (not yet read by anybody)
    --send TEXT  write TEXT + newline to down channel 0 (shell input) after attaching
"""
import argparse
import struct
import sys
import time

from pyocd.core.helpers import ConnectHelper

TARGET = "stm32h573ritx"
RAM_START, RAM_END = 0x20000000, 0x20000000 + 0x40000  # control block is in the first 256 KiB
ID = b"SEGGER RTT"


def find_block(t):
    for base in range(RAM_START, RAM_END, 0x1000):
        try:
            data = bytes(t.read_memory_block8(base, 0x1000 + 16))
        except Exception:  # AP error while the core is resetting: try again later
            return None
        i = data.find(ID)
        while i >= 0:
            if i % 4 == 0 and base + i < RAM_END:
                return base + i
            i = data.find(ID, i + 1)
    return None


def channel(t, addr, up, n=0):
    ups, downs = struct.unpack("<II", bytes(t.read_memory_block8(addr + 16, 8)))
    if n >= (ups if up else downs):
        return None
    off = addr + 24 + (0 if up else 24 * ups) + 24 * n
    _, buf, size, wr, rd, _ = struct.unpack("<IIIIII", bytes(t.read_memory_block8(off, 24)))
    return off, buf, size


def connect():
    """The first SWD attempt after the probe was idle sometimes fails (Get IDCODE error)."""
    for attempt in range(4):
        try:
            sess = ConnectHelper.session_with_chosen_probe(
                target_override=TARGET, connect_mode="attach", options={"frequency": 4000000})
            sess.open()
            return sess
        except Exception as e:  # pyocd raises ProbeError / TransferError here
            last = e
            time.sleep(0.5)
    sys.exit("[ERR] cannot connect over SWD: %s" % last)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seconds", type=float, default=0)
    ap.add_argument("--reset", action="store_true")
    ap.add_argument("--all", action="store_true")
    ap.add_argument("--send")
    a = ap.parse_args()

    with connect() as s:
        t = s.target
        addr = find_block(t)
        if addr is None and not a.reset:
            sys.exit("[ERR] RTT control block not found (is the firmware running?)")
        if a.reset:
            if addr is not None:
                t.write_memory_block8(addr, bytes(16))  # the id is written last at init
            t.reset()
            t0 = time.time()
            addr = None
            while addr is None and time.time() - t0 < 10:
                time.sleep(0.05)
                addr = find_block(t)
            if addr is None:
                sys.exit("[ERR] no RTT control block 10 s after reset")
            time.sleep(0.05)
        up = channel(t, addr, True)
        down = channel(t, addr, False)
        if up is None:
            sys.exit("[ERR] RTT has no up channel")
        up_off, buf, size = up
        if not a.all and not a.reset:
            wr = t.read32(up_off + 12)
            t.write32(up_off + 16, wr)  # skip old data
        if a.send and down:
            d_off, d_buf, d_size = down
            wr = t.read32(d_off + 12)
            msg = (a.send + "\n").encode()
            for b in msg:
                t.write8(d_buf + wr, b)
                wr = (wr + 1) % d_size
            t.write32(d_off + 12, wr)
        end = time.time() + a.seconds if a.seconds else None
        try:
            while end is None or time.time() < end:
                wr, rd = struct.unpack("<II", bytes(t.read_memory_block8(up_off + 12, 8)))
                if wr != rd and wr < size and rd < size:
                    if wr > rd:
                        data = bytes(t.read_memory_block8(buf + rd, wr - rd))
                    else:
                        data = bytes(t.read_memory_block8(buf + rd, size - rd)) + \
                            (bytes(t.read_memory_block8(buf, wr)) if wr else b"")
                    t.write32(up_off + 16, wr)
                    sys.stdout.write(data.decode(errors="replace"))
                    sys.stdout.flush()
                else:
                    time.sleep(0.05)
        except KeyboardInterrupt:
            pass


if __name__ == "__main__":
    main()
