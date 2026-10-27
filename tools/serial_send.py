import serial, time, sys
import os
def kalos_port():   # local USB (COM3) or the laptop over RFC2217 – set with tools/port.sh, stored in tools/port.cfg
    try: return open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "port.cfg")).read().strip() or "COM3"
    except OSError: return "COM3"
# Remote (esp_rfc2217_server on the laptop): the server switches the reset lines itself when a client connects,
# so EVERY remote connection restarts the gadget ("RTS off" = its hard reset). Tools wait for the boot first.
REMOTE = kalos_port().startswith("rfc2217")
BOOT_WAIT_S = 7 if REMOTE else 0
# usage: serial_send.py "CMD 1" "CMD 2" ...   sends each line to the gadget (port from tools/port.cfg, default COM3; no reset) and prints what comes back.
# A number instead of a command just waits that many seconds (e.g. to watch the log).
s = serial.serial_for_url(kalos_port(), do_not_open=True); s.baudrate = 115200; s.timeout = 0.1; s.dtr = False; s.rts = False; s.open()
def drain(sec):
    out, t = b"", time.time()
    while time.time() - t < sec: out += s.read(1024)
    sys.stdout.write(out.decode("ascii", "replace").replace("\ufffd", "?"))   # Windows console: ASCII only
drain(0.5 + BOOT_WAIT_S)
for cmd in sys.argv[1:]:
    try: wait = float(cmd)
    except ValueError: wait = None
    if wait is not None: drain(wait); continue
    print(">> " + cmd)
    s.write((cmd + "\n").encode()); drain(0.8)
s.close()
