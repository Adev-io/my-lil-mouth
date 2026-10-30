# My lil Mouth

Your voice as a controller for VCV Rack 2. Sing and get a note, a gate and a velocity. Beatbox and four trained sounds
fire four triggers. Change the vowel and two CVs move. By A_Development.

Patch a microphone into **IN** (VCV Audio, or any audio input module). A dynamic mic close to the mouth works best;
a laptop's built-in mic hears the room too.

## Set up

1. Patch the mic into **IN** and say something. The mascot's mouth moves with the input.
2. Turn **GAIN** up until a normal voice moves the mouth well (more gain also picks up more room noise).
3. Be quiet and press **CAL**. It listens to the room for 2 seconds (blinking magenta), then stays lime. If CAL
   lights solid magenta for a moment instead, it heard nothing (check the cable) or heard someone talking: the old
   setting stays, try again. CAL remembers the GAIN it was done at, so turning GAIN later moves the threshold with it.
4. **THRESH** is how far above that room sound your voice must be to count (default 12 dB).

## Pitch

| Output | What it gives |
|---|---|
| **NOTE** | V/Oct, held on the nearest semitone. It moves only when you sit more than **LOCK** cents away for two hops (about 11 ms), so vibrato does not wobble it: 50 is plain nearest-note, 100 waits until you reach the next note. **GLIDE** slides it to a new note (0 to 200 ms). |
| **RAW** | V/Oct that follows your voice exactly, bends and vibrato included. |
| **GATE** | 10 V while you sing. It drops for 2 ms when the note changes, so envelopes start again. |
| **VEL** | 0 to 10 V: how loud you were when the note started or last changed. |

C4 = 0 V. The 12 lamps show the note (dim: the last note, after you stop). To sing in a key, patch NOTE through
My lil Arpeggio's quantizer. Right-click *Pitch range* picks Bass (50 to 500 Hz), Voice (70 to 1100 Hz, default) or
High (150 to 2000 Hz). A lower range listens to a longer stretch of sound, so it answers a little later.

## Triggers

Four slots, one per sound (kick, snare, hat, anything short).

- **Train**: hold **TRAIN n**, make the sound 3 or more times (up to 12), let go. The ring blinks magenta while you
  hold, flashes white on every sound it hears, and turns lime when the slot has learned. Fewer than 3 sounds: the slot
  keeps what it knew.
- **Clear**: hold TRAIN n for 3 seconds without a sound, then let go.
- **Play**: each sound fires the matching **TRIG** (10 V, 5 ms; right-click *Trigger pulse* for 1 ms) and flashes its ring.
  A sound unlike every trained one fires nothing.
- **SENS**: how sharp a sound must start to count. Turn it down if talking fires triggers.
- While any TRAIN button is held, no trigger fires: every sound goes to the slot being trained.

Train with the sounds you will really play, short and sharp, the same way each time. Two sounds that are too alike
will be mixed up: make them more different.

Voice looping: patch the TRIG outputs into a Merge and that into My lil Looper's poly REC or STOP.

## Vowel and level

| Output | What it gives |
|---|---|
| **X** | 0 to 10 V from the first formant: low for "ee" and "oo", high for "ah". |
| **Y** | 0 to 10 V from the second formant: low for "oo", high for "ee". |
| **BRIGHT** | 0 to 10 V: dark sounds low, hissy sounds high. |
| **ENV** | 0 to 10 V: how loud you are right now (a full-scale tone reads about 9 V). |

X, Y and BRIGHT hold their last value while you are quiet. X and Y also hold while you sing above about 400 Hz,
where the vowel cannot be read reliably.

## Saving

Trained sounds, the CAL floor and the GAIN it was taken at, the pitch range and the trigger pulse are saved with the patch.

## Building

```
make            # the plugin (RACK_DIR = the Rack SDK)
make test       # the core checks
make dist       # the .vcvplugin
```

On Windows, `pwsh -File tools/mk.ps1 [test|dist]` runs these in MSYS2. The panel and `src/layout.hpp` come from
`tools/panel.py` (Python 3 with fontTools; it reads DejaVu Sans from the Rack install, or the font named by RACK_FONT).

## License

MIT. Inspired by vocal controllers such as Vochlea's Dubler 2, studied from the outside only; no code or data from
them is used.
