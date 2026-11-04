// Mirrors AudioPluginAudioProcessorEditor::paint() and resized()
import { memo } from 'react';
import type { PluginBackend } from './backend.ts';
import { Knob, ToggleText } from './controls.tsx';
import { Spectrum } from './Spectrum.tsx';

export const EDITOR_WIDTH = 700;
export const EDITOR_HEIGHT = 250;

const TITLE_HEIGHT = 100;

const KNOB_SIZE = 100;
const KNOB_SPACING = 15;
const KNOB_TOTAL_HEIGHT = KNOB_SIZE + 30;

const KNOBS: { id: string; label: string; unit?: string }[] = [
  { id: 'highpassFreq', label: 'HP FREQ', unit: 'Hz' },
  { id: 'sendGain', label: 'SEND GAIN' },
  { id: 'roomSize', label: 'ROOM SIZE' },
  { id: 'damping', label: 'DAMPING' },
  { id: 'width', label: 'WIDTH' },
  { id: 'mix', label: 'MIX' },
];

const knobsLeft = Math.trunc((EDITOR_WIDTH - (KNOB_SIZE * KNOBS.length + KNOB_SPACING * (KNOBS.length - 1))) / 2);
const knobsTop = TITLE_HEIGHT + Math.trunc((EDITOR_HEIGHT - TITLE_HEIGHT - KNOB_TOTAL_HEIGHT) / 2);
const knobLeft = (index: number) => knobsLeft + (KNOB_SIZE + KNOB_SPACING) * index;

// Between SEND GAIN and ROOM SIZE, and WIDTH and MIX
const separatorLeft = (before: number) => (knobLeft(before) + KNOB_SIZE + knobLeft(before + 1)) / 2;
const SEPARATORS = [separatorLeft(1), separatorLeft(4)];

export const Editor = memo(function Editor({ backend }: { backend: PluginBackend }) {
  return (
    <div
      className="relative overflow-hidden bg-linear-to-b from-top to-bottom select-none"
      style={{ width: EDITOR_WIDTH, height: EDITOR_HEIGHT }}
    >
      <div className="absolute inset-x-0 top-0 flex items-center justify-center text-fg" style={{ height: TITLE_HEIGHT, fontSize: 28 }}>
        <span>
          <span className="font-bold">SampleRealm: </span>REVERB
        </span>
      </div>
      <div className="absolute text-right text-red/80" style={{ left: EDITOR_WIDTH - 50, top: TITLE_HEIGHT - 12, width: 50, height: 12, fontSize: 9 }}>
        v{backend.about.version}
      </div>

      {SEPARATORS.map((left) => (
        <div key={left} className="absolute bg-red" style={{ left: left - 0.5, top: knobsTop, width: 1, height: KNOB_SIZE }} />
      ))}

      {KNOBS.map(({ id, label, unit }, index) => (
        <div key={id} className="absolute" style={{ left: knobLeft(index), top: knobsTop }}>
          <Knob backend={backend} id={id} label={label} unit={unit} size={KNOB_SIZE} />
        </div>
      ))}

      <Spectrum backend={backend} width={EDITOR_WIDTH} height={TITLE_HEIGHT} />
      <ToggleText backend={backend} id="spectrumBypass" text="ANALYSER" style={{ left: EDITOR_WIDTH - 66, top: 6, width: 58, height: 20 }} />
    </div>
  );
});
