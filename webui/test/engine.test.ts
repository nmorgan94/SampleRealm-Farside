import assert from 'node:assert/strict';
import { test } from 'node:test';
import { blockSize, sampleRate, sine } from '@samplerealm/modules/testing/engine.ts';
import { engine } from './engine.ts';

const samples = (seconds: number) => Math.round(seconds * sampleRate);

const impulse = (seconds: number) => Float32Array.from({ length: samples(seconds) }, (_, sample) => (sample === 0 ? 1 : 0));

const rms = (signal: Float32Array, from = 0, to = signal.length) => {
  let sum = 0;
  for (let sample = from; sample < to; sample++) sum += signal[sample]! ** 2;
  return Math.sqrt(sum / (to - from));
};

test('the ui info carries the version, the commit, the build type and every parameter as ParameterSpecs defines it', () => {
  const { info } = engine();
  const real = (id: string, name: string, min: number, max: number, defaultValue: number, step: number, skew = 1) =>
    ({ id, name, kind: 'real', min, max, defaultValue, step, skew, unit: '', choices: [] });

  assert.match(info.version, /^\d+\.\d+\.\d+$/);
  assert.match(info.commit, /^([0-9a-f]{7,}(-local)?|unknown)$/);
  assert.match(info.build, /^(Debug|Release)$/);
  assert.deepEqual(info.params, [
    real('roomSize', 'Room Size', 0, 1, 0.8, 0.01),
    real('damping', 'Damping', 0, 1, 0.8, 0.01),
    real('width', 'Width', 0, 1, 0.5, 0.01),
    real('mix', 'Mix', 0, 1, 0.5, 0.01),
    real('sendGain', 'Send Gain', 0, 2, 1, 0.01),
    real('highpassFreq', 'Highpass Freq', 20, 20000, 1000, 1, 0.2),
    { id: 'spectrumBypass', name: 'Spectrum Bypass', kind: 'toggle', min: 0, max: 1, defaultValue: 0, step: 1, skew: 1, unit: '', choices: [] },
  ]);
});

test('the output stays finite for silence, denormals, full scale and very loud input across the send path', () => {
  for (const amplitude of [0, 1e-40, 1, 1000]) {
    const reverb = engine();
    for (const [sendGain, highpassFreq, mix] of [[0, 20, 0], [1, 1000, 0.5], [2, 20000, 1], [2, 20, 1]] as const) {
      reverb.set('sendGain', sendGain);
      reverb.set('highpassFreq', highpassFreq);
      reverb.set('mix', mix);
      const output = reverb.processBlocks([sine(0.05, amplitude), sine(0.05, amplitude, 220)]);
      assert.ok(output.every((channel) => channel.every(Number.isFinite)), `${amplitude} at ${sendGain}, ${highpassFreq} Hz, ${mix}`);
    }
  }
});

test('mix 0 gives the input back untouched', () => {
  const reverb = engine();
  reverb.set('mix', 0);
  const input = [sine(0.1, 0.5), sine(0.1, 0.25, 220)];

  assert.deepEqual(reverb.processBlocks(input), input);
});

test('mix 1 with no send is silent', () => {
  const reverb = engine();
  reverb.set('mix', 1);
  reverb.set('sendGain', 0);

  const output = reverb.processBlocks([sine(0.1, 0.5), sine(0.1, 0.25, 220)]);

  assert.ok(output.every((channel) => channel.every((sample) => sample === 0)));
});

test('an impulse leaves a tail that decays', () => {
  const reverb = engine();
  reverb.set('mix', 1);
  reverb.set('highpassFreq', 20);
  const [left] = reverb.processBlocks([impulse(3), impulse(3)]);

  const early = rms(left!, samples(0.1), samples(0.5));
  const late = rms(left!, samples(2.5), samples(3));

  assert.ok(early > 1e-3, `early tail ${early}`);
  assert.ok(late < early / 10, `late ${late} against early ${early}`);
});

test('width 0 makes the wet signal mono', () => {
  const reverb = engine();
  reverb.set('mix', 1);
  reverb.set('width', 0);
  const [left, right] = reverb.processBlocks([sine(0.5, 0.5), sine(0.5, 0.25, 220)]);

  // After the reverb's 10 ms gain ramp from its defaults
  const settled = samples(0.05);
  assert.ok(rms(left!, settled) > 1e-3);
  assert.deepEqual(left!.subarray(settled), right!.subarray(settled));
});

test('the highpass keeps low end out of the reverb', () => {
  const wetRms = (highpassFreq: number) => {
    const reverb = engine();
    reverb.set('mix', 1);
    reverb.set('highpassFreq', highpassFreq);
    const [left] = reverb.processBlocks([sine(0.5, 0.5, 50), sine(0.5, 0.5, 50)]);
    return rms(left!, samples(0.2));
  };

  const open = wetRms(20);
  const filtered = wetRms(5000);

  assert.ok(open > 1e-2, `unfiltered ${open}`);
  assert.ok(filtered < open / 100, `filtered ${filtered} against ${open}`);
});

test('the highpass stays stable when set above half a low sample rate', () => {
  const lowRate = 22050;
  const reverb = engine();
  reverb.engine.prepare(lowRate, 2);
  reverb.set('mix', 1);
  reverb.set('highpassFreq', 18000);
  const output = reverb.processBlocks([sine(0.5, 0.5, 440, lowRate), sine(0.5, 0.25, 220, lowRate)]);

  assert.ok(output.every((channel) => channel.every(Number.isFinite)));
});

test('a mono layout processes only the first channel', () => {
  const mono = engine(1);
  mono.set('mix', 0.5);
  mono.view(0, blockSize).fill(0.5);
  mono.view(1, blockSize).fill(0.5);
  mono.engine.render(blockSize);

  assert.ok(mono.view(0, blockSize).every((sample) => Number.isFinite(sample) && sample !== 0.5));
  assert.ok(mono.view(1, blockSize).every((sample) => sample === 0.5));
});

// The shared processor.js calls it. There's no meter, so no peak
test('setTransport is exported, and peak is not', () => {
  const reverb = engine();

  assert.doesNotThrow(() => reverb.engine.setTransport(120, true, 4));
  assert.equal(reverb.engine.peak, undefined);
});
