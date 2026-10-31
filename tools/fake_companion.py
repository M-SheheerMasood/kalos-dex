import serial, time, sys
PORT = "COM3"   # the gadget always sits on this PC's USB (COM3)
# Pretends to be the PC companion (for testing the Music screen before the real one exists).
# usage: fake_companion.py SECONDS   – plays a pretend playlist, reacts to the gadget's KEY lines, logs them.
SONGS = [("Kalos Route 4", "Pokemon X/Y OST", 151), ("A really long song title that has to scroll across the screen", "Some Artist feat. Another Artist", 245), ("Lumiose City", "Pokemon X/Y OST", 98)]
s = serial.Serial(); s.port = PORT; s.baudrate = 115200; s.timeout = 0.05; s.dtr = False; s.rts = False; s.open()
idx, pos, playing, vol, muted = 0, 0.0, True, 40, False
def send(line): s.write((line + "\n").encode())
def track(): send("TRACK %s\t%s" % SONGS[idx][:2])
send("HELLO fake-0.1"); track(); send("VOL %d %d" % (vol, muted))
end, last, buf = time.time() + float(sys.argv[1]), time.time(), b""
while time.time() < end:
    buf += s.read(512)
    while b"\n" in buf:
        line, buf = buf.split(b"\n", 1)
        line = line.decode("ascii", "replace").strip()
        if line.startswith("screen ->"): print("        " + line, flush=True)
        if not line.startswith("KEY "): continue
        print("%6.1f  gadget sent: %s" % (time.time() % 1000, line), flush=True)
        k = line[4:]
        if k == "PLAYPAUSE": playing = not playing
        elif k == "NEXT": idx, pos = (idx + 1) % len(SONGS), 0; track()
        elif k == "PREV": idx, pos = (idx - 1) % len(SONGS), 0; track()
        elif k == "MUTE": muted = not muted
        elif k.startswith("VOL "): vol = max(0, min(100, vol + 2 * int(k[4:]))); muted = muted and int(k[4:]) <= 0
        send("VOL %d %d" % (vol, muted))
    if time.time() - last >= 1:
        if playing: pos += time.time() - last
        last = time.time()
        if pos >= SONGS[idx][2]: idx, pos = (idx + 1) % len(SONGS), 0; track()
        send("STATE %s %d %d" % ("playing" if playing else "paused", pos, SONGS[idx][2]))
s.close()
print("fake companion stopped (the gadget shows 'PC offline' ~4 s later)")
