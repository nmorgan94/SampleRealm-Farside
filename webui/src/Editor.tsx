// Mirrors AudioPluginAudioProcessorEditor::paint() and resized()
import { memo } from 'react';
import { LabeledSlider } from '@samplerealm/modules/editor/LabeledSlider.tsx';
import { centred, LABEL_HEIGHT } from '@samplerealm/modules/editor/layout.ts';
import type { ReverbBackend } from './backend.ts';
import { ReverbRotary, ToggleText } from './controls.tsx';
import { Spectrum } from './Spectrum.tsx';

export const EDITOR_WIDTH = 700;
export const EDITOR_HEIGHT = 250;

const TITLE_HEIGHT = 100;

const KNOB_SIZE = 100;
const KNOB_SPACING = 15;
// What the row is centred by, taller than a knob with its label
const KNOB_ROW_HEIGHT = KNOB_SIZE + 30;

// The unit is the editor's, as the parameter's own is empty for hosts
const KNOBS: { id: string; label: string; unit?: string }[] = [
  { id: 'highpassFreq', label: 'HP FREQ', unit: 'Hz' },
  { id: 'sendGain', label: 'SEND GAIN' },
  { id: 'roomSize', label: 'ROOM SIZE' },
  { id: 'damping', label: 'DAMPING' },
  { id: 'width', label: 'WIDTH' },
  { id: 'mix', label: 'MIX' },
];

const knobsLeft = centred(EDITOR_WIDTH, KNOB_SIZE * KNOBS.length + KNOB_SPACING * (KNOBS.length - 1));
const knobsTop = TITLE_HEIGHT + centred(EDITOR_HEIGHT - TITLE_HEIGHT, KNOB_ROW_HEIGHT);
const knobLeft = (index: number) => knobsLeft + (KNOB_SIZE + KNOB_SPACING) * index;

// Between SEND GAIN and ROOM SIZE, and WIDTH and MIX
const separatorLeft = (before: number) => (knobLeft(before) + KNOB_SIZE + knobLeft(before + 1)) / 2;
const SEPARATORS = [separatorLeft(1), separatorLeft(4)];

export const Editor = memo(function Editor({ backend }: { backend: ReverbBackend }) {
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
        v{backend.info.version}
      </div>

      {SEPARATORS.map((left) => (
        <div key={left} className="absolute bg-red" style={{ left: left - 0.5, top: knobsTop, width: 1, height: KNOB_SIZE }} />
      ))}

      {KNOBS.map(({ id, label, unit }, index) => (
        <div key={id} className="absolute" style={{ left: knobLeft(index), top: knobsTop }}>
          <LabeledSlider
            backend={backend}
            id={id}
            label={label}
            unit={unit}
            width={KNOB_SIZE}
            height={KNOB_SIZE + LABEL_HEIGHT}
            rotary={ReverbRotary}
            // CustomLookAndFeel::drawLabel and its Label colours
            labelClassName="rounded-[5px] bg-blue/50 text-red"
          />
        </div>
      ))}

      <Spectrum backend={backend} width={EDITOR_WIDTH} height={TITLE_HEIGHT} />
      <ToggleText backend={backend} id="spectrumBypass" text="ANALYSER" style={{ left: EDITOR_WIDTH - 66, top: 6, width: 58, height: 20 }} />
    </div>
  );
});
