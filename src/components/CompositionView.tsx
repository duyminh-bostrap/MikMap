import React, { useState } from 'react';
import { 
  FolderOpen, 
  ChevronRight,
  ChevronDown,
  Sliders, 
  Monitor, 
  Upload, 
  Sparkles,
  Volume2,
  Play
} from 'lucide-react';
import { Layer, Clip, BlendMode, Language, SensorPoint } from '../types';
import { translations } from '../utils/localization';
import { LiveCanvas } from './LiveCanvas';

interface CompositionViewProps {
  layers: Layer[];
  setLayers: React.Dispatch<React.SetStateAction<Layer[]>>;
  clips: Record<number, Record<number, Clip>>;
  setClips: React.Dispatch<React.SetStateAction<Record<number, Record<number, Clip>>>>;
  lang: Language;
  activeTouchPoints: SensorPoint[];
  showTestGrid: boolean;
}

export const CompositionView: React.FC<CompositionViewProps> = ({
  layers,
  setLayers,
  clips,
  setClips,
  lang,
  activeTouchPoints,
  showTestGrid
}) => {
  const t = translations[lang];
  const [activeLayerId, setActiveLayerId] = useState<number>(3);
  const [selectedClipCoords, setSelectedClipCoords] = useState<{ layerId: number; colId: number } | null>({
    layerId: 3,
    colId: 2
  });
  const [propTab, setPropTab] = useState<'clip' | 'layer'>('layer');
  const [collapsedLayers, setCollapsedLayers] = useState<Record<number, boolean>>({});

  const activeLayer = layers.find((l) => l.id === activeLayerId) || layers[0];
  const selectedClip = selectedClipCoords
    ? clips[selectedClipCoords.layerId]?.[selectedClipCoords.colId]
    : null;

  // Layer property updater
  const updateActiveLayer = (updates: Partial<Layer>) => {
    setLayers((prev) =>
      prev.map((l) => (l.id === activeLayerId ? { ...l, ...updates } : l))
    );
  };

  // Clip property updater
  const updateSelectedClip = (updates: Partial<Clip>) => {
    if (!selectedClipCoords) return;
    const { layerId, colId } = selectedClipCoords;
    setClips((prev) => {
      const layerClips = prev[layerId] || {};
      const current = layerClips[colId];
      if (!current) return prev;
      return {
        ...prev,
        [layerId]: {
          ...layerClips,
          [colId]: { ...current, ...updates }
        }
      };
    });
  };

  // Trigger single clip or column on double click
  const handleClipDoubleClick = (_layerId: number, colId: number) => {
    handleColumnTrigger(colId);
  };

  // Trigger entire column across all layers (guarantees exactly 1 column is LIVE)
  const handleColumnTrigger = (colId: number) => {
    setLayers((prev) =>
      prev.map((l) => ({
        ...l,
        activeCol: colId // Every layer selects colId, even if its clip slot is empty
      }))
    );
    setClips((prev) => {
      const next: Record<number, Record<number, Clip>> = {};
      Object.keys(prev).forEach((lIdStr) => {
        const lId = Number(lIdStr);
        next[lId] = {};
        Object.keys(prev[lId]).forEach((cIdStr) => {
          const cId = Number(cIdStr);
          const clip = prev[lId][cId];
          next[lId][cId] = {
            ...clip,
            active: clip.loaded && cId === colId
          };
        });
      });
      return next;
    });
  };

  // Custom media upload handler
  const handleFileUpload = (e: React.ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0];
    if (!file || !selectedClipCoords) return;
    const { layerId, colId } = selectedClipCoords;
    const isImage = file.type.startsWith('image/');
    
    updateSelectedClip({
      name: file.name.replace(/\.[^/.]+$/, ''),
      loaded: true,
      category: isImage ? 'Custom Images' : 'Custom Loops',
      color: '#06D6A0'
    });
  };

  const blendModes: BlendMode[] = [
    'ALPHA',
    'ADDITIVE',
    'SCREEN',
    'MULTIPLY',
    'LIGHTEN',
    'DIFFERENCE'
  ];

  return (
    <div className="flex-grow flex flex-col w-full h-full bg-[#0a0a0a] overflow-hidden">
      {/* TOP HALF: Browser + Monitors + Properties */}
      <div className="flex h-[48%] min-h-[290px] w-full shrink-0 border-b border-[#2a2a2a]">
        {/* Left Browser: Files & Effects */}
        <div className="w-[230px] flex flex-col bg-[#121212] border-r border-[#2a2a2a] shrink-0">
          <div className="p-2.5 border-b border-[#2a2a2a] flex items-center justify-between bg-[#181818]">
            <span className="text-[10px] font-bold tracking-widest text-[#E0E0E0]">
              {t.filesAndFx}
            </span>
            <FolderOpen size={13} className="text-[#888]" />
          </div>

          <div className="p-2.5 flex-grow overflow-y-auto space-y-1">
            {/* Media folders */}
            <div className="text-[9px] font-bold text-[#555] uppercase tracking-wider px-1 py-0.5">
              Media Sources
            </div>
            {[
              { name: 'Generators', count: 6, color: '#FF7F50' },
              { name: 'VJ_Loops_4K', count: 12, color: '#FFD166' },
              { name: 'Audio_Reactive', count: 4, color: '#06D6A0' },
              { name: 'Masks_Stencils', count: 5, color: '#118AB2' }
            ].map((folder, i) => (
              <div
                key={i}
                className="flex items-center justify-between py-1.5 px-2 rounded text-[11px] text-[#888] hover:text-white hover:bg-[#1c1c1c] cursor-pointer transition-colors"
              >
                <div className="flex items-center gap-2 truncate">
                  <ChevronRight size={12} className="text-[#555]" />
                  <FolderOpen size={12} style={{ color: folder.color }} />
                  <span className="truncate">{folder.name}</span>
                </div>
                <span className="text-[9px] font-mono text-[#555]">{folder.count}</span>
              </div>
            ))}

            {/* Video effects */}
            <div className="mt-3 pt-2.5 border-t border-[#222]">
              <div className="text-[9px] font-bold text-[#555] uppercase tracking-wider px-1 mb-1">
                {t.videoFx}
              </div>
              {[
                { name: 'Colorize', color: '#FF7F50' },
                { name: 'Glow Bloom', color: '#FFD166' },
                { name: 'Hue Rotate', color: '#06D6A0' },
                { name: 'Pixelate FX', color: '#118AB2' }
              ].map((fx, i) => (
                <div
                  key={i}
                  className="flex items-center gap-2 py-1 px-2 text-[11px] text-[#999] hover:text-white hover:bg-[#1a1a1a] rounded cursor-pointer transition-colors"
                >
                  <Sliders size={11} style={{ color: fx.color }} />
                  <span>{fx.name}</span>
                </div>
              ))}
            </div>

            {/* Upload media file */}
            <div className="mt-3 pt-2">
              <label className="flex items-center justify-center gap-2 py-2 px-3 rounded border border-dashed border-[#333] hover:border-[#FF7F50] bg-[#161616] text-[#999] hover:text-white text-[10px] font-bold cursor-pointer transition-colors">
                <Upload size={12} className="text-[#FF7F50]" />
                <span>Import Video / Image</span>
                <input
                  type="file"
                  accept="image/*,video/*"
                  onChange={handleFileUpload}
                  className="hidden"
                />
              </label>
            </div>
          </div>
        </div>

        {/* Center: Dual Monitors */}
        <div className="flex-grow flex bg-[#050505] p-2.5 gap-3">
          {/* PREVIEW Monitor */}
          <div className="flex-1 flex flex-col bg-[#121212] border border-[#2a2a2a] rounded-lg overflow-hidden shadow-sm">
            <div className="h-6 bg-[#181818] flex items-center justify-between px-3 border-b border-[#2a2a2a] border-t-2 border-t-[#118AB2]">
              <span className="text-[9px] font-bold tracking-widest text-[#118AB2]">
                {t.preview}
              </span>
              <span className="text-[9px] font-mono text-[#666]">
                {selectedClip ? `${selectedClip.name} (${selectedClip.category})` : 'NO CLIP SELECTED'}
              </span>
            </div>
            <div className="flex-grow relative flex items-center justify-center p-3 bg-[#080808]">
              {selectedClip ? (
                <div className="w-full h-full border border-[#333] rounded flex flex-col items-center justify-center gap-2 relative overflow-hidden bg-black">
                  <div
                    className="absolute inset-0 opacity-40 mix-blend-screen"
                    style={{
                      background: `radial-gradient(circle, ${selectedClip.color} 0%, transparent 70%)`
                    }}
                  />
                  <div className="z-10 flex flex-col items-center gap-1">
                    <Monitor size={28} style={{ color: selectedClip.color }} />
                    <span className="text-xs font-bold text-white">{selectedClip.name}</span>
                    <span className="text-[9px] font-mono text-[#888]">
                      {selectedClip.loopMode.toUpperCase()} @ {selectedClip.speed}x
                    </span>
                  </div>
                </div>
              ) : (
                <div className="w-full h-full border border-[#222] border-dashed rounded flex flex-col items-center justify-center gap-1.5 text-[#444]">
                  <Monitor size={24} />
                  <span className="font-mono text-[10px]">{t.selectClipPrompt}</span>
                </div>
              )}
            </div>
          </div>

          {/* LIVE OUTPUT Monitor */}
          <div className="flex-1 flex flex-col bg-[#121212] border border-[#2a2a2a] rounded-lg overflow-hidden shadow-lg">
            <div className="h-6 bg-[#181818] flex items-center justify-between px-3 border-b border-[#2a2a2a] border-t-2 border-t-[#FF7F50]">
              <div className="flex items-center gap-2">
                <div className="w-2 h-2 rounded-full bg-[#FF7F50] animate-pulse shadow-[0_0_6px_#FF7F50]" />
                <span className="text-[9px] font-bold tracking-widest text-[#FF7F50]">
                  {t.liveOutput}
                </span>
              </div>
              <span className="text-[9px] font-mono text-[#888]">COMPOSITION 1920x1080</span>
            </div>
            <div className="flex-grow relative flex items-center justify-center p-1 bg-black overflow-hidden">
              <LiveCanvas
                layers={layers}
                clips={clips}
                activeTouchPoints={activeTouchPoints}
                showTestGrid={showTestGrid}
                interactive={true}
              />
            </div>
          </div>
        </div>

        {/* Right Properties Panel */}
        <div className="w-[260px] flex flex-col bg-[#121212] border-l border-[#2a2a2a] shrink-0">
          <div className="flex bg-[#181818] border-b border-[#2a2a2a]">
            <button
              onClick={() => setPropTab('clip')}
              className={`flex-1 py-2 text-[10px] font-bold tracking-wider transition-colors ${
                propTab === 'clip'
                  ? 'text-[#FFD166] border-b-2 border-[#FFD166] bg-[#202020]'
                  : 'text-[#666] hover:bg-[#141414]'
              }`}
            >
              {t.clipProps}
            </button>
            <button
              onClick={() => setPropTab('layer')}
              className={`flex-1 py-2 text-[10px] font-bold tracking-wider transition-colors ${
                propTab === 'layer'
                  ? 'text-[#FF7F50] border-b-2 border-[#FF7F50] bg-[#202020]'
                  : 'text-[#666] hover:bg-[#141414]'
              }`}
            >
              {t.layerProps} {activeLayerId}
            </button>
          </div>

          <div className="p-3.5 flex-grow overflow-y-auto space-y-4">
            {propTab === 'layer' ? (
              /* Layer Controls */
              <>
                <div>
                  <div className="flex justify-between text-[10px] text-[#888] mb-1 font-bold">
                    <span className="text-[#FF7F50]">VISUAL OPACITY (V)</span>
                    <span className="font-mono text-[#E0E0E0]">
                      {Math.round(activeLayer.opacity * 100)}%
                    </span>
                  </div>
                  <input
                    type="range"
                    min="0"
                    max="1"
                    step="0.01"
                    value={activeLayer.opacity}
                    onChange={(e) => updateActiveLayer({ opacity: parseFloat(e.target.value) })}
                    className="w-full accent-[#FF7F50] h-1.5 bg-[#050505] rounded-lg cursor-pointer"
                  />
                </div>

                <div>
                  <div className="flex justify-between text-[10px] text-[#888] mb-1 font-bold">
                    <span className="text-[#06D6A0]">AUDIO VOLUME (A)</span>
                    <span className="font-mono text-[#E0E0E0]">
                      {Math.round((activeLayer.audioVolume ?? 0.5) * 100)}%
                    </span>
                  </div>
                  <input
                    type="range"
                    min="0"
                    max="1"
                    step="0.01"
                    value={activeLayer.audioVolume ?? 0.5}
                    onChange={(e) => updateActiveLayer({ audioVolume: parseFloat(e.target.value) })}
                    className="w-full accent-[#06D6A0] h-1.5 bg-[#050505] rounded-lg cursor-pointer"
                  />
                </div>

                <div>
                  <label className="text-[10px] font-bold text-[#888] block mb-1.5 uppercase">
                    {t.blend} MODE
                  </label>
                  <div className="grid grid-cols-2 gap-1.5">
                    {blendModes.map((b) => (
                      <button
                        key={b}
                        onClick={() => updateActiveLayer({ blendMode: b })}
                        className={`py-1 px-2 rounded text-[9px] font-mono font-bold transition-colors ${
                          activeLayer.blendMode === b
                            ? 'bg-[#FF7F50]/20 text-[#FF7F50] border border-[#FF7F50]/40'
                            : 'bg-[#181818] text-[#777] hover:text-white border border-[#2a2a2a]'
                        }`}
                      >
                        {b}
                      </button>
                    ))}
                  </div>
                </div>

                <div className="pt-2 border-t border-[#222]">
                  <div className="text-[10px] font-bold text-[#888] mb-2">LAYER SWITCHES</div>
                  <div className="grid grid-cols-3 gap-2">
                    <button
                      onClick={() => updateActiveLayer({ solo: !activeLayer.solo })}
                      className={`py-1.5 rounded text-[10px] font-bold border transition-colors ${
                        activeLayer.solo
                          ? 'bg-[#118AB2] text-white border-[#118AB2]'
                          : 'bg-[#1a1a1a] text-[#777] border-[#333] hover:text-white'
                      }`}
                    >
                      SOLO (S)
                    </button>
                    <button
                      onClick={() => updateActiveLayer({ blind: !activeLayer.blind })}
                      className={`py-1.5 rounded text-[10px] font-bold border transition-colors ${
                        activeLayer.blind
                          ? 'bg-red-700 text-white border-red-600'
                          : 'bg-[#1a1a1a] text-[#777] border-[#333] hover:text-white'
                      }`}
                    >
                      BLIND (B)
                    </button>
                    <button
                      onClick={() => updateActiveLayer({ bypass: !activeLayer.bypass })}
                      className={`py-1.5 rounded text-[10px] font-bold border transition-colors ${
                        activeLayer.bypass
                          ? 'bg-[#FFD166] text-black border-[#FFD166]'
                          : 'bg-[#1a1a1a] text-[#777] border-[#333] hover:text-white'
                      }`}
                    >
                      BYPASS (V)
                    </button>
                  </div>
                </div>
              </>
            ) : selectedClip ? (
              /* Clip Controls */
              <>
                <div className="border-b border-[#222] pb-2">
                  <span className="text-xs font-bold text-white block">{selectedClip.name}</span>
                  <span className="text-[9px] font-mono text-[#FFD166]">{selectedClip.category}</span>
                </div>

                <div>
                  <div className="flex justify-between text-[10px] text-[#888] mb-1 font-bold">
                    <span>{t.scale}</span>
                    <span className="font-mono text-[#E0E0E0]">{Math.round(selectedClip.scale * 100)}%</span>
                  </div>
                  <input
                    type="range"
                    min="0.2"
                    max="2.5"
                    step="0.05"
                    value={selectedClip.scale}
                    onChange={(e) => updateSelectedClip({ scale: parseFloat(e.target.value) })}
                    className="w-full accent-[#FFD166] h-1.5 bg-[#050505] rounded-lg cursor-pointer"
                  />
                </div>

                <div>
                  <div className="flex justify-between text-[10px] text-[#888] mb-1 font-bold">
                    <span>{t.rotation}</span>
                    <span className="font-mono text-[#E0E0E0]">{selectedClip.rotation}°</span>
                  </div>
                  <input
                    type="range"
                    min="-180"
                    max="180"
                    step="5"
                    value={selectedClip.rotation}
                    onChange={(e) => updateSelectedClip({ rotation: parseInt(e.target.value) })}
                    className="w-full accent-[#FFD166] h-1.5 bg-[#050505] rounded-lg cursor-pointer"
                  />
                </div>

                <div>
                  <div className="flex justify-between text-[10px] text-[#888] mb-1 font-bold">
                    <span>{t.speed}</span>
                    <span className="font-mono text-[#E0E0E0]">{selectedClip.speed}x</span>
                  </div>
                  <input
                    type="range"
                    min="0.25"
                    max="3.0"
                    step="0.25"
                    value={selectedClip.speed}
                    onChange={(e) => updateSelectedClip({ speed: parseFloat(e.target.value) })}
                    className="w-full accent-[#FFD166] h-1.5 bg-[#050505] rounded-lg cursor-pointer"
                  />
                </div>

                <div className="pt-2 border-t border-[#222]">
                  <div className="text-[10px] font-bold text-[#888] mb-1.5">EFFECT TOGGLES</div>
                  <div className="space-y-1.5">
                    <label className="flex items-center justify-between text-[11px] text-[#aaa] cursor-pointer">
                      <span>Bloom Glow</span>
                      <input
                        type="checkbox"
                        checked={selectedClip.fx.glow}
                        onChange={(e) =>
                          updateSelectedClip({ fx: { ...selectedClip.fx, glow: e.target.checked } })
                        }
                        className="accent-[#FF7F50]"
                      />
                    </label>
                    <label className="flex items-center justify-between text-[11px] text-[#aaa] cursor-pointer">
                      <span>Color Shift</span>
                      <input
                        type="checkbox"
                        checked={selectedClip.fx.colorize}
                        onChange={(e) =>
                          updateSelectedClip({ fx: { ...selectedClip.fx, colorize: e.target.checked } })
                        }
                        className="accent-[#FF7F50]"
                      />
                    </label>
                  </div>
                </div>
              </>
            ) : selectedClipCoords ? (
              /* Selected Empty Slot Inspector */
              <div className="space-y-4">
                <div className="border-b border-[#2a2a2a] pb-2.5">
                  <div className="flex items-center justify-between">
                    <span className="text-xs font-bold text-white flex items-center gap-1.5">
                      <span className="w-2 h-2 rounded-full bg-[#FF7F50]" />
                      Slot Trống (Layer {selectedClipCoords.layerId}, Cột {selectedClipCoords.colId})
                    </span>
                    {layers.find((l) => l.id === selectedClipCoords.layerId)?.activeCol === selectedClipCoords.colId ? (
                      <span className="text-[9px] font-bold text-[#FF7F50] bg-[#FF7F50]/20 px-1.5 py-0.5 rounded border border-[#FF7F50]/30 font-mono">
                        ĐANG PHÁT (CLEAR)
                      </span>
                    ) : (
                      <span className="text-[9px] font-mono text-[#666] bg-[#1a1a1a] px-1.5 py-0.5 rounded">
                        CHỜ
                      </span>
                    )}
                  </div>
                  <p className="text-[10px] text-[#777] mt-1">
                    Layer này hiện không phát video tại Cột {selectedClipCoords.colId} (màn hình trong suốt/đen).
                  </p>
                </div>

                <div className="p-3 rounded bg-[#141414] border border-[#262626] space-y-2.5">
                  <span className="text-[10px] font-bold text-[#bbb] block">Hành động nhanh</span>
                  <button
                    onClick={() => handleColumnTrigger(selectedClipCoords.colId)}
                    className="w-full py-1.5 px-3 rounded bg-[#222] hover:bg-[#2a2a2a] text-[#FF7F50] hover:text-white border border-[#333] hover:border-[#FF7F50] text-[11px] font-bold font-mono transition-all flex items-center justify-center gap-1.5"
                  >
                    <Play size={11} className="fill-current" />
                    Kích Hoạt Cột {selectedClipCoords.colId} LIVE
                  </button>
                  <button
                    onClick={() => {
                      const { layerId, colId } = selectedClipCoords;
                      setClips((prev) => ({
                        ...prev,
                        [layerId]: {
                          ...(prev[layerId] || {}),
                          [colId]: {
                            id: `l${layerId}-c${colId}-${Date.now()}`,
                            name: `Gen Pulse L${layerId}`,
                            category: 'Generators',
                            type: 'generator',
                            loaded: true,
                            active: layers.find((l) => l.id === layerId)?.activeCol === colId,
                            color: '#06D6A0',
                            speed: 1.0,
                            progress: 0,
                            blendMode: 'ALPHA',
                            scale: 1.0,
                            rotation: 0,
                            opacity: 1.0,
                            posX: 0,
                            posY: 0,
                            loopMode: 'loop',
                            duration: 6.0,
                            fx: {
                              colorize: false,
                              glow: true,
                              hueRotate: 0,
                              pixelate: false,
                              edgeGlow: false
                            }
                          }
                        }
                      }));
                    }}
                    className="w-full py-1.5 px-3 rounded bg-[#FF7F50] hover:bg-[#ff936b] text-black text-[11px] font-bold transition-all shadow-sm"
                  >
                    + Tạo Visual Cho Ô Này
                  </button>
                </div>
              </div>
            ) : (
              <div className="text-center py-6 text-xs text-[#555]">
                {t.selectClipPrompt}
              </div>
            )}
          </div>
        </div>
      </div>

      {/* BOTTOM HALF: Layer Headers & Clip Grid Matrix */}
      <div className="flex-grow flex flex-col bg-[#121212] overflow-hidden">
        <div className="flex-grow overflow-auto flex relative">
          {/* STICKY LEFT: Layer Headers with Visual and Audio Sliders */}
          <div className="sticky left-0 z-20 flex flex-col w-[240px] bg-[#141414] border-r border-[#2a2a2a] shrink-0 shadow-[4px_0_15px_rgba(0,0,0,0.5)]">
            {/* Header: LAYERS + Collapse All + Sliders icon */}
            <div className="h-9 bg-[#141414] border-b border-[#2a2a2a] px-3 flex items-center justify-between shrink-0">
              <div className="flex items-center gap-2">
                <span className="text-[10px] font-black tracking-widest text-[#888] uppercase">LAYERS</span>
                <button
                  onClick={() => {
                    const allCollapsed = layers.every((l) => collapsedLayers[l.id]);
                    const next: Record<number, boolean> = {};
                    layers.forEach((l) => {
                      next[l.id] = !allCollapsed;
                    });
                    setCollapsedLayers(next);
                  }}
                  title="Thu gọn / Mở rộng tất cả Layer"
                  className="text-[8px] font-mono text-[#777] hover:text-[#FF7F50] px-1.5 py-0.5 rounded bg-[#1c1c1c] hover:bg-[#252525] border border-[#2e2e2e] transition-colors"
                >
                  {layers.every((l) => collapsedLayers[l.id]) ? 'EXPAND' : 'MIN'}
                </button>
              </div>
              <Sliders size={13} className="text-[#666]" />
            </div>

            {/* Layer Headers (Stacked 3, 2, 1) */}
            {layers.map((layer) => {
              const isSelected = activeLayerId === layer.id;
              const isCollapsed = !!collapsedLayers[layer.id];

              if (isCollapsed) {
                return (
                  <div
                    key={layer.id}
                    onClick={() => {
                      setActiveLayerId(layer.id);
                      setPropTab('layer');
                    }}
                    className={`h-[38px] px-2.5 shrink-0 flex items-center justify-between cursor-pointer transition-colors border-b border-[#252525] relative select-none ${
                      isSelected ? 'bg-[#181818]' : 'bg-[#121212] hover:bg-[#161616]'
                    }`}
                  >
                    {/* Left Orange Indicator Bar when selected */}
                    {isSelected && (
                      <div className="absolute left-0 top-0 bottom-0 w-[3px] bg-[#FF7F50] shadow-[0_0_8px_#FF7F50]" />
                    )}

                    {/* Left: Collapse toggle + Layer name + opacity */}
                    <div className="flex items-center gap-1.5 min-w-0">
                      <button
                        onClick={(e) => {
                          e.stopPropagation();
                          setCollapsedLayers((prev) => ({ ...prev, [layer.id]: false }));
                        }}
                        className="p-0.5 text-[#777] hover:text-white rounded hover:bg-[#222] transition-colors"
                        title="Mở rộng Layer"
                      >
                        <ChevronRight size={13} />
                      </button>
                      <span
                        className={`text-[11px] font-bold truncate transition-colors ${
                          isSelected ? 'text-[#FF7F50]' : 'text-[#E0E0E0]'
                        }`}
                      >
                        {layer.name}
                      </span>
                      <span className="text-[9px] font-mono text-[#888]">
                        {Math.round(layer.opacity * 100)}%
                      </span>
                    </div>

                    {/* Right: Compact V S B buttons */}
                    <div className="flex items-center gap-1 shrink-0">
                      <button
                        onClick={(e) => {
                          e.stopPropagation();
                          setLayers((prev) =>
                            prev.map((l) => (l.id === layer.id ? { ...l, bypass: !l.bypass } : l))
                          );
                        }}
                        title="Bypass (V)"
                        className={`w-4 h-4 text-[8px] font-mono font-bold rounded flex items-center justify-center border transition-all ${
                          !layer.bypass
                            ? 'bg-[#222] text-white border-[#444]'
                            : 'bg-[#141414] text-[#555] border-[#252525]'
                        }`}
                      >
                        V
                      </button>
                      <button
                        onClick={(e) => {
                          e.stopPropagation();
                          setLayers((prev) =>
                            prev.map((l) => (l.id === layer.id ? { ...l, solo: !l.solo } : l))
                          );
                        }}
                        title="Solo (S)"
                        className={`w-4 h-4 text-[8px] font-mono font-bold rounded flex items-center justify-center border transition-all ${
                          layer.solo
                            ? 'bg-[#2563EB] text-white border-[#3B82F6]'
                            : 'bg-[#141414] text-[#555] border-[#252525]'
                        }`}
                      >
                        S
                      </button>
                      <button
                        onClick={(e) => {
                          e.stopPropagation();
                          setLayers((prev) =>
                            prev.map((l) => (l.id === layer.id ? { ...l, blind: !l.blind } : l))
                          );
                        }}
                        title="Blind (B)"
                        className={`w-4 h-4 text-[8px] font-mono font-bold rounded flex items-center justify-center border transition-all ${
                          layer.blind
                            ? 'bg-red-700 text-white border-red-600'
                            : 'bg-[#141414] text-[#555] border-[#252525]'
                        }`}
                      >
                        B
                      </button>
                    </div>
                  </div>
                );
              }

              return (
                <div
                  key={layer.id}
                  onClick={() => {
                    setActiveLayerId(layer.id);
                    setPropTab('layer');
                  }}
                  className={`h-[116px] px-3 py-2 shrink-0 flex flex-col justify-between cursor-pointer transition-colors border-b border-[#252525] relative select-none ${
                    isSelected ? 'bg-[#181818]' : 'bg-[#121212] hover:bg-[#161616]'
                  }`}
                >
                  {/* Left Orange Indicator Bar when selected */}
                  {isSelected && (
                    <div className="absolute left-0 top-0 bottom-0 w-[3px] bg-[#FF7F50] shadow-[0_0_8px_#FF7F50]" />
                  )}

                  {/* Top Row: Collapse Button + Layer Title + V S B buttons */}
                  <div className="flex justify-between items-center">
                    <div className="flex items-center gap-1.5 min-w-0">
                      <button
                        onClick={(e) => {
                          e.stopPropagation();
                          setCollapsedLayers((prev) => ({ ...prev, [layer.id]: true }));
                        }}
                        className="p-0.5 text-[#777] hover:text-white rounded hover:bg-[#222] transition-colors"
                        title="Thu gọn Layer"
                      >
                        <ChevronDown size={13} />
                      </button>
                      <span
                        className={`text-xs font-bold transition-colors truncate ${
                          isSelected ? 'text-[#FF7F50]' : 'text-[#E0E0E0]'
                        }`}
                      >
                        {layer.name}
                      </span>
                    </div>
                    <div className="flex items-center gap-1">
                      <button
                        onClick={(e) => {
                          e.stopPropagation();
                          setLayers((prev) =>
                            prev.map((l) => (l.id === layer.id ? { ...l, bypass: !l.bypass } : l))
                          );
                        }}
                        title="Bypass (V)"
                        className={`w-5 h-5 text-[9px] font-mono font-bold rounded flex items-center justify-center border transition-all ${
                          !layer.bypass
                            ? 'bg-[#222] text-white border-[#444] shadow-xs'
                            : 'bg-[#141414] text-[#555] border-[#252525]'
                        }`}
                      >
                        V
                      </button>
                      <button
                        onClick={(e) => {
                          e.stopPropagation();
                          setLayers((prev) =>
                            prev.map((l) => (l.id === layer.id ? { ...l, solo: !l.solo } : l))
                          );
                        }}
                        title="Solo (S)"
                        className={`w-5 h-5 text-[9px] font-mono font-bold rounded flex items-center justify-center border transition-all ${
                          layer.solo
                            ? 'bg-[#2563EB] text-white border-[#3B82F6]'
                            : 'bg-[#141414] text-[#555] border-[#252525] hover:text-white'
                        }`}
                      >
                        S
                      </button>
                      <button
                        onClick={(e) => {
                          e.stopPropagation();
                          setLayers((prev) =>
                            prev.map((l) => (l.id === layer.id ? { ...l, blind: !l.blind } : l))
                          );
                        }}
                        title="Blind (B)"
                        className={`w-5 h-5 text-[9px] font-mono font-bold rounded flex items-center justify-center border transition-all ${
                          layer.blind
                            ? 'bg-red-700 text-white border-red-600'
                            : 'bg-[#141414] text-[#555] border-[#252525] hover:text-white'
                        }`}
                      >
                        B
                      </button>
                    </div>
                  </div>

                  {/* Visual Opacity Slider Row (V) */}
                  <div className="flex items-center gap-2">
                    <span className="text-[11px] font-mono font-black text-[#FF7F50] w-3 text-center shrink-0">
                      V
                    </span>
                    <div className="flex-grow flex items-center relative">
                      <input
                        type="range"
                        min="0"
                        max="1"
                        step="0.01"
                        value={layer.opacity}
                        onClick={(e) => e.stopPropagation()}
                        onChange={(e) => {
                          const val = parseFloat(e.target.value);
                          setLayers((prev) =>
                            prev.map((l) => (l.id === layer.id ? { ...l, opacity: val } : l))
                          );
                        }}
                        className="w-full h-1.5 rounded-full cursor-pointer bg-[#222] accent-[#FF7F50]"
                        style={{
                          background: `linear-gradient(to right, #FF7F50 ${layer.opacity * 100}%, #222 ${layer.opacity * 100}%)`
                        }}
                      />
                    </div>
                    <span className="text-[10px] font-mono text-[#aaa] w-8 text-right shrink-0">
                      {Math.round(layer.opacity * 100)}%
                    </span>
                  </div>

                  {/* Audio Volume Slider Row (A) */}
                  <div className="flex items-center gap-2">
                    <span className="text-[11px] font-mono font-black text-[#06D6A0] w-3 text-center shrink-0">
                      A
                    </span>
                    <div className="flex-grow flex items-center relative">
                      <input
                        type="range"
                        min="0"
                        max="1"
                        step="0.01"
                        value={layer.audioVolume ?? 0.5}
                        onClick={(e) => e.stopPropagation()}
                        onChange={(e) => {
                          const val = parseFloat(e.target.value);
                          setLayers((prev) =>
                            prev.map((l) => (l.id === layer.id ? { ...l, audioVolume: val } : l))
                          );
                        }}
                        className="w-full h-1.5 rounded-full cursor-pointer bg-[#222] accent-[#06D6A0]"
                        style={{
                          background: `linear-gradient(to right, #06D6A0 ${(layer.audioVolume ?? 0.5) * 100}%, #222 ${(layer.audioVolume ?? 0.5) * 100}%)`
                        }}
                      />
                    </div>
                    <span className="text-[10px] font-mono text-[#aaa] w-8 text-right shrink-0">
                      {Math.round((layer.audioVolume ?? 0.5) * 100)}%
                    </span>
                  </div>

                  {/* Blend Mode Box */}
                  <div
                    onClick={(e) => {
                      e.stopPropagation();
                      const idx = blendModes.indexOf(layer.blendMode);
                      const nextMode = blendModes[(idx + 1) % blendModes.length];
                      setLayers((prev) =>
                        prev.map((l) => (l.id === layer.id ? { ...l, blendMode: nextMode } : l))
                      );
                    }}
                    className="h-[22px] bg-[#0c0c0c] border border-[#222] hover:border-[#383838] rounded px-2.5 flex items-center justify-between transition-colors"
                    title="Click to cycle blend mode"
                  >
                    <span className="text-[9px] font-mono tracking-wider text-[#666] font-bold">BLEND</span>
                    <span
                      className="text-[9px] font-mono font-bold"
                      style={{
                        color:
                          layer.blendMode === 'ADDITIVE'
                            ? '#FF7F50'
                            : layer.blendMode === 'ALPHA'
                            ? '#06D6A0'
                            : '#118AB2'
                      }}
                    >
                      {layer.blendMode}
                    </span>
                  </div>
                </div>
              );
            })}
          </div>

          {/* CLIP GRID */}
          <div className="flex flex-col min-w-max">
            {/* Column Trigger Headers */}
            <div className="h-9 bg-[#141414] flex items-center gap-1.5 p-1 border-b border-[#2a2a2a] sticky top-0 z-10 shrink-0">
              {[1, 2, 3, 4, 5, 6, 7, 8].map((col) => {
                // Exactly 1 column can be LIVE across all layers
                const activeLiveCol = layers[0]?.activeCol ?? null;
                const isColActive = activeLiveCol === col;
                return (
                  <button
                    key={col}
                    onClick={() => handleColumnTrigger(col)}
                    title={`Trigger Column ${col} across all layers`}
                    className={`min-w-[130px] w-[130px] h-full shrink-0 rounded px-2.5 flex items-center justify-between font-mono transition-all group active:scale-95 border ${
                      isColActive
                        ? 'bg-[#FF7F50]/20 border-[#FF7F50] text-[#FF7F50] shadow-[0_0_12px_rgba(255,127,80,0.35)]'
                        : 'bg-[#1c1c1c] hover:bg-[#252525] border-[#333] hover:border-[#FFD166] text-[#E0E0E0] shadow-xs'
                    }`}
                  >
                    <div className="flex items-center gap-1.5">
                      <Play
                        size={11}
                        className={`transition-transform duration-150 group-hover:scale-125 ${
                          isColActive
                            ? 'fill-[#FF7F50] text-[#FF7F50]'
                            : 'fill-[#777] text-[#777] group-hover:fill-[#FFD166] group-hover:text-[#FFD166]'
                        }`}
                      />
                      <span className="font-black text-[11px] tracking-wider">
                        {t.col} {col}
                      </span>
                    </div>

                    {isColActive ? (
                      <span className="flex items-center gap-1">
                        <span className="text-[8px] font-bold px-1 py-0.2 rounded bg-[#FF7F50] text-black">
                          LIVE
                        </span>
                        <span className="w-1.5 h-1.5 rounded-full bg-[#06D6A0] animate-ping" />
                      </span>
                    ) : (
                      <span className="text-[8px] px-1 py-0.5 rounded bg-[#121212] text-[#666] group-hover:text-[#FFD166] border border-[#262626] font-semibold">
                        TRIG
                      </span>
                    )}
                  </button>
                );
              })}
            </div>

            {/* Matrix rows corresponding to Layer 3, 2, 1 */}
            {layers.map((layer) => {
              const isCollapsed = !!collapsedLayers[layer.id];

              return (
                <div
                  key={`clips-layer-${layer.id}`}
                  className={`flex ${isCollapsed ? 'h-[38px]' : 'h-[116px]'} gap-1.5 p-1 bg-[#121212] border-b border-[#2a2a2a] shrink-0 transition-all`}
                >
                  {[1, 2, 3, 4, 5, 6, 7, 8].map((col) => {
                    const clip = clips[layer.id]?.[col];
                    const isSelected =
                      selectedClipCoords?.layerId === layer.id && selectedClipCoords?.colId === col;
                    const isLayerActiveAtCol = layer.activeCol === col;

                    if (isCollapsed) {
                      // Collapsed Clip Cell (h-[30px] inside h-[38px] row)
                      return (
                        <div
                          key={col}
                          onClick={() => {
                            setSelectedClipCoords({ layerId: layer.id, colId: col });
                            setActiveLayerId(layer.id);
                          }}
                          onDoubleClick={() => handleClipDoubleClick(layer.id, col)}
                          className={`min-w-[130px] w-[130px] h-[30px] shrink-0 rounded relative overflow-hidden cursor-pointer transition-all border px-2 flex items-center justify-between ${
                            isLayerActiveAtCol && clip?.loaded
                              ? 'border-[#FF7F50] bg-[#FF7F50] text-black shadow-[0_0_10px_rgba(255,127,80,0.35)]'
                              : isLayerActiveAtCol && !clip?.loaded
                              ? 'border-[#FF7F50]/80 bg-[#FF7F50]/10 shadow-[inset_0_0_8px_rgba(255,127,80,0.2)]'
                              : isSelected && clip?.loaded
                              ? 'border-[#FFD166] ring-2 ring-[#FFD166] bg-[#3B1D0E] text-[#F5D0B5]'
                              : isSelected
                              ? 'border-[#FFD166] bg-[#222]'
                              : clip?.loaded
                              ? 'border-[#4A2411] bg-[#2A150A] text-[#E8C4A2] hover:bg-[#3B1D0E] hover:border-[#6B3419] shadow-xs'
                              : 'border-[#222] bg-[#101010] hover:bg-[#161616]'
                          }`}
                        >
                          {clip?.loaded ? (
                            <>
                              <span
                                className={`text-[10px] truncate text-left flex-grow mr-1 leading-tight ${
                                  isLayerActiveAtCol ? 'font-black text-black' : 'font-semibold text-[#E8C4A2]'
                                }`}
                                title={clip.name}
                              >
                                {clip.name}
                              </span>
                              {isLayerActiveAtCol && (
                                <div className="w-2 h-2 rounded-full bg-[#06D6A0] animate-pulse shrink-0 ring-1 ring-black/40" />
                              )}
                            </>
                          ) : (
                            /* Collapsed Empty slot - NO text */
                            isLayerActiveAtCol ? (
                              <div className="w-full flex justify-end">
                                <div className="w-1.5 h-1.5 rounded-full bg-[#FF7F50] animate-pulse" />
                              </div>
                            ) : null
                          )}
                        </div>
                      );
                    }

                    // Expanded Clip Cell (h-full inside h-[116px] row)
                    return (
                      <div
                        key={col}
                        onClick={() => {
                          setSelectedClipCoords({ layerId: layer.id, colId: col });
                          setActiveLayerId(layer.id);
                        }}
                        onDoubleClick={() => handleClipDoubleClick(layer.id, col)}
                        className={`min-w-[130px] w-[130px] h-full shrink-0 rounded relative overflow-hidden cursor-pointer transition-all border p-1.5 flex flex-col justify-between ${
                          isLayerActiveAtCol && clip?.loaded
                            ? isSelected
                              ? 'border-[#FF7F50] bg-[#FF7F50]/15 ring-2 ring-[#FFD166] shadow-[0_0_16px_rgba(255,127,80,0.35)]'
                              : 'border-[#FF7F50] bg-[#FF7F50]/15 shadow-[0_0_12px_rgba(255,127,80,0.25)]'
                            : isLayerActiveAtCol && !clip?.loaded
                            ? 'border-[#FF7F50]/80 bg-[#FF7F50]/10 shadow-[inset_0_0_12px_rgba(255,127,80,0.18)] ring-1 ring-[#FF7F50]/50'
                            : isSelected && clip?.loaded
                            ? 'border-[#FFD166] ring-2 ring-[#FFD166] bg-[#3B1D0E]/60 shadow-[0_0_14px_rgba(255,209,102,0.3)]'
                            : isSelected
                            ? 'border-[#FFD166] bg-[#222]'
                            : clip?.loaded
                            ? 'border-[#4A2411] bg-[#1A0E07] hover:border-[#6B3419] hover:bg-[#2A150A] shadow-xs'
                            : 'border-[#222] bg-[#101010] hover:bg-[#161616] hover:border-[#333]'
                        }`}
                      >
                        {clip?.loaded ? (
                          <>
                            {/* Full width title banner: vibrant orange for active box, deep burnt brown for inactive box */}
                            <div
                              className={`-mx-1.5 -mt-1.5 px-2 py-1 flex items-center justify-between gap-1.5 z-10 shrink-0 transition-colors ${
                                isLayerActiveAtCol
                                  ? 'bg-[#FF7F50] text-black border-b border-black/20 shadow-xs'
                                  : 'bg-[#3B1D0E] text-[#F5D0B5] border-b border-[#5C2B14]/60 shadow-xs'
                              }`}
                            >
                              <span
                                className={`text-[10.5px] truncate flex-grow text-left leading-tight ${
                                  isLayerActiveAtCol ? 'font-black text-black' : 'font-semibold text-[#F5D0B5]'
                                }`}
                                title={clip.name}
                              >
                                {clip.name}
                              </span>
                              {isLayerActiveAtCol && (
                                <div className="w-2.5 h-2.5 rounded-full bg-[#06D6A0] animate-pulse shrink-0 ring-1.5 ring-black/40 shadow-[0_0_6px_#06D6A0]" />
                              )}
                            </div>

                            {/* Decorative thumbnail gradient */}
                            <div
                              className={`absolute inset-0 pointer-events-none ${
                                isLayerActiveAtCol
                                  ? 'opacity-25 mix-blend-screen bg-gradient-to-br from-[#FF7F50] to-[#FFD166]'
                                  : 'opacity-20 bg-gradient-to-tr from-[#140B05] to-[#3B1D0E]'
                              }`}
                            />

                            {/* Progress bar if active */}
                            {isLayerActiveAtCol && (
                              <div className="absolute bottom-0 left-0 right-0 h-1 bg-black/60">
                                <div
                                  className="h-full bg-[#FF7F50] shadow-[0_0_6px_#FF7F50] animate-[pulse_2s_infinite]"
                                  style={{ width: '65%' }}
                                />
                              </div>
                            )}

                            {/* Bottom info */}
                            <div
                              className={`flex justify-between items-center text-[8px] font-mono z-10 mt-auto ${
                                isLayerActiveAtCol
                                  ? 'text-[#FFD166] font-bold'
                                  : 'text-[#9E6E4B] font-medium'
                              }`}
                            >
                              <span>{clip.loopMode.substring(0, 4).toUpperCase()}</span>
                              <span>{clip.duration}s</span>
                            </div>
                          </>
                        ) : (
                          /* Empty slot - NO text */
                          <div className="w-full h-full flex flex-col justify-between z-10">
                            {isLayerActiveAtCol ? (
                              <div className="w-full flex justify-end">
                                <div
                                  className="w-2 h-2 rounded-full bg-[#FF7F50] animate-pulse shadow-[0_0_8px_#FF7F50]"
                                  title="Active Slot (Cleared)"
                                />
                              </div>
                            ) : null}
                          </div>
                        )}
                      </div>
                    );
                  })}
                </div>
              );
            })}
          </div>
        </div>
      </div>
    </div>
  );
};
