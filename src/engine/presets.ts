import { LevelObject, OrbType, PadType, SpecialBlockType } from '../types/gd';

export interface LevelPreset {
  id: string;
  name: string;
  difficulty: 'Easy' | 'Normal' | 'Hard' | 'Insane' | 'Demon';
  description: string;
  versionTag: string;
  objects: LevelObject[];
}

export const LEVEL_PRESETS: LevelPreset[] = [
  {
    id: 'fingerdash-21',
    name: 'Fingerdash 2.1 Showcase',
    difficulty: 'Insane',
    versionTag: 'GD 2.1',
    description: 'Features Green Dash Orbs, Gravity Dash Orbs, Spider raycast teleports, and S/H/D special blocks.',
    objects: [
      // Starting platform
      { id: 'b1', typeId: 1, x: 60, y: 15, width: 60, height: 30, rotation: 0, category: 'block', name: 'Base' },
      { id: 's1', typeId: 8, x: 140, y: 15, width: 25, height: 25, rotation: 0, category: 'spike', name: 'Spike' },
      { id: 'o1', typeId: 1704, x: 180, y: 55, width: 32, height: 32, rotation: 0, category: 'orb', orbType: OrbType.Dash, name: 'Green Dash Orb' },
      
      // S-Block to release dash
      { id: 'sb1', typeId: 1813, x: 380, y: 55, width: 30, height: 30, rotation: 0, category: 'special', specialType: SpecialBlockType.S, name: 'S-Block' },
      { id: 'b2', typeId: 1, x: 420, y: 45, width: 60, height: 30, rotation: 0, category: 'block', name: 'Platform 1' },
      { id: 'p_spider', typeId: 1331, x: 480, y: 75, width: 30, height: 80, rotation: 0, category: 'portal', portalType: 'vehicle_spider', name: 'Spider Portal' },
      
      // Spider section: high ceiling blocks and ground hazards
      { id: 's_fl1', typeId: 8, x: 560, y: 15, width: 30, height: 25, rotation: 0, category: 'spike', name: 'Floor Spikes' },
      { id: 's_fl2', typeId: 8, x: 590, y: 15, width: 30, height: 25, rotation: 0, category: 'spike', name: 'Floor Spikes' },
      { id: 'cb1', typeId: 1, x: 580, y: 220, width: 120, height: 30, rotation: 0, category: 'block', name: 'Ceiling Platform' },
      
      { id: 's_ceil1', typeId: 8, x: 670, y: 205, width: 25, height: 25, rotation: 180, category: 'spike', name: 'Ceiling Spike' },
      { id: 'b3', typeId: 1, x: 740, y: 45, width: 100, height: 30, rotation: 0, category: 'block', name: 'Lower Platform' },
      
      // Spider Orb
      { id: 'so1', typeId: 1411, x: 860, y: 90, width: 32, height: 32, rotation: 0, category: 'orb', orbType: OrbType.Spider, name: 'Spider Orb' },
      { id: 'cb2', typeId: 1, x: 920, y: 240, width: 100, height: 30, rotation: 0, category: 'block', name: 'Spider Landing Ceiling' },
      
      // Gravity Dash Orb
      { id: 'gdo1', typeId: 1751, x: 1040, y: 210, width: 32, height: 32, rotation: 0, category: 'orb', orbType: OrbType.GravityDash, name: 'Gravity Dash Orb' },
      { id: 'b4', typeId: 1, x: 1220, y: 60, width: 90, height: 30, rotation: 0, category: 'block', name: 'Exit Block' },
    ]
  },
  {
    id: 'wave-dblocks',
    name: 'Wave & D-Blocks Corridor',
    difficulty: 'Demon',
    versionTag: 'GD 2.0 / 2.1',
    description: 'High speed Wave sliding along D-blocks and micro-adjusting through hazard gaps.',
    objects: [
      { id: 'p_wave', typeId: 660, x: 80, y: 60, width: 30, height: 80, rotation: 0, category: 'portal', portalType: 'vehicle_wave', name: 'Wave Portal' },
      { id: 'p_speed3', typeId: 202, x: 140, y: 60, width: 30, height: 60, rotation: 0, category: 'portal', portalType: 'speed_3', name: '3x Speed Portal' },
      
      // Wave Corridor with D-blocks on ceiling and floor
      { id: 'b_fl1', typeId: 1, x: 300, y: 15, width: 240, height: 30, rotation: 0, category: 'block', name: 'Floor Run' },
      { id: 'd_fl1', typeId: 1816, x: 300, y: 15, width: 240, height: 30, rotation: 0, category: 'special', specialType: SpecialBlockType.D, name: 'D-Block Slide Floor' },
      
      { id: 'b_cl1', typeId: 1, x: 300, y: 150, width: 240, height: 30, rotation: 0, category: 'block', name: 'Ceiling Run' },
      { id: 'd_cl1', typeId: 1816, x: 300, y: 150, width: 240, height: 30, rotation: 0, category: 'special', specialType: SpecialBlockType.D, name: 'D-Block Slide Ceiling' },
      
      // Middle spikes requiring wave zig-zag
      { id: 'sp1', typeId: 8, x: 240, y: 80, width: 25, height: 25, rotation: 0, category: 'spike', name: 'Hazard 1' },
      { id: 'sp2', typeId: 8, x: 360, y: 80, width: 25, height: 25, rotation: 0, category: 'spike', name: 'Hazard 2' },
      { id: 'sp3', typeId: 8, x: 480, y: 80, width: 25, height: 25, rotation: 0, category: 'spike', name: 'Hazard 3' },
      
      { id: 'b_end', typeId: 1, x: 600, y: 50, width: 100, height: 30, rotation: 0, category: 'block', name: 'Goal Block' },
    ]
  },
  {
    id: 'stereo-classic',
    name: 'Stereo Madness - Triple Spike',
    difficulty: 'Easy',
    versionTag: 'GD 1.0',
    description: 'Standard Cube physics, yellow pad jump, and the iconic triple spike sequence.',
    objects: [
      { id: 's1', typeId: 8, x: 150, y: 15, width: 25, height: 25, rotation: 0, category: 'spike', name: 'Spike 1' },
      { id: 'pad1', typeId: 140, x: 240, y: 15, width: 30, height: 10, rotation: 0, category: 'pad', padType: PadType.Yellow, name: 'Yellow Pad' },
      { id: 'b1', typeId: 1, x: 360, y: 60, width: 60, height: 30, rotation: 0, category: 'block', name: 'High Platform' },
      { id: 'orb1', typeId: 36, x: 450, y: 90, width: 32, height: 32, rotation: 0, category: 'orb', orbType: OrbType.Yellow, name: 'Yellow Orb' },
      
      // Triple Spike
      { id: 'ts1', typeId: 8, x: 570, y: 15, width: 25, height: 25, rotation: 0, category: 'spike', name: 'Triple 1' },
      { id: 'ts2', typeId: 8, x: 595, y: 15, width: 25, height: 25, rotation: 0, category: 'spike', name: 'Triple 2' },
      { id: 'ts3', typeId: 8, x: 620, y: 15, width: 25, height: 25, rotation: 0, category: 'spike', name: 'Triple 3' },
      
      { id: 'b2', typeId: 1, x: 740, y: 15, width: 90, height: 30, rotation: 0, category: 'block', name: 'End Platform' },
    ]
  },
  {
    id: 'h-block-robot',
    name: 'H-Block & Robot Jumps',
    difficulty: 'Hard',
    versionTag: 'GD 2.1',
    description: 'Demonstrating H-Blocks saving Cube/Robot from head collisions on low ceilings.',
    objects: [
      { id: 'p_robot', typeId: 745, x: 80, y: 60, width: 30, height: 80, rotation: 0, category: 'portal', portalType: 'vehicle_robot', name: 'Robot Portal' },
      { id: 's1', typeId: 8, x: 200, y: 15, width: 25, height: 25, rotation: 0, category: 'spike', name: 'Spike' },
      
      // Low ceiling with H-Block
      { id: 'ceil1', typeId: 1, x: 280, y: 70, width: 140, height: 30, rotation: 0, category: 'block', name: 'Low Ceiling' },
      { id: 'h1', typeId: 1815, x: 280, y: 70, width: 140, height: 30, rotation: 0, category: 'special', specialType: SpecialBlockType.H, name: 'H-Block (Head Safe)' },
      
      { id: 'orb_pink', typeId: 141, x: 420, y: 40, width: 30, height: 30, rotation: 0, category: 'orb', orbType: OrbType.Pink, name: 'Pink Orb' },
      { id: 'b_land', typeId: 1, x: 540, y: 50, width: 80, height: 30, rotation: 0, category: 'block', name: 'Landing Block' },
    ]
  }
];
