# SampleRealm: FARSIDE

A whole send in one insert. A copy of the input is filtered, driven and echoed into a reverb, and the result is turned down while the input plays, then mixed back with it. The stages run top to bottom.

## Send

- HP FREQ removes the lows from everything that follows, so the reverb and echoes stay out of the low end. The input itself is never filtered.
- LP FREQ removes the highs in the same way, for a darker reverb. Fully right it does nothing.
- DRIVE adds harmonics before the echoes and the reverb, without making the send much louder. At 0 it does nothing.

## Delay

- The echoes bounce between left and right. On a mono output they are a single echo.
- TIME is a note length while SYNC is lit, following the host's tempo, and a time in milliseconds otherwise. Click SYNC to switch.
- FEEDBACK sets how long the echoes repeat.
- LEVEL sets how loud they are, both in the output and going into the reverb. At 0 there are none.

## Reverb

- The word under ROOM SIZE is the kind of reverb. Click it to move on: CLASSIC is a plain room, PLATE is bright and dense, HALL is big and smooth and takes a moment to build.
- PRE-DELAY holds the reverb back, so the input is heard clearly before it. The echoes are not held back.
- ROOM SIZE sets how long the reverb rings, the same for all three kinds.
- DAMPING makes the highs die away faster.
- WIDTH goes from mono at 0 to full stereo at 1. On a mono output it does nothing.

## Output

- DUCK turns the reverb and echoes down while the input is playing, and lets them swell back in the gaps. At 0 it does nothing, and at 1 they are silent under the input, however loud or quiet the track is.
- MIX brings the reverb and echoes in under the input, up to half way, where both are at full level. Past half way the input fades out, and at 1 only the reverb and echoes are heard.

## Display

- The spectrum shows the reverb and echoes in red over the input, the grey shape behind, as loud as each is in the output. The curve along the top is what HP FREQ and LP FREQ let through, and the bar on the right drops as DUCK holds the reverb down. Analyser in the ... menu switches it off and on.
- GAIN, beside the meter, turns the whole output up or down, in dB.
- The meter in the corner shows the output peak, and its light turns on when the output goes over 0 dB.
- Click a knob's name to type a value.

## Presets

- The arrows step through the presets, and the name opens the full list.
- A * after the name means the sound has changed since the preset was loaded. The Analyser switch doesn't count.
- Save overwrites the current user preset. For a factory preset it asks for a name.
- The ... menu has Init Preset, Save As, Rename and Delete. In the plugin it also has Show Preset Folder.
- Factory presets can't be renamed, deleted or overwritten.
