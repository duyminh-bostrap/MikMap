import React, { useState } from 'react';
import { 
  Layers, 
  MonitorPlay, 
  Activity, 
  Grid, 
  Maximize2, 
  Wand2, 
  Eye, 
  Download, 
  Upload, 
  RefreshCw,
  Globe,
  Code2
} from 'lucide-react';
import { Language } from '../types';
import { translations } from '../utils/localization';
import { CppCodeModal } from './CppCodeModal';

interface HeaderProps {
  activeTab: 'comp' | 'mapping' | 'sensor';
  setActiveTab: (tab: 'comp' | 'mapping' | 'sensor') => void;
  lang: Language;
  setLang: (l: Language) => void;
  showMode: boolean;
  setShowMode: (s: boolean | ((v: boolean) => boolean)) => void;
  testGrid: boolean;
  setTestGrid: (g: boolean | ((v: boolean) => boolean)) => void;
  onAutoCalibrate: () => void;
  onExportProject: () => void;
  onImportProject: () => void;
  onResetDemo: () => void;
  fps: number;
  latencyMs: number;
}

export const Header: React.FC<HeaderProps> = ({
  activeTab,
  setActiveTab,
  lang,
  setLang,
  showMode,
  setShowMode,
  testGrid,
  setTestGrid,
  onAutoCalibrate,
  onExportProject,
  onImportProject,
  onResetDemo,
  fps,
  latencyMs
}) => {
  const t = translations[lang];
  const [showCppModal, setShowCppModal] = useState(false);

  const handleFullscreen = () => {
    if (!document.fullscreenElement) {
      document.documentElement.requestFullscreen().catch(() => {});
    } else {
      document.exitFullscreen().catch(() => {});
    }
  };

  return (
    <header className="h-14 bg-[#121212] border-b border-[#2a2a2a] flex items-center justify-between px-4 shrink-0 z-30 shadow-md">
      {/* Left: Brand & Navigation */}
      <div className="flex items-center gap-6">
        <div className="flex items-center gap-2.5 cursor-pointer" onClick={() => setActiveTab('comp')}>
          <div className="w-6 h-6 flex items-center justify-center rounded bg-gradient-to-br from-[#FF7F50] to-[#FFD166] shadow-[0_0_12px_rgba(255,127,80,0.6)]">
            <div className="w-2.5 h-2.5 bg-[#121212] rounded-xs" />
          </div>
          <div className="flex flex-col">
            <span className="text-sm font-black tracking-tighter text-[#F3F3F3] leading-none">MIKMAP</span>
            <span className="text-[9px] font-mono text-[#888] tracking-wider leading-none mt-0.5">ENGINE v1.0</span>
          </div>
        </div>

        {/* Tab switcher */}
        <div className="flex bg-[#0a0a0a] p-1 rounded-lg border border-[#2a2a2a]">
          <button
            onClick={() => setActiveTab('comp')}
            className={`px-3.5 py-1.5 rounded-md text-xs font-bold flex items-center gap-2 transition-all ${
              activeTab === 'comp'
                ? 'bg-[#1e1e1e] text-[#FF7F50] border border-[#383838] shadow-[0_0_10px_rgba(255,127,80,0.15)]'
                : 'text-[#777] hover:text-[#E0E0E0] border border-transparent'
            }`}
          >
            <Layers size={14} />
            <span>{t.tabComposition}</span>
          </button>
          <button
            onClick={() => setActiveTab('mapping')}
            className={`px-3.5 py-1.5 rounded-md text-xs font-bold flex items-center gap-2 transition-all ${
              activeTab === 'mapping'
                ? 'bg-[#1e1e1e] text-[#FF7F50] border border-[#383838] shadow-[0_0_10px_rgba(255,127,80,0.15)]'
                : 'text-[#777] hover:text-[#E0E0E0] border border-transparent'
            }`}
          >
            <MonitorPlay size={14} />
            <span>{t.tabMapping}</span>
          </button>
          <button
            onClick={() => setActiveTab('sensor')}
            className={`px-3.5 py-1.5 rounded-md text-xs font-bold flex items-center gap-2 transition-all ${
              activeTab === 'sensor'
                ? 'bg-[#1e1e1e] text-[#FF7F50] border border-[#383838] shadow-[0_0_10px_rgba(255,127,80,0.15)]'
                : 'text-[#777] hover:text-[#E0E0E0] border border-transparent'
            }`}
          >
            <Activity size={14} />
            <span>{t.tabSensor}</span>
          </button>
        </div>
      </div>

      {/* Center / Right: Quick Controls & Status */}
      <div className="flex items-center gap-3">
        {/* Quick Toolbar */}
        <div className="flex items-center gap-1.5 bg-[#0a0a0a] p-1 rounded-lg border border-[#2a2a2a]">
          <button
            onClick={() => setShowMode((prev) => !prev)}
            title="Toggle Show Mode (Space) - Hide/Show edit handles"
            className={`px-2.5 py-1 rounded text-[11px] font-medium flex items-center gap-1.5 transition-colors ${
              showMode ? 'bg-[#06D6A0]/15 text-[#06D6A0] border border-[#06D6A0]/40' : 'text-[#888] hover:text-[#E0E0E0]'
            }`}
          >
            <Eye size={13} />
            <span>{t.showMode}</span>
          </button>

          <button
            onClick={() => setTestGrid((prev) => !prev)}
            title="Toggle Test Card / Alignment Grid (G)"
            className={`px-2.5 py-1 rounded text-[11px] font-medium flex items-center gap-1.5 transition-colors ${
              testGrid ? 'bg-[#FFD166]/15 text-[#FFD166] border border-[#FFD166]/40' : 'text-[#888] hover:text-[#E0E0E0]'
            }`}
          >
            <Grid size={13} />
            <span>{t.testGrid}</span>
          </button>

          <button
            onClick={onAutoCalibrate}
            title="Auto-Calibrate Mock Sensor (A)"
            className="px-2.5 py-1 rounded text-[11px] font-medium text-[#118AB2] hover:bg-[#118AB2]/10 hover:text-[#28b8e6] flex items-center gap-1.5 transition-colors"
          >
            <Wand2 size={13} />
            <span>{t.autoCalib}</span>
          </button>

          <button
            onClick={handleFullscreen}
            title="Fullscreen Projector Display (F11)"
            className="p-1.5 rounded text-[#888] hover:text-[#E0E0E0] transition-colors"
          >
            <Maximize2 size={14} />
          </button>
        </div>

        {/* C++ Code Export Button */}
        <button
          onClick={() => setShowCppModal(true)}
          className="flex items-center gap-1.5 bg-[#0a0a0a] hover:bg-[#1a1a1a] px-2.5 py-1.5 rounded-lg border border-[#FF7F50]/40 text-xs font-mono text-[#FF7F50] hover:text-[#ff9d7a] transition-all cursor-pointer shadow-xs active:scale-95"
          title="View & Export C++ Dear ImGui Source Files"
        >
          <Code2 size={13} />
          <span className="font-bold">C++ UI</span>
        </button>

        {/* Project IO */}
        <div className="flex items-center gap-1 bg-[#0a0a0a] p-1 rounded-lg border border-[#2a2a2a]">
          <button
            onClick={onExportProject}
            title="Export .hexmap JSON Project"
            className="p-1.5 rounded text-[#888] hover:text-white hover:bg-[#1a1a1a] transition-colors"
          >
            <Download size={14} />
          </button>
          <button
            onClick={onImportProject}
            title="Import .hexmap JSON Project"
            className="p-1.5 rounded text-[#888] hover:text-white hover:bg-[#1a1a1a] transition-colors"
          >
            <Upload size={14} />
          </button>
          <button
            onClick={onResetDemo}
            title="Reset to Factory Demo"
            className="p-1.5 rounded text-[#888] hover:text-white hover:bg-[#1a1a1a] transition-colors"
          >
            <RefreshCw size={14} />
          </button>
        </div>

        {/* Language Switcher */}
        <button
          onClick={() => setLang(lang === 'en' ? 'vi' : 'en')}
          className="flex items-center gap-1.5 bg-[#0a0a0a] px-2.5 py-1.5 rounded-lg border border-[#2a2a2a] text-xs font-mono text-[#aaa] hover:text-white transition-colors"
          title="Switch Language (VI / EN)"
        >
          <Globe size={13} className="text-[#FF7F50]" />
          <span className="font-bold text-[#E0E0E0]">{lang.toUpperCase()}</span>
        </button>

        {/* Performance Metrics */}
        <div className="flex items-center gap-3 text-xs font-mono bg-[#0a0a0a] px-3 py-1.5 rounded-lg border border-[#2a2a2a]">
          <div className="flex items-center gap-1.5">
            <span className="text-[#666]">FPS:</span>
            <span className="text-[#06D6A0] font-bold">{fps.toFixed(1)}</span>
          </div>
          <div className="h-3 w-px bg-[#2a2a2a]" />
          <div className="flex items-center gap-1.5">
            <span className="text-[#666]">LATENCY:</span>
            <span className="text-[#06D6A0]">{latencyMs.toFixed(1)}ms</span>
          </div>
          <div className="h-3 w-px bg-[#2a2a2a]" />
          <div className="flex items-center gap-1.5 text-[#888]">
            <div className="w-2 h-2 rounded-full bg-[#06D6A0] shadow-[0_0_6px_#06D6A0]" />
            <span className="hidden sm:inline">1920x1080</span>
          </div>
        </div>
      </div>

      <CppCodeModal isOpen={showCppModal} onClose={() => setShowCppModal(false)} />
    </header>
  );
};
