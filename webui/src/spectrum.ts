// Ports SpectrumScope::computeScope (Source/ui/SpectrumScope.cpp)
export const FFT_SIZE = 2048;
export const SCOPE_SIZE = 512;
export const MIN_FREQ = 20;
export const MAX_FREQ = 20000;
export const UPDATE_HZ = 30;

// Bin magnitudes from 0 Hz to just below Nyquist, as many as half the FFT size
export function scopeFromMagnitudes(magnitudes: ArrayLike<number>, sampleRate: number): Float32Array {
  const bins = magnitudes.length;
  let maxMagnitude = 1e-5;
  for (let i = 0; i < bins; i++) maxMagnitude = Math.max(maxMagnitude, magnitudes[i]!);

  const scope = new Float32Array(SCOPE_SIZE);
  for (let i = 0; i < SCOPE_SIZE; i++) {
    const freq = MIN_FREQ * Math.pow(MAX_FREQ / MIN_FREQ, i / (SCOPE_SIZE - 1));
    const bin = (freq / (sampleRate / 2)) * bins;
    const whole = Math.trunc(bin);
    const index = Math.min(Math.max(whole, 0), bins - 2);
    const low = magnitudes[index]!;
    const high = magnitudes[index + 1]!;
    scope[i] = (low + (bin - whole) * (high - low)) / maxMagnitude;
  }
  return scope;
}

// AnalyserNode.getFloatFrequencyData's dB, where silence is -Infinity
export const magnitudesFromDecibels = (decibels: Float32Array) => decibels.map((db) => 10 ** (db / 20));
