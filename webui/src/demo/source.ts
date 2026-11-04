import { useCallback, useRef, useState } from 'react';
import type { Audio } from './audio.ts';

export type Source =
  | { kind: 'loop'; name: string }
  | { kind: 'file'; name: string; element: HTMLAudioElement; node: MediaElementAudioSourceNode; url: string };

const LOOP: Source = { kind: 'loop', name: 'Demo loop' };

// Streamed rather than decoded whole, so a file of any length fits in memory
function openFile({ context, node }: Audio, file: File): Promise<Source> {
  const url = URL.createObjectURL(file);
  const element = document.createElement('audio');
  element.loop = true;

  return new Promise((resolve, reject) => {
    element.addEventListener('loadedmetadata', () => {
      const source = context.createMediaElementSource(element);
      source.connect(node);
      resolve({ kind: 'file', name: file.name, element, node: source, url });
    }, { once: true });
    element.addEventListener('error', () => {
      URL.revokeObjectURL(url);
      reject(element.error);
    }, { once: true });
    element.src = url;
  });
}

export function release(source: Source) {
  if (source.kind !== 'file') return;
  source.element.pause();
  source.node.disconnect();
  URL.revokeObjectURL(source.url);
}

export function useSource(audio: Audio | null) {
  const [source, setSource] = useState<Source>(LOOP);
  const [loadError, setLoadError] = useState<string | null>(null);
  // Only the latest load counts
  const loads = useRef(0);

  const load = useCallback((file: File) => {
    if (!audio) return;
    const id = ++loads.current;
    openFile(audio, file).then(
      (opened) => {
        if (id !== loads.current) return release(opened);
        setLoadError(null);
        setSource(opened);
      },
      () => id === loads.current && setLoadError(`Couldn't read ${file.name}`),
    );
  }, [audio]);

  const backToLoop = () => {
    loads.current++;
    setLoadError(null);
    setSource(LOOP);
  };

  return { source, loadError, load, backToLoop };
}
