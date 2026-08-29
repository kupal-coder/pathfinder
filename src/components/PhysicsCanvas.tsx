import React, { useRef, useEffect, useState, useCallback } from 'react';
import { PlayerState, LevelObject, OrbType, PadType, SpecialBlockType, VehicleType } from '../types/gd';
import { GDPhysicsEngine, PLAYER_SIZE_NORMAL, PLAYER_SIZE_MINI } from '../engine/physics';
import { Play, Pause, RotateCcw, ZoomIn, ZoomOut, Eye, Layers } from 'lucide-react';

interface PhysicsCanvasProps {
  objects: LevelObject[];
  trajectory: PlayerState[];
  currentFrame: number;
  onFrameChange: (frame: number) => void;
  isPlaying: boolean;
  onTogglePlay: () => void;
  onManualInput: (pressed: boolean) => void;
}

export const PhysicsCanvas: React.FC<PhysicsCanvasProps> = ({
  objects,
  trajectory,
  currentFrame,
  onFrameChange,
  isPlaying,
  onTogglePlay,
  onManualInput,
}) => {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const [zoom, setZoom] = useState<number>(1.4);
  const [showHitboxes, setShowHitboxes] = useState<boolean>(true);
  const [showSpiderRays, setShowSpiderRays] = useState<boolean>(true);
  const [showTrajectoryTrail, setShowTrajectoryTrail] = useState<boolean>(true);
  const [isMouseDown, setIsMouseDown] = useState<boolean>(false);

  const activeState = trajectory[currentFrame] || GDPhysicsEngine.createInitialState();

  // Keyboard controls (Space / Up to jump / click)
  useEffect(() => {
    const handleKeyDown = (e: KeyboardEvent) => {
      if (e.code === 'Space' || e.code === 'ArrowUp') {
        e.preventDefault();
        onManualInput(true);
      }
    };
    const handleKeyUp = (e: KeyboardEvent) => {
      if (e.code === 'Space' || e.code === 'ArrowUp') {
        e.preventDefault();
        onManualInput(false);
      }
    };
    window.addEventListener('keydown', handleKeyDown);
    window.addEventListener('keyup', handleKeyUp);
    return () => {
      window.removeEventListener('keydown', handleKeyDown);
      window.removeEventListener('keyup', handleKeyUp);
    };
  }, [onManualInput]);

  // Main Canvas Rendering Loop
  const render = useCallback(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    const width = canvas.width;
    const height = canvas.height;

    // Clear background
    ctx.fillStyle = '#0a0a0f';
    ctx.fillRect(0, 0, width, height);

    ctx.save();

    // Camera follow player with center offset
    const cameraX = activeState.x;
    const cameraY = Math.max(80, Math.min(180, activeState.y));

    ctx.translate(width / 3, height / 2 + 50);
    ctx.scale(zoom, -zoom); // Invert Y so up is positive
    ctx.translate(-cameraX, -cameraY);

    // 1. Draw Ground and Grid Lines
    ctx.strokeStyle = '#181825';
    ctx.lineWidth = 1;
    const startGridX = Math.floor((cameraX - 400) / 30) * 30;
    const endGridX = cameraX + 800;

    for (let x = startGridX; x <= endGridX; x += 30) {
      ctx.beginPath();
      ctx.moveTo(x, 0);
      ctx.lineTo(x, 320);
      ctx.stroke();
    }
    for (let y = 0; y <= 320; y += 30) {
      ctx.beginPath();
      ctx.moveTo(startGridX, y);
      ctx.lineTo(endGridX, y);
      ctx.stroke();
    }

    // Floor and Ceiling Line
    ctx.strokeStyle = '#00f0ff';
    ctx.lineWidth = 2;
    ctx.beginPath();
    ctx.moveTo(startGridX, 0);
    ctx.lineTo(endGridX, 0);
    ctx.stroke();

    ctx.strokeStyle = '#3b82f6';
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.moveTo(startGridX, 320);
    ctx.lineTo(endGridX, 320);
    ctx.stroke();

    // 2. Draw Trajectory Trail
    if (showTrajectoryTrail && trajectory.length > 1) {
      ctx.beginPath();
      ctx.strokeStyle = 'rgba(6, 182, 212, 0.4)';
      ctx.lineWidth = 2;
      for (let i = 0; i < trajectory.length; i++) {
        const p = trajectory[i];
        if (i === 0) ctx.moveTo(p.x, p.y);
        else ctx.lineTo(p.x, p.y);
      }
      ctx.stroke();

      // Draw jump/click marks along trajectory
      for (let i = 0; i < trajectory.length; i++) {
        const p = trajectory[i];
        if (p.input) {
          ctx.fillStyle = '#22c55e';
          ctx.beginPath();
          ctx.arc(p.x, p.y, 2.5, 0, Math.PI * 2);
          ctx.fill();
        }
      }
    }

    // 3. Draw Level Objects
    for (const obj of objects) {
      ctx.save();
      ctx.translate(obj.x, obj.y);
      if (obj.rotation) {
        ctx.rotate((-obj.rotation * Math.PI) / 180);
      }

      const halfW = obj.width / 2;
      const halfH = obj.height / 2;

      switch (obj.category) {
        case 'block':
          ctx.fillStyle = '#1e293b';
          ctx.strokeStyle = '#38bdf8';
          ctx.lineWidth = 2;
          ctx.fillRect(-halfW, -halfH, obj.width, obj.height);
          ctx.strokeRect(-halfW, -halfH, obj.width, obj.height);
          // Inner grid cross
          ctx.strokeStyle = 'rgba(56, 189, 248, 0.2)';
          ctx.beginPath();
          ctx.moveTo(-halfW, -halfH); ctx.lineTo(halfW, halfH);
          ctx.moveTo(-halfW, halfH); ctx.lineTo(halfW, -halfH);
          ctx.stroke();
          break;

        case 'spike':
          ctx.fillStyle = '#ef4444';
          ctx.strokeStyle = '#f87171';
          ctx.lineWidth = 1.5;
          ctx.beginPath();
          ctx.moveTo(0, halfH);
          ctx.lineTo(-halfW, -halfH);
          ctx.lineTo(halfW, -halfH);
          ctx.closePath();
          ctx.fill();
          ctx.stroke();
          break;

        case 'saw':
          ctx.fillStyle = '#7f1d1d';
          ctx.strokeStyle = '#ef4444';
          ctx.lineWidth = 2;
          ctx.beginPath();
          ctx.arc(0, 0, halfW, 0, Math.PI * 2);
          ctx.fill();
          ctx.stroke();
          // Blade spokes
          ctx.beginPath();
          ctx.moveTo(-halfW, 0); ctx.lineTo(halfW, 0);
          ctx.moveTo(0, -halfH); ctx.lineTo(0, halfH);
          ctx.stroke();
          break;

        case 'orb': {
          let orbColor = '#eab308';
          if (obj.orbType === OrbType.Pink) orbColor = '#ec4899';
          else if (obj.orbType === OrbType.Red) orbColor = '#ef4444';
          else if (obj.orbType === OrbType.Blue) orbColor = '#3b82f6';
          else if (obj.orbType === OrbType.Green) orbColor = '#22c55e';
          else if (obj.orbType === OrbType.Black) orbColor = '#64748b';
          else if (obj.orbType === OrbType.Spider) orbColor = '#a855f7';
          else if (obj.orbType === OrbType.Dash) orbColor = '#10b981';
          else if (obj.orbType === OrbType.GravityDash) orbColor = '#f43f5e';

          ctx.fillStyle = orbColor;
          ctx.beginPath();
          ctx.arc(0, 0, halfW * 0.7, 0, Math.PI * 2);
          ctx.fill();

          ctx.strokeStyle = orbColor;
          ctx.lineWidth = 2;
          ctx.beginPath();
          ctx.arc(0, 0, halfW, 0, Math.PI * 2);
          ctx.stroke();

          // Dash arrow or Spider mark
          if (obj.orbType === OrbType.Dash || obj.orbType === OrbType.GravityDash) {
            ctx.fillStyle = '#ffffff';
            ctx.beginPath();
            ctx.moveTo(halfW * 0.4, 0);
            ctx.lineTo(-halfW * 0.3, halfH * 0.4);
            ctx.lineTo(-halfW * 0.3, -halfH * 0.4);
            ctx.closePath();
            ctx.fill();
          }
          break;
        }

        case 'pad': {
          let padColor = '#eab308';
          if (obj.padType === PadType.Pink) padColor = '#ec4899';
          else if (obj.padType === PadType.Red) padColor = '#ef4444';
          else if (obj.padType === PadType.Blue) padColor = '#3b82f6';
          else if (obj.padType === PadType.Spider) padColor = '#a855f7';

          ctx.fillStyle = padColor;
          ctx.fillRect(-halfW, -halfH, obj.width, obj.height);
          break;
        }

        case 'portal': {
          let portalColor = '#38bdf8';
          if (obj.portalType?.includes('spider')) portalColor = '#c084fc';
          else if (obj.portalType?.includes('wave')) portalColor = '#3b82f6';
          else if (obj.portalType?.includes('robot')) portalColor = '#f97316';
          else if (obj.portalType?.includes('gravity')) portalColor = '#a855f7';
          else if (obj.portalType?.includes('speed')) portalColor = '#eab308';
          else if (obj.portalType?.includes('dual')) portalColor = '#ec4899';

          ctx.strokeStyle = portalColor;
          ctx.lineWidth = 3;
          ctx.strokeRect(-halfW, -halfH, obj.width, obj.height);
          ctx.fillStyle = `${portalColor}33`;
          ctx.fillRect(-halfW, -halfH, obj.width, obj.height);
          break;
        }

        case 'special': {
          let specialColor = '#f97316';
          let label = 'S';
          if (obj.specialType === SpecialBlockType.J) { specialColor = '#06b6d4'; label = 'J'; }
          else if (obj.specialType === SpecialBlockType.H) { specialColor = '#eab308'; label = 'H'; }
          else if (obj.specialType === SpecialBlockType.D) { specialColor = '#a855f7'; label = 'D'; }

          ctx.fillStyle = `${specialColor}22`;
          ctx.strokeStyle = specialColor;
          ctx.lineWidth = 2;
          ctx.fillRect(-halfW, -halfH, obj.width, obj.height);
          ctx.strokeRect(-halfW, -halfH, obj.width, obj.height);

          ctx.save();
          ctx.scale(1, -1);
          ctx.fillStyle = specialColor;
          ctx.font = 'bold 14px "JetBrains Mono", monospace';
          ctx.textAlign = 'center';
          ctx.textBaseline = 'middle';
          ctx.fillText(label, 0, 0);
          ctx.restore();
          break;
        }
      }

      ctx.restore();
    }

    // 4. Draw Spider Raycast (Violet Beam)
    if (showSpiderRays && activeState.lastSpiderRay) {
      ctx.strokeStyle = '#c084fc';
      ctx.lineWidth = 3;
      ctx.setLineDash([4, 4]);
      ctx.beginPath();
      ctx.moveTo(activeState.x, activeState.lastSpiderRay.fromY);
      ctx.lineTo(activeState.x, activeState.lastSpiderRay.toY);
      ctx.stroke();
      ctx.setLineDash([]);

      ctx.fillStyle = activeState.lastSpiderRay.hitHazard ? '#ef4444' : '#a855f7';
      ctx.beginPath();
      ctx.arc(activeState.x, activeState.lastSpiderRay.toY, 5, 0, Math.PI * 2);
      ctx.fill();
    }

    // 5. Draw Player Character
    ctx.save();
    ctx.translate(activeState.x, activeState.y);
    const pSize = activeState.small ? PLAYER_SIZE_MINI : PLAYER_SIZE_NORMAL;
    const pHalfW = pSize.width / 2;
    const pHalfH = pSize.height / 2;

    ctx.rotate((-activeState.rotation * Math.PI) / 180);

    // Player body based on Vehicle
    let pFill = '#00f0ff';
    let pStroke = '#38bdf8';
    if (activeState.dead) {
      pFill = '#ef4444';
      pStroke = '#b91c1c';
    } else if (activeState.vehicle === VehicleType.Wave) {
      pFill = '#3b82f6';
      pStroke = '#60a5fa';
    } else if (activeState.vehicle === VehicleType.Spider) {
      pFill = '#a855f7';
      pStroke = '#c084fc';
    } else if (activeState.vehicle === VehicleType.Robot) {
      pFill = '#f97316';
      pStroke = '#fb923c';
    } else if (activeState.vehicle === VehicleType.Ship) {
      pFill = '#ec4899';
      pStroke = '#f472b6';
    }

    if (activeState.vehicle === VehicleType.Wave) {
      // Draw Wave dart triangle
      ctx.fillStyle = pFill;
      ctx.strokeStyle = pStroke;
      ctx.lineWidth = 2;
      ctx.beginPath();
      ctx.moveTo(pHalfW, 0);
      ctx.lineTo(-pHalfW, pHalfH);
      ctx.lineTo(-pHalfW * 0.5, 0);
      ctx.lineTo(-pHalfW, -pHalfH);
      ctx.closePath();
      ctx.fill();
      ctx.stroke();
    } else if (activeState.vehicle === VehicleType.Ball) {
      // Draw Ball circle
      ctx.fillStyle = pFill;
      ctx.strokeStyle = pStroke;
      ctx.lineWidth = 2;
      ctx.beginPath();
      ctx.arc(0, 0, pHalfW, 0, Math.PI * 2);
      ctx.fill();
      ctx.stroke();
    } else {
      // Standard Cube / Robot / Spider box with inner face
      ctx.fillStyle = pFill;
      ctx.strokeStyle = pStroke;
      ctx.lineWidth = 2;
      ctx.fillRect(-pHalfW, -pHalfH, pSize.width, pSize.height);
      ctx.strokeRect(-pHalfW, -pHalfH, pSize.width, pSize.height);

      // Eye
      ctx.fillStyle = '#ffffff';
      ctx.fillRect(pHalfW * 0.1, pHalfH * 0.1, pHalfW * 0.6, pHalfH * 0.6);
      ctx.fillStyle = '#000000';
      ctx.fillRect(pHalfW * 0.3, pHalfH * 0.2, pHalfW * 0.3, pHalfH * 0.3);
    }

    // Inner Hitbox Debug Overlay
    if (showHitboxes) {
      ctx.strokeStyle = 'rgba(239, 68, 68, 0.8)';
      ctx.lineWidth = 1;
      const innerW = pSize.width * 0.7;
      const innerH = pSize.height * 0.7;
      ctx.strokeRect(-innerW / 2, -innerH / 2, innerW, innerH);
    }

    ctx.restore();
    ctx.restore();
  }, [activeState, objects, trajectory, zoom, showHitboxes, showSpiderRays, showTrajectoryTrail]);

  useEffect(() => {
    render();
  }, [render]);

  return (
    <div id="physics-canvas-container" className="flex flex-col bg-neutral-900 border border-neutral-800 rounded-xl overflow-hidden shadow-2xl">
      {/* Canvas Header Controls */}
      <div id="canvas-header" className="flex flex-wrap items-center justify-between px-4 py-3 bg-neutral-950/80 border-b border-neutral-800 gap-2">
        <div className="flex items-center gap-3">
          <button
            id="play-pause-btn"
            onClick={onTogglePlay}
            className={`flex items-center gap-2 px-3 py-1.5 rounded-lg font-semibold text-xs transition-all ${
              isPlaying
                ? 'bg-amber-500/20 text-amber-300 border border-amber-500/30 hover:bg-amber-500/30'
                : 'bg-cyan-500/20 text-cyan-300 border border-cyan-500/30 hover:bg-cyan-500/30'
            }`}
          >
            {isPlaying ? <Pause className="w-3.5 h-3.5" /> : <Play className="w-3.5 h-3.5" />}
            {isPlaying ? 'Pause Sim' : 'Play 240Hz'}
          </button>

          <button
            id="reset-frame-btn"
            onClick={() => onFrameChange(0)}
            className="p-1.5 text-neutral-400 hover:text-neutral-200 bg-neutral-900 hover:bg-neutral-800 border border-neutral-700/50 rounded-lg transition-colors"
            title="Reset to Frame 0"
          >
            <RotateCcw className="w-3.5 h-3.5" />
          </button>

          <div className="flex items-center gap-2 text-xs font-mono text-neutral-400 border-l border-neutral-800 pl-3">
            <span>Frame: <strong className="text-cyan-400">{currentFrame}</strong> / {trajectory.length - 1}</span>
            <span>Time: <strong className="text-neutral-200">{(currentFrame / 240).toFixed(2)}s</strong></span>
          </div>
        </div>

        {/* View toggles & Zoom */}
        <div className="flex items-center gap-2">
          <button
            id="toggle-hitbox-btn"
            onClick={() => setShowHitboxes(!showHitboxes)}
            className={`px-2.5 py-1 text-xs rounded-md border flex items-center gap-1 transition-all ${
              showHitboxes ? 'bg-red-500/20 text-red-300 border-red-500/40' : 'bg-neutral-900 text-neutral-400 border-neutral-800'
            }`}
          >
            <Eye className="w-3 h-3" />
            Hitboxes
          </button>

          <button
            id="toggle-spider-btn"
            onClick={() => setShowSpiderRays(!showSpiderRays)}
            className={`px-2.5 py-1 text-xs rounded-md border flex items-center gap-1 transition-all ${
              showSpiderRays ? 'bg-purple-500/20 text-purple-300 border-purple-500/40' : 'bg-neutral-900 text-neutral-400 border-neutral-800'
            }`}
          >
            <Layers className="w-3 h-3" />
            Spider Rays
          </button>

          <div className="flex items-center bg-neutral-900 border border-neutral-800 rounded-lg p-0.5">
            <button
              id="zoom-out-btn"
              onClick={() => setZoom(z => Math.max(0.6, z - 0.2))}
              className="p-1 text-neutral-400 hover:text-white"
            >
              <ZoomOut className="w-3.5 h-3.5" />
            </button>
            <span className="text-[10px] font-mono px-1.5 text-neutral-400">{Math.round(zoom * 100)}%</span>
            <button
              id="zoom-in-btn"
              onClick={() => setZoom(z => Math.min(3.0, z + 0.2))}
              className="p-1 text-neutral-400 hover:text-white"
            >
              <ZoomIn className="w-3.5 h-3.5" />
            </button>
          </div>
        </div>
      </div>

      {/* Main HTML5 Canvas */}
      <div className="relative w-full h-[360px] bg-neutral-950 flex items-center justify-center">
        <canvas
          id="physics-main-canvas"
          ref={canvasRef}
          width={900}
          height={360}
          className="w-full h-full cursor-crosshair select-none"
          onMouseDown={() => {
            setIsMouseDown(true);
            onManualInput(true);
          }}
          onMouseUp={() => {
            setIsMouseDown(false);
            onManualInput(false);
          }}
          onMouseLeave={() => {
            if (isMouseDown) {
              setIsMouseDown(false);
              onManualInput(false);
            }
          }}
        />

        {/* Live HUD Floating Indicators */}
        <div id="live-hud" className="absolute top-3 left-3 flex flex-wrap gap-1.5 pointer-events-none text-[11px] font-mono">
          <span className="px-2 py-0.5 rounded bg-cyan-950/80 text-cyan-300 border border-cyan-500/40 backdrop-blur-sm">
            {activeState.vehicle}
          </span>
          <span className="px-2 py-0.5 rounded bg-neutral-900/80 text-neutral-300 border border-neutral-700/50 backdrop-blur-sm">
            {activeState.speed === 0 ? '0.5x' : `${activeState.speed}x`} Speed
          </span>
          <span className={`px-2 py-0.5 rounded border backdrop-blur-sm ${
            activeState.upsideDown ? 'bg-amber-950/80 text-amber-300 border-amber-500/40' : 'bg-emerald-950/80 text-emerald-300 border-emerald-500/40'
          }`}>
            {activeState.upsideDown ? 'Gravity: Inverted' : 'Gravity: Normal'}
          </span>
          {activeState.isDashing && (
            <span className="px-2 py-0.5 rounded bg-emerald-900/90 text-emerald-200 border border-emerald-400 animate-pulse">
              DASHING ({activeState.dashAngle}°)
            </span>
          )}
          {activeState.hasHBlock && (
            <span className="px-2 py-0.5 rounded bg-yellow-900/90 text-yellow-200 border border-yellow-400">
              H-BLOCK SAFE
            </span>
          )}
          {activeState.hasDBlock && (
            <span className="px-2 py-0.5 rounded bg-purple-900/90 text-purple-200 border border-purple-400">
              D-BLOCK SLIDE
            </span>
          )}
          {activeState.dead && (
            <span className="px-2.5 py-0.5 rounded bg-red-900/90 text-red-200 font-bold border border-red-500 animate-bounce">
              CRASH / DEAD
            </span>
          )}
        </div>

        {/* Interactive Click Button for Touch or Mouse */}
        <div className="absolute bottom-3 right-3 flex items-center gap-2">
          <button
            id="tap-screen-btn"
            onMouseDown={() => onManualInput(true)}
            onMouseUp={() => onManualInput(false)}
            onTouchStart={() => onManualInput(true)}
            onTouchEnd={() => onManualInput(false)}
            className="px-4 py-2 bg-gradient-to-r from-cyan-600 to-blue-600 active:from-cyan-500 active:to-blue-500 text-white font-bold text-xs rounded-xl shadow-lg border border-cyan-400/30 uppercase tracking-wider select-none hover:scale-105 active:scale-95 transition-transform"
          >
            Hold to Click (Space)
          </button>
        </div>
      </div>

      {/* Frame Scrubber Bar */}
      <div id="frame-scrubber-bar" className="px-4 py-2 bg-neutral-950 border-t border-neutral-800 flex items-center gap-3">
        <span className="text-[11px] font-mono text-neutral-400 whitespace-nowrap">F: 0</span>
        <input
          id="frame-range-slider"
          type="range"
          min={0}
          max={Math.max(0, trajectory.length - 1)}
          value={currentFrame}
          onChange={(e) => onFrameChange(Number(e.target.value))}
          className="w-full h-1.5 bg-neutral-800 rounded-lg appearance-none cursor-pointer accent-cyan-400"
        />
        <span className="text-[11px] font-mono text-neutral-400 whitespace-nowrap">F: {Math.max(0, trajectory.length - 1)}</span>
      </div>
    </div>
  );
};
