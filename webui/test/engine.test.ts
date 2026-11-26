import assert from 'node:assert/strict';
import { test } from 'node:test';
import { blockSize, maxError, rms, sampleRate, sine } from '@samplerealm/modules/testing/engine.ts';
import { engine, param } from './engine.ts';

const samples = (seconds: number) => Math.round(seconds * sampleRate);

const impulse = (seconds: number) => Float32Array.from({ length: samples(seconds) }, (_, sample) => (sample === 0 ? 1 : 0));

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
    real('highpassFreq', 'Highpass Freq', 20, 5000, 250, 1, 0.4),
    real('lowpassFreq', 'Lowpass Freq', 500, 20000, 20000, 1, 0.4),
    real('drive', 'Drive', 0, 1, 0, 0.01),
    { id: 'delaySync', name: 'Delay Sync', kind: 'toggle', min: 0, max: 1, defaultValue: 1, step: 1, skew: 1, unit: '', choices: [] },
    { id: 'delayNote', name: 'Delay Note', kind: 'choice', min: 0, max: 6, defaultValue: 4, step: 1, skew: 1, unit: '', choices: ['1/16', '1/8T', '1/8', '1/8D', '1/4', '1/4D', '1/2'] },
    real('delayTime', 'Delay Time', 10, 2000, 375, 1, 0.5),
    real('delayFeedback', 'Delay Feedback', 0, 0.95, 0.35, 0.01),
    real('delayLevel', 'Delay Level', 0, 1, 0, 0.01),
    real('duck', 'Duck', 0, 1, 0, 0.01),
    real('outputGain', 'Output Gain', -24, 12, 0, 0.1),
    { id: 'reverbType', name: 'Reverb Type', kind: 'choice', min: 0, max: 2, defaultValue: 0, step: 1, skew: 1, unit: '', choices: ['Classic', 'Plate', 'Hall'] },
    real('preDelay', 'Pre-Delay', 0, 250, 0, 1, 0.5),
    { id: 'spectrumBypass', name: 'Spectrum Bypass', kind: 'toggle', min: 0, max: 1, defaultValue: 0, step: 1, skew: 1, unit: '', choices: [], isPerformanceControl: true },
  ]);
});

test('the output stays finite for silence, denormals, full scale and very loud input across the send path', () => {
  for (const amplitude of [0, 1e-40, 1, 1000]) {
    const reverb = engine();
    reverb.set('drive', 1);
    reverb.set('delayLevel', 1);
    reverb.set('delayFeedback', 0.95);
    reverb.set('duck', 1);
    for (const [highpassFreq, mix] of [[20, 0], [1000, 0.5], [5000, 1], [20, 1]] as const) {
      reverb.set('highpassFreq', highpassFreq);
      reverb.set('mix', mix);
      const output = reverb.processBlocks([sine(0.05, amplitude), sine(0.05, amplitude, 220)]);
      assert.ok(output.every((channel) => channel.every(Number.isFinite)), `${amplitude} at ${highpassFreq} Hz, ${mix}`);
    }
  }
});

test('mix 0 gives the input back untouched', () => {
  const reverb = engine();
  reverb.set('mix', 0);
  const input = [sine(0.1, 0.5), sine(0.1, 0.25, 220)];

  assert.deepEqual(reverb.processBlocks(input), input);
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

test('the lowpass keeps the top end out of the reverb, and is off at the top of its range', () => {
  const wetRms = (lowpassFreq: number) => {
    const reverb = engine();
    reverb.set('mix', 1);
    reverb.set('highpassFreq', 20);
    reverb.set('lowpassFreq', lowpassFreq);
    const [left] = reverb.processBlocks([sine(0.5, 0.5, 8000), sine(0.5, 0.5, 8000)]);
    return rms(left!, samples(0.2));
  };

  const open = wetRms(20000);
  const filtered = wetRms(500);

  assert.ok(open > 1e-3, `unfiltered ${open}`);
  assert.ok(filtered < open / 100, `filtered ${filtered} against ${open}`);
});

test('the mix leaves the dry signal at unity up to half way, then fades it out', () => {
  // With no room, the wet signal is small beside the dry one
  const output = (mix: number) => {
    const reverb = engine();
    reverb.set('mix', mix);
    reverb.set('roomSize', 0);
    reverb.set('highpassFreq', 5000);
    const [left] = reverb.processBlocks([sine(0.5, 0.5, 100), sine(0.5, 0.5, 100)]);
    return rms(left!, samples(0.2));
  };

  const dry = output(0);
  assert.ok(Math.abs(output(0.5) / dry - 1) < 0.02, `half way ${output(0.5)} against ${dry}`);
  assert.ok(Math.abs(output(0.75) / dry - 0.5) < 0.02, `three quarters ${output(0.75)} against ${dry}`);
  assert.ok(output(1) < dry / 50, `fully wet ${output(1)}`);
});

test('the lowpass stays stable when set above half a low sample rate', () => {
  const lowRate = 22050;
  const reverb = engine();
  reverb.engine.prepare(lowRate, 2);
  reverb.set('mix', 1);
  reverb.set('highpassFreq', 20);
  reverb.set('lowpassFreq', 18000);
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

test('drive changes the send without making one at -12 dB much louder', () => {
  const wetRms = (drive: number) => {
    const reverb = engine();
    reverb.set('mix', 1);
    reverb.set('highpassFreq', 20);
    reverb.set('drive', drive);
    const [left] = reverb.processBlocks([sine(0.5, 0.25), sine(0.5, 0.25)]);
    return rms(left!, samples(0.2));
  };

  const louder = wetRms(1) / wetRms(0);
  // A square wave has 1.41 times the RMS of a sine with its peak
  assert.ok(louder > 1.1 && louder < 2, `${louder} times`);
});

// What the delay adds to the wet signal: an impulse's output with the delay up, less its output with it down.
// The reverb's shortest path is longer than a sample, so at the echo's own sample this is the echo alone
const echoesOfAnImpulse = (setUp: (reverb: ReturnType<typeof engine>) => void) => {
  const run = (delayLevel: number) => {
    const reverb = engine();
    reverb.set('mix', 1);
    reverb.set('highpassFreq', 20);
    reverb.set('delayLevel', delayLevel);
    setUp(reverb);
    return reverb.processBlocks([impulse(1), impulse(1)]);
  };

  const without = run(0);
  return run(1).map((channel, index) => channel.map((sample, at) => sample - without[index]![at]!));
};

test('an unsynced echo comes back after the delay time, on the left first', () => {
  const [left, right] = echoesOfAnImpulse((reverb) => {
    reverb.set('delaySync', 0);
    reverb.set('delayTime', 100);
  });
  const at = samples(0.1);

  assert.ok(Math.abs(left![at]! - 1) < 1e-4, `left ${left![at]}`);
  assert.ok(Math.abs(right![at]!) < 1e-4, `right ${right![at]}`);
  assert.ok(left!.subarray(0, at).every((sample) => sample === 0));
  assert.ok(right!.subarray(0, at).every((sample) => sample === 0));
});

test('a synced echo follows the note and the tempo', () => {
  const echoAt = (bpm: number, note: number) => {
    const [left] = echoesOfAnImpulse((reverb) => {
      reverb.engine.setTransport(bpm, true, 0);
      reverb.set('delayNote', note);
    });
    return left!.findIndex((sample) => Math.abs(sample) > 0.5);
  };

  // 1/4 and 1/8
  assert.equal(echoAt(120, 4), samples(0.5));
  assert.equal(echoAt(150, 4), samples(0.4));
  assert.equal(echoAt(120, 2), samples(0.25));
});

test('with the delay level at 0 the delay adds nothing, whatever its settings', () => {
  const run = (setUp: (reverb: ReturnType<typeof engine>) => void) => {
    const reverb = engine();
    setUp(reverb);
    return reverb.processBlocks([sine(0.5, 0.5), sine(0.5, 0.25, 220)]);
  };

  const changed = run((reverb) => {
    reverb.set('delaySync', 0);
    reverb.set('delayTime', 50);
    reverb.set('delayFeedback', 0.95);
  });

  assert.deepEqual(changed, run(() => {}));
});

test('duck holds the wet signal down under the dry one, and lets the tail up once it stops', () => {
  const wetLevels = (duck: number) => {
    const reverb = engine();
    reverb.set('mix', 1);
    reverb.set('duck', duck);
    const dry = new Float32Array(samples(2));
    dry.set(sine(1, 0.5, 2000));
    const [left] = reverb.processBlocks([dry, dry]);
    return { underDry: rms(left!, samples(0.5), samples(1)), afterDry: rms(left!, samples(1.5), samples(2)) };
  };

  const open = wetLevels(0);
  const ducked = wetLevels(1);

  assert.ok(open.underDry > 1e-2, `open ${open.underDry}`);
  assert.ok(ducked.underDry < open.underDry / 100, `ducked ${ducked.underDry} against ${open.underDry}`);
  assert.ok(ducked.afterDry > open.afterDry * 0.8, `tail ${ducked.afterDry} against ${open.afterDry}`);
});

test('a quiet dry signal ducks the wet one as far as a loud one does', () => {
  const ducked = (level: number) => {
    const wet = (duck: number) => {
      const reverb = engine();
      reverb.set('mix', 1);
      reverb.set('duck', duck);
      const [left] = reverb.processBlocks([sine(1, level, 2000), sine(1, level, 2000)]);
      return rms(left!, samples(0.5));
    };
    return wet(0.5) / wet(0);
  };

  // 20 dB apart
  const loud = ducked(0.5);
  assert.ok(Math.abs(loud - 0.5) < 0.05, `loud ${loud}`);
  assert.ok(Math.abs(ducked(0.05) - loud) < 0.01, `quiet ${ducked(0.05)} against ${loud}`);
});

const REVERB_TYPES = param('reverbType').choices;

test('every reverb leaves a tail that decays, stays finite with the room full, and is as loud as the others', () => {
  const levels = REVERB_TYPES.map((name, type) => {
    const reverb = engine();
    reverb.set('reverbType', type);
    reverb.set('mix', 1);
    reverb.set('highpassFreq', 20);
    const [left] = reverb.processBlocks([impulse(3), impulse(3)]);
    const early = rms(left!, samples(0.1), samples(0.5));
    const late = rms(left!, samples(2.5), samples(3));

    assert.ok(early > 1e-4, `${name}: early tail ${early}`);
    assert.ok(late < early / 10, `${name}: late ${late} against early ${early}`);

    const full = engine();
    full.set('reverbType', type);
    full.set('mix', 1);
    full.set('roomSize', 1);
    full.set('damping', 0);
    const [loud] = full.processBlocks([sine(5, 1), sine(5, 1, 220)]);
    assert.ok(loud!.every(Number.isFinite), `${name}: finite`);

    // Noise, as a tone can sit on one reverb's resonance and between another's
    const noisy = engine();
    noisy.set('reverbType', type);
    noisy.set('mix', 1);
    let seed = 1;
    const noise = Float32Array.from({ length: samples(3) }, () => {
      seed = (seed * 16807) % 2147483647;
      return (seed / 2147483647 - 0.5) * 0.5;
    });
    const [wet] = noisy.processBlocks([noise, noise]);
    return rms(wet!, samples(1.5));
  });

  for (const [type, level] of levels.entries()) {
    const db = 20 * Math.log10(level / levels[0]!);
    assert.ok(Math.abs(db) < 3, `${REVERB_TYPES[type]}: ${db} dB from Classic`);
  }
});

test('the pre-delay holds every reverb back', () => {
  for (const [type, name] of REVERB_TYPES.entries()) {
    const reverb = engine();
    reverb.set('reverbType', type);
    reverb.set('mix', 1);
    reverb.set('highpassFreq', 20);
    reverb.set('preDelay', 100);
    const [left] = reverb.processBlocks([impulse(0.5), impulse(0.5)]);

    assert.ok(left!.subarray(0, samples(0.1)).every((sample) => sample === 0), `${name}: silent before`);
    assert.ok(rms(left!, samples(0.1), samples(0.5)) > 1e-4, `${name}: heard after`);
  }
});

test('the output gain is in dB, on the dry and wet signals together', () => {
  const output = (outputGain: number) => {
    const reverb = engine();
    reverb.set('outputGain', outputGain);
    const [left] = reverb.processBlocks([sine(0.5, 0.5), sine(0.5, 0.25, 220)]);
    return left!;
  };

  const unity = output(0);
  const down = output(-6);
  const gain = 10 ** (-6 / 20);
  assert.ok(maxError(down, unity.map((sample) => sample * gain)) < 1e-6);
});

test('the peak is the loudest output since the last read, per channel', () => {
  const reverb = engine();
  reverb.set('mix', 0);
  reverb.processBlocks([sine(0.05, 0.8), sine(0.05, 0.4)]);

  assert.ok(Math.abs(reverb.engine.peak(0) - 0.8) < 1e-3, 'channel 0');
  assert.ok(Math.abs(reverb.engine.peak(1) - 0.4) < 1e-3, 'channel 1');
  assert.equal(reverb.engine.peak(0), 0);
  assert.equal(reverb.engine.peak(2), 0);
});
