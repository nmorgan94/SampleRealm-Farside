import { useEffect, useRef, useState } from 'react';
import { createRoot } from 'react-dom/client';
import { DemoFrame } from '@samplerealm/modules/demo/DemoFrame.tsx';
import { loadEngine } from '@samplerealm/modules/demo/engine.ts';
import { juceHeight } from '@samplerealm/modules/editor/fonts.ts';
import { EDITOR_HEIGHT, EDITOR_WIDTH, Editor } from '../Editor.tsx';
import { startAudio, type Audio } from './audio.ts';
import { useFileDrop } from './fileDrop.ts';
import { NowPlaying } from './NowPlaying.tsx';
import { usePlayer } from './player.ts';
import { useSource } from './source.ts';
import { workletBackend, type WorkletBackend } from './workletBackend.ts';
import './styles.css';

const ENGINE_URL = 'engine.wasm';

const BUTTON = 'h-7 w-20 shrink-0 cursor-pointer border border-red/60 bg-top text-fg disabled:cursor-default disabled:opacity-50';

function Demo() {
  const [engine, setEngine] = useState<{ backend: WorkletBackend; wasm: ArrayBuffer } | null>(null);
  const [audio, setAudio] = useState<Audio | null>(null);
  const [failed, setFailed] = useState(false);
  const context = useRef<AudioContext | null>(null);

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
    <DemoFrame
      width={EDITOR_WIDTH}
      height={EDITOR_HEIGHT}
      status={failed ? 'failed' : audio ? 'ready' : 'loading'}
      editor={engine && <Editor backend={engine.backend} />}
      overlay={
        dragging && audio && (
          <div className="absolute inset-0 z-40 flex items-center justify-center border-2 border-dashed border-red bg-top/80">
            <p className="text-sm font-bold">Drop to play through the reverb</p>
          </div>
        )
      }
    >
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
    </DemoFrame>
  );
}

const root = document.getElementById('root');
if (root) createRoot(root).render(<Demo />);
