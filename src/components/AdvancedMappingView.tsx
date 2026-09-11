import React, { useState, useRef, useEffect } from 'react';
import { 
  FolderTree, 
  Plus, 
  Trash2, 
  Eye, 
  EyeOff, 
  Scissors, 
  Maximize, 
  Move, 
  RotateCcw,
  Layers,
  Check,
  Monitor
} from 'lucide-react';
import { Screen, Slice, Mask, Language, Layer, Clip } from '../types';
import { translations } from '../utils/localization';
import { getPerspectiveTransformMatrix, Point2D } from '../utils/math';
import { LiveCanvas } from './LiveCanvas';

interface AdvancedMappingViewProps {
  screens: Screen[];
  setScreens: React.Dispatch<React.SetStateAction<Screen[]>>;
  lang: Language;
  layers: Layer[];
  clips: Record<number, Record<number, Clip>>;
  showTestGrid: boolean;
}

export const AdvancedMappingView: React.FC<AdvancedMappingViewProps> = ({
  screens,
  setScreens,
  lang,
  layers,
  clips,
  showTestGrid
}) => {
  const t = translations[lang];

  const [activeScreenId, setActiveScreenId] = useState<string>(screens[0]?.id || 'screen1');
  const [activeSliceId, setActiveSliceId] = useState<string>(screens[0]?.slices[0]?.id || 'slice1');
  const [activeMaskId, setActiveMaskId] = useState<string | null>(null);
  const [mappingMode, setMappingMode] = useState<'input' | 'output'>('output');

  // Dragging state for canvas handles
  const [dragHandle, setDragHandle] = useState<{
    type: 'input' | 'corner' | 'maskPoint';
    corner?: 'tl' | 'tr' | 'br' | 'bl';
    maskPointIndex?: number;
  } | null>(null);

  const stageRef = useRef<HTMLDivElement | null>(null);
  const screenTabsRef = useRef<HTMLDivElement | null>(null);

  const activeScreen = screens.find((s) => s.id === activeScreenId) || screens[0];
  const activeSlice = activeScreen?.slices.find((sl) => sl.id === activeSliceId) || activeScreen?.slices[0];
  const activeMask = activeSlice?.masks.find((m) => m.id === activeMaskId);

  // Auto-scroll the active screen tab into view if there are many screens
  useEffect(() => {
    if (screenTabsRef.current) {
      const activeBtn = screenTabsRef.current.querySelector<HTMLElement>('[data-active="true"]');
      if (activeBtn) {
        activeBtn.scrollIntoView({ behavior: 'smooth', block: 'nearest', inline: 'nearest' });
      }
    }
  }, [activeScreenId]);

  // Helper to update active screen
  const updateActiveScreen = (updates: Partial<Screen>) => {
    if (!activeScreen) return;
    setScreens((prev) =>
      prev.map((sc) => (sc.id === activeScreenId ? { ...sc, ...updates } : sc))
    );
  };

  // Add new screen
  const handleAddScreen = () => {
    const screenIndex = screens.length + 1;
    const newScreenId = `screen-${Date.now()}`;
    const newSliceId = `slice-${Date.now()}`;
    const newScreen: Screen = {
      id: newScreenId,
      name: `Screen ${screenIndex}`,
      outputDevice: `Projector / Display ${screenIndex}`,
      resolution: { w: 1920, h: 1080 },
      fps: 60,
      edgeBlending: false,
      slices: [
        {
          id: newSliceId,
          name: `Slice 1`,
          screenId: newScreenId,
          inputRect: { x: 0, y: 0, w: 1920, h: 1080 },
          outputQuad: {
            tl: [100, 100],
            tr: [1820, 100],
            br: [1820, 980],
            bl: [100, 980]
          },
          warpMode: 'cornerPin',
          visible: true,
          masks: []
        }
      ]
    };

    setScreens((prev) => [...prev, newScreen]);
    setActiveScreenId(newScreenId);
    setActiveSliceId(newSliceId);
    setActiveMaskId(null);
  };

  // Delete screen
  const handleDeleteScreen = (screenId: string, e?: React.MouseEvent) => {
    if (e) e.stopPropagation();
    if (screens.length <= 1) return;
    const remaining = screens.filter((s) => s.id !== screenId);
    setScreens(remaining);
    if (activeScreenId === screenId) {
      const nextScreen = remaining[0];
      setActiveScreenId(nextScreen.id);
      setActiveSliceId(nextScreen.slices[0]?.id || '');
      setActiveMaskId(null);
    }
  };

  // Helper to update active slice
  const updateActiveSlice = (updates: Partial<Slice>) => {
    if (!activeSlice) return;
    setScreens((prev) =>
      prev.map((sc) =>
        sc.id === activeScreenId
          ? {
              ...sc,
              slices: sc.slices.map((sl) => (sl.id === activeSlice.id ? { ...sl, ...updates } : sl))
            }
          : sc
      )
    );
  };

  // Add new slice
  const handleAddSlice = () => {
    const newId = `slice-${Date.now()}`;
    const newSlice: Slice = {
      id: newId,
      name: `Slice ${activeScreen.slices.length + 1}`,
      screenId: activeScreen.id,
      inputRect: { x: 200, y: 100, w: 800, h: 600 },
      outputQuad: {
        tl: [200, 100],
        tr: [1000, 100],
        br: [1000, 700],
        bl: [200, 700]
      },
      warpMode: 'cornerPin',
      visible: true,
      masks: []
    };

    setScreens((prev) =>
      prev.map((sc) =>
        sc.id === activeScreenId ? { ...sc, slices: [...sc.slices, newSlice] } : sc
      )
    );
    setActiveSliceId(newId);
  };

  // Delete active slice
  const handleDeleteSlice = (id: string) => {
    if (activeScreen.slices.length <= 1) return;
    setScreens((prev) =>
      prev.map((sc) =>
        sc.id === activeScreenId
          ? { ...sc, slices: sc.slices.filter((sl) => sl.id !== id) }
          : sc
      )
    );
    setActiveSliceId(activeScreen.slices.find((sl) => sl.id !== id)?.id || '');
  };

  // Add mask to active slice
  const handleAddMask = () => {
    if (!activeSlice) return;
    const maskId = `mask-${Date.now()}`;
    const newMask: Mask = {
      id: maskId,
      name: `Mask ${activeSlice.masks.length + 1}`,
      type: 'polygon',
      inverted: true,
      feather: 4,
      visible: true,
      points: [
        { x: 300, y: 300 },
        { x: 700, y: 300 },
        { x: 600, y: 600 },
        { x: 400, y: 600 }
      ]
    };
    updateActiveSlice({ masks: [...activeSlice.masks, newMask] });
    setActiveMaskId(maskId);
  };

  // Reset Keystone Warp
  const handleResetWarp = () => {
    if (!activeSlice) return;
    const { x, y, w, h } = activeSlice.inputRect;
    updateActiveSlice({
      outputQuad: {
        tl: [x, y],
        tr: [x + w, y],
        br: [x + w, y + h],
        bl: [x, y + h]
      }
    });
  };

  // Handle stage mouse move for handle dragging
  const handleMouseMove = (e: React.MouseEvent<HTMLDivElement>) => {
    if (!dragHandle || !stageRef.current || !activeSlice) return;

    const rect = stageRef.current.getBoundingClientRect();
    const scaleX = 1920 / rect.width;
    const scaleY = 1080 / rect.height;

    const mouseX = Math.round((e.clientX - rect.left) * scaleX);
    const mouseY = Math.round((e.clientY - rect.top) * scaleY);

    const clampedX = Math.max(0, Math.min(1920, mouseX));
    const clampedY = Math.max(0, Math.min(1080, mouseY));

    if (dragHandle.type === 'corner' && dragHandle.corner) {
      updateActiveSlice({
        outputQuad: {
          ...activeSlice.outputQuad,
          [dragHandle.corner]: [clampedX, clampedY]
        }
      });
    } else if (dragHandle.type === 'input' && dragHandle.corner) {
      const cur = activeSlice.inputRect;
      let newRect = { ...cur };
      if (dragHandle.corner === 'tl') {
        newRect = {
          x: clampedX,
          y: clampedY,
          w: cur.x + cur.w - clampedX,
          h: cur.y + cur.h - clampedY
        };
      } else if (dragHandle.corner === 'tr') {
        newRect = {
          x: cur.x,
          y: clampedY,
          w: clampedX - cur.x,
          h: cur.y + cur.h - clampedY
        };
      } else if (dragHandle.corner === 'br') {
        newRect = {
          x: cur.x,
          y: cur.y,
          w: clampedX - cur.x,
          h: clampedY - cur.y
        };
      } else if (dragHandle.corner === 'bl') {
        newRect = {
          x: clampedX,
          y: cur.y,
          w: cur.x + cur.w - clampedX,
          h: clampedY - cur.y
        };
      }
      if (newRect.w > 20 && newRect.h > 20) {
        updateActiveSlice({ inputRect: newRect });
      }
    } else if (dragHandle.type === 'maskPoint' && activeMask && dragHandle.maskPointIndex !== undefined) {
      const updatedPoints = [...activeMask.points];
      updatedPoints[dragHandle.maskPointIndex] = { x: clampedX, y: clampedY };
      const updatedMasks = activeSlice.masks.map((m) =>
        m.id === activeMask.id ? { ...m, points: updatedPoints } : m
      );
      updateActiveSlice({ masks: updatedMasks });
    }
  };

  const handleMouseUp = () => {
    setDragHandle(null);
  };

  return (
    <div
      className="flex-grow flex w-full h-full bg-[#0a0a0a] overflow-hidden select-none"
      onMouseMove={handleMouseMove}
      onMouseUp={handleMouseUp}
    >
      {/* LEFT: Hierarchy Tree (Screens -> Slices -> Masks) */}
      <div className="w-[240px] flex flex-col bg-[#121212] border-r border-[#2a2a2a] shrink-0">
        <div className="p-3 border-b border-[#2a2a2a] flex items-center justify-between bg-[#181818]">
          <span className="text-[10px] font-bold tracking-widest text-[#E0E0E0]">
            {t.mappingTree}
          </span>
          <FolderTree size={13} className="text-[#888]" />
        </div>

        <div className="p-2.5 flex-grow overflow-y-auto space-y-3">
          {/* Screens List */}
          {screens.map((screen) => (
            <div key={screen.id} className="space-y-1">
              <div
                onClick={() => {
                  setActiveScreenId(screen.id);
                  if (!screen.slices.some((sl) => sl.id === activeSliceId)) {
                    setActiveSliceId(screen.slices[0]?.id || '');
                    setActiveMaskId(null);
                  }
                }}
                className={`group flex items-center justify-between p-2 rounded text-xs font-bold cursor-pointer transition-colors ${
                  activeScreenId === screen.id
                    ? 'bg-[#222] text-[#FF7F50] border border-[#383838]'
                    : 'text-[#aaa] hover:bg-[#181818]'
                }`}
              >
                <div className="flex items-center gap-2 truncate">
                  <Monitor size={12} className={activeScreenId === screen.id ? 'text-[#FF7F50]' : 'text-[#888]'} />
                  <span className="truncate">{screen.name}</span>
                </div>
                <div className="flex items-center gap-1.5 shrink-0">
                  <span className="text-[9px] font-mono text-[#666]">
                    {screen.resolution.w}x{screen.resolution.h}
                  </span>
                  {screens.length > 1 && (
                    <button
                      onClick={(e) => handleDeleteScreen(screen.id, e)}
                      title="Delete Screen"
                      className="opacity-0 group-hover:opacity-100 p-0.5 text-[#666] hover:text-[#FF5555] transition-opacity cursor-pointer"
                    >
                      <Trash2 size={11} />
                    </button>
                  )}
                </div>
              </div>

              {/* Slices of this Screen */}
              <div className="pl-4 space-y-1">
                {screen.slices.map((slice) => {
                  const isSliceActive = activeSliceId === slice.id;
                  return (
                    <div key={slice.id} className="space-y-0.5">
                      <div
                        onClick={() => {
                          setActiveSliceId(slice.id);
                          setActiveMaskId(null);
                        }}
                        className={`flex items-center justify-between py-1.5 px-2 rounded text-[11px] cursor-pointer transition-colors ${
                          isSliceActive
                            ? 'bg-[#FF7F50]/15 text-[#FF7F50] font-bold border border-[#FF7F50]/30'
                            : 'text-[#888] hover:text-white hover:bg-[#181818]'
                        }`}
                      >
                        <div className="flex items-center gap-1.5 truncate">
                          <Maximize size={11} className="text-[#118AB2]" />
                          <span className="truncate">{slice.name}</span>
                        </div>
                        <div className="flex items-center gap-1">
                          <button
                            onClick={(e) => {
                              e.stopPropagation();
                              updateActiveSlice({ visible: !slice.visible });
                            }}
                            className="text-[#666] hover:text-white"
                          >
                            {slice.visible ? <Eye size={11} /> : <EyeOff size={11} />}
                          </button>
                        </div>
                      </div>

                      {/* Masks of this Slice */}
                      {slice.masks.map((mask) => (
                        <div
                          key={mask.id}
                          onClick={() => {
                            setActiveSliceId(slice.id);
                            setActiveMaskId(mask.id);
                          }}
                          className={`pl-5 pr-2 py-1 flex items-center justify-between rounded text-[10px] cursor-pointer transition-colors ${
                            activeMaskId === mask.id
                              ? 'bg-[#FFD166]/15 text-[#FFD166] font-bold'
                              : 'text-[#777] hover:text-white'
                          }`}
                        >
                          <div className="flex items-center gap-1.5 truncate">
                            <Scissors size={10} className="text-[#FFD166]" />
                            <span className="truncate">{mask.name}</span>
                          </div>
                        </div>
                      ))}
                    </div>
                  );
                })}
              </div>
            </div>
          ))}
        </div>

        {/* Action buttons (pinned at bottom corner) */}
        <div className="p-2.5 border-t border-[#222] bg-[#141414] space-y-1.5 shrink-0">
          <button
            onClick={handleAddScreen}
            className="w-full flex items-center justify-center gap-1.5 py-1.5 rounded bg-[#FF7F50]/15 hover:bg-[#FF7F50]/25 text-[#FF7F50] text-[10.5px] font-bold border border-[#FF7F50]/40 transition-all shadow-xs cursor-pointer active:scale-98"
          >
            <Monitor size={12} />
            <span>{t.addScreen}</span>
          </button>
          <button
            onClick={handleAddSlice}
            className="w-full flex items-center justify-center gap-1.5 py-1.5 rounded bg-[#1c1c1c] hover:bg-[#252525] text-white text-[10px] font-bold border border-[#333] transition-colors cursor-pointer"
          >
            <Plus size={11} />
            <span>{t.addSlice}</span>
          </button>
          <button
            onClick={handleAddMask}
            className="w-full flex items-center justify-center gap-1.5 py-1.5 rounded bg-[#1c1c1c] hover:bg-[#252525] text-[#FFD166] text-[10px] font-bold border border-[#333] transition-colors cursor-pointer"
          >
            <Scissors size={11} />
            <span>{t.addMask}</span>
          </button>
        </div>
      </div>

      {/* CENTER: Mapping Stage */}
      <div className="flex-grow flex flex-col bg-[#050505] overflow-hidden">
        {/* Stage Toolbar */}
        <div className="h-11 bg-[#121212] border-b border-[#2a2a2a] flex items-center justify-between px-3 md:px-4 shrink-0 gap-3 overflow-hidden">
          <div className="flex items-center gap-2 min-w-0 flex-1 overflow-hidden">
            {/* Input vs Output Switch */}
            <div className="flex bg-black p-0.5 rounded-lg border border-[#2a2a2a] shrink-0">
              <button
                onClick={() => setMappingMode('input')}
                className={`px-3 py-1 rounded text-xs font-bold transition-all cursor-pointer whitespace-nowrap ${
                  mappingMode === 'input'
                    ? 'bg-[#118AB2] text-white shadow-sm'
                    : 'text-[#777] hover:text-white'
                }`}
              >
                {t.inputSelection}
              </button>
              <button
                onClick={() => setMappingMode('output')}
                className={`px-3 py-1 rounded text-xs font-bold transition-all cursor-pointer whitespace-nowrap ${
                  mappingMode === 'output'
                    ? 'bg-[#FF7F50] text-white shadow-sm'
                    : 'text-[#777] hover:text-white'
                }`}
              >
                {t.outputRouting}
              </button>
            </div>

            <div className="h-4 w-px bg-[#2a2a2a] shrink-0" />

            {/* Screen switcher pills if multiple screens - horizontally scrollable without overflowing */}
            {screens.length > 1 && (
              <div
                ref={screenTabsRef}
                onWheel={(e) => {
                  if (screenTabsRef.current) {
                    screenTabsRef.current.scrollLeft += e.deltaY;
                  }
                }}
                className="flex items-center gap-1 min-w-0 max-w-[200px] sm:max-w-[280px] md:max-w-[380px] lg:max-w-[500px] overflow-x-auto bg-black p-0.5 rounded-lg border border-[#2a2a2a] shrink [scrollbar-width:none] [-ms-overflow-style:none] [&::-webkit-scrollbar]:hidden"
              >
                {screens.map((sc) => {
                  const isCurrent = activeScreenId === sc.id;
                  return (
                    <button
                      key={sc.id}
                      data-active={isCurrent ? 'true' : 'false'}
                      onClick={() => {
                        setActiveScreenId(sc.id);
                        setActiveSliceId(sc.slices[0]?.id || '');
                        setActiveMaskId(null);
                      }}
                      className={`px-2.5 py-0.5 rounded text-[10.5px] font-mono font-bold transition-all cursor-pointer whitespace-nowrap shrink-0 ${
                        isCurrent
                          ? 'bg-[#FF7F50] text-black shadow-xs font-black'
                          : 'text-[#888] hover:text-white hover:bg-[#1a1a1a]'
                      }`}
                      title={`${sc.name} (${sc.outputDevice})`}
                    >
                      {sc.name}
                    </button>
                  );
                })}
              </div>
            )}

            <span className="text-xs font-mono text-[#888] ml-1 truncate shrink min-w-0 hidden xl:inline-block">
              {mappingMode === 'input' ? 'Source Content: 1920x1080' : `${activeScreen.name} (${activeScreen.outputDevice})`}
            </span>
          </div>

          <div className="flex items-center gap-2 shrink-0">
            <button
              onClick={handleResetWarp}
              className="flex items-center gap-1.5 px-2.5 py-1 rounded bg-[#1a1a1a] text-[#888] hover:text-white hover:bg-[#252525] text-xs font-medium border border-[#333] transition-colors whitespace-nowrap cursor-pointer"
              title="Reset Keystone Corner Pin"
            >
              <RotateCcw size={12} />
              <span className="hidden sm:inline">Reset Warp</span>
            </button>

            {activeSlice && (
              <button
                onClick={() => handleDeleteSlice(activeSlice.id)}
                className="p-1 rounded bg-[#1a1a1a] text-[#888] hover:text-red-400 hover:bg-red-950/30 border border-[#333] transition-colors cursor-pointer"
                title="Delete Selected Slice"
              >
                <Trash2 size={13} />
              </button>
            )}
          </div>
        </div>

        {/* Stage Interactive Canvas Container */}
        <div className="flex-grow p-4 flex items-center justify-center relative overflow-hidden">
          {/* 16:9 Aspect Ratio Display Stage */}
          <div
            ref={stageRef}
            className="w-full max-w-[960px] aspect-video bg-[#0d0d0d] border border-[#2a2a2a] relative overflow-hidden shadow-2xl rounded"
          >
            {/* Background live canvas */}
            <div className="absolute inset-0 opacity-40">
              <LiveCanvas
                layers={layers}
                clips={clips}
                showTestGrid={showTestGrid}
                interactive={false}
              />
            </div>

            {/* Stage Grid Lines */}
            <svg className="absolute inset-0 w-full h-full pointer-events-none opacity-20">
              <defs>
                <pattern id="stage-grid" width="40" height="40" patternUnits="userSpaceOnUse">
                  <path d="M 40 0 L 0 0 0 40" fill="none" stroke="#555" strokeWidth="0.5" />
                </pattern>
              </defs>
              <rect width="100%" height="100%" fill="url(#stage-grid)" />
            </svg>

            {/* MODE 1: INPUT SELECTION OVERLAY */}
            {mappingMode === 'input' && activeSlice && (
              <div className="absolute inset-0 pointer-events-none">
                {/* Highlighted Input Rect */}
                <div
                  className="absolute border-2 border-[#118AB2] bg-[#118AB2]/15 pointer-events-auto"
                  style={{
                    left: `${(activeSlice.inputRect.x / 1920) * 100}%`,
                    top: `${(activeSlice.inputRect.y / 1080) * 100}%`,
                    width: `${(activeSlice.inputRect.w / 1920) * 100}%`,
                    height: `${(activeSlice.inputRect.h / 1080) * 100}%`
                  }}
                >
                  <div className="absolute top-1 left-1 bg-[#118AB2] text-white text-[9px] font-mono font-bold px-1.5 py-0.5 rounded shadow">
                    {activeSlice.name} ({activeSlice.inputRect.w}x{activeSlice.inputRect.h})
                  </div>

                  {/* Handles */}
                  {(['tl', 'tr', 'br', 'bl'] as const).map((corner) => {
                    const pos =
                      corner === 'tl'
                        ? 'top-0 left-0 -translate-x-1/2 -translate-y-1/2'
                        : corner === 'tr'
                        ? 'top-0 right-0 translate-x-1/2 -translate-y-1/2'
                        : corner === 'br'
                        ? 'bottom-0 right-0 translate-x-1/2 translate-y-1/2'
                        : 'bottom-0 left-0 -translate-x-1/2 translate-y-1/2';
                    return (
                      <div
                        key={corner}
                        onMouseDown={(e) => {
                          e.stopPropagation();
                          setDragHandle({ type: 'input', corner });
                        }}
                        className={`absolute w-3 h-3 bg-white border-2 border-[#118AB2] rounded-xs shadow cursor-pointer ${pos}`}
                      />
                    );
                  })}
                </div>
              </div>
            )}

            {/* MODE 2: OUTPUT ROUTING (KEYSTONE CORNER PIN WARPING) */}
            {mappingMode === 'output' && activeSlice && (
              <svg className="absolute inset-0 w-full h-full pointer-events-auto overflow-visible" viewBox="0 0 1920 1080">
                {/* Warped Quad Polygon */}
                {activeScreen.slices.map((slice) => {
                  const isSelected = slice.id === activeSlice.id;
                  const q = slice.outputQuad;
                  const pointsStr = `${q.tl[0]},${q.tl[1]} ${q.tr[0]},${q.tr[1]} ${q.br[0]},${q.br[1]} ${q.bl[0]},${q.bl[1]}`;

                  return (
                    <g key={slice.id}>
                      <polygon
                        points={pointsStr}
                        fill={isSelected ? 'rgba(255, 127, 80, 0.22)' : 'rgba(255, 255, 255, 0.05)'}
                        stroke={isSelected ? '#FF7F50' : '#444'}
                        strokeWidth={isSelected ? 3 : 1.5}
                        strokeDasharray={isSelected ? 'none' : '6 3'}
                        className="cursor-pointer"
                        onClick={() => setActiveSliceId(slice.id)}
                      />

                      {/* Slice label */}
                      <text
                        x={(q.tl[0] + q.tr[0]) / 2}
                        y={(q.tl[1] + q.bl[1]) / 2}
                        fill={isSelected ? '#FF7F50' : '#888'}
                        fontSize="22"
                        fontFamily="monospace"
                        fontWeight="bold"
                        textAnchor="middle"
                      >
                        {slice.name}
                      </text>

                      {/* Corner Drag Handles */}
                      {isSelected && (
                        <>
                          {([
                            { key: 'tl', p: q.tl, label: 'TL' },
                            { key: 'tr', p: q.tr, label: 'TR' },
                            { key: 'br', p: q.br, label: 'BR' },
                            { key: 'bl', p: q.bl, label: 'BL' }
                          ] as const).map((handle) => (
                            <g
                              key={handle.key}
                              className="cursor-move"
                              onMouseDown={(e) => {
                                e.stopPropagation();
                                setDragHandle({ type: 'corner', corner: handle.key });
                              }}
                            >
                              <circle
                                cx={handle.p[0]}
                                cy={handle.p[1]}
                                r="16"
                                fill="#FF7F50"
                                stroke="#FFFFFF"
                                strokeWidth="3"
                                className="filter drop-shadow-[0_0_8px_rgba(255,127,80,0.8)]"
                              />
                              <text
                                x={handle.p[0]}
                                y={handle.p[1] + 28}
                                fill="#FFFFFF"
                                fontSize="16"
                                fontFamily="monospace"
                                fontWeight="bold"
                                textAnchor="middle"
                              >
                                {handle.label} ({handle.p[0]},{handle.p[1]})
                              </text>
                            </g>
                          ))}
                        </>
                      )}

                      {/* Masks of slice */}
                      {slice.masks.map((mask) => {
                        const maskPts = mask.points.map((p) => `${p.x},${p.y}`).join(' ');
                        const isMaskSelected = activeMaskId === mask.id;
                        return (
                          <g key={mask.id}>
                            <polygon
                              points={maskPts}
                              fill={isMaskSelected ? 'rgba(255, 209, 102, 0.3)' : 'rgba(255, 0, 0, 0.2)'}
                              stroke={isMaskSelected ? '#FFD166' : '#FF5555'}
                              strokeWidth={2}
                              strokeDasharray="4 4"
                              onClick={() => {
                                setActiveSliceId(slice.id);
                                setActiveMaskId(mask.id);
                              }}
                            />
                            {isMaskSelected &&
                              mask.points.map((pt, idx) => (
                                <circle
                                  key={idx}
                                  cx={pt.x}
                                  cy={pt.y}
                                  r="12"
                                  fill="#FFD166"
                                  stroke="#FFFFFF"
                                  strokeWidth="2"
                                  className="cursor-move"
                                  onMouseDown={(e) => {
                                    e.stopPropagation();
                                    setDragHandle({ type: 'maskPoint', maskPointIndex: idx });
                                  }}
                                />
                              ))}
                          </g>
                        );
                      })}
                    </g>
                  );
                })}
              </svg>
            )}
          </div>
        </div>
      </div>

      {/* RIGHT: Properties Panel */}
      <div className="w-[280px] flex flex-col bg-[#121212] border-l border-[#2a2a2a] shrink-0">
        <div className="p-3 border-b border-[#2a2a2a] bg-[#181818]">
          <span className="text-[10px] font-bold tracking-widest text-[#E0E0E0] uppercase">
            {activeMask ? t.maskProps : t.sliceProps}
          </span>
        </div>

        <div className="p-3.5 flex-grow overflow-y-auto space-y-4">
          {activeSlice && !activeMask && (
            <>
              {/* Slice Name */}
              <div>
                <label className="text-[10px] font-bold text-[#888] block mb-1">SLICE NAME</label>
                <input
                  type="text"
                  value={activeSlice.name}
                  onChange={(e) => updateActiveSlice({ name: e.target.value })}
                  className="w-full bg-[#0a0a0a] border border-[#333] rounded px-2.5 py-1.5 text-xs text-white focus:border-[#FF7F50] outline-hidden"
                />
              </div>

              {/* Mode indicator */}
              <div>
                <label className="text-[10px] font-bold text-[#888] block mb-1">WARP MODE</label>
                <div className="grid grid-cols-2 gap-1.5">
                  <button
                    onClick={() => updateActiveSlice({ warpMode: 'cornerPin' })}
                    className={`py-1.5 px-2 rounded text-[10px] font-mono font-bold border transition-colors ${
                      activeSlice.warpMode === 'cornerPin'
                        ? 'bg-[#FF7F50]/20 text-[#FF7F50] border-[#FF7F50]/50'
                        : 'bg-[#181818] text-[#777] border-[#2a2a2a]'
                    }`}
                  >
                    4-POINT KEYSTONE
                  </button>
                  <button
                    onClick={() => updateActiveSlice({ warpMode: 'mesh3x3' })}
                    className={`py-1.5 px-2 rounded text-[10px] font-mono font-bold border transition-colors ${
                      activeSlice.warpMode === 'mesh3x3'
                        ? 'bg-[#FF7F50]/20 text-[#FF7F50] border-[#FF7F50]/50'
                        : 'bg-[#181818] text-[#777] border-[#2a2a2a]'
                    }`}
                  >
                    3x3 MESH WARP
                  </button>
                </div>
              </div>

              {/* Input Rect Coordinates */}
              <div className="pt-2 border-t border-[#222]">
                <div className="text-[10px] font-bold text-[#888] mb-1.5">INPUT RECTANGLE (PX)</div>
                <div className="grid grid-cols-2 gap-2 text-xs font-mono">
                  <div>
                    <span className="text-[9px] text-[#666] block">X</span>
                    <input
                      type="number"
                      value={activeSlice.inputRect.x}
                      onChange={(e) =>
                        updateActiveSlice({
                          inputRect: { ...activeSlice.inputRect, x: parseInt(e.target.value) || 0 }
                        })
                      }
                      className="w-full bg-[#0a0a0a] border border-[#333] rounded px-2 py-1 text-white"
                    />
                  </div>
                  <div>
                    <span className="text-[9px] text-[#666] block">Y</span>
                    <input
                      type="number"
                      value={activeSlice.inputRect.y}
                      onChange={(e) =>
                        updateActiveSlice({
                          inputRect: { ...activeSlice.inputRect, y: parseInt(e.target.value) || 0 }
                        })
                      }
                      className="w-full bg-[#0a0a0a] border border-[#333] rounded px-2 py-1 text-white"
                    />
                  </div>
                  <div>
                    <span className="text-[9px] text-[#666] block">WIDTH</span>
                    <input
                      type="number"
                      value={activeSlice.inputRect.w}
                      onChange={(e) =>
                        updateActiveSlice({
                          inputRect: { ...activeSlice.inputRect, w: parseInt(e.target.value) || 10 }
                        })
                      }
                      className="w-full bg-[#0a0a0a] border border-[#333] rounded px-2 py-1 text-white"
                    />
                  </div>
                  <div>
                    <span className="text-[9px] text-[#666] block">HEIGHT</span>
                    <input
                      type="number"
                      value={activeSlice.inputRect.h}
                      onChange={(e) =>
                        updateActiveSlice({
                          inputRect: { ...activeSlice.inputRect, h: parseInt(e.target.value) || 10 }
                        })
                      }
                      className="w-full bg-[#0a0a0a] border border-[#333] rounded px-2 py-1 text-white"
                    />
                  </div>
                </div>
              </div>

              {/* Output Quad Corner Pin coordinates */}
              <div className="pt-2 border-t border-[#222]">
                <div className="text-[10px] font-bold text-[#888] mb-1.5">CORNER PINS (H_w)</div>
                <div className="space-y-1.5 text-[10px] font-mono">
                  {(['tl', 'tr', 'br', 'bl'] as const).map((c) => (
                    <div key={c} className="flex items-center justify-between bg-[#181818] p-1.5 rounded border border-[#222]">
                      <span className="text-[#FF7F50] font-bold uppercase">{c}</span>
                      <span className="text-[#aaa]">
                        X: {activeSlice.outputQuad[c][0]} | Y: {activeSlice.outputQuad[c][1]}
                      </span>
                    </div>
                  ))}
                </div>
              </div>
            </>
          )}

          {/* Mask properties if mask selected */}
          {activeMask && activeSlice && (
            <div className="space-y-4">
              <div>
                <label className="text-[10px] font-bold text-[#888] block mb-1">MASK NAME</label>
                <input
                  type="text"
                  value={activeMask.name}
                  onChange={(e) => {
                    const updated = activeSlice.masks.map((m) =>
                      m.id === activeMask.id ? { ...m, name: e.target.value } : m
                    );
                    updateActiveSlice({ masks: updated });
                  }}
                  className="w-full bg-[#0a0a0a] border border-[#333] rounded px-2.5 py-1.5 text-xs text-white"
                />
              </div>

              <label className="flex items-center justify-between text-xs text-[#E0E0E0] cursor-pointer pt-2 border-t border-[#222]">
                <span>{t.invertMask}</span>
                <input
                  type="checkbox"
                  checked={activeMask.inverted}
                  onChange={(e) => {
                    const updated = activeSlice.masks.map((m) =>
                      m.id === activeMask.id ? { ...m, inverted: e.target.checked } : m
                    );
                    updateActiveSlice({ masks: updated });
                  }}
                  className="accent-[#FFD166]"
                />
              </label>

              <div>
                <div className="flex justify-between text-[10px] text-[#888] mb-1 font-bold">
                  <span>{t.feather}</span>
                  <span className="font-mono text-[#FFD166]">{activeMask.feather}px</span>
                </div>
                <input
                  type="range"
                  min="0"
                  max="40"
                  value={activeMask.feather}
                  onChange={(e) => {
                    const updated = activeSlice.masks.map((m) =>
                      m.id === activeMask.id ? { ...m, feather: parseInt(e.target.value) } : m
                    );
                    updateActiveSlice({ masks: updated });
                  }}
                  className="w-full accent-[#FFD166] h-1.5 bg-[#050505] rounded-lg cursor-pointer"
                />
              </div>

              <button
                onClick={() => {
                  const updated = activeSlice.masks.filter((m) => m.id !== activeMask.id);
                  updateActiveSlice({ masks: updated });
                  setActiveMaskId(null);
                }}
                className="w-full py-1.5 rounded bg-red-950/40 text-red-400 hover:bg-red-900/60 border border-red-800/50 text-xs font-bold transition-colors"
              >
                Delete Mask
              </button>
            </div>
          )}

          {/* Screen Output Info */}
          <div className="pt-3 border-t border-[#222]">
            <div className="text-[10px] font-bold text-[#888] mb-1.5 uppercase flex items-center justify-between">
              <span>{t.screenProps}</span>
              <span className="text-[9px] font-mono text-[#06D6A0]">{activeScreen.name}</span>
            </div>
            <div className="space-y-2 text-[10px] font-mono text-[#888] bg-[#181818] p-2.5 rounded border border-[#222]">
              <div>
                <label className="text-[9px] text-[#666] block mb-0.5 font-sans">SCREEN NAME</label>
                <input
                  type="text"
                  value={activeScreen.name}
                  onChange={(e) => updateActiveScreen({ name: e.target.value })}
                  className="w-full bg-[#0a0a0a] border border-[#333] rounded px-2 py-1 text-white text-xs font-sans focus:border-[#FF7F50] outline-hidden"
                />
              </div>
              <div>
                <label className="text-[9px] text-[#666] block mb-0.5 font-sans">{t.outputDevice}</label>
                <input
                  type="text"
                  value={activeScreen.outputDevice}
                  onChange={(e) => updateActiveScreen({ outputDevice: e.target.value })}
                  className="w-full bg-[#0a0a0a] border border-[#333] rounded px-2 py-1 text-white text-xs font-sans focus:border-[#FF7F50] outline-hidden"
                />
              </div>
              <div className="text-[9px] text-[#aaa]">
                RES: {activeScreen.resolution.w} x {activeScreen.resolution.h} @ {activeScreen.fps}Hz
              </div>
              <div className="flex items-center justify-between pt-1 border-t border-[#2a2a2a]">
                <span className="font-sans">{t.edgeBlending}</span>
                <button
                  onClick={() => updateActiveScreen({ edgeBlending: !activeScreen.edgeBlending })}
                  className={`px-2 py-0.5 rounded text-[9px] font-bold cursor-pointer transition-colors ${
                    activeScreen.edgeBlending
                      ? 'bg-[#06D6A0]/20 text-[#06D6A0] border border-[#06D6A0]/40'
                      : 'bg-[#222] text-[#666] border border-[#333]'
                  }`}
                >
                  {activeScreen.edgeBlending ? 'ENABLED' : 'DISABLED'}
                </button>
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
};
