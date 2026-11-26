// Mirrors the knob and the SYNC button as Source/ui/CustomLookAndFeel.h draws them
import { useId } from 'react';
import { useParam } from '@samplerealm/modules/editor/hooks.ts';
import { arcPath, ROTARY_END, ROTARY_START, type RotaryProps } from '@samplerealm/modules/editor/LabeledSlider.tsx';
import type { EditorBackend } from './backend.ts';

// CustomLookAndFeel::drawRotarySlider
export function FarsideRotary({ width, height, proportion }: RotaryProps) {
  const gradient = useId();
  const centreX = width / 2;
  const centreY = height / 2;
  // Scaled from a 70 px knob, so the header's small one is drawn the same
  const scale = Math.min(width, height) / 70;
  const radius = Math.min(width, height) / 2 - 10 * scale;
  const lineWidth = Math.min(5, radius * 0.2);
  const pointerThickness = Math.min(3, radius * 0.12);
  const arcRadius = radius - lineWidth * 0.5;
  const toAngle = ROTARY_START + proportion * (ROTARY_END - ROTARY_START);
  const dot = radius * 0.18;
  const pointerLength = radius * 0.55;

  return (
    <svg width={width} height={height} aria-hidden>
      <defs>
        <linearGradient id={`${gradient}arc`} gradientUnits="userSpaceOnUse" x1={0} y1={centreY - radius} x2={0} y2={centreY + radius}>
          <stop offset={0} stopColor="var(--color-red)" />
          <stop offset={1} stopColor="var(--color-pink)" />
        </linearGradient>
        <linearGradient id={`${gradient}dot`} gradientUnits="userSpaceOnUse" x1={0} y1={centreY - dot} x2={0} y2={centreY + dot}>
          <stop offset={0} stopColor="var(--color-red)" />
          <stop offset={1} stopColor="var(--color-pink)" />
        </linearGradient>
      </defs>
      <circle cx={centreX} cy={centreY} r={radius + 5 * scale} fill="var(--color-blue)" fillOpacity={0.3} />
      <path
        d={arcPath(centreX, centreY, arcRadius, ROTARY_START, ROTARY_END)}
        stroke="var(--color-blue)"
        strokeOpacity={0.6}
        strokeWidth={lineWidth}
        strokeLinecap="round"
        fill="none"
      />
      {proportion > 0 && (
        <path
          d={arcPath(centreX, centreY, arcRadius, ROTARY_START, toAngle)}
          stroke={`url(#${gradient}arc)`}
          strokeWidth={lineWidth}
          strokeLinecap="round"
          fill="none"
        />
      )}
      <rect
        x={-pointerThickness / 2}
        y={-radius + lineWidth + radius * 0.2}
        width={pointerThickness}
        height={pointerLength}
        rx={pointerThickness / 2}
        fill="var(--color-red)"
        transform={`translate(${centreX} ${centreY}) rotate(${(toAngle * 180) / Math.PI})`}
      />
      <circle cx={centreX} cy={centreY} r={dot} fill={`url(#${gradient}dot)`} />
    </svg>
  );
}

interface ToggleTextProps {
  backend: EditorBackend;
  id: string;
  text: string;
  style: { left: number; top: number; width: number; height: number };
}

// A text-only toggle button, lit while on
export function ToggleText({ backend, id, text, style }: ToggleTextProps) {
  const { value, commit } = useParam(backend, id);
  const on = value >= 0.5;

  return (
    <button
      type="button"
      aria-pressed={on}
      className={`absolute flex cursor-pointer items-center justify-center ${on ? 'text-red/80' : 'text-fg/30'}`}
      // CustomLookAndFeel::getTextButtonFont: Orbitron at 7 points
      style={{ ...style, fontSize: 7 }}
      onClick={() => commit(on ? 0 : 1)}
    >
      {text}
    </button>
  );
}

// A text-only button that shows a choice parameter's value, and moves on to the next when clicked
export function ChoiceText({ backend, id, style }: Omit<ToggleTextProps, 'text'>) {
  const { value, commit } = useParam(backend, id);
  const { choices, name } = backend.param(id);
  const index = Math.round(value);

  return (
    <button
      type="button"
      aria-label={name}
      className="absolute flex cursor-pointer items-center justify-center text-red/80 uppercase"
      style={{ ...style, fontSize: 7 }}
      onClick={() => commit((index + 1) % choices.length)}
    >
      {choices[index]}
    </button>
  );
}
