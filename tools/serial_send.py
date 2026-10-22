import serial, time, sys
# usage: serial_send.py "CMD 1" "CMD 2" ...   sends each line to the gadget (COM3, no reset) and prints what comes back.
# A number instead of a command just waits that many seconds (e.g. to watch the log).
s = serial.Serial(); s.port = "COM3"; s.baudrate = 115200; s.timeout = 0.1; s.dtr = False; s.rts = False; s.open()
def drain(sec):
    out, t = b"", time.time()
    while time.time() - t < sec: out += s.read(1024)
    sys.stdout.write(out.decode("ascii", "replace"))
drain(0.5)
for cmd in sys.argv[1:]:
    try: drain(float(cmd)); continue
    except ValueError: pass
    print(">> " + cmd)
    s.write((cmd + "\n").encode()); drain(0.8)
s.close()
