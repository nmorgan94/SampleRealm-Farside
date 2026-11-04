import assert from 'node:assert/strict';
import { test } from 'node:test';
import type { Scope } from '../../src/backend.ts';
import { SCOPE_SIZE, UPDATE_HZ } from '../../src/spectrum.ts';
import { workletBackend, type Analyser, type EnginePort } from '../../src/demo/workletBackend.ts';
import { info } from '../engine.ts';

const fakePort = () => {
  const posted: unknown[] = [];
  const port: EnginePort = { postMessage: (message: unknown) => void posted.push(message), onmessage: null };
  return { port, posted };
};

// Every bin at 0 dB, so the scope reads 1 everywhere
const fakeAnalyser = () => {
  const analyser: Analyser & { reads: number; context: { state: AudioContextState; sampleRate: number } } = {
    reads: 0,
    context: { state: 'running', sampleRate: 48000 },
    frequencyBinCount: 1024,
    getFloatFrequencyData: (array: Float32Array) => {
      analyser.reads++;
      array.fill(0);
    },
  };
  return analyser;
};

const index = (id: string) => info.params.findIndex((p) => p.id === id);

test('changes made before the worklet attaches reach it once it does, in order and once each', () => {
  const backend = workletBackend(info);
  backend.setValue('mix', 0.25);
  backend.setValue('mix', 0.75);

  const { port, posted } = fakePort();
  backend.attach(port);
  backend.setValue('roomSize', 0.1);

  assert.deepEqual(posted, [
    { type: 'param', index: index('mix'), value: 0.25 },
    { type: 'param', index: index('mix'), value: 0.75 },
    { type: 'param', index: index('roomSize'), value: 0.1 },
  ]);
});

test('values read back and notify before the worklet attaches', () => {
  const backend = workletBackend(info);
  const heard: number[] = [];
  backend.subscribe('highpassFreq', (value) => heard.push(value));

  assert.equal(backend.getValue('highpassFreq'), 1000);
  backend.setValue('highpassFreq', 440);

  assert.equal(backend.getValue('highpassFreq'), 440);
  assert.deepEqual(heard, [440]);
});

test('the spectrum is read from the analyser 30 times a second while someone listens, and cleared once while bypassed', (t) => {
  t.mock.timers.enable({ apis: ['setInterval'] });
  const backend = workletBackend(info);
  const analyser = fakeAnalyser();
  backend.attachAnalyser(analyser);

  const scopes: Scope[] = [];
  const stop = backend.onSpectrum((scope) => scopes.push(scope));

  // A hair over a second, as 30 intervals of 1000 / 30 ms add up to more than 1000
  t.mock.timers.tick(1001);
  assert.equal(scopes.length, UPDATE_HZ);
  assert.equal(scopes[0]!.length, SCOPE_SIZE);
  assert.ok(Array.from(scopes[0]!).every((point) => Math.abs(point - 1) < 1e-6));

  backend.setValue('spectrumBypass', 1);
  const readsBefore = analyser.reads;
  t.mock.timers.tick(1000 / UPDATE_HZ);
  assert.ok(Array.from(scopes.at(-1)!).every((point) => point === 0));
  assert.equal(analyser.reads, readsBefore);

  const cleared = scopes.length;
  t.mock.timers.tick(1000);
  assert.equal(scopes.length, cleared);

  stop();
  const heard = scopes.length;
  t.mock.timers.tick(1000);
  assert.equal(scopes.length, heard);
});

test('nothing is read or sent while the context is suspended', (t) => {
  t.mock.timers.enable({ apis: ['setInterval'] });
  const backend = workletBackend(info);
  const analyser = fakeAnalyser();
  analyser.context.state = 'suspended';
  backend.attachAnalyser(analyser);

  const scopes: Scope[] = [];
  backend.onSpectrum((scope) => scopes.push(scope));
  t.mock.timers.tick(1000);
  assert.equal(analyser.reads, 0);
  assert.equal(scopes.length, 0);

  analyser.context.state = 'running';
  t.mock.timers.tick(1000 / UPDATE_HZ);
  assert.equal(scopes.length, 1);
});

test('nothing is read before the analyser attaches', (t) => {
  t.mock.timers.enable({ apis: ['setInterval'] });
  const backend = workletBackend(info);
  const scopes: Scope[] = [];
  backend.onSpectrum((scope) => scopes.push(scope));
  t.mock.timers.tick(1000);
  assert.equal(scopes.length, 0);
});
