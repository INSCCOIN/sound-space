# SoundSpace

3D sound mapper for any Linux box. UI scale is derived from the real display size (320p deck → scale 1, 1080p desktop → scale 4). Plots **your** sound — a WAV file or the live mic — as a particle cloud.

Display backends, tried in order:

1. `/dev/fb0` (handhelds, tty framebuffer)
2. SDL2 window (desktop, resizable)
3. software canvas if neither exists

```bash
sudo apt install -y build-essential libasound2-dev libsdl2-dev
make clean && make
```

Copy source only. Do **not** copy `*.o` or a binary from another CPU.

If `libasound2-dev` is missing you keep WAV files, no live mic. If `libsdl2-dev` is missing, framebuffer-only machines still work.

PCM WAV only (8- or 16-bit). Convert anything else on the deck with:

```bash
ffmpeg -i input.mp3 -ac 1 -ar 16000 -c:a pcm_s16le birds.wav
```

## Run

```bash
./soundspace                         # picker; auto display
./soundspace birds.wav
./soundspace -m
./soundspace --fb /dev/fb0           # force framebuffer
./soundspace --fb sdl --geom 1280x720
```

Drop `.wav` files next to the binary or anywhere you can reach with the on-screen picker. Press `o` while visualizing to pick another file.

Needs write access to `/dev/fb0` and, for F-keys, read on `/dev/input/event*`. If the console owns the screen you will see the app paint over it. Optional:

```bash
# stop getty from fighting the LCD while it runs
# (WalnutOS: console_display=disable in config if you want this permanent)
sudo ./soundspace -m
```

## How sound becomes space

Each hop (256 samples @ 16 kHz) extracts:

| feature | meaning |
|---|---|
| RMS / peak | energy |
| peak Hz | strongest bin |
| spectral centroid | brightness / center of mass |
| flux | how fast the spectrum is changing |
| flatness | tone vs noise |
| slope | low-band vs high-band energy |

Those are **not** dropped onto time/freq/amp axes. They are mapped into a unit cube:

- **X** = log centroid (200 Hz → 6 kHz)
- **Y** = log peak frequency
- **Z** = spectral slope + a bit of flatness
- **color** = peak frequency (blue low → red high)
- **size** = RMS
- **lifetime** = energy
- particles then drift on Z so time is a trail, not an axis

That is the SPACE mode (Arese-style sculpture). Other modes reuse the same analysis:

- **SPEC** — log-frequency scrolling spectrogram
- **WAVE** — waveform + spectrum bars
- **FIELD** — 2D centroid × amplitude scatter (good for reading, worse as art)

## Keys

KeebDeck color keys as you have them bound (F1–F6), plus letters if you are on SSH.

| key | action |
|---|---|
| F1 / space / m | cycle modes |
| F2 / p | pause analysis |
| F3 / h | freeze the cloud (snapshot) |
| F4 / c | clear particles |
| F5 / ] | more emit gain |
| F6 / [ | less emit gain |
| arrows | orbit camera |
| + / − | zoom |
| . / , | longer / shorter particle life |
| q / ESC | quit |

## Files

```
src/main.c      loop, CLI
src/fb.c        /dev/fb0 RGB565/32/24 + software canvas
src/audio.c     ALSA + WAV + synth
src/fft.c       radix-2
src/analyze.c   descriptors
src/cloud.c     particle mapper
src/draw.c      3D project + 4 views + HUD
src/input.c     evdev + raw tty
```

No cap on source files. Link with `-lm -lpthread -lasound`.

## Mic notes on WalnutPi / SharkDeck

USB mics usually just appear as `hw:1,0` after `arecord -l`. Onboard audio on H618 boards is hit-or-miss; if `arecord -l` is empty, use WAV or the synth. Capture is mono S16 at 16 kHz.

## License of this tree

Written for your deck. FFT, font, and mapper are original and tiny.
