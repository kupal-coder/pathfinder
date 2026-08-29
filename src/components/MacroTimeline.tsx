import React, { useState } from 'react';
import { PlayerState, SolverResult } from '../types/gd';
import { LevelSolver } from '../engine/solver';
import { Download, Copy, Check, Terminal, FileCode, Sliders } from 'lucide-react';

interface MacroTimelineProps {
  solverResult: SolverResult | null;
  currentFrame: number;
  onSelectFrame: (frame: number) => void;
}

export const MacroTimeline: React.FC<MacroTimelineProps> = ({
  solverResult,
  currentFrame,
  onSelectFrame,
}) => {
  const [exportFormat, setExportFormat] = useState<'json' | 'xbot' | 'megahack' | 'plain'>('xbot');
  const [copied, setCopied] = useState<boolean>(false);

  if (!solverResult) {
    return (
      <div id="macro-timeline-empty" className="p-6 bg-neutral-900 border border-neutral-800 rounded-xl text-center text-neutral-400 text-sm">
        Click <strong className="text-cyan-400">"Run Pathfinder Solver"</strong> to calculate frame-exact inputs and generate the bot macro.
      </div>
    );
  }

  const macroText = LevelSolver.exportFormat(solverResult.macroEvents, exportFormat);

  const handleCopy = () => {
    navigator.clipboard.writeText(macroText);
    setCopied(true);
    setTimeout(() => setCopied(false), 2000);
  };

  const handleDownload = () => {
    const blob = new Blob([macroText], { type: 'text/plain;charset=utf-8' });
    const url = URL.createObjectURL(blob);
    const link = document.createElement('a');
    link.href = url;
    link.download = `pathfinder_macro.${exportFormat === 'json' ? 'json' : 'txt'}`;
    link.click();
    URL.revokeObjectURL(url);
  };

  return (
    <div id="macro-timeline-panel" className="flex flex-col bg-neutral-900 border border-neutral-800 rounded-xl overflow-hidden shadow-lg">
      {/* Header */}
      <div id="macro-header" className="flex flex-wrap items-center justify-between px-4 py-3 bg-neutral-950 border-b border-neutral-800 gap-2">
        <div className="flex items-center gap-2">
          <Terminal className="w-4 h-4 text-cyan-400" />
          <h3 className="font-semibold text-sm text-neutral-200">240Hz Macro Timeline & Exporter</h3>
          <span className="text-xs px-2 py-0.5 rounded-full bg-cyan-950 text-cyan-300 border border-cyan-800/60 font-mono">
            {solverResult.macroEvents.length} Actions
          </span>
        </div>

        <div className="flex items-center gap-2">
          <div className="flex bg-neutral-900 border border-neutral-800 rounded-lg p-0.5 text-xs font-mono">
            {(['xbot', 'megahack', 'json', 'plain'] as const).map((fmt) => (
              <button
                key={fmt}
                id={`format-btn-${fmt}`}
                onClick={() => setExportFormat(fmt)}
                className={`px-2.5 py-1 rounded capitalize transition-colors ${
                  exportFormat === fmt ? 'bg-cyan-500/20 text-cyan-300 font-bold' : 'text-neutral-400 hover:text-neutral-200'
                }`}
              >
                {fmt}
              </button>
            ))}
          </div>

          <button
            id="copy-macro-btn"
            onClick={handleCopy}
            className="flex items-center gap-1 px-3 py-1 bg-neutral-800 hover:bg-neutral-700 text-neutral-200 text-xs rounded-lg border border-neutral-700 transition-colors"
          >
            {copied ? <Check className="w-3.5 h-3.5 text-emerald-400" /> : <Copy className="w-3.5 h-3.5" />}
            {copied ? 'Copied' : 'Copy'}
          </button>

          <button
            id="download-macro-btn"
            onClick={handleDownload}
            className="flex items-center gap-1 px-3 py-1 bg-cyan-600 hover:bg-cyan-500 text-white font-semibold text-xs rounded-lg transition-colors shadow-sm"
          >
            <Download className="w-3.5 h-3.5" />
            Export
          </button>
        </div>
      </div>

      <div className="grid grid-cols-1 lg:grid-cols-2 gap-4 p-4">
        {/* Macro Events Visualizer */}
        <div id="events-scroller" className="flex flex-col bg-neutral-950 border border-neutral-800 rounded-lg p-3 h-64 overflow-y-auto font-mono text-xs">
          <div className="text-[11px] font-bold text-neutral-500 uppercase tracking-wider mb-2 border-b border-neutral-800/80 pb-1">
            Frame Action List (Click row to seek)
          </div>
          {solverResult.macroEvents.length === 0 ? (
            <div className="text-neutral-500 italic py-4 text-center">No inputs needed (Clean auto trajectory)</div>
          ) : (
            solverResult.macroEvents.map((evt, idx) => (
              <div
                key={idx}
                id={`macro-event-${idx}`}
                onClick={() => onSelectFrame(evt.frame)}
                className={`flex items-center justify-between py-1 px-2 rounded cursor-pointer transition-colors ${
                  Math.abs(currentFrame - evt.frame) <= 3
                    ? 'bg-cyan-950/70 text-cyan-300 border border-cyan-700/50'
                    : 'hover:bg-neutral-900 text-neutral-300'
                }`}
              >
                <div className="flex items-center gap-2">
                  <span className="text-neutral-500">#{idx + 1}</span>
                  <span className="font-bold text-cyan-400">F: {evt.frame}</span>
                  <span className="text-neutral-500">({(evt.frame / 240).toFixed(3)}s)</span>
                </div>
                <span className={`px-2 py-0.5 rounded text-[10px] font-bold ${
                  evt.down ? 'bg-emerald-950 text-emerald-300 border border-emerald-700/50' : 'bg-neutral-800 text-neutral-400'
                }`}>
                  {evt.down ? 'PRESS (DOWN)' : 'RELEASE (UP)'}
                </span>
              </div>
            ))
          )}
        </div>

        {/* Formatted Output Viewer */}
        <div className="flex flex-col bg-neutral-950 border border-neutral-800 rounded-lg p-3 h-64">
          <div className="flex items-center justify-between mb-2 border-b border-neutral-800/80 pb-1">
            <span className="text-[11px] font-bold text-neutral-500 uppercase tracking-wider">
              {exportFormat.toUpperCase()} Code Preview
            </span>
            <FileCode className="w-3.5 h-3.5 text-neutral-500" />
          </div>
          <textarea
            id="macro-output-textarea"
            readOnly
            value={macroText}
            className="w-full h-full bg-transparent font-mono text-xs text-neutral-300 resize-none outline-none focus:outline-none"
          />
        </div>
      </div>
    </div>
  );
};
