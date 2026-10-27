import serial, time, sys
import os
def kalos_port():   # local USB (COM3) or the laptop over RFC2217 – set with tools/port.sh, stored in tools/port.cfg
    try: return open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "port.cfg")).read().strip() or "COM3"
    except OSError: return "COM3"
# Remote (esp_rfc2217_server on the laptop): the server switches the reset lines itself when a client connects,
# so EVERY remote connection restarts the gadget ("RTS off" = its hard reset). Tools wait for the boot first.
REMOTE = kalos_port().startswith("rfc2217")
BOOT_WAIT_S = 7 if REMOTE else 0
# usage: serial_monitor.py SECONDS [reset]   (reset = pulse EN via RTS first, to catch the boot messages;
#        remote connections always restart the gadget anyway)
s = serial.serial_for_url(kalos_port(), do_not_open=True); s.baudrate = 115200; s.timeout = 0.1; s.dtr = False; s.rts = False; s.open()
if len(sys.argv) > 2 and sys.argv[2] == "reset":
    s.rts = True; time.sleep(0.2); s.rts = False
t = time.time(); buf = b""
while time.time() - t < float(sys.argv[1]):
    try: buf += s.read(1024)
    except Exception as e:   # board unplugged mid-recording: keep what we have
        buf += ("\n[serial lost: %s]\n" % e).encode(); break
try: s.close()
except Exception: pass
sys.stdout.write(buf.decode("ascii", "replace").replace("�", "?"))
