import React, { useState } from 'react';
import { LevelObject, OrbType, PadType, SpecialBlockType, VehicleType } from '../types/gd';
import { LEVEL_PRESETS, LevelPreset } from '../engine/presets';
import { Plus, Trash2, Sparkles, SlidersHorizontal, ArrowRight, Play } from 'lucide-react';

interface LevelEditorProps {
  objects: LevelObject[];
  onUpdateObjects: (objects: LevelObject[]) => void;
  onSelectPreset: (preset: LevelPreset) => void;
  selectedPresetId: string;
}

export const LevelEditor: React.FC<LevelEditorProps> = ({
  objects,
  onUpdateObjects,
  onSelectPreset,
  selectedPresetId,
}) => {
  const [newType, setNewType] = useState<'block' | 'spike' | 'saw' | 'orb' | 'pad' | 'special' | 'portal'>('spike');
  const [newX, setNewX] = useState<number>(300);
  const [newY, setNewY] = useState<number>(15);

  const handleAddObject = () => {
    const id = `custom_${Date.now()}`;
    let newObj: LevelObject;

    if (newType === 'block') {
      newObj = { id, typeId: 1, x: newX, y: newY, width: 60, height: 30, rotation: 0, category: 'block', name: 'Solid Block' };
    } else if (newType === 'spike') {
      newObj = { id, typeId: 8, x: newX, y: newY, width: 25, height: 25, rotation: 0, category: 'spike', name: 'Spike' };
    } else if (newType === 'saw') {
      newObj = { id, typeId: 88, x: newX, y: newY, width: 35, height: 35, rotation: 0, category: 'saw', name: 'Sawblade' };
    } else if (newType === 'orb') {
      newObj = { id, typeId: 1704, x: newX, y: newY, width: 32, height: 32, rotation: 0, category: 'orb', orbType: OrbType.Dash, name: 'Dash Orb' };
    } else if (newType === 'pad') {
      newObj = { id, typeId: 140, x: newX, y: newY, width: 30, height: 10, rotation: 0, category: 'pad', padType: PadType.Yellow, name: 'Yellow Pad' };
    } else if (newType === 'special') {
      newObj = { id, typeId: 1815, x: newX, y: newY, width: 30, height: 30, rotation: 0, category: 'special', specialType: SpecialBlockType.H, name: 'H-Block' };
    } else {
      newObj = { id, typeId: 1331, x: newX, y: newY, width: 30, height: 80, rotation: 0, category: 'portal', portalType: 'vehicle_spider', name: 'Spider Portal' };
    }

    onUpdateObjects([...objects, newObj]);
    setNewX(x => x + 60);
  };

  const handleRemoveObject = (id: string) => {
    onUpdateObjects(objects.filter(o => o.id !== id));
  };

  return (
    <div id="level-editor-panel" className="bg-neutral-900 border border-neutral-800 rounded-xl overflow-hidden shadow-lg p-4 space-y-4">
      {/* Level Presets Selection */}
      <div>
        <div className="flex items-center gap-2 mb-2">
          <Sparkles className="w-4 h-4 text-cyan-400" />
          <h3 className="text-xs font-bold uppercase tracking-wider text-neutral-400">Level Physics Presets</h3>
        </div>
        <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-4 gap-2">
          {LEVEL_PRESETS.map((preset) => (
            <button
              key={preset.id}
              id={`preset-${preset.id}`}
              onClick={() => onSelectPreset(preset)}
              className={`flex flex-col text-left p-3 rounded-lg border transition-all ${
                selectedPresetId === preset.id
                  ? 'bg-cyan-950/60 border-cyan-500/80 shadow-md shadow-cyan-950/40 ring-1 ring-cyan-500/50'
                  : 'bg-neutral-950 border-neutral-800 hover:border-neutral-700'
              }`}
            >
              <div className="flex items-center justify-between w-full mb-1">
                <span className="font-bold text-xs text-neutral-200">{preset.name}</span>
                <span className="text-[10px] font-mono px-1.5 py-0.5 rounded bg-neutral-800 text-cyan-400">
                  {preset.versionTag}
                </span>
              </div>
              <p className="text-[11px] text-neutral-400 line-clamp-2 leading-relaxed">
                {preset.description}
              </p>
            </button>
          ))}
        </div>
      </div>

      {/* Quick Add Custom Object */}
      <div className="pt-2 border-t border-neutral-800 flex flex-wrap items-center justify-between gap-3">
        <div className="flex flex-wrap items-center gap-2 text-xs">
          <span className="text-neutral-400 font-mono">Add Object:</span>
          <select
            id="object-type-select"
            value={newType}
            onChange={(e) => setNewType(e.target.value as any)}
            className="bg-neutral-950 border border-neutral-700 rounded-md px-2 py-1 text-neutral-200 outline-none"
          >
            <option value="spike">Spike (Hazard)</option>
            <option value="saw">Sawblade (Hazard)</option>
            <option value="block">Solid Block</option>
            <option value="orb">Dash Orb (2.1)</option>
            <option value="pad">Yellow Jump Pad</option>
            <option value="special">H-Block / S-Block (2.1)</option>
            <option value="portal">Spider Portal (2.1)</option>
          </select>

          <div className="flex items-center gap-1 font-mono">
            <span className="text-neutral-500">X:</span>
            <input
              id="new-x-input"
              type="number"
              value={newX}
              onChange={(e) => setNewX(Number(e.target.value))}
              className="w-16 bg-neutral-950 border border-neutral-700 rounded px-1.5 py-0.5 text-neutral-200 text-center"
            />
          </div>

          <div className="flex items-center gap-1 font-mono">
            <span className="text-neutral-500">Y:</span>
            <input
              id="new-y-input"
              type="number"
              value={newY}
              onChange={(e) => setNewY(Number(e.target.value))}
              className="w-14 bg-neutral-950 border border-neutral-700 rounded px-1.5 py-0.5 text-neutral-200 text-center"
            />
          </div>

          <button
            id="add-object-btn"
            onClick={handleAddObject}
            className="flex items-center gap-1 px-3 py-1 bg-cyan-600 hover:bg-cyan-500 text-white font-semibold rounded-md transition-colors shadow-sm"
          >
            <Plus className="w-3.5 h-3.5" />
            Place
          </button>
        </div>

        <div className="text-xs text-neutral-400 font-mono">
          Total Objects: <strong className="text-cyan-400">{objects.length}</strong>
        </div>
      </div>
    </div>
  );
};
