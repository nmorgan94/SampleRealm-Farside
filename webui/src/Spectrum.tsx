// Mirrors SpectrumAnalyzer::paint
import { useEffect, useRef } from 'react';
import { useParam } from '@samplerealm/modules/editor/hooks.ts';
import type { FarsideBackend } from './backend.ts';
import { centreOf, DRY_OFFSET, DUCK_GAIN, filtersDb, NUM_BANDS, proportion, silentFrame, TOP_DB, WET_OFFSET, type SpectrumFrame } from './spectrumFrame.ts';

const DUCK_BAR_WIDTH = 3;

// Half the filters' curve's width, so all of it shows at the top
const FILTERS_INSET = 0.5;

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

interface Cutoffs {
  highpass: number;
  lowpass: number;
}

// What the send's filters pass, against the top of the display. A cutoff of 0 is a filter that's off
function filtersCurve({ highpass, lowpass }: Cutoffs, width: number, height: number) {
  const path = new Path2D();

  for (let band = 0; band < NUM_BANDS; band++) {
    const db = filtersDb(centreOf(band), highpass, lowpass);
    path.lineTo((width * (band + 0.5)) / NUM_BANDS, FILTERS_INSET + height * (1 - proportion(TOP_DB + db)));
  }

  return path;
}

function draw(context: CanvasRenderingContext2D, frame: SpectrumFrame, cutoffs: Cutoffs, colours: Colours, width: number, height: number) {
  const ratio = window.devicePixelRatio || 1;
  context.setTransform(ratio, 0, 0, ratio, 0, 0);
  context.clearRect(0, 0, width, height);

  const filled = (path: Path2D) => {
    const shape = new Path2D(path);
    shape.lineTo(width, height);
    shape.lineTo(0, height);
    shape.closePath();
    return shape;
  };

  // The dry signal is a shape with no outline, to tell it from the filters' curve
  context.fillStyle = colours.fg;
  context.globalAlpha = 0.12;
  context.fill(filled(curve(frame, DRY_OFFSET, width, height)));

  const wet = curve(frame, WET_OFFSET, width, height);

  context.globalAlpha = 1;
  context.fillStyle = colours.wetFill;
  context.fill(filled(wet));

  context.strokeStyle = colours.red;
  context.lineWidth = 2;
  context.stroke(wet);

  context.strokeStyle = colours.fg;
  context.globalAlpha = 0.25;
  context.lineWidth = 1;
  context.stroke(filtersCurve(cutoffs, width, height));

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
  const lowpassFreq = useParam(backend, 'lowpassFreq').value;
  const cutoffs = useRef<Cutoffs>({ highpass: 0, lowpass: 0 });

  // At the end of its range a filter is off, and its side of the curve is flat
  useEffect(() => {
    cutoffs.current = {
      highpass: highpassFreq > backend.param('highpassFreq').min ? highpassFreq : 0,
      lowpass: lowpassFreq < backend.param('lowpassFreq').max ? lowpassFreq : 0,
    };
    redraw.current();
  }, [backend, highpassFreq, lowpassFreq]);

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
    redraw.current = () => draw(context, frame.current, cutoffs.current, colours, width, height);

    redraw.current();
    return backend.onSpectrum((next) => {
      frame.current = next;
      redraw.current();
    });
  }, [backend, width, height]);

  return <canvas ref={canvas} className="pointer-events-none absolute left-0" style={{ top, width, height }} aria-hidden />;
}
