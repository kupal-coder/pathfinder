export enum VehicleType {
  Cube = 'Cube',
  Ship = 'Ship',
  Ball = 'Ball',
  Ufo = 'Ufo',
  Wave = 'Wave',
  Robot = 'Robot',
  Spider = 'Spider',
  Swing = 'Swing',
}

export enum OrbType {
  Yellow = 'Yellow',
  Pink = 'Pink',
  Red = 'Red',
  Blue = 'Blue',
  Green = 'Green',
  Black = 'Black',
  Spider = 'Spider',
  Dash = 'Dash',
  GravityDash = 'GravityDash',
}

export enum PadType {
  Yellow = 'Yellow',
  Pink = 'Pink',
  Red = 'Red',
  Blue = 'Blue',
  Spider = 'Spider',
}

export enum SpecialBlockType {
  S = 'S', // Stop Dash (1813)
  J = 'J', // Prevent Jump Buffer (1814)
  H = 'H', // Head Collision Safety (1815)
  D = 'D', // Wave Slide (1816)
}

export interface Vec2 {
  x: number;
  y: number;
}

export interface LevelObject {
  id: string;
  typeId: number;
  x: number;
  y: number;
  width: number;
  height: number;
  rotation: number;
  category: 'block' | 'spike' | 'saw' | 'slope' | 'orb' | 'pad' | 'portal' | 'special';
  name: string;
  orbType?: OrbType;
  padType?: PadType;
  specialType?: SpecialBlockType;
  portalType?: 'gravity_down' | 'gravity_up' | 'size_normal' | 'size_mini' | 'speed_05' | 'speed_1' | 'speed_2' | 'speed_3' | 'speed_4' | 'mirror_on' | 'mirror_off' | 'dual_on' | 'dual_off' | 'vehicle_cube' | 'vehicle_ship' | 'vehicle_ball' | 'vehicle_ufo' | 'vehicle_wave' | 'vehicle_robot' | 'vehicle_spider' | 'vehicle_swing';
  slopeType?: 'regular' | 'steep' | 'gentle';
  flippedX?: boolean;
  flippedY?: boolean;
}

export interface PlayerState {
  frame: number;
  x: number;
  y: number;
  vx: number;
  vy: number;
  rotation: number;
  grounded: boolean;
  dead: boolean;
  upsideDown: boolean;
  small: boolean;
  speed: number; // 0=0.5x, 1=1x, 2=2x, 3=3x, 4=4x
  vehicle: VehicleType;
  isDashing: boolean;
  dashAngle: number;
  hasHBlock: boolean;
  hasDBlock: boolean;
  hasJBlock: boolean;
  dual: boolean;
  mirror: boolean;
  input: boolean;
  coyoteFrames: number;
  robotBoostFrames: number;
  lastSpiderRay?: { fromY: number; toY: number; hitHazard: boolean };
}

export interface SolverResult {
  solved: boolean;
  totalFrames: number;
  exploredNodes: number;
  timeMs: number;
  inputs: boolean[]; // true = frame clicked
  trajectory: PlayerState[];
  macroEvents: { frame: number; down: boolean }[];
}
