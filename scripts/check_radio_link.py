#!/usr/bin/env python3
"""
Automatic radio link check between two Meshtastic_mini devices.

Connects to both serial ports, sets node IDs (N1, N2), sends a message from
device 0 and checks that device 1 receives it and replies with "pong", then
optionally does the reverse. Exits with 0 on success, 1 on failure.

Usage:
  pip install pyserial   # once
  python3 scripts/check_radio_link.py
  python3 scripts/check_radio_link.py /dev/ttyUSB0 /dev/ttyUSB1
  python3 scripts/check_radio_link.py --timeout 8
"""
import argparse
import sys
import threading
import time

try:
    import serial
except ImportError:
    print("Install pyserial: pip install pyserial", file=sys.stderr)
    sys.exit(1)

BAUD = 115200
DEFAULT_PORTS = ["/dev/ttyUSB0", "/dev/ttyUSB1"]
READ_TIMEOUT = 0.2
DEFAULT_TEST_TIMEOUT = 15   # SF11 LoRa ~200–400 ms per packet; allow time for pong round-trip


def collect_output(ser, label, lines_list):
    """Read from serial and append lines to lines_list; each item is (label, line)."""
    buf = b""
    # args_quiet is set in main() before threads start
    global args_quiet
    try:
        while ser.is_open:
            data = ser.read(ser.in_waiting or 1)
            if not data:
                time.sleep(0.02)
                continue
            buf += data
            while b"\r" in buf or b"\n" in buf:
                i = buf.find(b"\r")
                j = buf.find(b"\n")
                k = min(i if i >= 0 else 9999, j if j >= 0 else 9999)
                if k == 9999:
                    break
                line = buf[:k].decode("utf-8", errors="replace").strip()
                buf = buf[k + 1:]
                if buf.startswith(b"\n"):
                    buf = buf[1:]
                elif buf.startswith(b"\r"):
                    buf = buf[1:]
                if line:
                    lines_list.append((label, line, time.monotonic()))
                    if not args_quiet:
                        print(f"[{label}] {line}")
    except (serial.SerialException, OSError):
        pass


def send_line(ser, line):
    if not line.endswith("\n"):
        line = line + "\n"
    ser.write(line.encode("utf-8"))
    ser.flush()


def main():
    ap = argparse.ArgumentParser(description="Automatic radio link check between two devices")
    ap.add_argument("port0", nargs="?", default=DEFAULT_PORTS[0], help="First port (device A)")
    ap.add_argument("port1", nargs="?", default=DEFAULT_PORTS[1], help="Second port (device B)")
    ap.add_argument("-b", "--baud", type=int, default=BAUD, help="Baud rate")
    ap.add_argument("-t", "--timeout", type=float, default=DEFAULT_TEST_TIMEOUT,
                    help="Seconds to wait for expected replies (default: %(default)s)")
    ap.add_argument("-q", "--quiet", action="store_true", help="Only print PASS/FAIL")

    global args_quiet
    args = ap.parse_args()
    args_quiet = args.quiet

    received = []
    ports = [args.port0, args.port1]
    labels = [args.port0.split("/")[-1], args.port1.split("/")[-1]]
    ser_list = []

    try:
        for port, label in zip(ports, labels):
            s = serial.Serial(port, args.baud, timeout=READ_TIMEOUT)
            ser_list.append((s, label))
            if not args.quiet:
                print(f"Opened {port} as [{label}]")
    except serial.SerialException as e:
        print(f"Error: {e}", file=sys.stderr)
        return 1

    try:
        for s, label in ser_list:
            t = threading.Thread(target=collect_output, args=(s, label, received), daemon=True)
            t.start()

        time.sleep(0.3)

        # Set node IDs
        if not args.quiet:
            print("Setting node_id: N1 on port0, N2 on port1...")
        send_line(ser_list[0][0], "N1")
        send_line(ser_list[1][0], "N2")
        time.sleep(0.5)

        # Clear any old lines from boot
        received.clear()

        def ping_pong(sender_idx, receiver_idx, msg):
            """Send msg from sender, expect RX on receiver + pong back to sender.
            Returns (ok, t_rx_ms, t_pong_ms) — latencies from send moment."""
            s_label = labels[sender_idx]
            r_label = labels[receiver_idx]
            if not args.quiet:
                print(f"[{s_label}] -> [{r_label}]: '{msg}'")

            t_send = time.monotonic()
            send_line(ser_list[sender_idx][0], msg)

            deadline = t_send + args.timeout
            t_rx = None
            t_pong = None
            while time.monotonic() < deadline:
                time.sleep(0.1)
                for label, line, ts in list(received):
                    if label == r_label and f"RX: {msg}" in line and t_rx is None:
                        t_rx = ts
                    if label == s_label and "RX: pong" in line and t_pong is None:
                        t_pong = ts
                if t_rx is not None and t_pong is not None:
                    break

            if t_rx is None:
                print(f"FAIL: [{r_label}] did not receive '{msg}'", file=sys.stderr)
                return False, None, None
            if t_pong is None:
                print(f"FAIL: [{s_label}] did not receive 'pong'", file=sys.stderr)
                return False, None, None

            rx_ms = (t_rx - t_send) * 1000.0
            pong_ms = (t_pong - t_send) * 1000.0
            if not args.quiet:
                print(f"  OK: [{r_label}] received '{msg}', [{s_label}] received 'pong'")
                print(f"  Latency: TX->RX {rx_ms:.0f} ms, round-trip {pong_ms:.0f} ms")
            return True, rx_ms, pong_ms

        # Direction 1: Device 0 → Device 1
        ok1, rx1, rt1 = ping_pong(0, 1, "ping")
        if not ok1:
            return 1

        time.sleep(1.0)
        received.clear()

        # Direction 2: Device 1 → Device 0
        ok2, rx2, rt2 = ping_pong(1, 0, "ping")
        if not ok2:
            return 1

        if not args.quiet:
            print(f"\n--- Latency summary ---")
            print(f"  {labels[0]} -> {labels[1]}:  TX->RX {rx1:.0f} ms,  round-trip {rt1:.0f} ms")
            print(f"  {labels[1]} -> {labels[0]}:  TX->RX {rx2:.0f} ms,  round-trip {rt2:.0f} ms")
            avg_rx = (rx1 + rx2) / 2
            avg_rt = (rt1 + rt2) / 2
            print(f"  Average:           TX->RX {avg_rx:.0f} ms,  round-trip {avg_rt:.0f} ms")
            print(f"\nPASS: radio link OK (both directions)")
        else:
            print("PASS")
        return 0

    finally:
        for s, _ in ser_list:
            try:
                s.close()
            except Exception:
                pass


if __name__ == "__main__":
    sys.exit(main())
