#!/usr/bin/env python3
"""Generates libs/PlayNote/sounds/wt_square.wav - a single-cycle 50%-duty
square wave, matching real Gamebuino Classic hardware's own default
instrument (Sound.cpp's own squareWaveInstrument): the real sound engine
drives its piezo speaker with a genuine 2-level PWM square wave (toggling
_chanState high/low every half-period - see updateOutput()/generateOutput()
in the real utility/Sound.cpp), not the smoother sawtooth PlayNote's own
generic sample (sounds/wt_saw.wav) ships by default.

Same format/shape as wt_saw.wav (inspected directly, not assumed), so it's
a drop-in replacement for WAVETABLE_PERIOD_SAMPLES in portVircon32.c with
no other code changes needed:
  - mono, 16-bit PCM, 44100Hz
  - 257 frames: one full 256-sample cycle plus one extra wraparound sample
    equal to sample[0], giving a clean loop point for the SPU's own
    sample-based looping/interpolation (set_sound_loop_end(256))
  - peak amplitude ~9000 (not full 16-bit range) - PlayNote can play up to
    16 simultaneous voices from this same wavetable, so each one is kept
    well below clipping when several sum together in the mixer

Run from the project root: `python tools/gen_square_wavetable.py`
"""

import wave
import struct

PERIOD_SAMPLES = 256
AMPLITUDE = 9000

samples = []
for i in range(PERIOD_SAMPLES):
    # First half of the cycle high, second half low - a real, symmetric
    # 50% duty cycle (real hardware's own _chanState toggles at exactly
    # half the period, with no separate duty-cycle control - amplitude
    # alone is what a real instrument step varies).
    if i < PERIOD_SAMPLES // 2:
        samples.append( AMPLITUDE )
    else:
        samples.append( -AMPLITUDE )

samples.append( samples[ 0 ] )  # wraparound sample, matches wt_saw.wav's own convention

with wave.open( "libs/PlayNote/sounds/wt_square.wav", "wb" ) as w:
    w.setnchannels( 1 )
    w.setsampwidth( 2 )
    w.setframerate( 44100 )
    w.writeframes( struct.pack( "<%dh" % len( samples ), *samples ) )

print( "Wrote libs/PlayNote/sounds/wt_square.wav (%d frames)" % len( samples ) )
