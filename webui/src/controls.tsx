// Mirrors the editor's knobs and the ANALYSER button, drawn by Source/ui/CustomLookAndFeel.h
import { useId, useState, type PointerEvent as ReactPointerEvent } from 'react';
import type { PluginBackend } from './backend.ts';
import { useParam } from './hooks.ts';
import { formatValue, fromProportion, toProportion } from './params.ts';

// juce::Slider's defaults
const ROTARY_START = 1.2 * Math.PI;
const ROTARY_END = 2.8 * Math.PI;
const PIXELS_FOR_FULL_DRAG = 250;

// Angles run clockwise from 12 o'clock, as in juce::Path::addCentredArc
const pointAt = (cx: number, cy: number, radius: number, angle: number) =>
  [cx + radius * Math.sin(angle), cy - radius * Math.cos(angle)] as const;

const arcPath = (cx: number, cy: number, radius: number, from: number, to: number) => {
  const [x1, y1] = pointAt(cx, cy, radius, from);
  const [x2, y2] = pointAt(cx, cy, radius, to);
  return `M ${x1} ${y1} A ${radius} ${radius} 0 ${to - from > Math.PI ? 1 : 0} 1 ${x2} ${y2}`;
};

// CustomLookAndFeel::drawRotarySlider
function RotaryGraphic({ size, proportion }: { size: number; proportion: number }) {
  const gradient = useId();
  const centre = size / 2;
  const radius = (size - 20) / 2;
  const lineWidth = Math.min(8, radius * 0.5);
  const arcRadius = radius - lineWidth * 0.5;
  const toAngle = ROTARY_START + proportion * (ROTARY_END - ROTARY_START);
  const dot = radius * 0.18;
  const pointerLength = radius * 0.55;

  return (
    <svg width={size} height={size} aria-hidden>
      <defs>
        <linearGradient id={`${gradient}arc`} gradientUnits="userSpaceOnUse" x1={0} y1={centre - radius} x2={0} y2={centre + radius}>
          <stop offset={0} stopColor="var(--color-red)" />
          <stop offset={1} stopColor="var(--color-pink)" />
        </linearGradient>
        <linearGradient id={`${gradient}dot`} gradientUnits="userSpaceOnUse" x1={0} y1={centre - dot} x2={0} y2={centre + dot}>
          <stop offset={0} stopColor="var(--color-red)" />
          <stop offset={1} stopColor="var(--color-pink)" />
        </linearGradient>
      </defs>
      <circle cx={centre} cy={centre} r={radius + 5} fill="var(--color-blue)" fillOpacity={0.3} />
      <path
        d={arcPath(centre, centre, arcRadius, ROTARY_START, ROTARY_END)}
        stroke="var(--color-blue)"
        strokeOpacity={0.6}
        strokeWidth={lineWidth}
        strokeLinecap="round"
        fill="none"
      />
      {proportion > 0 && (
        <path
          d={arcPath(centre, centre, arcRadius, ROTARY_START, toAngle)}
          stroke={`url(#${gradient}arc)`}
          strokeWidth={lineWidth}
          strokeLinecap="round"
          fill="none"
        />
      )}
      <rect
        x={-2}
        y={-radius + lineWidth + radius * 0.2}
        width={4}
        height={pointerLength}
        rx={2}
        fill="var(--color-red)"
        transform={`translate(${centre} ${centre}) rotate(${(toAngle * 180) / Math.PI})`}
      />
      <circle cx={centre} cy={centre} r={dot} fill={`url(#${gradient}dot)`} />
    </svg>
  );
}

type DragStart = { y: number; proportion: number };

interface KnobProps {
  backend: PluginBackend;
  id: string;
  label: string;
  // The editor's, as the parameter's own is empty for hosts
  unit?: string;
  size: number;
}

export const LABEL_GAP = 5;
export const LABEL_HEIGHT = 25;

// A Slider::RotaryVerticalDrag with a box below that shows its name, or its value while it's dragged
export function Knob({ backend, id, label, unit, size }: KnobProps) {
  const { value, set, commit, param } = useParam(backend, id);
  const [drag, setDrag] = useState<DragStart | null>(null);
  const proportion = toProportion(param, value);
  const text = unit ? `${formatValue(param, value)} ${unit}` : formatValue(param, value);

  const end = () => {
    if (!drag) return;
    setDrag(null);
    backend.endGesture(id);
  };

  return (
    <div className="relative" style={{ width: size, height: size + LABEL_GAP + LABEL_HEIGHT }}>
      <div
        className="absolute top-0 left-0 cursor-pointer touch-none"
        role="slider"
        aria-label={param.name}
        aria-valuemin={param.min}
        aria-valuemax={param.max}
        aria-valuenow={value}
        aria-valuetext={text}
        onPointerDown={(e: ReactPointerEvent) => {
          if (e.button !== 0) return;
          e.currentTarget.setPointerCapture(e.pointerId);
          setDrag({ y: e.clientY, proportion });
          backend.beginGesture(id);
        }}
        onPointerMove={(e: ReactPointerEvent) => {
          if (!drag) return;
          const next = fromProportion(param, drag.proportion + (drag.y - e.clientY) / PIXELS_FOR_FULL_DRAG);
          if (next !== value) set(next);
        }}
        onPointerUp={end}
        onPointerCancel={end}
        // As SliderAttachment's setDoubleClickReturnValue
        onDoubleClick={() => commit(param.defaultValue)}
      >
        <RotaryGraphic size={size} proportion={proportion} />
      </div>
      <div
        className="absolute left-0 flex items-center justify-center rounded-[5px] bg-blue/50 font-sans font-bold text-red"
        // juce::FontOptions (13.0f) in the system font
        style={{ top: size + LABEL_GAP, width: size, height: LABEL_HEIGHT, fontSize: 11 }}
      >
        {drag ? text : label}
      </div>
    </div>
  );
}

interface ToggleTextProps {
  backend: PluginBackend;
  id: string;
  text: string;
  style: { left: number; top: number; width: number; height: number };
}

// The ANALYSER button: text only, lit while off and dimmed once engaged
export function ToggleText({ backend, id, text, style }: ToggleTextProps) {
  const { value, commit } = useParam(backend, id);
  const on = value >= 0.5;

  return (
    <button
      type="button"
      aria-pressed={on}
      className={`absolute flex cursor-pointer items-center justify-center ${on ? 'text-fg/30' : 'text-red/80'}`}
      // CustomLookAndFeel::getTextButtonFont: Orbitron at 7 points
      style={{ ...style, fontSize: 7 }}
      onClick={() => commit(on ? 0 : 1)}
    >
      {text}
    </button>
  );
}
