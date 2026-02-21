#!/usr/bin/env python3
"""
Monitor two Meshtastic_mini devices on serial ports (e.g. /dev/ttyUSB0, /dev/ttyUSB1).
Shows output from both with [port] prefix. Optional: send to device by typing "0: text" or "1: text".

Usage:
  pip install pyserial   # once
  python3 scripts/dual_serial_monitor.py
  python3 scripts/dual_serial_monitor.py /dev/ttyUSB0 /dev/ttyACM1

Exit: Ctrl+C
"""
import argparse
import sys
import threading

try:
    import serial
except ImportError:
    print("Install pyserial: pip install pyserial", file=sys.stderr)
    sys.exit(1)

BAUD = 115200
DEFAULT_PORTS = ["/dev/ttyUSB0", "/dev/ttyUSB1"]


def read_loop(ser, label):
    try:
        while ser.is_open:
            data = ser.read(ser.in_waiting or 1)
            if not data:
                continue
            text = data.decode("utf-8", errors="replace")
            for line in text.splitlines(keepends=True):
                if line:
                    print(f"[{label}] {line}", end="" if line.endswith("\n") else "\n")
    except (serial.SerialException, OSError) as e:
        print(f"[{label}] Error: {e}", file=sys.stderr)
    finally:
        try:
            ser.close()
        except Exception:
            pass


def main():
    ap = argparse.ArgumentParser(description="Monitor two devices on serial ports")
    ap.add_argument("port0", nargs="?", default=DEFAULT_PORTS[0], help="First port (default: %(default)s)")
    ap.add_argument("port1", nargs="?", default=DEFAULT_PORTS[1], help="Second port (default: %(default)s)")
    ap.add_argument("-b", "--baud", type=int, default=BAUD, help=f"Baud rate (default: %(default)s)")
    ap.add_argument("--no-send", action="store_true", help="Only monitor; do not accept keyboard input to send")
    args = ap.parse_args()

    ports = [args.port0, args.port1]
    labels = [args.port0.split("/")[-1], args.port1.split("/")[-1]]
    ser_list = []

    for port, label in zip(ports, labels):
        try:
            s = serial.Serial(port, args.baud, timeout=0.1)
            ser_list.append((s, label))
            print(f"Opened {port} as [{label}]", file=sys.stderr)
        except serial.SerialException as e:
            print(f"Cannot open {port}: {e}", file=sys.stderr)
            sys.exit(1)

    for s, label in ser_list:
        t = threading.Thread(target=read_loop, args=(s, label), daemon=True)
        t.start()

    print("Monitoring. Type 0: text or 1: text to send to port 0 or 1. Ctrl+C to exit.", file=sys.stderr)
    print("---", file=sys.stderr)

    try:
        while True:
            line = sys.stdin.readline()
            if not line:
                break
            line = line.rstrip("\n\r")
            if args.no_send:
                continue
            if line.startswith("0:") and len(ser_list) > 0:
                data = (line[2:].strip() + "\r\n").encode("utf-8")
                try:
                    ser_list[0][0].write(data)
                except Exception as e:
                    print(f"Send error: {e}", file=sys.stderr)
            elif line.startswith("1:") and len(ser_list) > 1:
                data = (line[2:].strip() + "\r\n").encode("utf-8")
                try:
                    ser_list[1][0].write(data)
                except Exception as e:
                    print(f"Send error: {e}", file=sys.stderr)
    except KeyboardInterrupt:
        pass
    finally:
        for s, _ in ser_list:
            try:
                s.close()
            except Exception:
                pass
    print("\nExited.", file=sys.stderr)


if __name__ == "__main__":
    main()
