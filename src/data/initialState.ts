import { Layer, Clip, Screen, SensorDevice, ParameterRoute } from '../types';

export const INITIAL_LAYERS: Layer[] = [
  {
    id: 3,
    name: 'Layer 3',
    opacity: 1.0,
    audioVolume: 0.85,
    solo: false,
    blind: false,
    bypass: false,
    blendMode: 'ADDITIVE',
    activeCol: 2
  },
  {
    id: 2,
    name: 'Layer 2',
    opacity: 0.50,
    audioVolume: 0.0,
    solo: false,
    blind: false,
    bypass: false,
    blendMode: 'ALPHA',
    activeCol: 2
  },
  {
    id: 1,
    name: 'Layer 1',
    opacity: 1.0,
    audioVolume: 0.60,
    solo: false,
    blind: false,
    bypass: false,
    blendMode: 'ALPHA',
    activeCol: 2
  }
];

export const INITIAL_CLIPS: Record<number, Record<number, Clip>> = {
  3: {
    1: {
      id: 'l3-c1',
      name: 'Cyber Hex Grid',
      category: 'Generators',
      type: 'generator',
      color: '#FF7F50',
      loaded: true,
      active: false,
      progress: 0.35,
      duration: 8.0,
      speed: 1.0,
      loopMode: 'loop',
      scale: 1.0,
      posX: 0,
      posY: 0,
      rotation: 0,
      opacity: 1.0,
      blendMode: 'ADDITIVE',
      fx: { colorize: false, glow: true, hueRotate: 0, pixelate: false, edgeGlow: true }
    },
    2: {
      id: 'l3-c2',
      name: 'Plasma Waves 4K',
      category: 'VJ Loops',
      type: 'loop',
      color: '#FFD166',
      loaded: true,
      active: true,
      progress: 0.65,
      duration: 12.0,
      speed: 1.0,
      loopMode: 'loop',
      scale: 1.0,
      posX: 0,
      posY: 0,
      rotation: 0,
      opacity: 0.85,
      blendMode: 'ADDITIVE',
      fx: { colorize: true, glow: true, hueRotate: 45, pixelate: false, edgeGlow: false }
    },
    3: {
      id: 'l3-c3',
      name: 'Particle Vortex',
      category: 'Audio Reactive',
      type: 'interactive',
      color: '#06D6A0',
      loaded: true,
      active: false,
      progress: 0.12,
      duration: 10.0,
      speed: 1.2,
      loopMode: 'loop',
      scale: 1.1,
      posX: 0,
      posY: 0,
      rotation: 15,
      opacity: 0.9,
      blendMode: 'ADDITIVE',
      fx: { colorize: false, glow: true, hueRotate: 180, pixelate: false, edgeGlow: true }
    },
    4: {
      id: 'l3-c4',
      name: 'Strobe Tunnel',
      category: 'VJ Loops',
      type: 'loop',
      color: '#118AB2',
      loaded: true,
      active: false,
      progress: 0.8,
      duration: 6.0,
      speed: 1.5,
      loopMode: 'bounce',
      scale: 1.0,
      posX: 0,
      posY: 0,
      rotation: 0,
      opacity: 1.0,
      blendMode: 'SCREEN',
      fx: { colorize: false, glow: false, hueRotate: 0, pixelate: true, edgeGlow: false }
    }
  },
  2: {
    1: {
      id: 'l2-c1',
      name: 'Geometric Wireframe',
      category: 'Generators',
      type: 'generator',
      color: '#118AB2',
      loaded: true,
      active: true,
      progress: 0.42,
      duration: 16.0,
      speed: 0.8,
      loopMode: 'loop',
      scale: 1.0,
      posX: 0,
      posY: 0,
      rotation: 0,
      opacity: 0.7,
      blendMode: 'ALPHA',
      fx: { colorize: false, glow: true, hueRotate: 0, pixelate: false, edgeGlow: false }
    },
    2: {
      id: 'l2-c2',
      name: 'Liquid Chrome',
      category: 'VJ Loops',
      type: 'loop',
      color: '#06D6A0',
      loaded: true,
      active: false,
      progress: 0.05,
      duration: 14.0,
      speed: 1.0,
      loopMode: 'loop',
      scale: 1.0,
      posX: 0,
      posY: 0,
      rotation: 0,
      opacity: 1.0,
      blendMode: 'ALPHA',
      fx: { colorize: true, glow: false, hueRotate: 90, pixelate: false, edgeGlow: false }
    }
  },
  1: {
    1: {
      id: 'l1-c1',
      name: 'Deep Ambient Dark',
      category: 'Generators',
      type: 'generator',
      color: '#FF7F50',
      loaded: true,
      active: false,
      progress: 0.9,
      duration: 24.0,
      speed: 0.5,
      loopMode: 'loop',
      scale: 1.0,
      posX: 0,
      posY: 0,
      rotation: 0,
      opacity: 1.0,
      blendMode: 'ALPHA',
      fx: { colorize: false, glow: false, hueRotate: 0, pixelate: false, edgeGlow: false }
    }
  }
};

export const INITIAL_SCREENS: Screen[] = [
  {
    id: 'screen1',
    name: 'Main Projector 1',
    outputDevice: 'Display 1 (HDMI 1920x1080@60Hz)',
    resolution: { w: 1920, h: 1080 },
    fps: 60,
    edgeBlending: false,
    slices: [
      {
        id: 'slice1',
        name: 'Center Stage Wall',
        screenId: 'screen1',
        inputRect: { x: 100, y: 50, w: 1720, h: 980 },
        outputQuad: {
          tl: [180, 100],
          tr: [1740, 140],
          br: [1680, 960],
          bl: [240, 920]
        },
        warpMode: 'cornerPin',
        visible: true,
        masks: [
          {
            id: 'mask1',
            name: 'DJ Console Cutout',
            type: 'polygon',
            inverted: true,
            feather: 8,
            visible: true,
            points: [
              { x: 600, y: 700 },
              { x: 1320, y: 700 },
              { x: 1280, y: 920 },
              { x: 640, y: 920 }
            ]
          }
        ]
      },
      {
        id: 'slice2',
        name: 'Left Pillar Accent',
        screenId: 'screen1',
        inputRect: { x: 0, y: 0, w: 600, h: 1080 },
        outputQuad: {
          tl: [40, 80],
          tr: [220, 120],
          br: [200, 980],
          bl: [30, 960]
        },
        warpMode: 'cornerPin',
        visible: true,
        masks: []
      }
    ]
  }
];

export const INITIAL_DEVICES: SensorDevice[] = [
  {
    id: 'lidar-1',
    name: 'Hokuyo UST-10LX',
    type: 'lidar',
    endpoint: '192.168.1.10:10940 (Ethernet)',
    frequencyHz: 40,
    connected: true,
    fps: 40.2,
    latencyMs: 8.5,
    packets: 14280
  },
  {
    id: 'mock-1',
    name: 'HexMap Mock Engine',
    type: 'mock',
    endpoint: 'In-Memory Shared RingBuffer',
    frequencyHz: 60,
    connected: true,
    fps: 60.0,
    latencyMs: 1.2,
    packets: 21540
  },
  {
    id: 'osc-1',
    name: 'OSC / TUIO UDP Listener',
    type: 'osc',
    endpoint: 'udp://0.0.0.0:9000',
    frequencyHz: 120,
    connected: false,
    fps: 0,
    latencyMs: 0,
    packets: 0
  },
  {
    id: 'serial-1',
    name: 'Teensy Touch Sensor',
    type: 'serial',
    endpoint: 'COM3 @ 115200 baud',
    frequencyHz: 1000,
    connected: false,
    fps: 0,
    latencyMs: 0,
    packets: 0
  }
];

export const INITIAL_ROUTES: ParameterRoute[] = [
  {
    id: 'route-1',
    sourceKey: 'blob1.x',
    sourceLabel: 'Blob 1: X (Position)',
    targetKey: 'layer1.opacity',
    targetLabel: 'Layer 1: Opacity',
    active: true
  },
  {
    id: 'route-2',
    sourceKey: 'blob1.y',
    sourceLabel: 'Blob 1: Y (Depth)',
    targetKey: 'fx.hue',
    targetLabel: 'FX: Hue Rotate',
    active: true
  },
  {
    id: 'route-3',
    sourceKey: 'touch.down',
    sourceLabel: 'Touch Event: DOWN',
    targetKey: 'trigger.explosion',
    targetLabel: 'Explode Particle Burst',
    active: true
  }
];
