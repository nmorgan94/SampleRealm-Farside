import { UPDATE_HZ } from '@samplerealm/modules/editor/meter.ts';

export { UPDATE_HZ };

// As Source/shared/SpectrumFrame.h: the duck's gain, then the wet signal's bands and the dry signal's, in dB
export const NUM_BANDS = 96;
export const DUCK_GAIN = 0;
export const WET_OFFSET = 1;
export const DRY_OFFSET = WET_OFFSET + NUM_BANDS;
export const FRAME_SIZE = DRY_OFFSET + NUM_BANDS;

export const MIN_HZ = 20;
export const MAX_HZ = 20000;
export const FLOOR_DB = -120;
export const TOP_DB = -6;
export const BOTTOM_DB = -84;

export type SpectrumFrame = Float32Array;

// The frame with nothing heard
export const silentFrame = (): SpectrumFrame => {
  const frame = new Float32Array(FRAME_SIZE).fill(FLOOR_DB);
  frame[DUCK_GAIN] = 1;
  return frame;
};

// How far up the display a level is, from 0 at the bottom to 1
export const proportion = (db: number) => Math.min(Math.max((db - BOTTOM_DB) / (TOP_DB - BOTTOM_DB), 0), 1);

// The middle of a band
export const centreOf = (band: number) => MIN_HZ * (MAX_HZ / MIN_HZ) ** ((band + 0.5) / NUM_BANDS);

// As Effect::filtersDb: what the send's filters do to a frequency, in dB, a 24 dB/oct highpass and a 12 dB/oct lowpass. A cutoff of 0 is a filter that's off
export const filtersDb = (hz: number, highpassHz: number, lowpassHz: number) => {
  const stageDb = (ratio: number) => -10 * Math.log10(1 + ratio ** -4);
  return (highpassHz > 0 ? 2 * stageDb(hz / highpassHz) : 0) + (lowpassHz > 0 ? stageDb(lowpassHz / hz) : 0);
};

// The plugin sends a frame as its bytes in base64
export function decodeFrame(base64: string): SpectrumFrame {
  return new Float32Array(Uint8Array.from(atob(base64), (character) => character.charCodeAt(0)).buffer);
}
