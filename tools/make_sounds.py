#!/usr/bin/env python3
"""Generate the game's sound effects into assets/sounds/ as 16-bit mono WAVs.

    move.wav      wooden knock
    capture.wav   heavier double knock
    castle.wav    two quick knocks (king, then rook)
    check.wav     two-note bell
    promote.wav   rising three-note bell
    game_end.wav  longer three-note chord

Everything is synthesized from sine waves and filtered noise, so there are no
third-party recordings. Replace any file with your own WAV of the same name.

Usage: python3 tools/make_sounds.py   (standard library only)
"""
import math
import random
import struct
import wave
from pathlib import Path

RATE = 44100


def silence(seconds):
    return [0.0] * int(RATE * seconds)


def knock(pitch=200.0, seconds=0.09, noise=0.6, decay=45.0):
    """A short percussive hit: a pitch-dropping sine plus a low-passed noise burst."""
    out, lp = [], 0.0
    phase = 0.0
    for i in range(int(RATE * seconds)):
        t = i / RATE
        env = math.exp(-decay * t)
        freq = pitch * (1.0 + 0.6 * math.exp(-60 * t))   # quick downward sweep
        phase += 2 * math.pi * freq / RATE
        lp += 0.25 * (random.uniform(-1, 1) - lp)        # one-pole low-pass on the noise
        noise_env = math.exp(-180 * t)
        out.append(env * math.sin(phase) + noise * noise_env * lp * 4)
    return out


def bell(freq, seconds, decay=6.0):
    """A soft bell: fundamental plus two quieter inharmonic partials."""
    out = []
    for i in range(int(RATE * seconds)):
        t = i / RATE
        attack = min(1.0, t / 0.004)
        env = attack * math.exp(-decay * t)
        s = (math.sin(2 * math.pi * freq * t)
             + 0.35 * math.sin(2 * math.pi * freq * 2.76 * t) * math.exp(-4 * t)
             + 0.15 * math.sin(2 * math.pi * freq * 5.40 * t) * math.exp(-8 * t))
        out.append(env * s)
    return out


def mix(*tracks):
    """Overlay (offset_seconds, samples) tracks into one buffer."""
    length = max(int(RATE * off) + len(s) for off, s in tracks)
    out = [0.0] * length
    for off, s in tracks:
        start = int(RATE * off)
        for i, v in enumerate(s):
            out[start + i] += v
    return out


def write(path, samples, peak=0.8):
    top = max(abs(v) for v in samples) or 1.0
    fade = int(RATE * 0.005)                              # avoid a click at the very end
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        frames = bytearray()
        for i, v in enumerate(samples):
            gain = min(1.0, (len(samples) - i) / fade)
            frames += struct.pack("<h", int(v / top * peak * gain * 32767))
        w.writeframes(bytes(frames))
    print(f"wrote {path}")


def main():
    random.seed(7)  # same output on every run
    out = Path(__file__).resolve().parent.parent / "assets" / "sounds"
    out.mkdir(parents=True, exist_ok=True)

    write(out / "move.wav", knock(210, 0.10), peak=0.6)
    write(out / "capture.wav", mix((0, knock(170, 0.14, noise=1.0, decay=32)),
                                   (0.028, knock(240, 0.10, noise=0.8))), peak=0.85)
    write(out / "castle.wav", mix((0, knock(210, 0.10)), (0.11, knock(190, 0.10))), peak=0.6)
    write(out / "check.wav", mix((0, bell(880, 0.35, decay=9)), (0.12, bell(1175, 0.45, decay=8))),
          peak=0.55)
    write(out / "promote.wav", mix((0, bell(660, 0.3, decay=10)), (0.08, bell(880, 0.3, decay=10)),
                                   (0.16, bell(1320, 0.5, decay=7))), peak=0.55)
    write(out / "game_end.wav", mix((0, bell(523, 1.2, decay=3)), (0.15, bell(659, 1.1, decay=3)),
                                    (0.30, bell(784, 1.2, decay=3)), (0.30, silence(0.4))), peak=0.6)


if __name__ == "__main__":
    main()
