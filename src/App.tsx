import React, { useState, useEffect, useRef, useCallback } from 'react';
import { LevelObject, PlayerState, SolverResult } from './types/gd';
import { LEVEL_PRESETS, LevelPreset } from './engine/presets';
import { GDPhysicsEngine, PHYS_FPS } from './engine/physics';
import { LevelSolver } from './engine/solver';
import { PhysicsCanvas } from './components/PhysicsCanvas';
import { MacroTimeline } from './components/MacroTimeline';
import { LevelEditor } from './components/LevelEditor';
import { EngineInspector } from './components/EngineInspector';
import { 
  Play, 
  RotateCcw, 
  Cpu, 
  Sparkles, 
  Zap, 
  ShieldCheck, 
  CheckCircle2, 
  AlertCircle,
  Activity,
  Terminal
} from 'lucide-react';

export function App() {
  const [selectedPreset, setSelectedPreset] = useState<LevelPreset>(LEVEL_PRESETS[0]);
  const [objects, setObjects] = useState<LevelObject[]>(LEVEL_PRESETS[0].objects);
  const [solverResult, setSolverResult] = useState<SolverResult | null>(null);
  const [currentFrame, setCurrentFrame] = useState<number>(0);
  const [isPlaying, setIsPlaying] = useState<boolean>(false);
  const [isSolving, setIsSolving] = useState<boolean>(false);
  const [manualInput, setManualInput] = useState<boolean>(false);

  // Trajectory for manual play or solved run
  const [trajectory, setTrajectory] = useState<PlayerState[]>([]);

  // Calculate level distance for solver
  const maxObjectX = objects.reduce((max, obj) => Math.max(max, obj.x + 100), 1000);

  // Solve level automatically
  const runSolver = useCallback(() => {
    setIsSolving(true);
    setTimeout(() => {
      const res = LevelSolver.solve(objects, maxObjectX);
      setSolverResult(res);
      setTrajectory(res.trajectory);
      setCurrentFrame(0);
      setIsSolving(false);
      setIsPlaying(true);
    }, 50);
  }, [objects, maxObjectX]);

  // Initial solve on mount or preset switch
  useEffect(() => {
    runSolver();
  }, [runSolver]);

  const handleSelectPreset = (preset: LevelPreset) => {
    setSelectedPreset(preset);
    setObjects(preset.objects);
    setIsPlaying(false);
  };

  // 240Hz Animation playback loop
  useEffect(() => {
    if (!isPlaying) return;

    const interval = setInterval(() => {
      setCurrentFrame((prev) => {
        if (prev >= trajectory.length - 1) {
          setIsPlaying(false);
          return prev;
        }
        return prev + 1;
      });
    }, 1000 / 60); // 60fps display rate stepping through 240Hz frames

    return () => clearInterval(interval);
  }, [isPlaying, trajectory.length]);

  // Handle manual interaction when user clicks
  const handleManualInput = (pressed: boolean) => {
    setManualInput(pressed);
    if (!isPlaying && trajectory.length > 0) {
      // Step manual physics frame
      const current = trajectory[currentFrame] || GDPhysicsEngine.createInitialState();
      const next = GDPhysicsEngine.step(current, pressed, objects);
      const newTraj = [...trajectory.slice(0, currentFrame + 1), next];
      setTrajectory(newTraj);
      setCurrentFrame(currentFrame + 1);
    }
  };

  const activePlayer = trajectory[currentFrame] || GDPhysicsEngine.createInitialState();

  return (
    <div className="min-h-screen bg-neutral-950 text-neutral-100 flex flex-col font-sans selection:bg-cyan-500 selection:text-black">
      {/* Top Navigation Bar */}
      <header id="main-header" className="border-b border-neutral-800/80 bg-neutral-950/90 backdrop-blur-md sticky top-0 z-50 px-4 lg:px-8 py-3">
        <div className="max-w-7xl mx-auto flex flex-wrap items-center justify-between gap-4">
          <div className="flex items-center gap-3">
            <div className="w-9 h-9 rounded-xl bg-gradient-to-br from-cyan-500 to-blue-600 flex items-center justify-center shadow-lg shadow-cyan-500/20 border border-cyan-400/40">
              <Zap className="w-5 h-5 text-white" />
            </div>
            <div>
              <div className="flex items-center gap-2">
                <h1 className="font-extrabold text-base tracking-tight text-white">Pathfinding Pro</h1>
                <span className="text-[10px] font-mono font-bold px-1.5 py-0.5 rounded bg-cyan-950 text-cyan-400 border border-cyan-800/60">
                  by percjue
                </span>
                <span className="text-[10px] font-mono px-1.5 py-0.5 rounded bg-neutral-800 text-neutral-400">
                  240Hz Engine
                </span>
              </div>
              <p className="text-xs text-neutral-400">Geometry Dash Physics Engine & Frame-Exact Macro Solver</p>
            </div>
          </div>

          {/* Quick Solver Action Button */}
          <div className="flex items-center gap-3">
            <button
              id="run-solver-btn"
              disabled={isSolving}
              onClick={runSolver}
              className="flex items-center gap-2 px-4 py-2 bg-gradient-to-r from-cyan-500 to-blue-600 hover:from-cyan-400 hover:to-blue-500 text-white font-bold text-xs rounded-xl shadow-lg shadow-cyan-500/20 active:scale-95 transition-all disabled:opacity-50"
            >
              <Sparkles className="w-4 h-4" />
              {isSolving ? 'Solving Trajectory...' : 'Run Pathfinding Pro Solver'}
            </button>
          </div>
        </div>
      </header>

      {/* Main Content Area */}
      <main className="max-w-7xl mx-auto w-full p-4 lg:p-8 space-y-6 flex-1">
        {/* Physics Live Telemetry Strip */}
        <div id="telemetry-strip" className="grid grid-cols-2 sm:grid-cols-3 lg:grid-cols-6 gap-3">
          <div className="p-3 bg-neutral-900 border border-neutral-800 rounded-xl">
            <span className="text-[10px] font-mono uppercase text-neutral-500 block">Vehicle Mode</span>
            <span className="font-bold text-sm text-cyan-400">{activePlayer.vehicle}</span>
          </div>

          <div className="p-3 bg-neutral-900 border border-neutral-800 rounded-xl">
            <span className="text-[10px] font-mono uppercase text-neutral-500 block">Horizontal X</span>
            <span className="font-bold text-sm text-neutral-200 font-mono">{activePlayer.x.toFixed(1)} px</span>
          </div>

          <div className="p-3 bg-neutral-900 border border-neutral-800 rounded-xl">
            <span className="text-[10px] font-mono uppercase text-neutral-500 block">Vertical Y / Floor</span>
            <span className="font-bold text-sm text-neutral-200 font-mono">{activePlayer.y.toFixed(1)} px</span>
          </div>

          <div className="p-3 bg-neutral-900 border border-neutral-800 rounded-xl">
            <span className="text-[10px] font-mono uppercase text-neutral-500 block">Vertical Velocity</span>
            <span className="font-bold text-sm text-emerald-400 font-mono">{activePlayer.vy.toFixed(2)} px/f</span>
          </div>

          <div className="p-3 bg-neutral-900 border border-neutral-800 rounded-xl">
            <span className="text-[10px] font-mono uppercase text-neutral-500 block">Gravity Orientation</span>
            <span className={`font-bold text-sm ${activePlayer.upsideDown ? 'text-amber-400' : 'text-cyan-400'}`}>
              {activePlayer.upsideDown ? 'Inverted (Ceiling)' : 'Normal (Floor)'}
            </span>
          </div>

          <div className="p-3 bg-neutral-900 border border-neutral-800 rounded-xl">
            <span className="text-[10px] font-mono uppercase text-neutral-500 block">Solver Status</span>
            <span className="font-bold text-sm text-emerald-400 flex items-center gap-1">
              <CheckCircle2 className="w-3.5 h-3.5" />
              {solverResult?.solved ? '100% Solved' : 'Calculating'}
            </span>
          </div>
        </div>

        {/* Physics Canvas Viewport */}
        <PhysicsCanvas
          objects={objects}
          trajectory={trajectory}
          currentFrame={currentFrame}
          onFrameChange={setCurrentFrame}
          isPlaying={isPlaying}
          onTogglePlay={() => setIsPlaying(!isPlaying)}
          onManualInput={handleManualInput}
        />

        {/* Level Presets & Editor */}
        <LevelEditor
          objects={objects}
          onUpdateObjects={(newObjs) => {
            setObjects(newObjs);
            setIsPlaying(false);
          }}
          onSelectPreset={handleSelectPreset}
          selectedPresetId={selectedPreset.id}
        />

        {/* Macro Timeline & Exporter */}
        <MacroTimeline
          solverResult={solverResult}
          currentFrame={currentFrame}
          onSelectFrame={(f) => {
            setCurrentFrame(f);
            setIsPlaying(false);
          }}
        />

        {/* 2.1 Engine Documentation & Constant Inspector */}
        <EngineInspector />
      </main>

      {/* Footer */}
      <footer className="border-t border-neutral-800/80 py-4 px-4 text-center text-xs text-neutral-500 font-mono">
        Geometry Dash 2.1 Simulator & Macro Pathfinder • 240Hz Discrete Step Simulation
      </footer>
    </div>
  );
}
