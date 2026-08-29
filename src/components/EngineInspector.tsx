import React, { useState } from 'react';
import { VehicleType, OrbType, PadType, SpecialBlockType } from '../types/gd';
import { PHYS_FPS, PHYS_SPEEDS } from '../engine/physics';
import { Cpu, Zap, Activity, Info, CheckCircle2, ShieldCheck } from 'lucide-react';

export const EngineInspector: React.FC = () => {
  const [activeTab, setActiveTab] = useState<'vehicles' | 'orbs' | 'special' | 'mod'>('vehicles');

  return (
    <div id="engine-inspector-panel" className="bg-neutral-900 border border-neutral-800 rounded-xl overflow-hidden shadow-lg">
      <div className="flex flex-wrap items-center justify-between px-4 py-3 bg-neutral-950 border-b border-neutral-800 gap-2">
        <div className="flex items-center gap-2">
          <Cpu className="w-4 h-4 text-cyan-400" />
          <h3 className="font-semibold text-sm text-neutral-200">GD 2.1 Simulation Engine Architecture</h3>
        </div>

        <div className="flex bg-neutral-900 border border-neutral-800 rounded-lg p-0.5 text-xs font-mono">
          <button
            id="tab-vehicles"
            onClick={() => setActiveTab('vehicles')}
            className={`px-3 py-1 rounded transition-colors ${
              activeTab === 'vehicles' ? 'bg-cyan-500/20 text-cyan-300 font-bold' : 'text-neutral-400 hover:text-neutral-200'
            }`}
          >
            Vehicle Physics
          </button>
          <button
            id="tab-orbs"
            onClick={() => setActiveTab('orbs')}
            className={`px-3 py-1 rounded transition-colors ${
              activeTab === 'orbs' ? 'bg-cyan-500/20 text-cyan-300 font-bold' : 'text-neutral-400 hover:text-neutral-200'
            }`}
          >
            Orbs & 4x Speeds
          </button>
          <button
            id="tab-special"
            onClick={() => setActiveTab('special')}
            className={`px-3 py-1 rounded transition-colors ${
              activeTab === 'special' ? 'bg-cyan-500/20 text-cyan-300 font-bold' : 'text-neutral-400 hover:text-neutral-200'
            }`}
          >
            2.1 Special Blocks
          </button>
          <button
            id="tab-mod"
            onClick={() => setActiveTab('mod')}
            className={`px-3 py-1 rounded transition-colors ${
              activeTab === 'mod' ? 'bg-cyan-500/20 text-cyan-300 font-bold' : 'text-neutral-400 hover:text-neutral-200'
            }`}
          >
            Geode Mod (v5.9)
          </button>
        </div>
      </div>

      <div className="p-4 text-sm text-neutral-300">
        {activeTab === 'vehicles' && (
          <div className="space-y-4">
            <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-3">
              <div className="p-3 bg-neutral-950 border border-neutral-800 rounded-lg">
                <div className="font-bold text-cyan-400 mb-1 flex items-center justify-between">
                  <span>Spider (GD 2.1)</span>
                  <span className="text-[10px] font-mono bg-purple-950 text-purple-300 px-1.5 py-0.5 rounded">Raycast Teleport</span>
                </div>
                <p className="text-xs text-neutral-400 leading-relaxed">
                  Vertical collision raycasting scans all blocks/hazards in the active section. On input, gravity inverts and the player snaps instantly to the nearest ceiling or floor surface without transit frames.
                </p>
              </div>

              <div className="p-3 bg-neutral-950 border border-neutral-800 rounded-lg">
                <div className="font-bold text-cyan-400 mb-1 flex items-center justify-between">
                  <span>Wave (GD 2.0 / 2.1)</span>
                  <span className="text-[10px] font-mono bg-blue-950 text-blue-300 px-1.5 py-0.5 rounded">±45° Diagonal</span>
                </div>
                <p className="text-xs text-neutral-400 leading-relaxed">
                  Constant diagonal velocity matching horizontal speed. Hitting standard blocks is fatal, but when touching a <strong>D-Block</strong>, the Wave safely slides horizontally across the solid surface.
                </p>
              </div>

              <div className="p-3 bg-neutral-950 border border-neutral-800 rounded-lg">
                <div className="font-bold text-cyan-400 mb-1 flex items-center justify-between">
                  <span>Robot (GD 2.1 Boost)</span>
                  <span className="text-[10px] font-mono bg-orange-950 text-orange-300 px-1.5 py-0.5 rounded">Variable Boost</span>
                </div>
                <p className="text-xs text-neutral-400 leading-relaxed">
                  Initiates with initial upward velocity and applies continuous upward boost acceleration for up to 35 frames while the jump key is held down.
                </p>
              </div>

              <div className="p-3 bg-neutral-950 border border-neutral-800 rounded-lg">
                <div className="font-bold text-cyan-400 mb-1 flex items-center justify-between">
                  <span>Cube & Ball</span>
                  <span className="text-[10px] font-mono bg-emerald-950 text-emerald-300 px-1.5 py-0.5 rounded">240Hz Discrete Step</span>
                </div>
                <p className="text-xs text-neutral-400 leading-relaxed">
                  Calculated with exact gravity constant <code className="text-cyan-300 font-mono">-2793.4752</code> units/s². Coyote frames (1 frame grace) buffer jump inputs seamlessly.
                </p>
              </div>

              <div className="p-3 bg-neutral-950 border border-neutral-800 rounded-lg">
                <div className="font-bold text-cyan-400 mb-1 flex items-center justify-between">
                  <span>Ship & UFO</span>
                  <span className="text-[10px] font-mono bg-pink-950 text-pink-300 px-1.5 py-0.5 rounded">Continuous Thrust</span>
                </div>
                <p className="text-xs text-neutral-400 leading-relaxed">
                  Ship applies smooth upward thrust while button is held, capped at terminal velocity. UFO applies instant impulse velocity bursts per click.
                </p>
              </div>

              <div className="p-3 bg-neutral-950 border border-neutral-800 rounded-lg">
                <div className="font-bold text-cyan-400 mb-1 flex items-center justify-between">
                  <span>Dash Orbs</span>
                  <span className="text-[10px] font-mono bg-teal-950 text-teal-300 px-1.5 py-0.5 rounded">Gravity Suspended</span>
                </div>
                <p className="text-xs text-neutral-400 leading-relaxed">
                  Overrides gravity and locks player trajectory along the orb's rotation vector while input is maintained, terminating upon release or hitting an <strong>S-Block</strong>.
                </p>
              </div>
            </div>
          </div>
        )}

        {activeTab === 'orbs' && (
          <div className="space-y-3">
            <div className="text-xs text-neutral-400 mb-2">
              GD 2.1 expanded speed multipliers up to <strong>4x Speed</strong> (576.0 units/s). All impulse velocities are look-up indexed:
            </div>
            <div className="overflow-x-auto">
              <table className="w-full text-xs font-mono text-left border-collapse">
                <thead>
                  <tr className="bg-neutral-950 text-neutral-400 border-b border-neutral-800">
                    <th className="p-2">Speed Tier</th>
                    <th className="p-2">Multiplier</th>
                    <th className="p-2">Horizontal Velocity (Units/s)</th>
                    <th className="p-2">Delta X per 240Hz Frame</th>
                  </tr>
                </thead>
                <tbody className="divide-y divide-neutral-800/60">
                  {PHYS_SPEEDS.map((spd, idx) => (
                    <tr key={idx} className="hover:bg-neutral-950/50">
                      <td className="p-2 font-bold text-cyan-400">
                        {idx === 0 ? '0.5x (Slow)' : idx === 1 ? '1x (Normal)' : idx === 2 ? '2x (Fast)' : idx === 3 ? '3x (Very Fast)' : '4x (Super Fast)'}
                      </td>
                      <td className="p-2 text-neutral-300">{idx === 0 ? '0.7x' : idx === 1 ? '0.9x' : idx === 2 ? '1.14x' : idx === 3 ? '1.38x' : '1.7x'}</td>
                      <td className="p-2 text-neutral-200">{spd.toFixed(3)}</td>
                      <td className="p-2 text-emerald-400">{(spd / PHYS_FPS).toFixed(4)} px/f</td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          </div>
        )}

        {activeTab === 'special' && (
          <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
            <div className="p-3 bg-neutral-950 border border-neutral-800 rounded-lg flex items-start gap-3">
              <span className="w-8 h-8 rounded-lg bg-orange-950 text-orange-400 border border-orange-700/50 flex items-center justify-center font-bold text-base">S</span>
              <div>
                <h4 className="font-bold text-neutral-200 text-xs">S-Block (ID: 1813) - Stop Dash</h4>
                <p className="text-xs text-neutral-400 mt-1">Immediately cancels active Dash Orb trajectory, restoring standard gravitational acceleration.</p>
              </div>
            </div>

            <div className="p-3 bg-neutral-950 border border-neutral-800 rounded-lg flex items-start gap-3">
              <span className="w-8 h-8 rounded-lg bg-cyan-950 text-cyan-400 border border-cyan-700/50 flex items-center justify-center font-bold text-base">J</span>
              <div>
                <h4 className="font-bold text-neutral-200 text-xs">J-Block (ID: 1814) - Prevent Jump Buffer</h4>
                <p className="text-xs text-neutral-400 mt-1">Cancels held buffer inputs to prevent accidental ground or orb jumps on landing.</p>
              </div>
            </div>

            <div className="p-3 bg-neutral-950 border border-neutral-800 rounded-lg flex items-start gap-3">
              <span className="w-8 h-8 rounded-lg bg-yellow-950 text-yellow-400 border border-yellow-700/50 flex items-center justify-center font-bold text-base">H</span>
              <div>
                <h4 className="font-bold text-neutral-200 text-xs">H-Block (ID: 1815) - Head Collision Safety</h4>
                <p className="text-xs text-neutral-400 mt-1">Allows Cube and Robot to bump into ceiling blocks without crashing.</p>
              </div>
            </div>

            <div className="p-3 bg-neutral-950 border border-neutral-800 rounded-lg flex items-start gap-3">
              <span className="w-8 h-8 rounded-lg bg-purple-950 text-purple-400 border border-purple-700/50 flex items-center justify-center font-bold text-base">D</span>
              <div>
                <h4 className="font-bold text-neutral-200 text-xs">D-Block (ID: 1816) - Wave Slide</h4>
                <p className="text-xs text-neutral-400 mt-1">Allows the Wave vehicle to slide smoothly along solid block surfaces without exploding.</p>
              </div>
            </div>
          </div>
        )}

        {activeTab === 'mod' && (
          <div className="space-y-3">
            <div className="flex items-center gap-2 p-2.5 bg-neutral-950 border border-neutral-800 rounded-lg text-xs font-mono">
              <ShieldCheck className="w-4 h-4 text-emerald-400 shrink-0" />
              <span>Geode Mod Target: <strong>camila314.pathfinder</strong> (Geode v5.10.0+, GD v2.2081 / 2.1 Engine)</span>
            </div>
            <p className="text-xs text-neutral-400 leading-relaxed">
              Pathfinder utilizes sub-process physics checking with <code className="text-cyan-300 font-mono">gd-sim</code> and Geode <code className="text-cyan-300 font-mono">PlayerObject</code> memory hooks. The simulator accurately mirrors in-game frame calculations down to discrete sub-pixel floating point precision.
            </p>
          </div>
        )}
      </div>
    </div>
  );
};
