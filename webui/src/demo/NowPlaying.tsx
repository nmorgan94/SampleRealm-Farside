import { useEffect, useState, type ReactNode } from 'react';
import { juceHeight } from '@samplerealm/modules/editor/fonts.ts';
import type { Player } from './player.ts';
import type { Source } from './source.ts';

const formatTime = (seconds: number) =>
  Number.isFinite(seconds) ? `${Math.floor(seconds / 60)}:${String(Math.floor(seconds % 60)).padStart(2, '0')}` : '–:––';

function IconButton({ label, onClick, children }: { label: string; onClick: () => void; children: ReactNode }) {
  return (
    <button type="button" className="shrink-0 cursor-pointer text-fg/60 hover:text-fg" aria-label={label} title={label} onClick={onClick}>
      <svg viewBox="0 0 24 24" width="14" height="14" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" aria-hidden>
        {children}
      </svg>
    </button>
  );
}

// Its own component, so polling the position doesn't re-render the editor
export function NowPlaying({ player, source, onRemove }: { player: Player; source: Source; onRemove: () => void }) {
  const [position, setPosition] = useState(0);
  const [duration, setDuration] = useState(0);

  useEffect(() => {
    const update = () => {
      setPosition(player.position());
      setDuration(player.duration());
    };
    update();
    const timer = setInterval(update, 100);
    return () => clearInterval(timer);
  }, [player]);

  const seek = (seconds: number) => {
    player.seek(seconds);
    setPosition(seconds);
  };

  // Some streams don't know their length
  const seekable = Number.isFinite(duration) && duration > 0;

  return (
    <div className="mt-2.5 border border-red/60 bg-top px-2.5 py-2" style={{ fontSize: juceHeight(13) }}>
      <div className="flex items-center gap-2.5">
        <span className="min-w-0 flex-1 truncate">{source.name}</span>
        <span className="shrink-0 text-fg/60">
          {formatTime(position)} / {formatTime(duration)}
        </span>
        <IconButton label="Restart" onClick={() => seek(0)}>
          <polyline points="1 4 1 10 7 10" />
          <path d="M3.51 15a9 9 0 1 0 2.13-9.36L1 10" />
        </IconButton>
        {source.kind === 'file' && (
          <IconButton label="Remove, back to the demo loop" onClick={onRemove}>
            <polyline points="3 6 5 6 21 6" />
            <path d="M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2" />
          </IconButton>
        )}
      </div>
      <input
        type="range"
        aria-label="Position"
        className="mt-1.5 block w-full cursor-pointer accent-red disabled:cursor-default"
        min={0}
        max={seekable ? duration : 1}
        step="any"
        value={seekable ? Math.min(position, duration) : 0}
        disabled={!seekable}
        onChange={(event) => seek(event.target.valueAsNumber)}
      />
    </div>
  );
}
