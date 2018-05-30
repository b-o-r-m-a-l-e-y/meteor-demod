# meteor-demod
Meteor-demod is a simple demodulator for the signal transmitted by the Meteor-M2 satellite.

The DSP stuff is very primitive so I recommend choosing a better-performing program for any serious use.

It's meant to produce a .s file for further processing by other software from an input recording (raw complex 32-bit float samples).

Usage: `./meteor-demod <recording filename> <sample rate> <output s-file>`.
