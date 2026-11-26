import type { PluginBackend } from '@samplerealm/modules/backend.ts';
import type { SpectrumFrame } from './spectrumFrame.ts';

export * from '@samplerealm/modules/backend.ts';

export interface FarsideBackend extends PluginBackend {
  onSpectrum(listener: (frame: SpectrumFrame) => void): () => void;
}
