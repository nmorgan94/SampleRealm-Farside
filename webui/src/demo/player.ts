import { useEffect, useRef, useState } from 'react';
import type { Audio } from './audio.ts';
import { loopTransport } from './loop.ts';
import { release, type Source } from './source.ts';
import type { WorkletBackend } from './workletBackend.ts';

// -100 dB for half a second
const SILENCE = 1e-5;
const CHECK_MS = 100;
const SILENT_CHECKS = 5;

// Pause stops the source, not the engine, so the reverb rings out as it does in a host
export function usePlayer(audio: Audio | null, backend: WorkletBackend | undefined, source: Source) {
  const [playing, setPlaying] = useState(false);
  const running = useRef(false);
  const current = useRef(source);
  const loopNode = useRef<AudioBufferSourceNode | null>(null);
  // Seconds played, counting every time round the loop, as a host's position does
  const offset = useRef(0);
  const startedAt = useRef(0);
  const tailWatch = useRef<ReturnType<typeof setInterval> | undefined>(undefined);

  const stopWatchingTail = () => {
    clearInterval(tailWatch.current);
    tailWatch.current = undefined;
  };

  useEffect(() => stopWatchingTail, []);

  const suspendWhenSilent = () => {
    if (!audio) return;
    const samples = new Float32Array(audio.analyser.fftSize);
    let silentChecks = 0;
    stopWatchingTail();

    tailWatch.current = setInterval(() => {
      audio.analyser.getFloatTimeDomainData(samples);
      const peak = samples.reduce((max, sample) => Math.max(max, Math.abs(sample)), 0);
      silentChecks = peak < SILENCE ? silentChecks + 1 : 0;
      if (silentChecks < SILENT_CHECKS) return;
      stopWatchingTail();
      audio.context.suspend();
    }, CHECK_MS);
  };

  const stopCurrent = () => {
    const active = current.current;
    if (active.kind === 'file') active.element.pause();
    loopNode.current?.stop();
    loopNode.current?.disconnect();
    loopNode.current = null;
  };

  const pause = () => {
    if (!audio || !backend || !running.current) return;
    offset.current = audio.context.currentTime - startedAt.current;
    stopCurrent();
    suspendWhenSilent();
    backend.setTransport(loopTransport(false, offset.current));
    running.current = false;
    setPlaying(false);
  };

  const startCurrent = () => {
    if (!audio) return;
    const active = current.current;

    if (active.kind === 'file') {
      // A play() interrupted by switching source rejects too
      active.element.play().catch(() => current.current === active && pause());
      return;
    }

    const next = new AudioBufferSourceNode(audio.context, { buffer: audio.loop, loop: true });
    next.connect(audio.node);
    next.start(0, offset.current % audio.loop.duration);
    loopNode.current = next;
  };

  const play = () => {
    if (!audio || !backend || running.current) return;
    stopWatchingTail();
    // Before resuming, so the engine has it for the first block
    backend.setTransport(loopTransport(true, offset.current));
    audio.context.resume();
    startedAt.current = audio.context.currentTime - offset.current;
    running.current = true;
    setPlaying(true);
    startCurrent();
  };

  // A new source starts from its beginning, and keeps playing if the old one was
  useEffect(() => {
    const previous = current.current;
    if (previous === source || !audio || !backend) return;
    stopCurrent();
    release(previous);
    current.current = source;
    offset.current = 0;
    backend.setTransport(loopTransport(running.current, 0));

    if (running.current) {
      startedAt.current = audio.context.currentTime;
      startCurrent();
    }
  }, [audio, backend, source]);

  const position = () => {
    if (!audio) return 0;
    const active = current.current;
    if (active.kind === 'file') return active.element.currentTime;
    const played = running.current ? audio.context.currentTime - startedAt.current : offset.current;
    return played % audio.loop.duration;
  };

  const duration = () => {
    const active = current.current;
    return active.kind === 'file' ? active.element.duration : (audio?.loop.duration ?? 0);
  };

  const seek = (seconds: number) => {
    if (!audio || !backend) return;
    offset.current = seconds;
    startedAt.current = audio.context.currentTime - seconds;
    backend.setTransport(loopTransport(running.current, seconds));

    const active = current.current;
    if (active.kind === 'file') active.element.currentTime = seconds;
    else if (running.current) {
      stopCurrent();
      startCurrent();
    }
  };

  return { playing, toggle: playing ? pause : play, position, duration, seek };
}

export type Player = ReturnType<typeof usePlayer>;
