class EngineProcessor extends AudioWorkletProcessor {
  constructor({ processorOptions: { wasm, channels } }) {
    super();

    try {
      // WASI imports are never called
      const stub = new Proxy({}, { get: () => () => 0 });
      const instance = new WebAssembly.Instance(new WebAssembly.Module(wasm), new Proxy({}, { get: () => stub }));
      this.engine = instance.exports;
      this.engine._initialize?.();
      this.engine.prepare(sampleRate, channels);
    } catch (error) {
      this.engine = null;
      this.port.postMessage({ type: 'error', message: String(error) });
      return;
    }

    this.channels = channels;
    this.views = [];

    this.port.onmessage = ({ data }) => {
      if (data.type === 'param') this.engine.setParam(data.index, data.value);
      else if (data.type === 'transport') this.engine.setTransport(data.bpm, data.playing, data.ppq);
    };

    this.port.postMessage({ type: 'ready' });
  }

  view(channel, frames) {
    const pointer = this.engine.channel(channel);
    const current = this.views[channel];
    // Only changes if memory grows
    if (current?.buffer !== this.engine.memory.buffer || current.byteOffset !== pointer || current.length !== frames) {
      this.views[channel] = new Float32Array(this.engine.memory.buffer, pointer, frames);
    }
    return this.views[channel];
  }

  process([input], [output]) {
    if (!this.engine) return false;

    const frames = output[0].length;

    for (let channel = 0; channel < this.channels; channel++) {
      // No input while the loop is paused
      if (input[channel]) this.view(channel, frames).set(input[channel]);
      else this.view(channel, frames).fill(0);
    }

    this.engine.render(frames);

    for (let index = 0; index < output.length; index++) output[index].set(this.view(Math.min(index, this.channels - 1), frames));

    return true;
  }
}

registerProcessor('engine-processor', EngineProcessor);
