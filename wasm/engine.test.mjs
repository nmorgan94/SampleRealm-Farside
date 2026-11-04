import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { test } from 'node:test';
import { localEnginePath } from '../webui/localEngine.ts';
import { readUiInfo, stubImports } from '../webui/src/demo/workletBackend.ts';

const sampleRate = 48000;
const blockSize = 128;

const wasm = new WebAssembly.Module(readFileSync(localEnginePath()));

function engine(numChannels = 2, rate = sampleRate) {
  const { exports } = new WebAssembly.Instance(wasm, stubImports());
  exports._initialize?.();
  exports.prepare(rate, numChannels);

  const ids = readUiInfo(exports).params.map((p) => p.id);
  const set = (id, value) => {
    assert.ok(ids.includes(id), `no parameter ${id}`);
    exports.setParam(ids.indexOf(id), value);
  };

  const view = (channel, length) => new Float32Array(exports.memory.buffer, exports.channel(channel), length);

  // As an AudioWorklet: one block at a time, written in and read back from the engine's channels
  const processBlocks = (inputs) => {
    const outputs = inputs.map((input) => new Float32Array(input.length));
    for (let start = 0; start < inputs[0].length; start += blockSize) {
      const length = Math.min(blockSize, inputs[0].length - start);
      inputs.forEach((input, channel) => view(channel, length).set(input.subarray(start, start + length)));
      exports.render(length);
      outputs.forEach((output, channel) => output.set(view(channel, length), start));
    }
    return outputs;
  };

  return { ...exports, set, view, processBlocks };
}

const samples = (seconds) => Math.round(seconds * sampleRate);

const sine = (seconds, amplitude, frequency = 440, rate = sampleRate) =>
  Float32Array.from({ length: Math.round(seconds * rate) }, (_, i) => amplitude * Math.sin((2 * Math.PI * frequency * i) / rate));

const impulse = (seconds) => Float32Array.from({ length: samples(seconds) }, (_, i) => (i === 0 ? 1 : 0));

const rms = (signal, from = 0, to = signal.length) => {
  let sum = 0;
  for (let i = from; i < to; i++) sum += signal[i] ** 2;
  return Math.sqrt(sum / (to - from));
};

test('the ui info carries the version, the commit, the build type and every parameter as ParameterSpecs defines it', () => {
  const info = readUiInfo(engine());
  const real = (id, name, min, max, defaultValue, step, skew = 1) =>
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
    const e = engine();
    for (const [sendGain, highpassFreq, mix] of [[0, 20, 0], [1, 1000, 0.5], [2, 20000, 1], [2, 20, 1]]) {
      e.set('sendGain', sendGain);
      e.set('highpassFreq', highpassFreq);
      e.set('mix', mix);
      const output = e.processBlocks([sine(0.05, amplitude), sine(0.05, amplitude, 220)]);
      assert.ok(output.every((channel) => channel.every(Number.isFinite)), `${amplitude} at ${sendGain}, ${highpassFreq} Hz, ${mix}`);
    }
  }
});

test('mix 0 gives the input back untouched', () => {
  const e = engine();
  e.set('mix', 0);
  const input = [sine(0.1, 0.5), sine(0.1, 0.25, 220)];

  assert.deepEqual(e.processBlocks(input), input);
});

test('mix 1 with no send is silent', () => {
  const e = engine();
  e.set('mix', 1);
  e.set('sendGain', 0);

  const output = e.processBlocks([sine(0.1, 0.5), sine(0.1, 0.25, 220)]);

  assert.ok(output.every((channel) => channel.every((sample) => sample === 0)));
});

test('an impulse leaves a tail that decays', () => {
  const e = engine();
  e.set('mix', 1);
  e.set('highpassFreq', 20);
  const [left] = e.processBlocks([impulse(3), impulse(3)]);

  const early = rms(left, samples(0.1), samples(0.5));
  const late = rms(left, samples(2.5), samples(3));

  assert.ok(early > 1e-3, `early tail ${early}`);
  assert.ok(late < early / 10, `late ${late} against early ${early}`);
});

test('width 0 makes the wet signal mono', () => {
  const e = engine();
  e.set('mix', 1);
  e.set('width', 0);
  const [left, right] = e.processBlocks([sine(0.5, 0.5), sine(0.5, 0.25, 220)]);

  // After the reverb's 10 ms gain ramp from its defaults
  const settled = samples(0.05);
  assert.ok(rms(left, settled) > 1e-3);
  assert.deepEqual(left.subarray(settled), right.subarray(settled));
});

test('the highpass keeps low end out of the reverb', () => {
  const wetRms = (highpassFreq) => {
    const e = engine();
    e.set('mix', 1);
    e.set('highpassFreq', highpassFreq);
    const [left] = e.processBlocks([sine(0.5, 0.5, 50), sine(0.5, 0.5, 50)]);
    return rms(left, samples(0.2));
  };

  const open = wetRms(20);
  const filtered = wetRms(5000);

  assert.ok(open > 1e-2, `unfiltered ${open}`);
  assert.ok(filtered < open / 100, `filtered ${filtered} against ${open}`);
});

test('the highpass stays stable when set above half a low sample rate', () => {
  const rate = 22050;
  const e = engine(2, rate);
  e.set('mix', 1);
  e.set('highpassFreq', 18000);
  const output = e.processBlocks([sine(0.5, 0.5, 440, rate), sine(0.5, 0.25, 220, rate)]);

  assert.ok(output.every((channel) => channel.every(Number.isFinite)));
});

test('a mono layout processes only the first channel', () => {
  const e = engine(1);
  e.set('mix', 0.5);
  e.view(0, blockSize).fill(0.5);
  e.view(1, blockSize).fill(0.5);
  e.render(blockSize);

  assert.ok(e.view(0, blockSize).every((sample) => Number.isFinite(sample) && sample !== 0.5));
  assert.ok(e.view(1, blockSize).every((sample) => sample === 0.5));
});

// processor.js calls it
test('setTransport is exported', () => {
  assert.doesNotThrow(() => engine().setTransport(120, true, 4));
});
