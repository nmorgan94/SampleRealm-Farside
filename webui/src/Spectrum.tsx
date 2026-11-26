// Mirrors SpectrumAnalyzer::paint
import { useEffect, useRef } from 'react';
import { useParam } from '@samplerealm/modules/editor/hooks.ts';
import type { FarsideBackend } from './backend.ts';
import { across, DRY_OFFSET, DUCK_GAIN, NUM_BANDS, proportion, silentFrame, WET_OFFSET, type SpectrumFrame } from './spectrumFrame.ts';

const DUCK_BAR_WIDTH = 3;

interface Colours {
  red: string;
  wetFill: CanvasGradient;
  pink: string;
  fg: string;
}

function curve(frame: SpectrumFrame, offset: number, width: number, height: number) {
  const pointAt = (band: number) => ({
    x: (width * (band + 0.5)) / NUM_BANDS,
    y: height * (1 - proportion(frame[offset + band] ?? 0)),
  });

  // Through the middle of each pair of points, bending at the points themselves
  const path = new Path2D();
  let previous = pointAt(0);
  path.moveTo(0, previous.y);
  path.lineTo(previous.x, previous.y);

  for (let band = 1; band < NUM_BANDS; band++) {
    const next = pointAt(band);
    path.quadraticCurveTo(previous.x, previous.y, (previous.x + next.x) / 2, (previous.y + next.y) / 2);
    previous = next;
  }

  path.lineTo(previous.x, previous.y);
  path.lineTo(width, previous.y);
  return path;
}

function draw(context: CanvasRenderingContext2D, frame: SpectrumFrame, highpassFreq: number, highpassOff: number, colours: Colours, width: number, height: number) {
  const ratio = window.devicePixelRatio || 1;
  context.setTransform(ratio, 0, 0, ratio, 0, 0);
  context.clearRect(0, 0, width, height);

  context.strokeStyle = colours.fg;
  context.globalAlpha = 0.35;
  context.lineWidth = 1;
  context.stroke(curve(frame, DRY_OFFSET, width, height));

  const wet = curve(frame, WET_OFFSET, width, height);
  const fill = new Path2D(wet);
  fill.lineTo(width, height);
  fill.lineTo(0, height);
  fill.closePath();

  context.globalAlpha = 1;
  context.fillStyle = colours.wetFill;
  context.fill(fill);

  context.strokeStyle = colours.red;
  context.lineWidth = 2;
  context.stroke(wet);

  // At the bottom of its range the highpass is off
  if (highpassFreq > highpassOff) {
    context.fillStyle = colours.fg;
    context.globalAlpha = 0.5;
    context.fillRect(Math.floor(width * across(highpassFreq)), 0, 1, height);
  }

  // Down from the top, by as much as the duck is holding the wet signal down
  const ducked = 1 - (frame[DUCK_GAIN] ?? 1);

  if (ducked > 0) {
    context.globalAlpha = 1;
    context.fillStyle = colours.pink;
    context.fillRect(width - DUCK_BAR_WIDTH, 0, DUCK_BAR_WIDTH, height * ducked);
  }

  context.globalAlpha = 1;
}

export function Spectrum({ backend, top, width, height }: { backend: FarsideBackend; top: number; width: number; height: number }) {
  const canvas = useRef<HTMLCanvasElement>(null);
  const frame = useRef(silentFrame());
  const redraw = useRef(() => {});
  const highpassFreq = useParam(backend, 'highpassFreq').value;
  const highpassOff = backend.param('highpassFreq').min;
  const cutoff = useRef(highpassFreq);

  useEffect(() => {
    cutoff.current = highpassFreq;
    redraw.current();
  }, [highpassFreq]);

  useEffect(() => {
    const element = canvas.current;
    const context = element?.getContext('2d');
    if (!element || !context) return;
    const ratio = window.devicePixelRatio || 1;
    element.width = width * ratio;
    element.height = height * ratio;

    const style = getComputedStyle(element);
    const token = (name: string) => style.getPropertyValue(name).trim();
    const red = token('--color-red');

    // --color-red is #rrggbb, so two more digits set its opacity: 40% and 10%
    const wetFill = context.createLinearGradient(0, 0, 0, height);
    wetFill.addColorStop(0, `${red}66`);
    wetFill.addColorStop(1, `${red}1a`);

    const colours = { red, wetFill, pink: token('--color-pink'), fg: token('--color-fg') };
    redraw.current = () => draw(context, frame.current, cutoff.current, highpassOff, colours, width, height);

    redraw.current();
    return backend.onSpectrum((next) => {
      frame.current = next;
      redraw.current();
    });
  }, [backend, width, height, highpassOff]);

  return <canvas ref={canvas} className="pointer-events-none absolute left-0" style={{ top, width, height }} aria-hidden />;
}
