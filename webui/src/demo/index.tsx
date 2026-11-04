import { useEffect, useRef, useState } from 'react';
import { createRoot } from 'react-dom/client';
import { EDITOR_HEIGHT, EDITOR_WIDTH, Editor } from '../Editor.tsx';
import { juceHeight } from '../fonts.ts';
import { startAudio, type Audio } from './audio.ts';
import { useFileDrop } from './fileDrop.ts';
import { NowPlaying } from './NowPlaying.tsx';
import { usePlayer } from './player.ts';
import { useSource } from './source.ts';
import { loadEngine, workletBackend, type WorkletBackend } from './workletBackend.ts';
import './styles.css';

const ENGINE_URL = 'engine.wasm';

const SIZE_MESSAGE = 'samplerealm-demo-size';

function useScale() {
  const [scale, setScale] = useState(1);
  const ref = useRef<HTMLDivElement>(null);

  useEffect(() => {
    const element = ref.current;
    if (!element) return;
    const observer = new ResizeObserver(() => setScale(Math.min(1, element.clientWidth / EDITOR_WIDTH)));
    observer.observe(element);
    return () => observer.disconnect();
  }, []);

  return { ref, scale };
}

const BUTTON = 'h-7 w-20 shrink-0 cursor-pointer border border-red/60 bg-top text-fg disabled:cursor-default disabled:opacity-50';

function Demo() {
  const [engine, setEngine] = useState<{ backend: WorkletBackend; wasm: ArrayBuffer } | null>(null);
  const [audio, setAudio] = useState<Audio | null>(null);
  const [failed, setFailed] = useState(false);
  const context = useRef<AudioContext | null>(null);
  const { ref, scale } = useScale();
  const editor = useRef<HTMLDivElement>(null);

  // Not the document's height, which never shrinks below the frame. The editor's box is measured,
  // as its scale catches up with the frame's width a render late
  useEffect(() => {
    const content = ref.current;
    const box = editor.current;
    if (window.parent === window || !content || !box) return;

    const report = () => {
      const below = Math.round(content.getBoundingClientRect().height - box.getBoundingClientRect().height);
      window.parent.postMessage({ type: SIZE_MESSAGE, editorWidth: EDITOR_WIDTH, editorHeight: EDITOR_HEIGHT, below }, '*');
    };
    const observer = new ResizeObserver(report);
    observer.observe(content);
    return () => observer.disconnect();
  }, [ref]);

  const { source, loadError, load, backToLoop } = useSource(audio);
  const player = usePlayer(audio, engine?.backend, source);
  const dragging = useFileDrop(load);
  const fileInput = useRef<HTMLInputElement>(null);

  useEffect(() => {
    loadEngine(ENGINE_URL)
      .then(({ wasm, info }) => setEngine({ backend: workletBackend(info), wasm }))
      .catch((error) => {
        console.error('Demo failed to load:', error);
        setFailed(true);
      });
  }, []);

  // Ready to play at once; the loop waits for Play
  useEffect(() => {
    if (!engine || context.current) return;

    const audioContext = new AudioContext();
    // It starts running where autoplay is allowed
    audioContext.suspend();
    context.current = audioContext;

    startAudio(engine.backend, engine.wasm, audioContext)
      .then(setAudio)
      .catch((error) => {
        console.error('Demo audio failed to start:', error);
        audioContext.close();
        setFailed(true);
      });
  }, [engine]);

  useEffect(() => () => void context.current?.close(), []);

  return (
    <div className="mx-auto" style={{ maxWidth: EDITOR_WIDTH }} ref={ref}>
      <div className="relative overflow-hidden rounded-xl" ref={editor} style={{ width: EDITOR_WIDTH * scale, height: EDITOR_HEIGHT * scale }}>
        <div className="origin-top-left" style={{ width: EDITOR_WIDTH, height: EDITOR_HEIGHT, transform: `scale(${scale})` }}>
          {engine && <Editor backend={engine.backend} />}
        </div>
        {(failed || !audio) && (
          <div className="absolute inset-0 z-30 flex items-center justify-center bg-top/55">
            {failed ? (
              <p className="text-sm text-red">The demo couldn't start. Please try again later.</p>
            ) : (
              <p className="text-sm font-bold">Loading…</p>
            )}
          </div>
        )}
        {dragging && audio && (
          <div className="absolute inset-0 z-40 flex items-center justify-center border-2 border-dashed border-red bg-top/80">
            <p className="text-sm font-bold">Drop to play through the reverb</p>
          </div>
        )}
      </div>

      <div className="mt-2.5 flex items-center gap-2.5" style={{ fontSize: juceHeight(15) }}>
        <button type="button" className={BUTTON} disabled={!audio} aria-pressed={player.playing} onClick={player.toggle}>
          {player.playing ? 'PAUSE' : 'PLAY'}
        </button>
        <button type="button" className={BUTTON} disabled={!audio} onClick={() => fileInput.current?.click()}>
          LOAD
        </button>
        <input
          ref={fileInput}
          type="file"
          accept="audio/*"
          className="hidden"
          onChange={(event) => {
            const file = event.target.files?.[0];
            // So picking the same file again still fires
            event.target.value = '';
            if (file) load(file);
          }}
        />
        {loadError && <span className="min-w-0 truncate text-red">{loadError}</span>}
      </div>

      {audio && <NowPlaying player={player} source={source} onRemove={backToLoop} />}
    </div>
  );
}

const root = document.getElementById('root');
if (root) createRoot(root).render(<Demo />);
