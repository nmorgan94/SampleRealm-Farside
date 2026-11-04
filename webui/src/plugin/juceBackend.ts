// The C++ side of these names is Source/webview/WebViewEditor.cpp
import { getSliderState, getToggleState } from '@juce-framework/webview';
import { paramLookup, type ParamInfo, type PluginBackend, type UiInfo } from '../backend.ts';
import { toProportion } from '../params.ts';

// JUCE's type lists only its own initialisation data
type InitialisationData = { uiInfo: string[]; running: string[] };

type Control = {
  get(): number;
  set(value: number): void;
  begin(): void;
  end(): void;
  onChange(listener: () => void): () => void;
};

type ValueChangedEvent = ReturnType<typeof getSliderState>['valueChangedEvent'];

const listen = (event: ValueChangedEvent) => (listener: () => void) => {
  const handle = event.addListener(listener);
  return () => event.removeListener(handle);
};

// A WebToggleButtonRelay for toggles, a WebSliderRelay for the rest
function control(param: ParamInfo): Control {
  if (param.kind === 'toggle') {
    const state = getToggleState(param.id);
    return {
      get: () => (state.getValue() ? 1 : 0),
      set: (value) => state.setValue(value >= 0.5),
      begin: () => {},
      end: () => {},
      onChange: listen(state.valueChangedEvent),
    };
  }

  const state = getSliderState(param.id);
  return {
    get: () => state.getScaledValue(),
    set: (value) => state.setNormalisedValue(toProportion(param, value)),
    begin: () => state.sliderDragStarted(),
    end: () => state.sliderDragEnded(),
    onChange: listen(state.valueChangedEvent),
  };
}

// WebViewEditor.cpp's spectrumScale
const SPECTRUM_SCALE = 1000;

export function juceBackend(): PluginBackend {
  const { uiInfo, running } = window.__JUCE__.initialisationData as unknown as InitialisationData;
  const info = JSON.parse(uiInfo[0]) as UiInfo;
  const entry = paramLookup(new Map(info.params.map((param) => [param.id, { param, control: control(param) }])));

  const controlOf = (id: string) => entry(id).control;

  return {
    about: { version: info.version, commit: info.commit, build: info.build, running: running[0] },
    param: (id) => entry(id).param,
    getValue: (id) => controlOf(id).get(),
    setValue: (id, value) => controlOf(id).set(value),
    beginGesture: (id) => controlOf(id).begin(),
    endGesture: (id) => controlOf(id).end(),
    subscribe: (id, listener) => {
      const found = controlOf(id);
      return found.onChange(() => listener(found.get()));
    },
    onSpectrum: (listener) => {
      const handle = window.__JUCE__.backend.addEventListener('spectrum', (payload) =>
        listener(Float32Array.from(payload as number[], (point) => point / SPECTRUM_SCALE)),
      );
      return () => window.__JUCE__.backend.removeEventListener(handle);
    },
  };
}
