import { startWorklet } from '@samplerealm/modules/demo/engine.ts';
import processorUrl from '@samplerealm/modules/demo/processor.js?url';
import { FFT_SIZE } from '../spectrum.ts';
import { makeLoop } from './loop.ts';
import type { WorkletBackend } from './workletBackend.ts';

// As the plugin's default stereo layout
const CHANNELS = 2;

export interface Audio {
  context: AudioContext;
  node: AudioWorkletNode;
  loop: AudioBuffer;
  analyser: AnalyserNode;
}

export async function startAudio(backend: WorkletBackend, wasm: ArrayBuffer, context: AudioContext): Promise<Audio> {
  const node = await startWorklet(context, processorUrl, {
    numberOfInputs: 1,
    outputChannelCount: [CHANNELS],
    channelCount: CHANNELS,
    channelCountMode: 'explicit',
    processorOptions: { wasm, channels: CHANNELS },
  });

  node.connect(context.destination);
  backend.attach(node.port);

  // The display's input, as the plugin's SpectrumTap
  const analyser = new AnalyserNode(context, { fftSize: FFT_SIZE, smoothingTimeConstant: 0 });
  node.connect(analyser);
  backend.attachAnalyser(analyser);
  return { context, node, loop: makeLoop(context), analyser };
}
