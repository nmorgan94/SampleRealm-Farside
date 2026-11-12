// Mirrors SpectrumAnalyzer::paint
import { useEffect, useRef } from 'react';
import type { ReverbBackend, Scope } from './backend.ts';
import { SCOPE_SIZE } from './spectrum.ts';

const RED = '#e94560';

function draw(canvas: HTMLCanvasElement, scope: Scope, width: number, height: number) {
  const context = canvas.getContext('2d');
  if (!context) return;

  const ratio = window.devicePixelRatio || 1;
  context.setTransform(ratio, 0, 0, ratio, 0, 0);
  context.clearRect(0, 0, width, height);

  const path = new Path2D();
  path.moveTo(0, height);
  for (let i = 0; i < SCOPE_SIZE; i++) {
    path.lineTo((i / (SCOPE_SIZE - 1)) * width, height - (scope[i] ?? 0) * height);
  }

  context.strokeStyle = RED;
  context.lineWidth = 2;
  context.stroke(path);

  path.lineTo(width, height);
  path.closePath();
  const fill = context.createLinearGradient(0, 0, 0, height);
  fill.addColorStop(0, 'rgb(233 69 96 / 0.4)');
  fill.addColorStop(1, 'rgb(233 69 96 / 0.1)');
  context.fillStyle = fill;
  context.fill(path);
}

export function Spectrum({ backend, width, height }: { backend: ReverbBackend; width: number; height: number }) {
  const canvas = useRef<HTMLCanvasElement>(null);

  useEffect(() => {
    const element = canvas.current;
    if (!element) return;
    const ratio = window.devicePixelRatio || 1;
    element.width = width * ratio;
    element.height = height * ratio;
    draw(element, new Float32Array(SCOPE_SIZE), width, height);
    return backend.onSpectrum((scope) => draw(element, scope, width, height));
  }, [backend, width, height]);

  return <canvas ref={canvas} className="pointer-events-none absolute top-0 left-0" style={{ width, height }} aria-hidden />;
}
