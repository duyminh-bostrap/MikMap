import React, { useState, useEffect, useRef } from 'react';
import { 
    Cpu, Activity, Target, Workflow, Maximize, Power, 
    Settings2, BoxSelect, Trash2, Link as LinkIcon, Radio,
    Layers, MonitorPlay, FolderOpen, Sliders, LayoutGrid,
    Play, Square, ChevronRight, MousePointer2, Move, Focus,
    Copy, Plus, Monitor, Crop, Scissors, ChevronDown, ListTree, Check
} from 'lucide-react';

const COLORS = {
    primary: '#FF7F50', // Orange (Active/Trigger/ROI)
    warning: '#FFD166', // Yellow (Pending/Hover/Connecting)
    success: '#06D6A0', // Green (Live Data/Connected/Loaded)
    info: '#118AB2',    // Blue (Grid/Guides/Secondary)
    bgApp: '#080808',
    bgPanel: '#121212',
    bgCard: '#1a1a1a',
    border: '#2a2a2a'
};

/* ==========================================
   VIEW 1: SENSOR I/O (From previous design)
   ========================================== */
const DeviceManager = () => {
    const [isConnected, setIsConnected] = useState(true);
    const [isHovered, setIsHovered] = useState(false);

    return (
        <div className="flex flex-col h-full bg-[#121212] border-r border-[#2a2a2a] w-1/5 min-w-[260px] max-w-[320px]">
            <div className="p-4 border-b border-[#2a2a2a] flex items-center justify-between shrink-0">
                <h2 className="text-[#E0E0E0] font-bold text-xs tracking-widest flex items-center gap-2">
                    <Cpu size={14} style={{ color: COLORS.info }} /> DEVICE MANAGER
                </h2>
                <button className="text-[#888] hover:text-[#E0E0E0] transition-colors"><Settings2 size={16} /></button>
            </div>
            
            <div className="p-4 flex-grow overflow-y-auto">
                <div className="text-[10px] text-[#666] font-bold tracking-widest mb-3 uppercase">Active Sensors</div>
                <div className="bg-[#1a1a1a] border border-[#2a2a2a] rounded-lg p-3 mb-4 transition-all duration-300 hover:border-[#FFD166]/50">
                    <div className="flex justify-between items-start mb-3">
                        <div>
                            <h3 className="text-[#E0E0E0] text-sm font-semibold">Hokuyo UST-10LX</h3>
                            <p className="text-[#888] text-xs font-mono mt-0.5">192.168.1.10</p>
                        </div>
                        <div className={`w-2.5 h-2.5 rounded-full mt-1 ${isConnected ? 'bg-[#06D6A0] shadow-[0_0_8px_#06D6A0]' : 'bg-red-500 shadow-[0_0_8px_red]'}`}></div>
                    </div>
                    <div className="flex items-center justify-between text-[11px] text-[#666] font-mono mb-4 bg-[#0a0a0a] rounded px-2 py-1.5 border border-[#2a2a2a]">
                        <span>TYPE: 2D LiDAR</span><span>RATE: 40Hz</span>
                    </div>
                    <button 
                        onClick={() => setIsConnected(!isConnected)}
                        onMouseEnter={() => setIsHovered(true)} onMouseLeave={() => setIsHovered(false)}
                        className={`w-full py-2.5 rounded text-xs font-bold transition-all duration-300 flex items-center justify-center gap-2
                            ${isConnected 
                                ? isHovered ? 'bg-red-500/10 text-red-500 border border-red-500/50' : 'bg-[#FF7F50]/10 text-[#FF7F50] border border-[#FF7F50]/30 shadow-[0_0_15px_rgba(255,127,80,0.15)]'
                                : 'bg-[#118AB2]/10 text-[#118AB2] border border-[#118AB2]/30'
                            }`}
                    >
                        <Power size={14} /> {isConnected ? (isHovered ? 'DISCONNECT' : 'CONNECTED') : 'CONNECT SENSOR'}
                    </button>
                </div>

                <div className="text-[10px] text-[#666] font-bold tracking-widest mb-3 uppercase mt-6">Available Devices</div>
                <button className="w-full py-4 rounded-lg border border-dashed border-[#333] text-[#666] text-xs font-semibold hover:text-[#E0E0E0] hover:border-[#E0E0E0] transition-colors flex flex-col items-center justify-center gap-2 bg-[#0a0a0a]">
                    <div className="p-2 rounded-full bg-[#1a1a1a]"><Activity size={16} /></div>
                    ADD SENSOR NODE
                </button>
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
        let animationFrameId;
        let time = 0;

        const draw = () => {
            const width = canvas.width;
            const height = canvas.height;
            const cx = width / 2;
            const cy = height / 2;
            const radius = Math.min(width, height) / 2 - 60;

            ctx.fillStyle = COLORS.bgApp;
            ctx.fillRect(0, 0, width, height);

            // Grid
            ctx.strokeStyle = `${COLORS.info}26`;
            ctx.lineWidth = 1;
            for (let i = 1; i <= 5; i++) {
                ctx.beginPath();
                ctx.arc(cx, cy, (radius / 5) * i, 0, 2 * Math.PI);
                ctx.stroke();
            }

            // ROI Polygon
            const roiPoints = [
                {x: cx - 220, y: cy - 140}, {x: cx + 250, y: cy - 180},
                {x: cx + 280, y: cy + 190}, {x: cx - 180, y: cy + 250},
            ];
            const isEditing = roiMode === 'edit';
            ctx.strokeStyle = isEditing ? COLORS.info : COLORS.primary; 
            ctx.fillStyle = isEditing ? `${COLORS.info}0D` : `${COLORS.primary}08`;
            ctx.lineWidth = isEditing ? 1.5 : 2;
            ctx.setLineDash(isEditing ? [5, 5] : []); 
            
            ctx.beginPath();
            ctx.moveTo(roiPoints[0].x, roiPoints[0].y);
            roiPoints.forEach(p => ctx.lineTo(p.x, p.y));
            ctx.closePath();
            ctx.fill();
            ctx.stroke();
            ctx.setLineDash([]);

            // Point Cloud Simulation
            ctx.fillStyle = COLORS.success;
            for(let i=0; i<120; i++) {
                const angle = Math.PI + (Math.random() * Math.PI / 1.2);
                const r = radius * 0.7 + (Math.random() * 20 - 10);
                const px = cx + Math.cos(angle)*r;
                const py = cy + Math.sin(angle)*r;
                if (px > cx - 220 && px < cx + 250 && py > cy - 180 && py < cy + 250) {
                    ctx.fillRect(px, py, 2.5, 2.5);
                }
            }

            // Sweep
            const sweepAngle = (time * 3) % (Math.PI * 2);
            ctx.strokeStyle = `${COLORS.warning}80`;
            ctx.lineWidth = 2;
            ctx.beginPath();
            ctx.moveTo(cx, cy); ctx.lineTo(cx + Math.cos(sweepAngle) * radius, cy + Math.sin(sweepAngle) * radius);
            ctx.stroke();

            // Tracked Blob
            time += 0.016;
            const blobX = cx + Math.sin(time * 0.5) * 150 + Math.cos(time * 1.2) * 20;
            const blobY = cy + Math.cos(time * 0.4) * 100 - Math.sin(time * 0.8) * 30;
            
            ctx.strokeStyle = COLORS.success;
            ctx.lineWidth = 1;
            ctx.setLineDash([3, 3]);
            ctx.strokeRect(blobX - 24, blobY - 24, 48, 48);
            ctx.setLineDash([]);

            ctx.strokeStyle = COLORS.primary;
            ctx.lineWidth = 1.5;
            ctx.beginPath();
            ctx.moveTo(blobX - 12, blobY); ctx.lineTo(blobX + 12, blobY);
            ctx.moveTo(blobX, blobY - 12); ctx.lineTo(blobX, blobY + 12);
            ctx.stroke();

            animationFrameId = window.requestAnimationFrame(draw);
        };
        draw();
        return () => window.cancelAnimationFrame(animationFrameId);
    }, [roiMode]);

    return (
        <div className="flex-grow flex flex-col relative bg-[#050505] min-w-[500px]">
            <div className="absolute top-4 left-4 right-4 flex justify-between z-10 pointer-events-none">
                <div className="flex gap-2 pointer-events-auto bg-[#121212]/90 backdrop-blur-sm border border-[#2a2a2a] rounded-lg p-1">
                    <button onClick={() => setRoiMode(roiMode === 'active' ? 'edit' : 'active')} className="px-3 py-1.5 rounded text-xs font-semibold transition-colors flex items-center gap-2" style={roiMode === 'edit' ? { backgroundColor: `${COLORS.info}26`, color: COLORS.info } : { color: '#888' }}>
                        <BoxSelect size={14} /> {roiMode === 'edit' ? 'FINISH ROI' : 'EDIT ROI'}
                    </button>
                </div>
            </div>
            <div className="flex-grow w-full h-full flex items-center justify-center overflow-hidden">
                <canvas ref={canvasRef} width={900} height={700} className="w-full h-full object-cover" />
            </div>
        </div>
    );
};

const TrackingPanel = () => {
    return (
        <div className="flex flex-col h-full bg-[#121212] border-l border-[#2a2a2a] w-[28%] min-w-[320px] max-w-[400px]">
            <div className="p-4 flex-grow flex flex-col overflow-hidden bg-[#0a0a0a]">
                <h2 className="text-[#E0E0E0] font-bold text-xs tracking-widest flex items-center gap-2 mb-4">
                    <Workflow size={14} style={{ color: COLORS.info }} /> PARAMETER ROUTING
                </h2>
                <div className="flex-grow relative bg-[#121212] border border-[#2a2a2a] rounded-lg p-4 flex justify-between overflow-hidden">
                    <svg className="absolute inset-0 w-full h-full pointer-events-none" style={{ zIndex: 1 }}>
                        <path d="M 120 70 C 180 70, 150 150, 220 150" stroke={COLORS.primary} strokeWidth="2.5" fill="none" className="animate-pulse" />
                    </svg>
                    <div className="flex flex-col gap-3 z-10 w-[45%] h-full">
                        <div className="text-[10px] text-[#555] font-bold tracking-widest mb-1 pb-1 border-b border-[#2a2a2a]">SOURCES</div>
                        <div className="bg-[#1a1a1a] border border-[#333] text-[#E0E0E0] text-[11px] font-medium py-2 px-3 rounded-md flex items-center justify-between relative group">
                            <span>Blob 1: X Pos</span><div className="w-2.5 h-2.5 rounded-full bg-[#FF7F50] absolute -right-1.5 border-2 border-[#1a1a1a]"></div>
                        </div>
                    </div>
                    <div className="flex flex-col gap-3 z-10 w-[45%] h-full text-right">
                        <div className="text-[10px] text-[#555] font-bold tracking-widest mb-1 pb-1 border-b border-[#2a2a2a]">TARGETS</div>
                        <div className="h-[32px]"></div>
                        <div className="bg-[#1a1a1a] text-[#FF7F50] border border-[#FF7F50]/50 text-[11px] font-bold py-2 px-3 rounded-md flex items-center justify-between flex-row-reverse relative shadow-[0_0_10px_rgba(255,127,80,0.15)]">
                            <span>FX: Hue Shift</span><div className="w-2.5 h-2.5 rounded-full bg-[#FF7F50] absolute -left-1.5 border-2 border-[#1a1a1a]"></div>
                        </div>
                    </div>
                </div>
            </div>
        </div>
    );
};

/* ==========================================
   VIEW 2: COMPOSITION (Main VJ UI)
   ========================================== */
const CompositionView = () => {
    return (
        <div className="flex-grow flex flex-col w-full h-full bg-[#0a0a0a] overflow-hidden">
            
            {/* TOP SECTION: 55% Height */}
            <div className="flex h-[55%] min-h-[350px] w-full shrink-0">
                
                {/* Left: Browser */}
                <div className="w-[240px] flex flex-col bg-[#121212] border-r border-[#2a2a2a] shrink-0">
                    <div className="p-3 border-b border-[#2a2a2a] flex items-center justify-between bg-[#1a1a1a]">
                        <span className="text-xs font-bold tracking-widest text-[#E0E0E0]">FILES & EFFECTS</span>
                        <FolderOpen size={14} className="text-[#888]" />
                    </div>
                    <div className="p-3 flex-grow overflow-y-auto">
                        {['Generators', 'VJ_Loops_2026', 'Audio_React', 'Masks', 'Transitions'].map((folder, i) => (
                            <div key={i} className="flex items-center gap-2 py-1.5 text-xs text-[#888] hover:text-[#E0E0E0] cursor-pointer">
                                <ChevronRight size={14} />
                                <FolderOpen size={14} className="text-[#118AB2]" />
                                {folder}
                            </div>
                        ))}
                        <div className="mt-4 pt-4 border-t border-[#2a2a2a]">
                            <div className="text-[10px] font-bold text-[#555] mb-2 uppercase tracking-widest">Video Effects</div>
                            {['Colorize', 'Glow', 'Hue Rotate', 'Pixelate', 'Transform'].map((fx, i) => (
                                <div key={i} className="flex items-center gap-2 py-1.5 text-xs text-[#aaa] hover:text-white cursor-pointer pl-4">
                                    <Sliders size={12} className="text-[#FFD166]" /> {fx}
                                </div>
                            ))}
                        </div>
                    </div>
                </div>

                {/* Center: Monitors Top */}
                <div className="flex-grow border-r border-[#2a2a2a] flex p-4 gap-4 bg-[#080808] min-w-[400px]">
                    {/* Preview Monitor */}
                    <div className="flex-1 flex flex-col">
                        <div className="flex justify-between items-center mb-2">
                            <span className="text-[10px] font-bold tracking-widest text-[#888]">PREVIEW</span>
                        </div>
                        <div className="flex-grow bg-[#121212] border border-[#2a2a2a] rounded relative overflow-hidden flex items-center justify-center shadow-inner">
                            <div className="w-full h-full bg-gradient-to-br from-[#118AB2]/10 to-[#080808] absolute"></div>
                            <span className="text-[#444] font-mono text-sm z-10">PREVIEW</span>
                        </div>
                    </div>
                    {/* Output Monitor */}
                    <div className="flex-1 flex flex-col">
                        <div className="flex justify-between items-center mb-2">
                            <span className="text-[10px] font-bold tracking-widest text-[#FF7F50]">ACTIVE COMPOSITION</span>
                            <span className="text-[10px] font-mono text-[#06D6A0] bg-[#06D6A0]/10 px-1.5 py-0.5 rounded">60.0 FPS</span>
                        </div>
                        <div className="flex-grow bg-[#000] border border-[#FF7F50]/40 rounded relative overflow-hidden shadow-[0_0_20px_rgba(255,127,80,0.15)]">
                             {/* Simulated Output Visual */}
                             <div className="absolute inset-0 bg-[radial-gradient(ellipse_at_center,_var(--tw-gradient-stops))] from-[#FF7F50]/40 via-[#121212] to-black"></div>
                             <div className="absolute top-1/2 left-1/2 -translate-x-1/2 -translate-y-1/2 w-32 h-32 border-4 border-[#FFD166]/50 rounded-full animate-[spin_4s_linear_infinite] border-t-[#FF7F50] mix-blend-screen"></div>
                        </div>
                    </div>
                </div>

                {/* Right: Inspector */}
                <div className="w-[280px] bg-[#121212] shrink-0 flex flex-col">
                    <div className="p-3 border-b border-[#2a2a2a] flex items-center justify-between bg-[#1a1a1a]">
                        <span className="text-xs font-bold tracking-widest text-[#E0E0E0]">PROPERTIES</span>
                        <LayoutGrid size={14} className="text-[#FF7F50]" />
                    </div>
                    
                    <div className="p-4 flex-grow overflow-y-auto space-y-6">
                        {/* Transform Section */}
                        <div>
                            <div className="text-[10px] font-bold text-[#555] mb-3 uppercase tracking-widest">Transform</div>
                            
                            <div className="mb-3">
                                <div className="flex justify-between text-xs text-[#aaa] mb-1">
                                    <span>Scale</span> <span className="font-mono text-[#06D6A0]">100.0%</span>
                                </div>
                                <div className="h-1.5 bg-[#050505] rounded-full border border-[#222] relative cursor-pointer">
                                    <div className="absolute h-full bg-[#118AB2] w-1/2"></div>
                                    <div className="absolute top-1/2 -translate-y-1/2 left-1/2 w-2.5 h-2.5 bg-white rounded-full shadow"></div>
                                </div>
                            </div>

                            <div className="flex gap-2">
                                <div className="flex-1">
                                    <div className="text-xs text-[#aaa] mb-1">Pos X</div>
                                    <div className="bg-[#050505] border border-[#222] rounded p-1 text-xs text-center font-mono hover:border-[#118AB2] cursor-ns-resize">0.00</div>
                                </div>
                                <div className="flex-1">
                                    <div className="text-xs text-[#aaa] mb-1">Pos Y</div>
                                    <div className="bg-[#050505] border border-[#222] rounded p-1 text-xs text-center font-mono hover:border-[#118AB2] cursor-ns-resize">0.00</div>
                                </div>
                            </div>
                        </div>

                        {/* Effects Pipeline */}
                        <div>
                            <div className="text-[10px] font-bold text-[#555] mb-3 uppercase tracking-widest flex justify-between">
                                Effects <Plus size={12} className="text-[#E0E0E0] cursor-pointer hover:text-white" />
                            </div>
                            
                            <div className="bg-[#1a1a1a] border border-[#FF7F50]/50 rounded mb-2 overflow-hidden shadow-sm">
                                <div className="p-2 flex justify-between items-center bg-[#FF7F50]/10 border-b border-[#FF7F50]/20">
                                    <div className="flex items-center gap-2 text-xs font-bold text-[#FF7F50]">
                                        <Power size={12} /> Hue Shift
                                    </div>
                                    <LinkIcon size={12} className="text-[#06D6A0]" />
                                </div>
                                <div className="p-3">
                                    <div className="flex justify-between text-xs text-[#aaa] mb-1">
                                        <span>Hue</span> <span className="font-mono text-[#FF7F50]">180°</span>
                                    </div>
                                    <div className="h-1.5 bg-[#050505] rounded-full border border-[#222] overflow-hidden">
                                        <div className="h-full bg-gradient-to-r from-red-500 via-green-500 to-blue-500 w-[50%]"></div>
                                    </div>
                                    <div className="text-[9px] text-[#06D6A0] mt-2 font-mono flex items-center gap-1 bg-[#06D6A0]/10 p-1 rounded">
                                        <Radio size={10} className="animate-pulse" /> Mapped: Blob 1 X
                                    </div>
                                </div>
                            </div>
                        </div>
                    </div>
                </div>
            </div>

            {/* BOTTOM SECTION: Full-Width Layers/Decks */}
            <div className="flex-grow flex flex-col bg-[#0a0a0a] border-t border-[#2a2a2a] overflow-hidden">
                {/* Columns Header (like Resolume triggers) */}
                <div className="h-7 bg-[#121212] flex items-center pl-[220px] shrink-0 border-b border-[#2a2a2a]">
                    <div className="flex gap-1 px-1 w-full overflow-hidden">
                        {[1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12].map(col => (
                            <div key={col} className="min-w-[120px] w-[120px] text-center">
                                <span className="text-[9px] font-bold text-[#555] hover:text-[#E0E0E0] cursor-pointer">COL {col}</span>
                            </div>
                        ))}
                    </div>
                </div>

                {/* Layers Container */}
                <div className="flex-grow p-2 overflow-y-auto overflow-x-hidden flex flex-col gap-1.5 bg-[#121212]">
                    {[3, 2, 1].map((layerIndex) => (
                        <div key={layerIndex} className="flex h-[90px] bg-[#1a1a1a] rounded border border-[#2a2a2a] shrink-0 group">
                            
                            {/* Layer Controls (Fixed Width) */}
                            <div className="w-[220px] p-2.5 bg-[#161616] border-r border-[#2a2a2a] shrink-0 flex flex-col justify-between">
                                <div className="flex justify-between items-center">
                                    <span className="text-xs font-bold text-[#E0E0E0] group-hover:text-[#FF7F50] transition-colors">Layer {layerIndex}</span>
                                    <div className="flex gap-1">
                                        <button className="w-5 h-5 bg-[#222] text-[9px] rounded hover:bg-[#333] border border-[#333]">V</button>
                                        <button className="w-5 h-5 bg-[#222] text-[9px] rounded hover:bg-[#118AB2] hover:text-white border border-[#333]">S</button>
                                        <button className="w-5 h-5 bg-[#222] text-[9px] rounded hover:bg-red-900 hover:text-red-300 border border-[#333]">B</button>
                                    </div>
                                </div>
                                {/* Opacity Slider */}
                                <div className="mt-1">
                                    <div className="flex justify-between text-[9px] text-[#888] mb-1 font-mono">
                                        <span>OPACITY</span> <span>{layerIndex === 3 ? '100%' : '40%'}</span>
                                    </div>
                                    <div className="h-2 bg-[#050505] rounded-full w-full border border-[#222] relative cursor-pointer">
                                        <div className={`absolute top-0 left-0 h-full rounded-full ${layerIndex === 3 ? 'bg-[#FF7F50] w-full shadow-[0_0_8px_rgba(255,127,80,0.5)]' : 'bg-[#118AB2] w-[40%]'}`}></div>
                                        <div className="absolute top-1/2 -translate-y-1/2 w-3 h-3 bg-white rounded-full shadow" style={{ left: layerIndex === 3 ? 'calc(100% - 12px)' : 'calc(40% - 6px)' }}></div>
                                    </div>
                                </div>
                            </div>
                            
                            {/* Clips Grid (Horizontal Scrollable) */}
                            <div className="flex-grow flex gap-1 p-1 overflow-x-auto custom-scrollbar">
                                {[1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12].map((clipIndex) => {
                                    const isActive = layerIndex === 3 && clipIndex === 2;
                                    const isLoaded = (layerIndex === 3 && clipIndex < 6) || (layerIndex === 2 && (clipIndex === 1 || clipIndex === 4));
                                    
                                    return (
                                        <div 
                                            key={clipIndex} 
                                            className={`min-w-[120px] w-[120px] rounded relative overflow-hidden cursor-pointer transition-all border
                                                ${isActive ? 'border-[#FF7F50] bg-[#FF7F50]/10 shadow-[0_0_10px_rgba(255,127,80,0.2)]' : 
                                                  isLoaded ? 'border-[#333] bg-[#222] hover:border-[#118AB2]' : 'border-[#1a1a1a] bg-[#111] hover:bg-[#161616]'}`}
                                        >
                                            {isLoaded && (
                                                <>
                                                    <div className="absolute top-1 left-1 text-[9px] font-bold text-white bg-black/60 px-1 rounded z-10 backdrop-blur-sm">Clip {clipIndex}</div>
                                                    {/* Thumbnail dummy abstract gradient */}
                                                    <div className={`absolute inset-0 opacity-40 mix-blend-screen ${isActive ? 'bg-gradient-to-br from-[#FF7F50] to-[#FFD166]' : 'bg-gradient-to-tr from-[#118AB2] to-[#06D6A0]'}`}></div>
                                                </>
                                            )}
                                            
                                            {/* Playing Progress Bar */}
                                            {isActive && (
                                                <div className="absolute bottom-0 left-0 h-1.5 bg-[#FF7F50] w-[65%] shadow-[0_0_8px_#FF7F50] transition-all"></div>
                                            )}
                                        </div>
                                    )
                                })}
                            </div>
                        </div>
                    ))}
                </div>
            </div>
        </div>
    );
};

/* ==========================================
   VIEW 3: ADVANCED MAPPING
   ========================================== */
const MappingView = () => {
    const [mappingMode, setMappingMode] = useState('input'); // 'input' or 'output'
    const [showRightSidebar, setShowRightSidebar] = useState(true);
    const [showLeftSidebar, setShowLeftSidebar] = useState(true);

    /* TÌNH TRẠNG DỮ LIỆU CÂY PHÂN CẤP (MOCK DATA) */
    const [screens, setScreens] = useState([
        {
            id: 'scr_1', name: 'Main LED Wall', output: 'Display 1 (1920x1080)', expanded: true,
            slices: [
                {
                    id: 'slc_1', name: 'Center Slice', expanded: true,
                    masks: [{ id: 'msk_1', name: 'Pillar Mask' }]
                },
                {
                    id: 'slc_2', name: 'Side Wing L', expanded: false,
                    masks: []
                }
            ]
        },
        {
            id: 'scr_2', name: 'DJ Booth Projector', output: 'Projector 2 (1920x1080)', expanded: true,
            slices: [
                { id: 'slc_3', name: 'Front Face', expanded: false, masks: [] }
            ]
        }
    ]);
    
    // Quản lý item đang được chọn (Screen, Slice, hoặc Mask)
    const [selectedItem, setSelectedItem] = useState({ id: 'slc_1', type: 'slice' });

    /* Helper để render icon tùy theo loại */
    const getItemIcon = (type, active) => {
        const color = active ? COLORS.primary : '#888';
        if (type === 'screen') return <Monitor size={14} style={{ color }} />;
        if (type === 'slice') return <Crop size={14} style={{ color }} />;
        if (type === 'mask') return <Scissors size={14} style={{ color }} />;
        return null;
    };

    return (
        <div className="flex-grow flex flex-col w-full h-full bg-[#050505]">
            {/* Thanh công cụ Toolbar */}
            <div className="h-14 border-b border-[#2a2a2a] bg-[#121212] flex items-center justify-between px-4 relative shrink-0 z-20">
                <div className="flex gap-2 items-center">
                    {/* Nút bật/tắt Screen Setup */}
                    <button 
                        onClick={() => setShowLeftSidebar(!showLeftSidebar)}
                        className={`p-2 rounded border transition-colors mr-2 ${showLeftSidebar ? 'bg-[#222] text-[#E0E0E0] border-[#444]' : 'bg-[#1a1a1a] text-[#888] border-transparent hover:text-[#E0E0E0] hover:border-[#333]'}`}
                        title="Toggle Screen Setup"
                    >
                        <ListTree size={16} />
                    </button>
                    
                    <button className="p-2 rounded bg-[#222] text-[#E0E0E0] border border-[#444] hover:bg-[#333] transition-colors"><MousePointer2 size={16} /></button>
                    <button className="p-2 rounded bg-[#1a1a1a] text-[#888] hover:text-[#E0E0E0] border border-transparent hover:border-[#333] transition-colors"><Move size={16} /></button>
                    <div className="w-px h-6 bg-[#333] mx-2 mt-1"></div>
                    
                    <button className="px-3 py-1.5 rounded text-xs font-medium bg-[#1a1a1a] border border-[#333] hover:bg-[#222] flex items-center gap-2 text-[#E0E0E0] transition-colors">
                        <Plus size={14} className="text-[#06D6A0]" /> Add Slice
                    </button>
                    <button className="px-3 py-1.5 rounded text-xs font-medium bg-[#1a1a1a] border border-[#333] hover:bg-[#222] flex items-center gap-2 text-[#E0E0E0] transition-colors">
                        <Plus size={14} className="text-[#118AB2]" /> Add Mask
                    </button>
                </div>

                {/* Mode Toggle - Switch Input/Output chung một cửa sổ */}
                <div className="absolute left-1/2 -translate-x-1/2 flex bg-[#0a0a0a] p-1 rounded-lg border border-[#2a2a2a] shadow-inner">
                    <button 
                        onClick={() => setMappingMode('input')}
                        className={`px-8 py-1.5 rounded-md text-[11px] font-bold transition-all flex items-center gap-2
                            ${mappingMode === 'input' 
                                ? 'bg-[#1a1a1a] text-[#06D6A0] border border-[#333] shadow-[0_0_15px_rgba(6,214,160,0.15)]' 
                                : 'text-[#666] hover:text-[#E0E0E0] border border-transparent'}`}
                    >
                        INPUT SELECTION
                    </button>
                    <button 
                        onClick={() => setMappingMode('output')}
                        className={`px-8 py-1.5 rounded-md text-[11px] font-bold transition-all flex items-center gap-2
                            ${mappingMode === 'output' 
                                ? 'bg-[#1a1a1a] text-[#FF7F50] border border-[#333] shadow-[0_0_15px_rgba(255,127,80,0.15)]' 
                                : 'text-[#666] hover:text-[#E0E0E0] border border-transparent'}`}
                    >
                        OUTPUT ROUTING
                    </button>
                </div>

                <div className="flex gap-3 text-xs items-center">
                    <button className="px-4 py-1.5 rounded bg-[#118AB2]/10 text-[#118AB2] border border-[#118AB2]/30 font-bold hover:bg-[#118AB2]/20 transition-colors">Test Card</button>
                    <button className="px-4 py-1.5 rounded bg-[#FF7F50] text-[#000] font-bold hover:bg-[#FFD166] transition-colors shadow-[0_0_10px_rgba(255,127,80,0.3)]">Apply</button>
                    <div className="w-px h-6 bg-[#333] mx-1"></div>
                    {/* Toggle Sidebar Button */}
                    <button 
                        onClick={() => setShowRightSidebar(!showRightSidebar)}
                        className={`p-2 rounded border transition-colors ${showRightSidebar ? 'bg-[#222] text-[#E0E0E0] border-[#444]' : 'bg-[#1a1a1a] text-[#888] border-transparent hover:text-[#E0E0E0] hover:border-[#333]'}`}
                        title="Toggle Properties Panel"
                    >
                        <LayoutGrid size={16} />
                    </button>
                </div>
            </div>

            {/* Vùng làm việc chính gồm 3 cột */}
            <div className="flex-grow flex overflow-hidden">
                
                {/* TRÁI: Screen Setup (Danh sách cây) */}
                <div className={`bg-[#121212] border-r border-[#2a2a2a] shrink-0 transition-all duration-300 ease-[cubic-bezier(0.4,0,0.2,1)] flex flex-col overflow-hidden ${showLeftSidebar ? 'w-[260px]' : 'w-0 border-r-0'}`}>
                    <div className="p-3 border-b border-[#2a2a2a] bg-[#1a1a1a] shrink-0 flex justify-between items-center w-[260px]">
                        <span className="text-xs font-bold tracking-widest text-[#E0E0E0]">SCREEN SETUP</span>
                        <Plus size={14} className="text-[#888] cursor-pointer hover:text-white" />
                    </div>
                    <div className="flex-grow overflow-y-auto w-[260px] py-2">
                        {screens.map(screen => (
                            <div key={screen.id} className="mb-1">
                                {/* Screen Item */}
                                <div 
                                    onClick={() => setSelectedItem({ id: screen.id, type: 'screen' })}
                                    className={`flex items-center gap-2 px-3 py-1.5 cursor-pointer select-none transition-colors ${selectedItem.id === screen.id ? 'bg-[#118AB2]/20 border-l-2 border-[#118AB2]' : 'hover:bg-[#1a1a1a] border-l-2 border-transparent'}`}
                                >
                                    <ChevronDown size={14} className="text-[#555]" />
                                    {getItemIcon('screen', selectedItem.id === screen.id)}
                                    <span className={`text-xs font-bold ${selectedItem.id === screen.id ? 'text-white' : 'text-[#E0E0E0]'}`}>{screen.name}</span>
                                </div>
                                
                                {/* Slices */}
                                {screen.expanded && screen.slices.map(slice => (
                                    <div key={slice.id}>
                                        <div 
                                            onClick={() => setSelectedItem({ id: slice.id, type: 'slice' })}
                                            className={`flex items-center gap-2 pl-8 pr-3 py-1.5 cursor-pointer select-none transition-colors ${selectedItem.id === slice.id ? 'bg-[#FF7F50]/20 border-l-2 border-[#FF7F50]' : 'hover:bg-[#1a1a1a] border-l-2 border-transparent'}`}
                                        >
                                            {slice.masks.length > 0 ? <ChevronDown size={12} className="text-[#555]" /> : <div className="w-3"></div>}
                                            {getItemIcon('slice', selectedItem.id === slice.id)}
                                            <span className={`text-[11px] ${selectedItem.id === slice.id ? 'text-[#FF7F50] font-bold' : 'text-[#aaa]'}`}>{slice.name}</span>
                                        </div>

                                        {/* Masks */}
                                        {slice.expanded && slice.masks.map(mask => (
                                            <div 
                                                key={mask.id}
                                                onClick={() => setSelectedItem({ id: mask.id, type: 'mask' })}
                                                className={`flex items-center gap-2 pl-14 pr-3 py-1.5 cursor-pointer select-none transition-colors ${selectedItem.id === mask.id ? 'bg-[#FFD166]/20 border-l-2 border-[#FFD166]' : 'hover:bg-[#1a1a1a] border-l-2 border-transparent'}`}
                                            >
                                                {getItemIcon('mask', selectedItem.id === mask.id)}
                                                <span className={`text-[11px] ${selectedItem.id === mask.id ? 'text-[#FFD166] font-bold' : 'text-[#888]'}`}>{mask.name}</span>
                                            </div>
                                        ))}
                                    </div>
                                ))}
                            </div>
                        ))}
                    </div>
                </div>

                {/* GIỮA: Canvas */}
                <div className="flex-grow flex p-4 gap-4 bg-[url('data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAoAAAAKCAYAAACNMs+9AAAAAXNSR0IArs4c6QAAACVJREFUKFNjZCASMDKgAnv37v3/n52dHWyK0DQDjQzR1TDFKEEBAAAzJRAUjO2NAAAAAElFTkSuQmCC')] transition-all duration-300 relative z-0">
                    
                    <div className="flex-grow bg-[#000] border-2 border-[#2a2a2a] relative shadow-2xl overflow-hidden flex flex-col rounded-md">
                        
                        {/* Status Label Overlay */}
                        <div className="absolute top-4 left-4 z-10 pointer-events-none">
                            <h3 className="text-[10px] font-bold text-[#888] tracking-widest bg-[#121212]/90 px-4 py-1.5 rounded-md border border-[#333] backdrop-blur-sm shadow-lg flex items-center gap-2">
                                {mappingMode === 'input' ? (
                                    <><div className="w-2 h-2 rounded-full bg-[#06D6A0] animate-pulse"></div> INPUT SOURCE: COMPOSITION</>
                                ) : (
                                    <><div className="w-2 h-2 rounded-full bg-[#FF7F50] animate-pulse"></div> OUTPUT DESTINATION: PROJECTORS</>
                                )}
                            </h3>
                        </div>

                        {/* Workspace Content */}
                        {mappingMode === 'input' ? (
                            <>
                                {/* Composition Dummy Background */}
                                <div className="absolute inset-0 bg-gradient-to-br from-[#121212] to-[#1a1a1a]"></div>
                                <div className="absolute inset-0 bg-[linear-gradient(rgba(255,255,255,0.03)_1px,transparent_1px),linear-gradient(90deg,rgba(255,255,255,0.03)_1px,transparent_1px)] bg-[size:40px_40px]"></div>
                                
                                {/* Slice 1 Input */}
                                <div className={`absolute top-[20%] left-[25%] w-[40%] h-[50%] border ${selectedItem.id === 'slc_1' ? 'border-[#06D6A0] bg-[#06D6A0]/20 z-10' : 'border-[#06D6A0]/50 bg-[#06D6A0]/5'} cursor-move group hover:bg-[#06D6A0]/30 transition-colors`}>
                                    <div className="absolute -top-6 left-0 text-[11px] text-[#06D6A0] font-mono bg-[#06D6A0]/20 px-2 py-0.5 rounded border border-[#06D6A0]/30 shadow-sm">Center Slice</div>
                                    {selectedItem.id === 'slc_1' && (
                                        <>
                                            <div className="absolute -top-1.5 -left-1.5 w-3 h-3 bg-[#121212] border border-[#06D6A0] cursor-nwse-resize hover:scale-150 transition-all"></div>
                                            <div className="absolute -top-1.5 -right-1.5 w-3 h-3 bg-[#121212] border border-[#06D6A0] cursor-nesw-resize hover:scale-150 transition-all"></div>
                                            <div className="absolute -bottom-1.5 -left-1.5 w-3 h-3 bg-[#121212] border border-[#06D6A0] cursor-nesw-resize hover:scale-150 transition-all"></div>
                                            <div className="absolute -bottom-1.5 -right-1.5 w-3 h-3 bg-[#121212] border border-[#06D6A0] cursor-nwse-resize hover:scale-150 transition-all"></div>
                                        </>
                                    )}
                                </div>

                                {/* Slice 2 Input */}
                                <div className={`absolute top-[20%] left-[68%] w-[20%] h-[50%] border ${selectedItem.id === 'slc_2' ? 'border-[#06D6A0] bg-[#06D6A0]/20 z-10' : 'border-[#06D6A0]/50 bg-[#06D6A0]/5'} cursor-move group hover:bg-[#06D6A0]/30 transition-colors`}>
                                    <div className="absolute -top-6 left-0 text-[11px] text-[#06D6A0] font-mono bg-[#06D6A0]/20 px-2 py-0.5 rounded border border-[#06D6A0]/30 shadow-sm">Side Wing L</div>
                                </div>
                            </>
                        ) : (
                            <>
                                {/* Projector Dummy Background */}
                                <div className="absolute inset-0 bg-[#050505]"></div>
                                
                                {/* Ghosting Input */}
                                <div className="absolute top-[20%] left-[25%] w-[40%] h-[50%] border border-[#333] border-dashed pointer-events-none opacity-40"></div>

                                {/* Slice 1 Output (Warped) */}
                                <div className={`absolute top-[20%] left-[25%] w-[40%] h-[50%] border-2 ${selectedItem.id === 'slc_1' ? 'border-[#FF7F50] bg-[#FF7F50]/20 z-10 shadow-[0_0_30px_rgba(255,127,80,0.2)]' : 'border-[#FF7F50]/50 bg-[#FF7F50]/5'} cursor-move group transition-all`} 
                                     style={{ transform: 'perspective(1200px) rotateY(15deg) rotateX(5deg) scale(0.95)' }}>
                                    <div className="absolute -top-7 left-0 text-[11px] text-[#FF7F50] font-mono font-bold bg-[#FF7F50]/20 px-2 py-0.5 rounded shadow border border-[#FF7F50]/30">Center Slice</div>
                                    
                                    {selectedItem.id === 'slc_1' && (
                                        <>
                                            <div className="absolute -top-2.5 -left-2.5 w-5 h-5 bg-[#FF7F50] rounded-full border-2 border-[#121212] shadow-[0_0_10px_#FF7F50] cursor-pointer hover:scale-125 z-10"></div>
                                            <div className="absolute -top-2.5 -right-2.5 w-5 h-5 bg-[#FF7F50] rounded-full border-2 border-[#121212] shadow-[0_0_10px_#FF7F50] cursor-pointer hover:scale-125 z-10"></div>
                                            <div className="absolute -bottom-2.5 -left-2.5 w-5 h-5 bg-[#FF7F50] rounded-full border-2 border-[#121212] shadow-[0_0_10px_#FF7F50] cursor-pointer hover:scale-125 z-10"></div>
                                            <div className="absolute -bottom-2.5 -right-2.5 w-5 h-5 bg-[#FF7F50] rounded-full border-2 border-[#121212] shadow-[0_0_10px_#FF7F50] cursor-pointer hover:scale-125 z-10"></div>
                                            
                                            {/* Bezier Guide Lines */}
                                            <svg className="absolute inset-0 w-full h-full pointer-events-none overflow-visible" style={{ zIndex: 5 }}>
                                                <line x1="0" y1="0" x2="-45" y2="-30" stroke="#FF7F50" strokeWidth="1.5" strokeDasharray="4 4" />
                                                <circle cx="-45" cy="-30" r="5" fill="#121212" stroke="#FF7F50" strokeWidth="2" />
                                            </svg>
                                        </>
                                    )}

                                    {/* Inner Grid */}
                                    <div className="absolute inset-0 bg-[linear-gradient(rgba(255,255,255,0.1)_1px,transparent_1px),linear-gradient(90deg,rgba(255,255,255,0.1)_1px,transparent_1px)] bg-[size:30px_30px]"></div>

                                    {/* MASK visualization inside Slice 1 */}
                                    {selectedItem.id === 'msk_1' && (
                                        <div className="absolute top-[20%] left-[40%] w-[20%] h-[60%] bg-[#000]/80 border-2 border-[#FFD166] border-dashed shadow-[inset_0_0_20px_#000]">
                                            <div className="absolute -top-5 left-0 text-[9px] text-[#FFD166] font-mono bg-black px-1 border border-[#FFD166]">Mask</div>
                                            <div className="absolute -top-1.5 -left-1.5 w-3 h-3 bg-[#121212] border border-[#FFD166] rounded-full"></div>
                                            <div className="absolute -top-1.5 -right-1.5 w-3 h-3 bg-[#121212] border border-[#FFD166] rounded-full"></div>
                                            <div className="absolute -bottom-1.5 -left-1.5 w-3 h-3 bg-[#121212] border border-[#FFD166] rounded-full"></div>
                                            <div className="absolute -bottom-1.5 -right-1.5 w-3 h-3 bg-[#121212] border border-[#FFD166] rounded-full"></div>
                                        </div>
                                    )}
                                </div>
                            </>
                        )}
                    </div>
                </div>

                {/* PHẢI: Properties Sidebar */}
                <div className={`bg-[#121212] border-l border-[#2a2a2a] shrink-0 transition-all duration-300 ease-[cubic-bezier(0.4,0,0.2,1)] flex flex-col overflow-hidden ${showRightSidebar ? 'w-[280px]' : 'w-0 border-l-0'}`}>
                    <div className="p-3 border-b border-[#2a2a2a] bg-[#1a1a1a] shrink-0 w-[280px] flex items-center gap-2">
                        {getItemIcon(selectedItem.type, true)}
                        <span className="text-xs font-bold tracking-widest text-[#E0E0E0] uppercase">{selectedItem.type} SETTINGS</span>
                    </div>
                    
                    <div className="p-4 space-y-5 flex-grow overflow-y-auto w-[280px]">
                        {/* Dynamic Properties based on Type */}
                        
                        {/* SCREEN PROPERTIES */}
                        {selectedItem.type === 'screen' && (
                            <>
                                <div>
                                    <div className="text-xs text-[#aaa] mb-1.5 font-medium">Screen Name</div>
                                    <input type="text" defaultValue="Main LED Wall" className="w-full bg-[#050505] border border-[#333] rounded px-3 py-2 text-xs text-[#E0E0E0] outline-none focus:border-[#118AB2]" />
                                </div>
                                <div className="pt-3 border-t border-[#2a2a2a]">
                                    <div className="text-[10px] font-bold text-[#555] uppercase tracking-widest mb-2">Device Routing</div>
                                    <select className="w-full bg-[#050505] border border-[#333] rounded px-2 py-2 text-xs text-[#E0E0E0] outline-none focus:border-[#118AB2] mb-3">
                                        <option>Display 1 (1920x1080)</option>
                                        <option>Display 2 (1920x1080)</option>
                                        <option>NDI Output 1</option>
                                        <option>Spout Sender</option>
                                    </select>
                                    <div className="flex items-center justify-between bg-[#1a1a1a] p-2 rounded border border-[#333]">
                                        <span className="text-xs text-[#E0E0E0]">Virtual Resolution</span>
                                        <span className="text-xs font-mono text-[#06D6A0]">1920x1080</span>
                                    </div>
                                </div>
                                <div className="pt-3 border-t border-[#2a2a2a]">
                                    <div className="text-[10px] font-bold text-[#555] uppercase tracking-widest mb-3">Color Adjustments</div>
                                    <div className="mb-3">
                                        <div className="flex justify-between text-[10px] text-[#888] mb-1"><span>Brightness</span><span>100%</span></div>
                                        <div className="h-1.5 bg-[#050505] rounded-full border border-[#222] relative"><div className="absolute h-full bg-[#118AB2] w-full"></div></div>
                                    </div>
                                    <div className="mb-3">
                                        <div className="flex justify-between text-[10px] text-[#888] mb-1"><span>Contrast</span><span>0</span></div>
                                        <div className="h-1.5 bg-[#050505] rounded-full border border-[#222] relative"><div className="absolute h-full bg-[#118AB2] w-1/2"></div></div>
                                    </div>
                                </div>
                            </>
                        )}

                        {/* SLICE PROPERTIES */}
                        {selectedItem.type === 'slice' && (
                            <>
                                <div>
                                    <div className="text-xs text-[#aaa] mb-1.5 font-medium">Slice Name</div>
                                    <input type="text" defaultValue="Center Slice" className="w-full bg-[#050505] border border-[#333] rounded px-3 py-2 text-xs text-[#FF7F50] outline-none focus:border-[#FF7F50]" />
                                </div>
                                <div className="pt-3 border-t border-[#2a2a2a]">
                                    <div className="text-[10px] font-bold text-[#555] uppercase tracking-widest mb-2">Input Source</div>
                                    <select className="w-full bg-[#050505] border border-[#333] rounded px-2 py-2 text-xs text-[#E0E0E0] outline-none focus:border-[#06D6A0]">
                                        <option>Main Composition</option>
                                        <option>Layer 1</option>
                                        <option>Layer 2</option>
                                    </select>
                                </div>
                                <div className="pt-3 border-t border-[#2a2a2a]">
                                    <div className="text-[10px] font-bold text-[#555] uppercase tracking-widest mb-3">Output Warping</div>
                                    <div className="flex bg-[#0a0a0a] p-1 rounded border border-[#2a2a2a] mb-3">
                                        <button className="flex-1 py-1.5 bg-[#1a1a1a] text-[#FF7F50] border border-[#333] shadow-[0_0_10px_rgba(255,127,80,0.1)] rounded text-[10px] font-bold">Perspective</button>
                                        <button className="flex-1 py-1.5 text-[#888] hover:text-[#E0E0E0] rounded text-[10px]">Bezier</button>
                                    </div>
                                    <div className="flex gap-2">
                                        <div className="flex-1">
                                            <div className="text-[10px] text-[#666] mb-1 font-medium">Point X</div>
                                            <div className="bg-[#050505] border border-[#333] hover:border-[#FF7F50] rounded p-1.5 text-xs text-center font-mono text-[#E0E0E0] cursor-ns-resize">350.5</div>
                                        </div>
                                        <div className="flex-1">
                                            <div className="text-[10px] text-[#666] mb-1 font-medium">Point Y</div>
                                            <div className="bg-[#050505] border border-[#333] hover:border-[#FF7F50] rounded p-1.5 text-xs text-center font-mono text-[#E0E0E0] cursor-ns-resize">220.0</div>
                                        </div>
                                    </div>
                                </div>
                                <div className="pt-3 border-t border-[#2a2a2a]">
                                     <button className="w-full py-2.5 rounded border border-[#118AB2] text-[#118AB2] text-xs font-semibold hover:bg-[#118AB2]/10 transition-colors flex justify-center items-center gap-2">
                                        <Focus size={14} /> Edge Blending
                                     </button>
                                </div>
                            </>
                        )}

                        {/* MASK PROPERTIES */}
                        {selectedItem.type === 'mask' && (
                            <>
                                <div>
                                    <div className="text-xs text-[#aaa] mb-1.5 font-medium">Mask Name</div>
                                    <input type="text" defaultValue="Pillar Mask" className="w-full bg-[#050505] border border-[#333] rounded px-3 py-2 text-xs text-[#FFD166] outline-none focus:border-[#FFD166]" />
                                </div>
                                <div className="pt-3 border-t border-[#2a2a2a]">
                                    <div className="text-[10px] font-bold text-[#555] uppercase tracking-widest mb-3">Mask Settings</div>
                                    
                                    <div className="flex items-center justify-between bg-[#1a1a1a] p-2.5 rounded border border-[#333] mb-3">
                                        <span className="text-xs text-[#E0E0E0]">Invert Mask</span>
                                        <div className="w-8 h-4 bg-[#FFD166] rounded-full relative cursor-pointer shadow-[0_0_8px_rgba(255,209,102,0.3)]">
                                            <div className="w-3 h-3 bg-[#121212] rounded-full absolute top-0.5 right-0.5"></div>
                                        </div>
                                    </div>

                                    <div className="mb-3">
                                        <div className="flex justify-between text-[10px] text-[#888] mb-1"><span>Feathering</span><span className="font-mono text-[#FFD166]">15 px</span></div>
                                        <div className="h-1.5 bg-[#050505] rounded-full border border-[#222] relative cursor-pointer">
                                            <div className="absolute h-full bg-[#FFD166] w-[15%]"></div>
                                            <div className="absolute top-1/2 -translate-y-1/2 left-[15%] w-2.5 h-2.5 bg-white rounded-full shadow"></div>
                                        </div>
                                    </div>
                                </div>
                            </>
                        )}
                    </div>
                </div>
            </div>
        </div>
    );
};

/* ==========================================
   MAIN APP SHELL
   ========================================== */
export default function App() {
    const [activeTab, setActiveTab] = useState('comp'); // 'comp', 'mapping', 'sensor'

    return (
        <div className="h-screen w-full bg-[#080808] flex flex-col font-sans overflow-hidden select-none">
            {/* Top Navigation Bar */}
            <div className="h-14 bg-[#121212] border-b border-[#2a2a2a] flex items-center justify-between px-5 shrink-0 z-20 shadow-md">
                <div className="flex items-center gap-8">
                    {/* Brand / Logo */}
                    <div className="flex items-center gap-2.5">
                        <div className="w-5 h-5 flex items-center justify-center rounded-sm bg-gradient-to-br from-[#FF7F50] to-[#FFD166] shadow-[0_0_10px_rgba(255,127,80,0.5)]">
                            <div className="w-2 h-2 bg-[#121212] rounded-sm"></div>
                        </div>
                        <h1 className="text-lg font-black text-[#E0E0E0] tracking-tighter">MIKMAP</h1>
                    </div>
                    
                    {/* View Switcher Tabs */}
                    <div className="flex bg-[#0a0a0a] p-1 rounded-lg border border-[#2a2a2a]">
                        <button 
                            onClick={() => setActiveTab('comp')}
                            className={`px-4 py-1.5 rounded-md text-[11px] font-bold flex items-center gap-2 transition-all
                                ${activeTab === 'comp' 
                                    ? 'bg-[#1a1a1a] text-[#FF7F50] border border-[#333] shadow-[0_0_10px_rgba(255,127,80,0.1)]' 
                                    : 'text-[#666] hover:text-[#E0E0E0]'}`}
                        >
                            <Layers size={14} /> COMPOSITION
                        </button>
                        <button 
                            onClick={() => setActiveTab('mapping')}
                            className={`px-4 py-1.5 rounded-md text-[11px] font-bold flex items-center gap-2 transition-all
                                ${activeTab === 'mapping' 
                                    ? 'bg-[#1a1a1a] text-[#FF7F50] border border-[#333] shadow-[0_0_10px_rgba(255,127,80,0.1)]' 
                                    : 'text-[#666] hover:text-[#E0E0E0]'}`}
                        >
                            <MonitorPlay size={14} /> ADVANCED MAPPING
                        </button>
                        <button 
                            onClick={() => setActiveTab('sensor')}
                            className={`px-4 py-1.5 rounded-md text-[11px] font-bold flex items-center gap-2 transition-all
                                ${activeTab === 'sensor' 
                                    ? 'bg-[#1a1a1a] text-[#FF7F50] border border-[#333] shadow-[0_0_10px_rgba(255,127,80,0.1)]' 
                                    : 'text-[#666] hover:text-[#E0E0E0]'}`}
                        >
                            <Activity size={14} /> SENSOR I/O
                        </button>
                    </div>
                </div>
                
                {/* Global Master Status */}
                <div className="flex items-center gap-5 text-[11px] font-mono">
                    <div className="flex items-center gap-2 bg-[#0a0a0a] px-3 py-1.5 rounded border border-[#2a2a2a]">
                        <span className="text-[#666]">FPS:</span>
                        <span className="text-[#06D6A0] font-bold">60.0</span>
                    </div>
                    <div className="flex items-center gap-2 text-[#888]">
                        <div className="w-2 h-2 rounded-full bg-[#06D6A0] shadow-[0_0_5px_#06D6A0]"></div>
                        OUTPUT 1: 1920x1080
                    </div>
                </div>
            </div>

            {/* Main Content Area Routing */}
            <div className="flex-grow w-full overflow-hidden relative">
                {activeTab === 'comp' && <CompositionView />}
                {activeTab === 'mapping' && <MappingView />}
                {activeTab === 'sensor' && (
                    <div className="flex h-full w-full">
                        <DeviceManager />
                        <RadarView />
                        <TrackingPanel />
                    </div>
                )}
            </div>
        </div>
    );
}