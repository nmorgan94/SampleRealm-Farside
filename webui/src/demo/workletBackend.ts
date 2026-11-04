import { paramLookup, type PluginBackend, type Scope, type UiInfo } from '../backend.ts';
import { magnitudesFromDecibels, SCOPE_SIZE, scopeFromMagnitudes, UPDATE_HZ } from '../spectrum.ts';

// WASI imports are never called
export const stubImports = () => {
  const stub = new Proxy({}, { get: () => () => 0 });
  return new Proxy({}, { get: () => stub }) as WebAssembly.Imports;
};

export type Engine = Record<string, CallableFunction> & { memory: WebAssembly.Memory };

export const readUiInfo = (engine: Engine) => {
  const bytes = new Uint8Array(engine.memory.buffer, engine.uiInfo());
  return JSON.parse(new TextDecoder().decode(bytes.subarray(0, bytes.indexOf(0)))) as UiInfo;
};

export async function loadEngine(url: string): Promise<{ wasm: ArrayBuffer; info: UiInfo }> {
  const response = await fetch(url);
  if (!response.ok) throw new Error(`${url}: ${response.status}`);

  const wasm = await response.arrayBuffer();
  const { instance } = await WebAssembly.instantiate(wasm, stubImports());
  const engine = instance.exports as Engine;
  engine._initialize?.();
  return { wasm, info: readUiInfo(engine) };
}

// The node's port, so tests can stand in for it
export type EnginePort = Pick<MessagePort, 'postMessage'> & { onmessage: ((e: MessageEvent) => void) | null };

// The AnalyserNode after the worklet, so tests can stand in for it
export type Analyser = Pick<AnalyserNode, 'frequencyBinCount' | 'getFloatFrequencyData'> & { context: Pick<BaseAudioContext, 'state' | 'sampleRate'> };

// Source/shared/Transport.h
export type Transport = { bpm: number; playing: boolean; ppq: number };

export type WorkletBackend = PluginBackend & {
  attach(port: EnginePort): void;
  attachAnalyser(analyser: Analyser): void;
  // The page plays the host
  setTransport(transport: Transport): void;
};

const SPECTRUM_BYPASS = 'spectrumBypass';

export function workletBackend(info: UiInfo): WorkletBackend {
  const entry = paramLookup(new Map(info.params.map((param, index) => [param.id, { param, index, value: param.defaultValue }])));
  const listeners = new Map<string, Set<(value: number) => void>>();
  const spectrumListeners = new Set<(scope: Scope) => void>();
  let port: EnginePort | null = null;
  // Sent once the worklet attaches
  let pending: unknown[] = [];
  let analyser: { node: Analyser; decibels: Float32Array<ArrayBuffer> } | null = null;
  let timer: ReturnType<typeof setInterval> | undefined;
  let scopeCleared = false;

  const post = (message: unknown) => {
    if (port) port.postMessage(message);
    else pending.push(message);
  };

  const apply = (id: string, value: number) => {
    const found = entry(id);
    post({ type: 'param', index: found.index, value });
    found.value = value;
    listeners.get(id)?.forEach((listener) => listener(value));
  };

  // As SpectrumScope::update: cleared once while bypassed, and nothing new while suspended
  const readSpectrum = (): Scope | null => {
    if (!analyser) return null;

    if (entry(SPECTRUM_BYPASS).value >= 0.5) {
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
    about: { version: info.version, commit: info.commit, build: info.build, running: 'Web demo' },
    param: (id) => entry(id).param,
    getValue: (id) => entry(id).value,
    setValue: apply,
    beginGesture: () => {},
    endGesture: () => {},
    subscribe: (id, listener) => {
      const set = listeners.get(id) ?? new Set();
      listeners.set(id, set.add(listener));
      return () => set.delete(listener);
    },
    onSpectrum: (listener) => {
      spectrumListeners.add(listener);
      updateTimer();
      return () => {
        spectrumListeners.delete(listener);
        updateTimer();
      };
    },
    setTransport: (transport) => post({ type: 'transport', ...transport }),
    attach: (enginePort) => {
      port = enginePort;
      pending.forEach((message) => enginePort.postMessage(message));
      pending = [];
    },
    attachAnalyser: (node) => {
      analyser = { node, decibels: new Float32Array(node.frequencyBinCount) };
      updateTimer();
    },
  };
}
