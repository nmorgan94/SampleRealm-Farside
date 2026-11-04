type ParamKind = 'real' | 'integer' | 'choice' | 'toggle';

export interface ParamInfo {
  id: string;
  name: string;
  kind: ParamKind;
  min: number;
  max: number;
  defaultValue: number;
  step: number;
  skew: number;
  unit: string;
  choices: string[];
}

export interface UiInfo {
  version: string;
  commit: string;
  build: 'Debug' | 'Release';
  params: ParamInfo[];
}

export interface About extends Pick<UiInfo, 'version' | 'commit' | 'build'> {
  // "VST3 in Ableton Live", or where else the editor is running
  running: string;
}

// Source/ui/SpectrumScope.h: 0..1 per point, log frequency from 20 Hz to 20 kHz
export type Scope = ArrayLike<number>;

export interface PluginBackend {
  about: About;
  param(id: string): ParamInfo;
  // Real values, not 0..1
  getValue(id: string): number;
  setValue(id: string, value: number): void;
  beginGesture(id: string): void;
  endGesture(id: string): void;
  subscribe(id: string, listener: (value: number) => void): () => void;
  onSpectrum(listener: (scope: Scope) => void): () => void;
}

export const paramLookup = <T>(entries: Map<string, T>) => (id: string) => {
  const found = entries.get(id);
  if (!found) throw new Error(`No parameter ${id}`);
  return found;
};
