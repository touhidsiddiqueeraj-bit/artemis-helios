#!/usr/bin/env python3
"""
check_boot.py — probe STM32 boot mode via FTDI
Tests both bootloader (8E1 0x7F -> 0x79) and run mode (8N1 HEL->ART)
Fix: deassert DTR/RTS after opening to avoid reset pulse.
Usage: python3 check_boot.py [/dev/ttyUSB0]
"""
import serial, time, sys

port = sys.argv[1] if len(sys.argv) > 1 else "/dev/ttyUSB0"

def probe(port, baud, parity_str, payload, label):
    parity = serial.PARITY_EVEN if parity_str == 'E' else serial.PARITY_NONE
    try:
        s = serial.Serial(port, baud, parity=parity, bytesize=8, stopbits=1, timeout=1.2)
        # Fix: deassert DTR/RTS immediately after open to avoid reset pulse
        s.dtr = False
        s.rts = False
        time.sleep(0.5)
        s.reset_input_buffer()
        s.write(payload)
        s.flush()
        time.sleep(0.7)
        data = s.read(800)
        s.close()
        if payload == b"\x7F":
            ok = b"\x79" in data
            status = "PASS 0x79" if ok else "no 0x79"
        else:
            ok = b"ART:" in data
            status = "PASS ART:" if ok else "no ART:"
        print(f"{label:25s} {baud} {parity_str} -> {status:12s} hex:{data.hex()[:50] if data else 'no data'} txt:{data[:70]!r}")
        return ok
    except Exception as e:
        print(f"{label} ERR {e}")
        return False

print(f"Probing {port} (close Arduino Serial Monitor first!)")
# Probe run mode first (avoid 8E1 confusing run-mode firmware)
ok_run = probe(port, 115200, 'N', b"HEL:VP=16.80,GP=642.0,AL=0.350\r\n", "Run mode 115200 8N1")
time.sleep(0.6)
ok_boot = probe(port, 115200, 'E', b"\x7F", "Bootloader 115200 8E1")

if ok_boot and not ok_run:
    print("-> STM32 is in BOOTLOADER (BOOT0=1, middle→3.3V). For run, power off, cap middle→GND (1-2), NRST.")
elif ok_run and not ok_boot:
    print("-> STM32 is in RUN MODE (BOOT0=0, middle→GND, correct for HEL).")
elif not ok_boot and not ok_run:
    print("-> No response on either. Check: FTDI TX→PA10, RX←PA9, GND, 3.3V, and BOOT0 jumper (middle→GND for run, middle→3.3V for bootloader), then NRST.")
else:
    print("-> Both responded (rare). Check wiring/baud.")
