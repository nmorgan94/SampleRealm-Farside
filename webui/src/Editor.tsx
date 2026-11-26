// Mirrors AudioPluginAudioProcessorEditor::paint() and resized()
import { memo, useState } from 'react';
import { EditorRootContext } from '@samplerealm/modules/editor/editorRoot.ts';
import { HeaderBar, type HeaderLayout } from '@samplerealm/modules/editor/HeaderBar.tsx';
import { useParam } from '@samplerealm/modules/editor/hooks.ts';
import { LabeledSlider } from '@samplerealm/modules/editor/LabeledSlider.tsx';
import { centred } from '@samplerealm/modules/editor/layout.ts';
import { LevelMeter, PREFERRED_WIDTH } from '@samplerealm/modules/editor/LevelMeter.tsx';
import { useManual } from '@samplerealm/modules/editor/ManualOverlay.tsx';
import { usePrompt } from '@samplerealm/modules/editor/PromptOverlay.tsx';
import manualText from '../../Assets/Manual.md?raw';
import type { FarsideBackend } from './backend.ts';
import { FarsideRotary, ToggleText } from './controls.tsx';
import { Spectrum } from './Spectrum.tsx';

export const EDITOR_WIDTH = 430;
export const EDITOR_HEIGHT = 556;

// The output gain and the meter beside it, in the header's meter slot
const GAIN_WIDTH = 48;
const GAIN_LABEL_HEIGHT = 12;
const GAIN_NAME_HEIGHT = 8;
const GAIN_GAP = 4;
const METER_INSET = 2;

// CustomLookAndFeel::drawLabel and its Label colours
const NAME_BOX = 'rounded-[5px] bg-blue/50 text-red';

// AudioPluginAudioProcessorEditor::headerLayout. The title is behind the spectrum instead
const HEADER_LAYOUT: HeaderLayout = {
  height: 44,
  edge: 10,
  titleWidth: 0,
  titleHeight: 12,
  gap: 8,
  controlsHeight: 24,
  meterWidth: GAIN_WIDTH + GAIN_GAP + PREFERRED_WIDTH,
  meterInset: 2,
};

const SPECTRUM_HEIGHT = 100;

// The name is wider than its knob
const SLIDER_WIDTH = 88;
const KNOB_SIZE = 70;
const LABEL_HEIGHT = 20;
const NAME_HEIGHT = 13;
const KNOB_SPACING = 27;
const KNOBS_PER_ROW = 3;

const ROWS_TOP = HEADER_LAYOUT.height + SPECTRUM_HEIGHT + 4;
const ROW_PITCH = 104;

// The stage's name, small, in a column left of its knobs
const STAGE_WIDTH = 64;
const STAGE_GAP = 4;
const STAGE_NAME_TOP = 27;
const STAGE_NAME_HEIGHT = 16;
const STAGE_ARROW_TOP = 47;
const ARROW_HEAD = 6;

const SYNC_WIDTH = 58;
const SYNC_HEIGHT = 12;

// The unit is the editor's, as the parameter's own is empty for hosts
type Knob = { id: string; label: string; unit?: string };

// In the TIME slot: the note while synced, the time otherwise
const DELAY_NOTE: Knob = { id: 'delayNote', label: 'TIME' };
const DELAY_TIME: Knob = { id: 'delayTime', label: 'TIME', unit: 'ms' };

// Top down, in the order the signal goes through them
const STAGES: { name: string; knobs: Knob[] }[] = [
  {
    name: 'SEND',
    knobs: [
      { id: 'highpassFreq', label: 'HP FREQ', unit: 'Hz' },
      { id: 'lowpassFreq', label: 'LP FREQ', unit: 'Hz' },
      { id: 'drive', label: 'DRIVE' },
    ],
  },
  { name: 'DELAY', knobs: [DELAY_NOTE, { id: 'delayFeedback', label: 'FEEDBACK' }, { id: 'delayLevel', label: 'LEVEL' }] },
  {
    name: 'REVERB',
    knobs: [
      { id: 'roomSize', label: 'ROOM SIZE' },
      { id: 'damping', label: 'DAMPING' },
      { id: 'width', label: 'WIDTH' },
    ],
  },
  { name: 'OUTPUT', knobs: [{ id: 'duck', label: 'DUCK' }, { id: 'mix', label: 'MIX' }] },
];

const DELAY_ROW = STAGES.findIndex(({ knobs }) => knobs.includes(DELAY_NOTE));

const stageLeft = centred(EDITOR_WIDTH, STAGE_WIDTH + STAGE_GAP + SLIDER_WIDTH * KNOBS_PER_ROW + KNOB_SPACING * (KNOBS_PER_ROW - 1));
const knobsLeft = stageLeft + STAGE_WIDTH + STAGE_GAP;
const knobLeft = (column: number) => knobsLeft + (SLIDER_WIDTH + KNOB_SPACING) * column;
const rowTop = (row: number) => ROWS_TOP + ROW_PITCH * row;

// Under the knob it switches
const SYNC_BOUNDS = {
  left: knobLeft(0) + SLIDER_WIDTH / 2 - SYNC_WIDTH / 2,
  top: rowTop(DELAY_ROW) + KNOB_SIZE + LABEL_HEIGHT + 1,
  width: SYNC_WIDTH,
  height: SYNC_HEIGHT,
};

// AudioPluginAudioProcessorEditor::drawStage
function Stage({ row, name }: { row: number; name: string }) {
  const arrowHeight = ROW_PITCH + STAGE_NAME_TOP - 4 - STAGE_ARROW_TOP;
  const base = arrowHeight - ARROW_HEAD;

  return (
    <>
      <div
        className="absolute flex items-center justify-center font-bold text-fg/80"
        style={{ left: stageLeft, top: rowTop(row) + STAGE_NAME_TOP, width: STAGE_WIDTH, height: STAGE_NAME_HEIGHT, fontSize: 8 }}
      >
        {name}
      </div>
      {row < STAGES.length - 1 && (
        <svg
          className="absolute text-red/60"
          style={{ left: stageLeft + STAGE_WIDTH / 2 - 4, top: rowTop(row) + STAGE_ARROW_TOP }}
          width={8}
          height={arrowHeight}
          aria-hidden
        >
          <line x1={4} y1={0} x2={4} y2={base} stroke="currentColor" strokeWidth={1} />
          <path d={`M 0 ${base} L 8 ${base} L 4 ${arrowHeight} Z`} fill="currentColor" />
        </svg>
      )}
    </>
  );
}

export const Editor = memo(function Editor({ backend }: { backend: FarsideBackend }) {
  const synced = useParam(backend, 'delaySync').value >= 0.5;
  const spectrumBypass = useParam(backend, 'spectrumBypass');
  const analysing = spectrumBypass.value < 0.5;
  const [root, setRoot] = useState<HTMLDivElement | null>(null);
  const { prompt, overlay } = usePrompt();
  const manual = useManual(manualText);

  return (
    <EditorRootContext.Provider value={root}>
      <div
        ref={setRoot}
        className="relative overflow-hidden bg-linear-to-b from-top to-bottom select-none"
        style={{ width: EDITOR_WIDTH, height: EDITOR_HEIGHT }}
      >
        {STAGES.map(({ name, knobs }, row) => (
          <div key={name}>
            <Stage row={row} name={name} />
            {knobs.map((knob, column) => {
              const { id, label, unit } = knob === DELAY_NOTE && !synced ? DELAY_TIME : knob;

              return (
                <div key={id} className="absolute" style={{ left: knobLeft(column), top: rowTop(row) }}>
                  <LabeledSlider
                    backend={backend}
                    id={id}
                    label={label}
                    unit={unit}
                    width={SLIDER_WIDTH}
                    height={KNOB_SIZE + LABEL_HEIGHT}
                    labelHeight={LABEL_HEIGHT}
                    nameHeight={NAME_HEIGHT}
                    rotary={FarsideRotary}
                    labelClassName={NAME_BOX}
                  />
                </div>
              );
            })}
          </div>
        ))}

        <ToggleText backend={backend} id="delaySync" text="SYNC" style={SYNC_BOUNDS} />

        <h1
          className="absolute inset-x-0 flex items-center justify-center text-fg"
          style={{ top: HEADER_LAYOUT.height, height: SPECTRUM_HEIGHT, fontSize: 28 }}
        >
          <span>
            <span className="font-bold">SampleRealm: </span>FARSIDE
          </span>
        </h1>
        <Spectrum backend={backend} top={HEADER_LAYOUT.height} width={EDITOR_WIDTH} height={SPECTRUM_HEIGHT} />

        <HeaderBar
          title={() => null}
          width={EDITOR_WIDTH}
          layout={HEADER_LAYOUT}
          backend={backend}
          prompt={prompt}
          showManual={manual.showManual}
          ownMenuItems={() => [{ kind: 'item', label: 'Analyser', ticked: analysing, onSelect: () => spectrumBypass.commit(analysing ? 1 : 0) }]}
          meter={({ left, top, width, height }) => (
            <>
              <div className="absolute" style={{ left, top }}>
                <LabeledSlider
                  backend={backend}
                  id="outputGain"
                  label="GAIN"
                  unit="dB"
                  width={GAIN_WIDTH}
                  height={height}
                  rotary={FarsideRotary}
                  labelClassName={NAME_BOX}
                  labelHeight={GAIN_LABEL_HEIGHT}
                  nameHeight={GAIN_NAME_HEIGHT}
                />
              </div>
              <div className="meter-colours contents">
                <LevelMeter
                  backend={backend}
                  x={left + width - PREFERRED_WIDTH}
                  y={top + METER_INSET}
                  width={PREFERRED_WIDTH}
                  height={height - 2 * METER_INSET}
                />
              </div>
            </>
          )}
        />
        {overlay}
        {manual.overlay}
      </div>
    </EditorRootContext.Provider>
  );
});
