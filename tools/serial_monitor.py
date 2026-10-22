import serial, time, sys
# usage: mon.py SECONDS [reset]   (reset = pulse EN via RTS first, to catch the boot messages)
s = serial.Serial(); s.port = "COM3"; s.baudrate = 115200; s.timeout = 0.1; s.dtr = False; s.rts = False; s.open()
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
