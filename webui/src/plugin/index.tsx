import { createRoot } from 'react-dom/client';
import { editorBackend, onEvent } from '@samplerealm/modules/plugin/pluginBackend.ts';
import type { ReverbBackend, Scope } from '../backend.ts';
import { Editor } from '../Editor.tsx';
import './styles.css';

// As createEditor sends it in Source/PluginProcessor.cpp: the scope in thousandths
const SPECTRUM_SCALE = 1000;

const onSpectrum = (listener: (scope: Scope) => void) =>
  onEvent<number[]>('spectrum', (points) => listener(Float32Array.from(points, (point) => point / SPECTRUM_SCALE)));

const reverbBackend = (): ReverbBackend => ({ ...editorBackend(), onSpectrum });

const root = createRoot(document.getElementById('root')!);

try {
  root.render(<Editor backend={reverbBackend()} />);
} catch (error) {
  console.error(error);
  root.render(<pre className="p-5 text-fg">{`The editor couldn't connect to the plugin.\n${error}`}</pre>);
}
