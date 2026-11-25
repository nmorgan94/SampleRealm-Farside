// The shared demo backend, with the spectrum read from an AnalyserNode
import type { PresetStorage } from '@samplerealm/modules/demo/localPresets.ts';
import { workletBackend as sharedBackend, type WorkletBackend as SharedBackend } from '@samplerealm/modules/demo/workletBackend.ts';
import type { FarsideBackend, Scope, UiInfo } from '../backend.ts';
import { magnitudesFromDecibels, SCOPE_SIZE, scopeFromMagnitudes, UPDATE_HZ } from '../spectrum.ts';

// The AnalyserNode after the worklet, so tests can stand in for it
export type Analyser = Pick<AnalyserNode, 'frequencyBinCount' | 'getFloatFrequencyData'> & { context: Pick<BaseAudioContext, 'state' | 'sampleRate'> };

export type WorkletBackend = SharedBackend & FarsideBackend & { attachAnalyser(analyser: Analyser): void };

// This plugin's, as a site's demos share its storage
const STORAGE_KEY = 'samplerealm-farside-presets';

const SPECTRUM_BYPASS = 'spectrumBypass';

export function workletBackend(info: UiInfo, { factoryPresets, storage }: { factoryPresets?: string[]; storage?: PresetStorage } = {}): WorkletBackend {
  const backend = sharedBackend(info, { storageKey: STORAGE_KEY, factoryPresets, storage });
  const spectrumListeners = new Set<(scope: Scope) => void>();
  let analyser: { node: Analyser; decibels: Float32Array<ArrayBuffer> } | null = null;
  let timer: ReturnType<typeof setInterval> | undefined;
  let scopeCleared = false;

  // As SpectrumScope::update: cleared once while bypassed, and nothing new while suspended
  const readSpectrum = (): Scope | null => {
    if (!analyser) return null;

    if (backend.getValue(SPECTRUM_BYPASS) >= 0.5) {
      if (scopeCleared) return null;
      scopeCleared = true;
      return new Float32Array(SCOPE_SIZE);
    }

    if (analyser.node.context.state !== 'running') return null;
    scopeCleared = false;
    analyser.node.getFloatFrequencyData(analyser.decibels);
    return scopeFromMagnitudes(magnitudesFromDecibels(analyser.decibels), analyser.node.context.sampleRate);
  };

  const updateTimer = () => {
    const wanted = analyser !== null && spectrumListeners.size > 0;
    if (wanted && timer === undefined) {
      timer = setInterval(() => {
        const scope = readSpectrum();
        if (scope) spectrumListeners.forEach((listener) => listener(scope));
      }, 1000 / UPDATE_HZ);
    } else if (!wanted && timer !== undefined) {
      clearInterval(timer);
      timer = undefined;
    }
  };

  return {
    ...backend,
    onSpectrum: (listener) => {
      spectrumListeners.add(listener);
      updateTimer();
      return () => {
        spectrumListeners.delete(listener);
        updateTimer();
      };
    },
    attachAnalyser: (node) => {
      analyser = { node, decibels: new Float32Array(node.frequencyBinCount) };
      updateTimer();
    },
  };
}
