export type Language = 'vi' | 'en';

export type BlendMode = 
  | 'ALPHA' 
  | 'ADDITIVE' 
  | 'SCREEN' 
  | 'MULTIPLY' 
  | 'LIGHTEN' 
  | 'DIFFERENCE';

export interface Clip {
  id: string;
  name: string;
  category: string;
  type: 'generator' | 'loop' | 'interactive' | 'custom';
  color: string;
  loaded: boolean;
  active: boolean;
  progress: number;
  duration: number; // seconds
  speed: number;
  loopMode: 'loop' | 'bounce' | 'oneshot';
  scale: number;
  posX: number;
  posY: number;
  rotation: number;
  opacity: number;
  blendMode: BlendMode;
  fx: {
    colorize: boolean;
    glow: boolean;
    hueRotate: number;
    pixelate: boolean;
    edgeGlow: boolean;
  };
}

export interface Layer {
  id: number;
  name: string;
  opacity: number;
  audioVolume: number;
  solo: boolean;
  blind: boolean;
  bypass: boolean;
  blendMode: BlendMode;
  activeCol: number | null;
}

export interface Mask {
  id: string;
  name: string;
  type: 'polygon' | 'circle' | 'bezier';
  inverted: boolean;
  feather: number;
  visible: boolean;
  points: Array<{ x: number; y: number; c1?: { x: number; y: number }; c2?: { x: number; y: number } }>;
}

export interface Slice {
  id: string;
  name: string;
  screenId: string;
  inputRect: { x: number; y: number; w: number; h: number };
  outputQuad: {
    tl: [number, number]; // [x, y] in percentage or px
    tr: [number, number];
    br: [number, number];
    bl: [number, number];
  };
  warpMode: 'cornerPin' | 'mesh3x3';
  meshPoints?: [number, number][][]; // 3x3 grid
  masks: Mask[];
  visible: boolean;
}

export interface Screen {
  id: string;
  name: string;
  outputDevice: string;
  resolution: { w: number; h: number };
  fps: number;
  edgeBlending: boolean;
  slices: Slice[];
}

export interface SensorPoint {
  id: number;
  x: number; // mm or normalized
  y: number;
  z: number;
  confidence: number;
  state: 'down' | 'move' | 'up';
  vx?: number;
  vy?: number;
}

export interface SensorDevice {
  id: string;
  name: string;
  type: 'lidar' | 'serial' | 'osc' | 'depth' | 'mock';
  endpoint: string;
  frequencyHz: number;
  connected: boolean;
  fps: number;
  latencyMs: number;
  packets: number;
}

export interface CalibrationPair {
  id: number;
  sensor: [number, number];
  output: [number, number];
  reprojectionError: number;
  valid: boolean;
}

export interface ParameterRoute {
  id: string;
  sourceKey: string;
  sourceLabel: string;
  targetKey: string;
  targetLabel: string;
  active: boolean;
}
