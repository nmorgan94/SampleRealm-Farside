// The shared demo backend, with the spectrum's frames from the engine
import type { PresetStorage } from '@samplerealm/modules/demo/localPresets.ts';
import { workletBackend as sharedBackend, type WorkletBackend as SharedBackend } from '@samplerealm/modules/demo/workletBackend.ts';
import { listenerSet, type FarsideBackend, type UiInfo } from '../backend.ts';
import { UPDATE_HZ, type SpectrumFrame } from '../spectrumFrame.ts';

export type WorkletBackend = SharedBackend & FarsideBackend;

// This plugin's, as a site's demos share its storage
const STORAGE_KEY = 'samplerealm-farside-presets';

// What wasm/Engine.cpp exports beside the shared ones
export const FRAMES = { name: 'frame', size: 'frameSize', perSecond: UPDATE_HZ };

export function workletBackend(info: UiInfo, { factoryPresets, storage }: { factoryPresets?: string[]; storage?: PresetStorage } = {}): WorkletBackend {
  const backend = sharedBackend(info, { storageKey: STORAGE_KEY, factoryPresets, storage });
  const spectrumListeners = listenerSet<[SpectrumFrame]>();

  let last: SpectrumFrame | null = null;

  // The engine posts every frame: only one that has changed is drawn
  backend.onMessage((data) => {
    if (data.type !== 'frame') return;
    const frame = data.frame as SpectrumFrame;
    if (last?.every((value, index) => value === frame[index])) return;
    last = frame;
    spectrumListeners.notify(frame);
  });

  return { ...backend, onSpectrum: spectrumListeners.add };
}
