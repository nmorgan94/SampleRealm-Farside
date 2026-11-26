// The spectrum this plugin adds to the shared demo backend
import assert from 'node:assert/strict';
import { test } from 'node:test';
import type { EnginePort } from '@samplerealm/modules/demo/workletBackend.ts';
import { memoryStorage } from '@samplerealm/modules/testing/presets.ts';
import { silentFrame, type SpectrumFrame } from '../../src/spectrumFrame.ts';
import { workletBackend } from '../../src/demo/workletBackend.ts';
import { info } from '../engine.ts';

test("the engine's frames go to whoever listens for the spectrum, until they stop", () => {
  const backend = workletBackend(info, { storage: memoryStorage() });
  const port: EnginePort = { postMessage: () => {}, onmessage: null };
  backend.attach(port);

  const frames: SpectrumFrame[] = [];
  const stop = backend.onSpectrum((frame) => frames.push(frame));

  const frame = silentFrame();
  port.onmessage?.({ data: { type: 'frame', frame } } as MessageEvent);
  port.onmessage?.({ data: { type: 'level', peaks: [0, 0] } } as MessageEvent);
  assert.deepEqual(frames, [frame]);

  stop();
  port.onmessage?.({ data: { type: 'frame', frame: frame.map((db) => db + 1) } } as MessageEvent);
  assert.equal(frames.length, 1);
});

test('a frame the same as the last one is not sent on', () => {
  const backend = workletBackend(info, { storage: memoryStorage() });
  const port: EnginePort = { postMessage: () => {}, onmessage: null };
  backend.attach(port);

  let frames = 0;
  backend.onSpectrum(() => frames++);
  const post = (frame: SpectrumFrame) => port.onmessage?.({ data: { type: 'frame', frame } } as MessageEvent);

  post(silentFrame());
  post(silentFrame());
  assert.equal(frames, 1);

  const louder = silentFrame();
  louder[5] = -20;
  post(louder);
  assert.equal(frames, 2);
});
