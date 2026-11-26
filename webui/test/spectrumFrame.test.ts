import assert from 'node:assert/strict';
import { test } from 'node:test';
import { sine } from '@samplerealm/modules/testing/engine.ts';
import { BOTTOM_DB, decodeFrame, DRY_OFFSET, DUCK_GAIN, filtersDb, FLOOR_DB, FRAME_SIZE, MAX_HZ, MIN_HZ, NUM_BANDS, proportion, silentFrame, TOP_DB, UPDATE_HZ, WET_OFFSET } from '../src/spectrumFrame.ts';
import { engine } from './engine.ts';

const bandOf = (hz: number) => Math.floor((NUM_BANDS * Math.log(hz / MIN_HZ)) / Math.log(MAX_HZ / MIN_HZ));

// The frame after a second of a 1 kHz sine, with one read as often as the display reads one
const frameAfterTone = (set: Record<string, number> = {}) => {
  const reverb = engine();
  Object.entries(set).forEach(([id, value]) => reverb.set(id, value));
  const exports = reverb.engine;
  const tone = sine(1, 0.5, 1000);
  const perFrame = tone.length / UPDATE_HZ;
  let pointer = 0;

  for (let start = 0; start < tone.length; start += perFrame) {
    const block = tone.subarray(start, start + perFrame);
    reverb.processBlocks([block, block]);
    pointer = exports.frame(1 / UPDATE_HZ);
  }

  return new Float32Array(exports.memory.buffer, pointer, exports.frameSize());
};

test("the frame is laid out as the engine's, and starts silent", () => {
  const exports = engine().engine;
  const frame = new Float32Array(exports.memory.buffer, exports.frame(0), exports.frameSize());

  assert.equal(exports.frameSize(), FRAME_SIZE);
  assert.deepEqual(Array.from(frame), Array.from(silentFrame()));
});

test('a tone shows in its band of the dry signal, and of the wet one with the mix up', () => {
  const band = bandOf(1000);
  const mixed = frameAfterTone({ mix: 0.5, highpassFreq: 20 });
  const dry = frameAfterTone({ mix: 0 });

  // The dry signal is at unity up to half way: a 0.5 sine is -6 dB
  assert.ok(Math.abs(mixed[DRY_OFFSET + band]! - -6) < 2, `dry ${mixed[DRY_OFFSET + band]}`);
  assert.ok(mixed[WET_OFFSET + band]! > -40, `wet ${mixed[WET_OFFSET + band]}`);
  assert.ok(mixed[DRY_OFFSET + bandOf(100)]! < -60);
  assert.equal(mixed[DUCK_GAIN], 1);

  assert.ok(Math.abs(dry[DRY_OFFSET + band]! - -6) < 2, `dry alone ${dry[DRY_OFFSET + band]}`);
  assert.ok(Array.from(dry.subarray(WET_OFFSET, DRY_OFFSET)).every((db) => db === FLOOR_DB));
});

test('the duck gain falls while a full duck holds the wet signal down', () => {
  assert.ok(frameAfterTone({ duck: 1 })[DUCK_GAIN]! < 0.05);
});

test('the frame is silent while the analyser is bypassed', () => {
  assert.deepEqual(Array.from(frameAfterTone({ spectrumBypass: 1 })), Array.from(silentFrame()));
});

test('the level scale runs from the bottom to the top', () => {
  assert.equal(proportion(BOTTOM_DB), 0);
  assert.equal(proportion(TOP_DB), 1);
  assert.equal(proportion(-200), 0);
  assert.equal(proportion(0), 1);
});

test("a frame comes back from the plugin's base64 as the floats it was", () => {
  const frame = silentFrame();
  frame[WET_OFFSET] = -12.5;
  const base64 = Buffer.from(frame.buffer).toString('base64');

  assert.deepEqual(decodeFrame(base64), frame);
});

test("the filters' curve is 6 dB down at the highpass, 3 dB at the lowpass, and falls 24 and 12 dB an octave", () => {
  const near = (actual: number, expected: number) => assert.ok(Math.abs(actual - expected) < 0.1, `${actual} against ${expected}`);

  near(filtersDb(250, 250, 0), -6.02);
  near(filtersDb(4000, 0, 4000), -3.01);
  near(filtersDb(25, 400, 0) - filtersDb(50, 400, 0), -24);
  near(filtersDb(16000, 0, 1000) - filtersDb(8000, 0, 1000), -12);
  near(filtersDb(1000, 100, 10000), 0);
  assert.equal(filtersDb(1000, 0, 0), 0);
});
