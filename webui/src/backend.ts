import type { EditorBackend } from '@samplerealm/modules/backend.ts';

export * from '@samplerealm/modules/backend.ts';

// Source/ui/SpectrumScope.h: 0..1 per point, log frequency from 20 Hz to 20 kHz
export type Scope = ArrayLike<number>;

export interface ReverbBackend extends EditorBackend {
  onSpectrum(listener: (scope: Scope) => void): () => void;
}
