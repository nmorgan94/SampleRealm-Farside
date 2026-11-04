import { FFT_SIZE } from '../spectrum.ts';
import { makeLoop } from './loop.ts';
import type { WorkletBackend } from './workletBackend.ts';

const PROCESSOR_URL = 'processor.js';

// As the plugin's default stereo layout
const CHANNELS = 2;

export interface Audio {
  context: AudioContext;
  node: AudioWorkletNode;
  loop: AudioBuffer;
  analyser: AnalyserNode;
}

export async function startAudio(backend: WorkletBackend, wasm: ArrayBuffer, context: AudioContext): Promise<Audio> {
  await context.audioWorklet.addModule(PROCESSOR_URL);
  const loop = makeLoop(context);

  const node = new AudioWorkletNode(context, 'engine-processor', {
    numberOfInputs: 1,
    outputChannelCount: [CHANNELS],
    channelCount: CHANNELS,
    channelCountMode: 'explicit',
    processorOptions: { wasm, channels: CHANNELS },
  });

  await new Promise<void>((resolve, reject) => {
    node.port.onmessage = ({ data }) => {
      if (data.type === 'ready') resolve();
      else if (data.type === 'error') reject(new Error(data.message));
    };
  });

  node.connect(context.destination);
  backend.attach(node.port);

  // The display's input, as the plugin's SpectrumTap
  const analyser = new AnalyserNode(context, { fftSize: FFT_SIZE, smoothingTimeConstant: 0 });
  node.connect(analyser);
  backend.attachAnalyser(analyser);
  return { context, node, loop, analyser };
}
