import assert from 'node:assert/strict';
import { test } from 'node:test';
import { magnitudesFromDecibels, MAX_FREQ, MIN_FREQ, SCOPE_SIZE, scopeFromMagnitudes } from '../src/spectrum.ts';

const sampleRate = 48000;
const bins = 1024;

const pointFor = (frequency: number) => Math.round(((SCOPE_SIZE - 1) * Math.log(frequency / MIN_FREQ)) / Math.log(MAX_FREQ / MIN_FREQ));

test('a single tone peaks at 1 where its frequency is on the log scale, and nowhere else', () => {
  const magnitudes = new Float32Array(bins);
  // 1 kHz at 48 kHz with 1024 bins up to Nyquist
  const bin = (1000 / (sampleRate / 2)) * bins;
  magnitudes[Math.round(bin)] = 3;

  const scope = scopeFromMagnitudes(magnitudes, sampleRate);
  const loudest = scope.indexOf(Math.max(...scope));

  assert.equal(scope.length, SCOPE_SIZE);
  assert.ok(Math.abs(loudest - pointFor(1000)) <= 2, `peak at point ${loudest}`);
  assert.ok(Math.max(...scope) <= 1);
  assert.equal(scope[0], 0);
  assert.equal(scope[SCOPE_SIZE - 1], 0);
});

test('a flat spectrum reads 1 across the display', () => {
  const scope = scopeFromMagnitudes(new Float32Array(bins).fill(0.2), sampleRate);

  assert.ok(scope.every((point) => Math.abs(point - 1) < 1e-6));
});

test('silence reads 0, from the analyser\'s -Infinity dB', () => {
  const magnitudes = magnitudesFromDecibels(new Float32Array(bins).fill(-Infinity));
  const scope = scopeFromMagnitudes(magnitudes, sampleRate);

  assert.ok(scope.every((point) => point === 0));
});
