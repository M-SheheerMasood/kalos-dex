import serial, time, sys
PORT = "COM3"   # the gadget always sits on this PC's USB (COM3)
# usage: serial_send.py "CMD 1" "CMD 2" ...   sends each line to the gadget (COM3, no reset) and prints what comes back.
# A number instead of a command just waits that many seconds (e.g. to watch the log).
s = serial.Serial(); s.port = PORT; s.baudrate = 115200; s.timeout = 0.1; s.dtr = False; s.rts = False; s.open()
def drain(sec):
    out, t = b"", time.time()
    while time.time() - t < sec: out += s.read(1024)
    sys.stdout.write(out.decode("ascii", "replace").replace("\ufffd", "?"))   # Windows console: ASCII only
drain(0.5)
for cmd in sys.argv[1:]:
    try: wait = float(cmd)
    except ValueError: wait = None
    if wait is not None: drain(wait); continue
    print(">> " + cmd)
    s.write((cmd + "\n").encode()); drain(0.8)
s.close()
