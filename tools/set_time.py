"""Send the PC's time to the Kalos Dex over USB (first piece of the PC companion).
usage: set_time.py [COM3] [offset_seconds]    (offset: test only, e.g. -120 = send a time 2 min early)
Opens the port WITHOUT resetting the board, sends "TIME <unix UTC>", prints the answer."""
import serial, sys, time
port = sys.argv[1] if len(sys.argv) > 1 else "COM3"
offset = int(sys.argv[2]) if len(sys.argv) > 2 else 0
s = serial.Serial(); s.port = port; s.baudrate = 115200; s.timeout = 0.1; s.dtr = False; s.rts = False; s.open()
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
