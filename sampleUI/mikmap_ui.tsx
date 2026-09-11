import React, { useState, useEffect, useRef } from 'react';
import { 
    Cpu, Activity, Target, Workflow, Maximize, Power, 
    Settings2, BoxSelect, Trash2, Link as LinkIcon, Radio,
    Layers, MonitorPlay, FolderOpen, Sliders, Monitor, 
    Plus, ChevronRight, ChevronDown, MousePointer2, Move,
    Crop, Scissors, Eye, EyeOff, Check, X, PanelRightClose, PanelRightOpen
} from 'lucide-react';

const COLORS = {
    primary: '#FF7F50', // Orange (Active/Trigger/ROI)
    warning: '#FFD166', // Yellow (Pending/Hover/Connecting)
    success: '#06D6A0', // Green (Live Data/Connected/Loaded)
    info: '#118AB2',    // Blue (Grid/Guides/Secondary)
    bgApp: '#050505',
    bgPanel: '#121212',
    bgCard: '#1a1a1a',
    border: '#2a2a2a'
};

/* ==========================================
   VIEW 1: COMPOSITION (VJ Main Interface)
   ========================================== */
const CompositionView = () => {
    const [activeLayer, setActiveLayer] = useState(3);
    const [activePropTab, setActivePropTab] = useState('layer');

    useEffect(() => {
        setActivePropTab('layer');
    }, [activeLayer]);

    return (
        <div className="flex-grow flex flex-col w-full h-full bg-[#0a0a0a] overflow-hidden">
            {/* TOP HALF: Browser + Monitors + Preferences */}
            <div className="flex h-[50%] min-h-[300px] w-full shrink-0 border-b border-[#2a2a2a]">
                {/* Browser */}
                <div className="w-[220px] flex flex-col bg-[#121212] border-r border-[#2a2a2a] shrink-0">
                    <div className="p-2 border-b border-[#2a2a2a] flex items-center justify-between bg-[#1a1a1a]">
                        <span className="text-[10px] font-bold tracking-widest text-[#E0E0E0] font-sans">FILES & EFFECTS</span>
                        <FolderOpen size={12} className="text-[#888]" />
                    </div>
                    <div className="p-3 flex-grow overflow-y-auto">
                        {['Generators', 'VJ_Loops_2026', 'Audio_React', 'Masks'].map((folder, i) => (
                            <div key={i} className="flex items-center gap-2 py-1.5 text-[11px] text-[#888] hover:text-[#E0E0E0] cursor-pointer transition-colors font-sans">
                                <ChevronRight size={12} />
                                <FolderOpen size={12} style={{ color: COLORS.info }} />
                                <span className="truncate">{folder}</span>
                            </div>
                        ))}
                        <div className="mt-4 pt-3 border-t border-[#2a2a2a]">
                            <div className="text-[9px] font-bold text-[#555] mb-2 uppercase tracking-widest font-sans">Video Effects</div>
                            {['Colorize', 'Glow', 'Hue Rotate', 'Pixelate'].map((fx, i) => (
                                <div key={i} className="flex items-center gap-2 py-1 text-[11px] text-[#aaa] hover:text-white cursor-pointer pl-4 font-sans">
                                    <Sliders size={10} style={{ color: COLORS.warning }} /> {fx}
                                </div>
                            ))}
                        </div>
                    </div>
                </div>

                {/* Monitors */}
                <div className="flex-grow flex bg-[#050505] p-2 gap-2">
                    <div className="flex-1 flex flex-col bg-[#121212] border border-[#2a2a2a] rounded overflow-hidden">
                        <div className="h-5 bg-[#1a1a1a] flex items-center px-3 border-b border-[#2a2a2a] border-t-2" style={{ borderTopColor: COLORS.info }}>
                            <span className="text-[9px] font-bold tracking-widest font-sans" style={{ color: COLORS.info }}>PREVIEW</span>
                        </div>
                        <div className="flex-grow relative flex items-center justify-center p-4">
                            <div className="w-full h-full border border-[#333] border-dashed rounded flex flex-col items-center justify-center gap-2 text-[#333] bg-[#0a0a0a]">
                                <Monitor size={24} />
                                <span className="font-mono text-[10px]">SELECT CLIP</span>
                            </div>
                        </div>
                    </div>
                    
                    <div className="flex-1 flex flex-col bg-[#121212] border border-[#2a2a2a] rounded overflow-hidden shadow-lg">
                        <div className="h-5 bg-[#1a1a1a] flex items-center justify-between px-3 border-b border-[#2a2a2a] border-t-2" style={{ borderTopColor: COLORS.primary }}>
                            <span className="text-[9px] font-bold tracking-widest flex items-center gap-2 font-sans" style={{ color: COLORS.primary }}>
                                <div className="w-1.5 h-1.5 rounded-full animate-pulse shadow-[0_0_5px_#FF7F50]" style={{ backgroundColor: COLORS.primary }}></div> LIVE OUTPUT
                            </span>
                        </div>
                        <div className="flex-grow relative flex items-center justify-center overflow-hidden bg-black">
                            <div className="w-[80%] h-[80%] border border-[#FF7F50]/20 bg-[#FF7F50]/5 rounded flex items-center justify-center relative shadow-[0_0_30px_rgba(255,127,80,0.1)]">
                                <div className="absolute inset-0 bg-gradient-to-tr from-[#118AB2]/30 to-[#06D6A0]/10 mix-blend-screen"></div>
                                <div className="absolute bottom-2 right-2 font-mono text-[9px] bg-black/50 px-1 rounded" style={{ color: COLORS.primary }}>1920x1080</div>
                            </div>
                        </div>
                    </div>
                </div>

                {/* Properties Panel */}
                <div className="w-[250px] flex flex-col bg-[#121212] border-l border-[#2a2a2a] shrink-0">
                    <div className="flex bg-[#1a1a1a] border-b border-[#2a2a2a]">
                        <button onClick={() => setActivePropTab('clip')} className={`flex-1 py-2 text-[9px] font-bold tracking-wider transition-colors font-sans ${activePropTab==='clip'?'text-[#FFD166] border-b-2 border-[#FFD166] bg-[#222]':'text-[#666] hover:bg-[#111]'}`}>CLIP</button>
                        <button onClick={() => setActivePropTab('layer')} className={`flex-1 py-2 text-[9px] font-bold tracking-wider transition-colors font-sans ${activePropTab==='layer'?'text-[#FF7F50] border-b-2 border-[#FF7F50] bg-[#222]':'text-[#666] hover:bg-[#111]'}`}>LAYER {activeLayer}</button>
                    </div>
                    
                    <div className="p-3 flex-grow overflow-y-auto">
                        <div className="space-y-4">
                            {['Scale', 'Position X', 'Position Y', 'Rotation'].map((prop, i) => (
                                <div key={i}>
                                    <div className="flex justify-between text-[9px] text-[#888] mb-1.5 font-bold tracking-wider font-sans">
                                        <span>{prop}</span>
                                        <span className="font-mono text-[#aaa]">{prop === 'Scale' ? '100.0%' : (prop === 'Rotation' ? '0.0°' : '0.00')}</span>
                                    </div>
                                    <div className="h-1.5 w-full bg-[#050505] rounded-full border border-[#222] relative cursor-pointer">
                                        <div className="absolute top-0 left-0 h-full w-[50%] transition-colors" style={{ backgroundColor: activePropTab === 'layer' ? COLORS.primary : COLORS.info }}></div>
                                        <div className="absolute top-1/2 -translate-y-1/2 w-2 h-2 bg-white rounded-full shadow border border-[#aaa]" style={{ left: '50%', marginLeft: '-4px' }}></div>
                                    </div>
                                </div>
                            ))}
                        </div>
                    </div>
                </div>
            </div>

            {/* BOTTOM HALF: Decks & Layers */}
            <div className="flex-grow flex flex-col bg-[#121212] overflow-hidden">
                <div className="flex-grow overflow-auto flex relative">
                    
                    {/* STICKY LEFT: SHARED SLIDER & HEADERS */}
                    <div className="sticky left-0 z-20 flex flex-col w-[220px] bg-[#121212] border-r border-[#2a2a2a] shrink-0 shadow-[4px_0_15px_rgba(0,0,0,0.5)]">
                        
                        {/* SHARED OPACITY SLIDER */}
                        <div className="h-8 bg-[#1a1a1a] border-b border-[#FF7F50]/40 shrink-0 flex items-center px-2.5 gap-2 relative">
                            <div className="absolute left-0 top-0 bottom-0 w-[3px]" style={{ backgroundColor: COLORS.primary, boxShadow: `0 0 8px ${COLORS.primary}` }}></div>
                            <span className="text-[9px] font-bold shrink-0 tracking-wider font-sans w-[35px]" style={{ color: COLORS.primary }}>L{activeLayer} OP</span>
                            <div className="flex-grow h-1.5 bg-[#050505] rounded-full w-full border border-[#222] relative cursor-pointer group">
                                <div className="absolute top-0 left-0 h-full rounded-full w-[80%] transition-shadow" style={{ background: `linear-gradient(90deg, ${COLORS.info}, ${COLORS.primary})` }}></div>
                                <div className="absolute top-1/2 -translate-y-1/2 w-2.5 h-2.5 bg-white rounded-full shadow border border-[#ccc]" style={{ left: 'calc(80% - 5px)' }}></div>
                            </div>
                            <span className="text-[9px] font-mono text-[#E0E0E0] w-6 text-right">80%</span>
                        </div>
                        
                        {/* Layer Headers */}
                        {[3, 2, 1].map((layerIndex) => {
                            const isSelected = activeLayer === layerIndex;
                            return (
                                <div 
                                    key={layerIndex} 
                                    onClick={() => setActiveLayer(layerIndex)}
                                    className={`h-[90px] p-2.5 shrink-0 flex flex-col justify-between group cursor-pointer transition-colors border-b border-[#2a2a2a]
                                        ${isSelected ? 'bg-[#1e1e1e] border-l-2' : 'bg-[#161616] border-l-2 border-l-transparent hover:bg-[#1a1a1a]'}`}
                                    style={isSelected ? { borderLeftColor: COLORS.primary } : {}}
                                >
                                    <div className="flex justify-between items-center">
                                        <span className={`text-xs font-bold transition-colors font-sans ${isSelected ? 'text-[#FF7F50]' : 'text-[#E0E0E0] group-hover:text-[#FF7F50]'}`}>
                                            Layer {layerIndex}
                                        </span>
                                        <div className="flex gap-1">
                                            <button className={`w-5 h-5 text-[9px] rounded border transition-colors ${isSelected ? 'bg-[#333] border-[#555] text-white' : 'bg-[#222] border-[#333] text-[#888] hover:bg-[#333]'}`}>V</button>
                                            <button className="w-5 h-5 bg-[#222] text-[9px] rounded text-[#888] border border-[#333] hover:bg-[#118AB2] hover:text-white transition-colors">S</button>
                                            <button className="w-5 h-5 bg-[#222] text-[9px] rounded text-[#888] border border-[#333] hover:bg-red-900 hover:text-white transition-colors">B</button>
                                        </div>
                                    </div>
                                    <div className="mt-2 flex justify-between items-center text-[9px] text-[#888] font-mono bg-[#111] px-2 py-1.5 rounded border border-[#222]">
                                        <span>BLEND</span>
                                        <span style={{ color: isSelected ? COLORS.primary : COLORS.info }}>
                                            {layerIndex === 3 ? 'ADDITIVE' : 'ALPHA'}
                                        </span>
                                    </div>
                                </div>
                            );
                        })}
                    </div>

                    {/* CLIP GRID */}
                    <div className="flex flex-col min-w-max">
                        <div className="h-8 bg-[#121212] flex items-center px-1 border-b border-[#2a2a2a] sticky top-0 z-10 shrink-0">
                            {[1, 2, 3, 4, 5, 6, 7, 8].map(col => (
                                <div key={col} className="min-w-[120px] w-[120px] text-center shrink-0">
                                    <span className="text-[9px] font-bold text-[#555] hover:text-[#E0E0E0] cursor-pointer font-sans">COL {col}</span>
                                </div>
                            ))}
                        </div>

                        {[3, 2, 1].map((layerIndex) => (
                            <div key={`clips-${layerIndex}`} className="flex h-[90px] gap-1 p-1 bg-[#121212] border-b border-[#2a2a2a] shrink-0">
                                {[1, 2, 3, 4, 5, 6, 7, 8].map((clipIndex) => {
                                    const isActive = layerIndex === 3 && clipIndex === 2;
                                    const isLoaded = (layerIndex === 3 && clipIndex < 5) || (layerIndex === 2 && clipIndex === 1);
                                    
                                    return (
                                        <div 
                                            key={clipIndex} 
                                            className={`min-w-[120px] w-[120px] shrink-0 rounded relative overflow-hidden cursor-pointer transition-all border
                                                ${isActive ? 'bg-[#FF7F50]/10 shadow-[0_0_10px_rgba(255,127,80,0.2)]' : 
                                                  isLoaded ? 'border-[#333] bg-[#222] hover:border-[#118AB2]' : 'border-[#1a1a1a] bg-[#111] hover:bg-[#161616]'}`}
                                            style={isActive ? { borderColor: COLORS.primary } : {}}
                                        >
                                            {isLoaded && (
                                                <>
                                                    <div className="absolute top-1 left-1 text-[9px] font-bold font-sans text-white bg-black/60 px-1 rounded z-10 backdrop-blur-sm">Clip {clipIndex}</div>
                                                    <div className={`absolute inset-0 opacity-40 mix-blend-screen ${isActive ? 'bg-gradient-to-br from-[#FF7F50] to-[#FFD166]' : 'bg-gradient-to-tr from-[#118AB2] to-[#06D6A0]'}`}></div>
                                                </>
                                            )}
                                            {isActive && (
                                                <div className="absolute bottom-0 left-0 h-1.5 w-[65%] shadow-[0_0_8px_#FF7F50]" style={{ backgroundColor: COLORS.primary }}></div>
                                            )}
                                        </div>
                                    )
                                })}
                            </div>
                        ))}
                    </div>
                </div>
            </div>
        </div>
    );
};

/* ==========================================
   VIEW 2: ADVANCED MAPPING
   ========================================== */
const AdvancedMappingView = () => {
    const [mappingMode, setMappingMode] = useState('input'); // 'input' or 'output'
    const [showProps, setShowProps] = useState(true);
    const [selectedItem, setSelectedItem] = useState('slice1'); // 'screen1', 'slice1', 'mask1'

    // Mock tree data
    const treeData = [
        { id: 'screen1', type: 'screen', name: 'Screen 1', children: [
            { id: 'slice1', type: 'slice', name: 'Center LED Wall', children: [
                { id: 'mask1', type: 'mask', name: 'DJ Booth Mask' }
            ]},
            { id: 'slice2', type: 'slice', name: 'Left Pillar', children: [] }
        ]}
    ];

    return (
        <div className="flex-grow flex w-full h-full bg-[#0a0a0a] overflow-hidden">
            
            {/* LEFT: HIERARCHY TREE */}
            <div className="w-[240px] flex flex-col bg-[#121212] border-r border-[#2a2a2a] shrink-0">
                <div className="p-3 border-b border-[#2a2a2a] flex items-center justify-between">
                    <span className="text-xs font-bold tracking-widest text-[#E0E0E0] font-sans">MAPPING TREE</span>
                    <Plus size={14} className="text-[#888] hover:text-white cursor-pointer" />
                </div>
                
                <div className="flex-grow overflow-y-auto p-2">
                    {/* Screen Level */}
                    <div className="flex flex-col gap-1">
                        <div 
                            onClick={() => setSelectedItem('screen1')}
                            className={`flex items-center gap-2 py-1.5 px-2 rounded cursor-pointer transition-colors ${selectedItem === 'screen1' ? 'bg-[#FF7F50]/10 text-[#FF7F50]' : 'text-[#E0E0E0] hover:bg-[#1a1a1a]'}`}
                        >
                            <ChevronDown size={14} className="text-[#888]" />
                            <Monitor size={14} />
                            <span className="text-[11px] font-sans font-semibold">Screen 1</span>
                        </div>
                        
                        {/* Slices Level */}
                        <div className="pl-6 flex flex-col gap-1 border-l border-[#333] ml-3">
                            {/* Slice 1 */}
                            <div 
                                onClick={() => setSelectedItem('slice1')}
                                className={`flex items-center gap-2 py-1.5 px-2 rounded cursor-pointer transition-colors ${selectedItem === 'slice1' ? 'bg-[#118AB2]/20 text-[#118AB2]' : 'text-[#aaa] hover:bg-[#1a1a1a]'}`}
                            >
                                <ChevronDown size={14} className="text-[#555]" />
                                <Crop size={14} />
                                <span className="text-[11px] font-sans">Center LED Wall</span>
                            </div>
                            
                            {/* Mask Level */}
                            <div className="pl-6 flex flex-col gap-1 border-l border-[#333] ml-3">
                                <div 
                                    onClick={() => setSelectedItem('mask1')}
                                    className={`flex items-center gap-2 py-1.5 px-2 rounded cursor-pointer transition-colors ${selectedItem === 'mask1' ? 'bg-[#FFD166]/10 text-[#FFD166]' : 'text-[#888] hover:bg-[#1a1a1a]'}`}
                                >
                                    <Scissors size={12} />
                                    <span className="text-[11px] font-sans">DJ Booth Mask</span>
                                    <Eye size={12} className="ml-auto text-[#555]" />
                                </div>
                            </div>

                            {/* Slice 2 */}
                            <div 
                                onClick={() => setSelectedItem('slice2')}
                                className={`flex items-center gap-2 py-1.5 px-2 rounded cursor-pointer transition-colors mt-1 ${selectedItem === 'slice2' ? 'bg-[#118AB2]/20 text-[#118AB2]' : 'text-[#aaa] hover:bg-[#1a1a1a]'}`}
                            >
                                <ChevronRight size={14} className="text-[#555]" />
                                <Crop size={14} />
                                <span className="text-[11px] font-sans">Left Pillar</span>
                            </div>
                        </div>
                    </div>
                </div>
            </div>

            {/* MIDDLE: SHARED CANVAS */}
            <div className="flex-grow flex flex-col bg-[#050505] relative overflow-hidden">
                
                {/* Canvas Toolbar Overlays */}
                <div className="absolute top-4 left-4 right-4 flex justify-between z-10 pointer-events-none">
                    
                    {/* Left Tools */}
                    <div className="flex gap-2 pointer-events-auto bg-[#121212]/90 backdrop-blur-sm border border-[#2a2a2a] rounded-lg p-1">
                        <button className="p-1.5 rounded bg-[#2a2a2a] text-white"><MousePointer2 size={14} /></button>
                        <button className="p-1.5 rounded hover:bg-[#222] text-[#888]"><Crop size={14} /></button>
                        <button className="p-1.5 rounded hover:bg-[#222] text-[#888]"><Scissors size={14} /></button>
                    </div>

                    {/* Center Mode Switch */}
                    <div className="flex pointer-events-auto bg-[#121212]/90 backdrop-blur-sm border border-[#2a2a2a] rounded-lg p-1 shadow-lg">
                        <button 
                            onClick={() => setMappingMode('input')}
                            className={`px-4 py-1.5 rounded text-[10px] font-bold font-sans transition-all flex items-center gap-2
                                ${mappingMode === 'input' ? 'bg-[#FF7F50]/10 text-[#FF7F50] border border-[#FF7F50]/30 shadow-[0_0_10px_rgba(255,127,80,0.1)]' : 'text-[#888] hover:text-white'}`}
                        >
                            <Layers size={12} /> INPUT SELECTION
                        </button>
                        <button 
                            onClick={() => setMappingMode('output')}
                            className={`px-4 py-1.5 rounded text-[10px] font-bold font-sans transition-all flex items-center gap-2
                                ${mappingMode === 'output' ? 'bg-[#118AB2]/20 text-[#118AB2] border border-[#118AB2]/30 shadow-[0_0_10px_rgba(17,138,178,0.1)]' : 'text-[#888] hover:text-white'}`}
                        >
                            <MonitorPlay size={12} /> OUTPUT ROUTING
                        </button>
                    </div>

                    {/* Right Tools (Toggle Properties) */}
                    <div className="pointer-events-auto">
                        <button 
                            onClick={() => setShowProps(!showProps)}
                            className={`p-2 rounded-lg border backdrop-blur-sm transition-colors flex items-center justify-center
                                ${showProps ? 'bg-[#121212]/90 border-[#2a2a2a] text-[#888] hover:text-white' : 'bg-[#FF7F50]/20 border-[#FF7F50]/50 text-[#FF7F50]'}`}
                        >
                            {showProps ? <PanelRightClose size={16} /> : <PanelRightOpen size={16} />}
                        </button>
                    </div>
                </div>

                {/* Simulated Canvas Rendering */}
                <div className="flex-grow w-full h-full flex items-center justify-center p-12">
                    <div 
                        className="relative border border-[#222] shadow-[0_0_50px_rgba(0,0,0,0.8)] overflow-hidden transition-all duration-500"
                        style={{ 
                            width: '80%', height: '70%',
                            backgroundColor: mappingMode === 'input' ? '#111' : '#000',
                            backgroundImage: mappingMode === 'input' ? 'repeating-linear-gradient(45deg, #1a1a1a 25%, transparent 25%, transparent 75%, #1a1a1a 75%, #1a1a1a), repeating-linear-gradient(45deg, #1a1a1a 25%, #111 25%, #111 75%, #1a1a1a 75%, #1a1a1a)' : 'none',
                            backgroundPosition: '0 0, 10px 10px', backgroundSize: '20px 20px'
                        }}
                    >
                        {/* Background label */}
                        <div className="absolute top-2 left-2 text-[10px] font-mono text-[#444]">
                            {mappingMode === 'input' ? 'COMPOSITION 1920x1080' : 'PHYSICAL DISPLAYS'}
                        </div>

                        {/* Simulated Slice Box */}
                        <div 
                            className="absolute border border-dashed flex items-center justify-center transition-all duration-500"
                            style={{
                                top: mappingMode === 'input' ? '20%' : '30%',
                                left: mappingMode === 'input' ? '20%' : '10%',
                                width: mappingMode === 'input' ? '60%' : '70%',
                                height: mappingMode === 'input' ? '60%' : '50%',
                                borderColor: mappingMode === 'input' ? COLORS.primary : COLORS.info,
                                backgroundColor: mappingMode === 'input' ? `${COLORS.primary}10` : `${COLORS.info}10`,
                                transform: mappingMode === 'output' ? 'perspective(500px) rotateY(15deg) rotateX(5deg)' : 'none'
                            }}
                        >
                            <span className="font-sans font-bold text-[12px] opacity-50" style={{ color: mappingMode === 'input' ? COLORS.primary : COLORS.info }}>
                                Center LED Wall
                            </span>

                            {/* Simulated Mask (Only visible if mask selected) */}
                            {selectedItem === 'mask1' && (
                                <div className="absolute bottom-0 left-1/4 w-1/2 h-1/3 border border-[#FFD166] bg-[#FFD166]/20 bg-stripes flex items-center justify-center">
                                     <span className="text-[9px] font-mono text-[#FFD166]">MASKED AREA</span>
                                </div>
                            )}

                            {/* Corner Anchor Points */}
                            <div className="absolute -top-1 -left-1 w-2.5 h-2.5 bg-white border border-[#333]"></div>
                            <div className="absolute -top-1 -right-1 w-2.5 h-2.5 bg-white border border-[#333]"></div>
                            <div className="absolute -bottom-1 -left-1 w-2.5 h-2.5 bg-white border border-[#333]"></div>
                            <div className="absolute -bottom-1 -right-1 w-2.5 h-2.5 bg-white border border-[#333]"></div>
                        </div>
                    </div>
                </div>
            </div>

            {/* RIGHT: DYNAMIC PROPERTIES PANEL */}
            {showProps && (
                <div className="w-[280px] flex flex-col bg-[#121212] border-l border-[#2a2a2a] shrink-0 transition-all">
                    <div className="p-3 border-b border-[#2a2a2a] flex items-center gap-2">
                        <Settings2 size={14} className="text-[#888]" />
                        <span className="text-xs font-bold tracking-widest text-[#E0E0E0] font-sans uppercase">
                            {selectedItem.includes('screen') ? 'Screen Properties' : selectedItem.includes('slice') ? 'Slice Properties' : 'Mask Properties'}
                        </span>
                    </div>
                    
                    <div className="p-4 flex-grow overflow-y-auto">
                        
                        {/* Contextual Properties based on Selection */}
                        {selectedItem === 'screen1' && (
                            <div className="space-y-5">
                                <div>
                                    <label className="text-[10px] text-[#666] font-bold tracking-widest mb-2 block font-sans">OUTPUT DEVICE</label>
                                    <div className="bg-[#0a0a0a] border border-[#333] p-2 rounded text-xs text-[#E0E0E0] flex justify-between items-center cursor-pointer hover:border-[#555]">
                                        <span>Display 1 (HDMI)</span>
                                        <ChevronDown size={14} className="text-[#888]" />
                                    </div>
                                    <div className="mt-2 text-[10px] font-mono text-[#888]">Resolution: <span className="text-[#E0E0E0]">1920 x 1080 @ 60Hz</span></div>
                                </div>
                                <div className="border-t border-[#2a2a2a] pt-4">
                                    <label className="text-[10px] text-[#666] font-bold tracking-widest mb-2 block font-sans">EDGE BLENDING</label>
                                    <button className="w-full py-2 bg-[#1a1a1a] border border-[#333] rounded text-[11px] text-[#888] hover:text-white transition-colors">Enable Blending</button>
                                </div>
                            </div>
                        )}

                        {selectedItem === 'slice1' && (
                            <div className="space-y-5">
                                <div>
                                    <div className="flex items-center justify-between mb-3">
                                        <label className="text-[10px] text-[#666] font-bold tracking-widest font-sans">TRANSFORM</label>
                                        <div className="flex gap-1 bg-[#0a0a0a] rounded p-0.5 border border-[#333]">
                                            <button className={`px-2 py-0.5 text-[9px] rounded font-mono ${mappingMode === 'input' ? 'bg-[#333] text-white' : 'text-[#666]'}`}>IN</button>
                                            <button className={`px-2 py-0.5 text-[9px] rounded font-mono ${mappingMode === 'output' ? 'bg-[#333] text-white' : 'text-[#666]'}`}>OUT</button>
                                        </div>
                                    </div>
                                    <div className="grid grid-cols-2 gap-2">
                                        {['X', 'Y', 'W', 'H'].map(prop => (
                                            <div key={prop} className="bg-[#0a0a0a] border border-[#333] rounded flex p-1.5 items-center">
                                                <span className="text-[#888] text-[9px] font-bold w-4">{prop}</span>
                                                <span className="text-[#E0E0E0] text-[11px] font-mono flex-grow text-right">
                                                    {prop === 'W' || prop === 'H' ? '1920' : '0.00'}
                                                </span>
                                            </div>
                                        ))}
                                    </div>
                                </div>
                                <div className="border-t border-[#2a2a2a] pt-4">
                                    <label className="text-[10px] text-[#666] font-bold tracking-widest mb-2 flex justify-between font-sans">
                                        <span>BEZIER WARPING</span>
                                        <span className="text-[#118AB2]">DISABLED</span>
                                    </label>
                                    <button className="w-full py-2 bg-[#118AB2]/10 border border-[#118AB2]/30 text-[#118AB2] rounded text-[11px] font-bold hover:bg-[#118AB2]/20 transition-colors">Edit Warping Points</button>
                                </div>
                            </div>
                        )}

                        {selectedItem === 'mask1' && (
                            <div className="space-y-5">
                                <div>
                                    <label className="text-[10px] text-[#666] font-bold tracking-widest mb-2 block font-sans">MASK SHAPE</label>
                                    <div className="flex gap-2">
                                        <button className="flex-1 py-1.5 bg-[#FFD166]/10 border border-[#FFD166]/30 text-[#FFD166] rounded text-[11px] font-bold">Polygon</button>
                                        <button className="flex-1 py-1.5 bg-[#1a1a1a] border border-[#333] text-[#888] rounded text-[11px] hover:text-white">Circle</button>
                                    </div>
                                </div>
                                <div className="border-t border-[#2a2a2a] pt-4">
                                    <label className="flex items-center gap-2 text-[11px] text-[#E0E0E0] cursor-pointer">
                                        <div className="w-4 h-4 rounded border border-[#FFD166] flex items-center justify-center bg-[#FFD166]/20">
                                            <Check size={10} className="text-[#FFD166]"/>
                                        </div>
                                        Invert Mask (Cut hole)
                                    </label>
                                </div>
                                <div className="pt-2">
                                    <div className="flex justify-between text-[10px] text-[#888] mb-1 font-sans">
                                        <span>Feather</span>
                                        <span className="font-mono text-[#FFD166]">0.0%</span>
                                    </div>
                                    <div className="h-1.5 w-full bg-[#050505] rounded-full border border-[#222] relative cursor-pointer">
                                        <div className="absolute top-1/2 -translate-y-1/2 w-2 h-2 bg-white rounded-full shadow border border-[#aaa]" style={{ left: '0%' }}></div>
                                    </div>
                                </div>
                            </div>
                        )}
                    </div>
                </div>
            )}
        </div>
    );
};

/* ==========================================
   VIEW 3: SENSOR I/O (From previous turns)
   ========================================== */
const DeviceManager = () => {
    const [isConnected, setIsConnected] = useState(true);
    const [isHovered, setIsHovered] = useState(false);

    return (
        <div className="flex flex-col h-full bg-[#121212] border-r border-[#2a2a2a] w-[240px] shrink-0">
            <div className="p-4 border-b border-[#2a2a2a] flex items-center justify-between">
                <h2 className="text-[#E0E0E0] font-semibold text-xs tracking-widest flex items-center gap-2 font-sans">
                    <Cpu size={14} className="text-[#118AB2]" /> DEVICE MANAGER
                </h2>
                <button className="text-[#118AB2] hover:text-[#FFD166] transition-colors"><Settings2 size={14} /></button>
            </div>
            <div className="p-4 flex-grow overflow-y-auto">
                <div className="bg-[#1a1a1a] border border-[#2a2a2a] rounded-lg p-3 mb-4 transition-all duration-300 hover:border-[#FFD166]/50">
                    <div className="flex justify-between items-start mb-3">
                        <div>
                            <h3 className="text-[#E0E0E0] text-sm font-medium font-sans">Hokuyo UST-10LX</h3>
                            <p className="text-[#666] text-xs font-mono mt-0.5">192.168.1.10:10940</p>
                        </div>
                        <div className={`w-2.5 h-2.5 rounded-full mt-1 ${isConnected ? 'bg-[#06D6A0] shadow-[0_0_8px_#06D6A0]' : 'bg-red-500 shadow-[0_0_8px_red]'}`}></div>
                    </div>
                    <div className="flex items-center justify-between text-[11px] text-[#888] font-mono mb-4 bg-[#0a0a0a] rounded px-2 py-1.5 border border-[#2a2a2a]">
                        <span>2D LiDAR</span><span>40Hz</span>
                    </div>
                    <button 
                        onClick={() => setIsConnected(!isConnected)}
                        onMouseEnter={() => setIsHovered(true)} onMouseLeave={() => setIsHovered(false)}
                        className={`w-full py-2.5 rounded text-xs font-bold font-sans transition-all duration-300 flex items-center justify-center gap-2
                            ${isConnected ? isHovered ? 'bg-red-500/10 text-red-500 border border-red-500/50' : 'bg-[#FF7F50]/10 text-[#FF7F50] border border-[#FF7F50]/30 shadow-[0_0_15px_rgba(255,127,80,0.15)]'
                                : 'bg-[#118AB2]/10 text-[#118AB2] border border-[#118AB2]/30 hover:bg-[#FFD166]/10 hover:text-[#FFD166]'}`}
                    >
                        <Power size={14} /> {isConnected ? (isHovered ? 'DISCONNECT' : 'CONNECTED') : 'CONNECT SENSOR'}
                    </button>
                </div>
                <button className="w-full py-4 rounded-lg border border-dashed border-[#2a2a2a] text-[#666] text-xs hover:text-[#118AB2] hover:border-[#118AB2] transition-colors flex flex-col items-center justify-center gap-2 font-sans bg-[#0a0a0a]">
                    <div className="p-2 rounded-full bg-[#1a1a1a]"><Activity size={16} /></div> ADD NEW SENSOR
                </button>
            </div>
            <div className="p-3 border-t border-[#2a2a2a] flex justify-between items-center text-[10px] font-mono bg-[#0a0a0a]">
                <span className="text-[#666]">SYS: <span style={{ color: COLORS.success }}>OK</span></span>
                <span className="text-[#666]">LATENCY: <span style={{ color: COLORS.success }}>12ms</span></span>
            </div>
        </div>
    );
};

const RadarView = () => {
    const canvasRef = useRef(null);
    const [roiMode, setRoiMode] = useState('active');

    useEffect(() => {
        const canvas = canvasRef.current;
        const ctx = canvas.getContext('2d');
        let animationFrameId; let time = 0;

        const draw = () => {
            const width = canvas.width; const height = canvas.height;
            const cx = width / 2; const cy = height / 2;
            const radius = Math.min(width, height) / 2 - 40;

            ctx.fillStyle = '#050505'; ctx.fillRect(0, 0, width, height);
            ctx.strokeStyle = 'rgba(17, 138, 178, 0.15)'; ctx.lineWidth = 1;
            for (let i = 1; i <= 4; i++) {
                ctx.beginPath(); ctx.arc(cx, cy, (radius / 4) * i, 0, 2 * Math.PI); ctx.stroke();
                ctx.fillStyle = 'rgba(17, 138, 178, 0.4)'; ctx.font = '10px monospace';
                ctx.fillText(`${i}m`, cx + (radius/4)*i + 5, cy + 12);
            }
            ctx.beginPath(); ctx.moveTo(cx - radius, cy); ctx.lineTo(cx + radius, cy);
            ctx.moveTo(cx, cy - radius); ctx.lineTo(cx, cy + radius); ctx.stroke();

            const roiPoints = [{x: cx-180, y: cy-120}, {x: cx+200, y: cy-150}, {x: cx+220, y: cy+160}, {x: cx-140, y: cy+220}];
            ctx.strokeStyle = COLORS.primary; ctx.fillStyle = 'rgba(255, 127, 80, 0.03)';
            ctx.lineWidth = 1.5; ctx.setLineDash(roiMode === 'draw' ? [5, 5] : []);
            ctx.beginPath(); ctx.moveTo(roiPoints[0].x, roiPoints[0].y);
            roiPoints.forEach(p => ctx.lineTo(p.x, p.y)); ctx.closePath();
            ctx.fill(); ctx.stroke(); ctx.setLineDash([]);
            ctx.fillStyle = COLORS.primary; ctx.font = 'bold 10px sans-serif';
            ctx.fillText("ROI_ZONE_1", roiPoints[0].x + 10, roiPoints[0].y + 20);

            ctx.fillStyle = COLORS.success;
            for(let i=0; i<80; i++) {
                const angle = Math.PI + (Math.random() * Math.PI / 1.5);
                const r = radius * 0.85 + (Math.random() * 15 - 7.5);
                ctx.fillRect(cx + Math.cos(angle)*r, cy + Math.sin(angle)*r, 2, 2);
            }

            const sweepAngle = (time * 2) % (Math.PI * 2);
            ctx.strokeStyle = 'rgba(255, 209, 102, 0.8)'; ctx.lineWidth = 2;
            ctx.beginPath(); ctx.moveTo(cx, cy); ctx.lineTo(cx + Math.cos(sweepAngle) * radius, cy + Math.sin(sweepAngle) * radius); ctx.stroke();
            ctx.fillStyle = 'rgba(255, 209, 102, 0.1)'; ctx.beginPath();
            ctx.moveTo(cx, cy); ctx.arc(cx, cy, radius, sweepAngle, sweepAngle - 0.4, true); ctx.fill();

            time += 0.02;
            const blobX = cx + Math.sin(time * 0.8) * 120 + 20; const blobY = cy + Math.cos(time * 1.2) * 80;
            ctx.strokeStyle = COLORS.primary; ctx.fillStyle = COLORS.primary; ctx.lineWidth = 1.5;
            ctx.beginPath(); ctx.moveTo(blobX - 15, blobY); ctx.lineTo(blobX + 15, blobY);
            ctx.moveTo(blobX, blobY - 15); ctx.lineTo(blobX, blobY + 15); ctx.stroke();
            if (Math.floor(time * 4) % 2 === 0) {
                ctx.beginPath(); ctx.arc(blobX, blobY, 4, 0, Math.PI * 2); ctx.fill();
            }
            ctx.strokeStyle = COLORS.success; ctx.lineWidth = 1; ctx.setLineDash([2, 2]);
            ctx.strokeRect(blobX - 20, blobY - 20, 40, 40); ctx.setLineDash([]);
            
            ctx.fillStyle = '#E0E0E0'; ctx.font = '11px monospace'; ctx.fillText(`ID:01`, blobX + 25, blobY - 10);
            ctx.fillStyle = COLORS.success; ctx.fillText(`X:${Math.round(blobX-cx)} Y:${Math.round(cy-blobY)}`, blobX + 25, blobY + 5);

            animationFrameId = window.requestAnimationFrame(draw);
        };
        draw(); return () => window.cancelAnimationFrame(animationFrameId);
    }, [roiMode]);

    return (
        <div className="flex-grow flex flex-col bg-[#050505] relative">
            <div className="absolute top-4 left-4 right-4 flex justify-between z-10 pointer-events-none">
                <div className="flex gap-2 pointer-events-auto">
                    <button onClick={() => setRoiMode(roiMode === 'active' ? 'draw' : 'active')}
                        className={`p-2 rounded flex items-center gap-2 text-xs font-medium font-sans transition-colors border shadow-lg backdrop-blur-sm
                            ${roiMode === 'draw' ? 'bg-[#FFD166]/10 text-[#FFD166] border-[#FFD166]/50' : 'bg-[#121212]/90 text-[#118AB2] border-[#2a2a2a] hover:text-[#E0E0E0]'}`}>
                        <BoxSelect size={14} /> {roiMode === 'draw' ? 'FINISH ROI' : 'EDIT ROI'}
                    </button>
                    <button className="p-2 rounded bg-[#121212]/90 text-[#118AB2] border border-[#2a2a2a] hover:text-[#E0E0E0] pointer-events-auto shadow-lg"><Maximize size={14} /></button>
                </div>
                <div className="bg-[#121212]/90 border border-[#2a2a2a] rounded px-3 py-1.5 flex items-center gap-3 shadow-lg">
                    <Radio size={14} className="text-[#06D6A0] animate-pulse" />
                    <span className="text-[#06D6A0] font-mono text-xs">RECEIVING 40 FPS</span>
                </div>
            </div>
            <div className="flex-grow flex items-center justify-center p-4 overflow-hidden">
                <canvas ref={canvasRef} width={800} height={600} className="w-full max-w-[800px] h-full object-contain" />
            </div>
        </div>
    );
};

const TrackingPanel = () => {
    return (
        <div className="flex flex-col h-full bg-[#121212] border-l border-[#2a2a2a] w-[280px] shrink-0">
            <div className="p-4 border-b border-[#2a2a2a]">
                <h2 className="text-[#E0E0E0] font-semibold text-xs tracking-widest flex items-center gap-2 mb-4 font-sans">
                    <Target size={14} className="text-[#118AB2]" /> BLOB TRACKING
                </h2>
                <div className="space-y-4">
                    <div>
                        <div className="flex justify-between text-[11px] mb-1 font-sans"><span className="text-[#888]">Noise Threshold</span><span className="text-[#06D6A0] font-mono">15%</span></div>
                        <div className="h-1.5 w-full bg-[#0a0a0a] rounded-full overflow-hidden border border-[#2a2a2a] relative"><div className="absolute left-0 top-0 h-full bg-[#06D6A0] w-[15%]"></div></div>
                    </div>
                    <div>
                        <div className="flex justify-between text-[11px] mb-1 font-sans"><span className="text-[#888]">Min Blob Size</span><span className="text-[#06D6A0] font-mono">0.2m</span></div>
                        <div className="h-1.5 w-full bg-[#0a0a0a] rounded-full overflow-hidden border border-[#2a2a2a] relative"><div className="absolute left-0 top-0 h-full bg-[#06D6A0] w-[30%]"></div></div>
                    </div>
                </div>
            </div>
            <div className="p-4 flex-grow flex flex-col bg-[#0a0a0a]">
                <h2 className="text-[#E0E0E0] font-semibold text-xs tracking-widest flex items-center gap-2 mb-4 font-sans">
                    <Workflow size={14} className="text-[#118AB2]" /> PARAMETER ROUTING
                </h2>
                <div className="flex-grow relative bg-[#121212] border border-[#2a2a2a] rounded-lg p-3 flex justify-between overflow-hidden">
                    <svg className="absolute inset-0 w-full h-full pointer-events-none" style={{ zIndex: 1 }}>
                        <path d="M 110 50 C 160 50, 140 130, 190 130" stroke={COLORS.primary} strokeWidth="2" fill="none" className="animate-pulse shadow-[0_0_5px_#FF7F50]" />
                        <path d="M 110 90 C 160 90, 140 50, 190 50" stroke={COLORS.info} strokeWidth="1.5" strokeDasharray="4 4" fill="none" />
                    </svg>
                    <div className="flex flex-col gap-3 z-10 w-[45%]">
                        <div className="text-[9px] text-[#666] font-bold tracking-wider mb-1 font-sans border-b border-[#2a2a2a] pb-1">DATA SOURCES</div>
                        <div className="bg-[#1a1a1a] border border-[#333] text-[#E0E0E0] text-[11px] font-sans py-2 px-2 rounded flex items-center justify-between shadow-[0_0_10px_rgba(255,127,80,0.05)] cursor-pointer">
                            <span className="truncate">Blob 1: X</span><div className={`w-2.5 h-2.5 rounded-full bg-[${COLORS.primary}] border-2 border-[#1a1a1a] box-content`}></div>
                        </div>
                        <div className="bg-[#1a1a1a] border border-[#333] text-[#E0E0E0] text-[11px] font-sans py-2 px-2 rounded flex items-center justify-between cursor-pointer">
                            <span className="truncate">Blob 1: Y</span><div className={`w-2 h-2 rounded-full bg-[${COLORS.info}]`}></div>
                        </div>
                    </div>
                    <div className="flex flex-col gap-3 z-10 w-[45%] text-right">
                        <div className="text-[9px] text-[#666] font-bold tracking-wider mb-1 font-sans border-b border-[#2a2a2a] pb-1">TARGETS</div>
                        <div className="bg-[#1a1a1a] border border-[#333] text-[#E0E0E0] text-[11px] font-sans py-2 px-2 rounded flex items-center justify-between flex-row-reverse cursor-pointer">
                            <span className="truncate">Layer 1: Op</span><div className={`w-2 h-2 rounded-full bg-[${COLORS.info}]`}></div>
                        </div>
                        <div className="h-[32px]"></div>
                        <div className="bg-[#1a1a1a] border border-[#FF7F50]/40 text-[#FF7F50] text-[11px] font-sans py-2 px-2 rounded flex items-center justify-between flex-row-reverse shadow-[0_0_15px_rgba(255,127,80,0.1)] cursor-pointer">
                            <span className="truncate font-bold">FX: Hue</span><div className={`w-2.5 h-2.5 rounded-full bg-[${COLORS.primary}] border-2 border-[#1a1a1a] box-content`}></div>
                        </div>
                    </div>
                </div>
                <button className="mt-4 w-full py-2 bg-[#1a1a1a] border border-[#2a2a2a] rounded text-[11px] font-bold text-[#E0E0E0] hover:bg-[#222] font-sans transition-colors flex justify-center items-center gap-2">
                    <LinkIcon size={12} className="text-[#118AB2]" /> AUTO-MAP CLOSEST BLOB
                </button>
            </div>
        </div>
    );
};


/* ==========================================
   MAIN APP SHELL
   ========================================== */
export default function App() {
    const [activeTab, setActiveTab] = useState('mapping'); // 'comp', 'mapping', 'sensor'

    return (
        <div className="h-screen w-full bg-[#080808] flex flex-col font-sans overflow-hidden select-none">
            {/* Top Navigation Bar */}
            <div className="h-14 bg-[#121212] border-b border-[#2a2a2a] flex items-center justify-between px-5 shrink-0 z-20 shadow-md">
                <div className="flex items-center gap-8">
                    {/* Brand */}
                    <div className="flex items-center gap-2.5">
                        <div className="w-5 h-5 flex items-center justify-center rounded-sm bg-gradient-to-br from-[#FF7F50] to-[#FFD166] shadow-[0_0_10px_rgba(255,127,80,0.5)]">
                            <div className="w-2 h-2 bg-[#121212] rounded-sm"></div>
                        </div>
                        <h1 className="text-lg font-black text-[#E0E0E0] tracking-tighter">MIKMAP</h1>
                    </div>
                    
                    {/* Tabs */}
                    <div className="flex bg-[#0a0a0a] p-1 rounded-lg border border-[#2a2a2a]">
                        <button 
                            onClick={() => setActiveTab('comp')}
                            className={`px-4 py-1.5 rounded-md text-[11px] font-bold flex items-center gap-2 transition-all
                                ${activeTab === 'comp' ? 'bg-[#1a1a1a] text-[#FF7F50] border border-[#333] shadow-[0_0_10px_rgba(255,127,80,0.1)]' : 'text-[#666] hover:text-[#E0E0E0] border border-transparent'}`}
                        >
                            <Layers size={14} /> COMPOSITION
                        </button>
                        <button 
                            onClick={() => setActiveTab('mapping')}
                            className={`px-4 py-1.5 rounded-md text-[11px] font-bold flex items-center gap-2 transition-all
                                ${activeTab === 'mapping' ? 'bg-[#1a1a1a] text-[#FF7F50] border border-[#333] shadow-[0_0_10px_rgba(255,127,80,0.1)]' : 'text-[#666] hover:text-[#E0E0E0] border border-transparent'}`}
                        >
                            <MonitorPlay size={14} /> ADVANCED MAPPING
                        </button>
                        <button 
                            onClick={() => setActiveTab('sensor')}
                            className={`px-4 py-1.5 rounded-md text-[11px] font-bold flex items-center gap-2 transition-all
                                ${activeTab === 'sensor' ? 'bg-[#1a1a1a] text-[#FF7F50] border border-[#333] shadow-[0_0_10px_rgba(255,127,80,0.1)]' : 'text-[#666] hover:text-[#E0E0E0] border border-transparent'}`}
                        >
                            <Activity size={14} /> SENSOR I/O
                        </button>
                    </div>
                </div>
                
                {/* Global Status */}
                <div className="flex items-center gap-5 text-[11px] font-mono">
                    <div className="flex items-center gap-2 bg-[#0a0a0a] px-3 py-1.5 rounded border border-[#2a2a2a]">
                        <span className="text-[#666]">FPS:</span>
                        <span className="text-[#06D6A0] font-bold">60.0</span>
                    </div>
                    <div className="flex items-center gap-2 text-[#888]">
                        <div className="w-2 h-2 rounded-full bg-[#06D6A0] shadow-[0_0_5px_#06D6A0]"></div>
                        1920x1080 @ DISPLAY 1
                    </div>
                </div>
            </div>

            {/* Main Content Area */}
            <div className="flex-grow w-full overflow-hidden flex">
                {activeTab === 'comp' && <CompositionView />}
                {activeTab === 'mapping' && <AdvancedMappingView />}
                {activeTab === 'sensor' && (
                    <>
                        <DeviceManager />
                        <RadarView />
                        <TrackingPanel />
                    </>
                )}
            </div>
        </div>
    );
}