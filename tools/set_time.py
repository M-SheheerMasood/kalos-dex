"""Send the PC's time to the Kalos Dex over USB (first piece of the PC companion).
usage: set_time.py [port] [offset_seconds]    (port: default = tools/port.cfg, else COM3)    (offset: test only, e.g. -120 = send a time 2 min early)
Opens the port WITHOUT resetting the board, sends "TIME <unix UTC>", prints the answer."""
import serial, sys, time
import os
def kalos_port():   # local USB (COM3) or the laptop over RFC2217 – set with tools/port.sh, stored in tools/port.cfg
    try: return open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "port.cfg")).read().strip() or "COM3"
    except OSError: return "COM3"
# Remote (esp_rfc2217_server on the laptop): the server switches the reset lines itself when a client connects,
# so EVERY remote connection restarts the gadget ("RTS off" = its hard reset). Tools wait for the boot first.
REMOTE = kalos_port().startswith("rfc2217")
BOOT_WAIT_S = 7 if REMOTE else 0
port = sys.argv[1] if len(sys.argv) > 1 else kalos_port()
offset = int(sys.argv[2]) if len(sys.argv) > 2 else 0
s = serial.serial_for_url(port, do_not_open=True); s.baudrate = 115200; s.timeout = 0.1; s.dtr = False; s.rts = False; s.open()
time.sleep(BOOT_WAIT_S)
s.reset_input_buffer()
# send right as a new second starts, so the board (which counts whole seconds) is as close as possible
time.sleep(1 - (time.time() % 1))
t = int(round(time.time())) + offset
s.write(f"TIME {t}\n".encode())
end, buf = time.time() + 3, b""
while time.time() < end:
    buf += s.read(256)
    if b"OK TIME" in buf or b"ERR" in buf: time.sleep(0.1); buf += s.read(256); break
s.close()
print(f"sent TIME {t} ({time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(t))} PC local)")
print(buf.decode("ascii", "replace").strip() or "(no answer – is the board running and the port free?)")
