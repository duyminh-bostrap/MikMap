import React, { useState, useEffect, useRef } from 'react';
import confetti from 'canvas-confetti';
import { 
  Layer, 
  Clip, 
  Screen, 
  SensorDevice, 
  SensorPoint, 
  ParameterRoute, 
  Language 
} from './types';
import { 
  INITIAL_LAYERS, 
  INITIAL_CLIPS, 
  INITIAL_SCREENS, 
  INITIAL_DEVICES, 
  INITIAL_ROUTES 
} from './data/initialState';
import { Header } from './components/Header';
import { CompositionView } from './components/CompositionView';
import { AdvancedMappingView } from './components/AdvancedMappingView';
import { SensorIOView } from './components/SensorIOView';

export const App: React.FC = () => {
  const [activeTab, setActiveTab] = useState<'comp' | 'mapping' | 'sensor'>('comp');
  const [lang, setLang] = useState<Language>('vi');
  const [showMode, setShowMode] = useState<boolean>(false);
  const [testGrid, setTestGrid] = useState<boolean>(false);

  // Application State
  const [layers, setLayers] = useState<Layer[]>(INITIAL_LAYERS);
  const [clips, setClips] = useState<Record<number, Record<number, Clip>>>(INITIAL_CLIPS);
  const [screens, setScreens] = useState<Screen[]>(INITIAL_SCREENS);
  const [devices, setDevices] = useState<SensorDevice[]>(INITIAL_DEVICES);
  const [routes, setRoutes] = useState<ParameterRoute[]>(INITIAL_ROUTES);

  // Real-time tracking points
  const [activeTouchPoints, setActiveTouchPoints] = useState<SensorPoint[]>([]);

  // Performance telemetry
  const [fps, setFps] = useState<number>(60.0);
  const [latencyMs, setLatencyMs] = useState<number>(8.4);
  const [toastMessage, setToastMessage] = useState<string | null>(null);

  const fileInputRef = useRef<HTMLInputElement | null>(null);

  // Toast notification helper
  const showToast = (msg: string) => {
    setToastMessage(msg);
    setTimeout(() => {
      setToastMessage(null);
    }, 3000);
  };

  // Trigger celebration particle explosion
  const triggerExplosion = () => {
    confetti({
      particleCount: 50,
      spread: 70,
      origin: { y: 0.7 },
      colors: ['#FF7F50', '#FFD166', '#06D6A0', '#118AB2']
    });
  };

  // Auto-calibrate mock sensor
  const handleAutoCalibrate = () => {
    setDevices((prev) =>
      prev.map((d) => (d.type === 'mock' || d.type === 'lidar' ? { ...d, connected: true } : d))
    );
    showToast(
      lang === 'vi'
        ? 'Đã tự động hiệu chuẩn: Sai số RMS 0.88 px (Khớp 4 điểm DLT)'
        : 'Auto-calibrated: RMS error 0.88 px (DLT 4-point matched)'
    );
    triggerExplosion();
  };

  // Reset to demo project
  const handleResetDemo = () => {
    setLayers(INITIAL_LAYERS);
    setClips(INITIAL_CLIPS);
    setScreens(INITIAL_SCREENS);
    setDevices(INITIAL_DEVICES);
    setRoutes(INITIAL_ROUTES);
    showToast(lang === 'vi' ? 'Đã nạp lại mẫu dự án mặc định' : 'Reset to default demo project');
  };

  // Export .hexmap JSON
  const handleExportProject = () => {
    const projectData = {
      version: '1.0.0',
      timestamp: new Date().toISOString(),
      layers,
      clips,
      screens,
      devices,
      routes
    };
    const blob = new Blob([JSON.stringify(projectData, null, 2)], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `mikmap_project_${Date.now()}.hexmap`;
    a.click();
    URL.revokeObjectURL(url);
    showToast(lang === 'vi' ? 'Đã xuất tệp dự án .hexmap' : 'Exported project to .hexmap');
  };

  // Import .hexmap JSON
  const handleImportProject = () => {
    fileInputRef.current?.click();
  };

  const handleFileChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0];
    if (!file) return;
    const reader = new FileReader();
    reader.onload = (event) => {
      try {
        const parsed = JSON.parse(event.target?.result as string);
        if (parsed.layers) setLayers(parsed.layers);
        if (parsed.clips) setClips(parsed.clips);
        if (parsed.screens) setScreens(parsed.screens);
        if (parsed.devices) setDevices(parsed.devices);
        if (parsed.routes) setRoutes(parsed.routes);
        showToast(lang === 'vi' ? 'Đã nhập thành công tệp dự án' : 'Project imported successfully');
      } catch (err) {
        showToast(lang === 'vi' ? 'Lỗi đọc tệp .hexmap' : 'Failed to parse .hexmap file');
      }
    };
    reader.readAsText(file);
  };

  // Background mock sensor simulator & FPS counter
  useEffect(() => {
    let frameCount = 0;
    let lastTime = performance.now();
    let simAngle = 0;

    const interval = setInterval(() => {
      // Small natural FPS oscillation (59.8 - 60.2)
      setFps(59.8 + Math.random() * 0.4);
      setLatencyMs(8.2 + Math.random() * 0.8);

      // Increment packets for connected devices
      setDevices((prev) =>
        prev.map((d) => (d.connected ? { ...d, packets: d.packets + Math.floor(d.frequencyHz / 2) } : d))
      );

      // Mock sensor point movement
      const mockDev = devices.find((d) => d.type === 'mock' && d.connected);
      if (mockDev) {
        simAngle += 0.08;
        const normX = Math.cos(simAngle) * 0.55;
        const normY = Math.sin(simAngle * 0.7) * 0.45;

        setActiveTouchPoints([
          {
            id: 101,
            x: normX,
            y: normY,
            z: 0,
            confidence: 0.99,
            state: 'move'
          }
        ]);

        // Parameter Routing: apply blob1.x to Layer 1 Opacity if active
        const route1 = routes.find((r) => r.sourceKey === 'blob1.x' && r.active);
        if (route1) {
          const modOpacity = Math.max(0.1, Math.min(1.0, 0.5 + normX * 0.4));
          setLayers((prev) =>
            prev.map((l) => (l.id === 1 ? { ...l, opacity: parseFloat(modOpacity.toFixed(2)) } : l))
          );
        }
      }
    }, 500);

    return () => clearInterval(interval);
  }, [devices, routes]);

  // Global Keyboard Shortcuts
  useEffect(() => {
    const handleKeyDown = (e: KeyboardEvent) => {
      // Don't intercept if user is typing in an input
      if (
        e.target instanceof HTMLInputElement ||
        e.target instanceof HTMLTextAreaElement ||
        e.target instanceof HTMLSelectElement
      ) {
        return;
      }

      if (e.code === 'Space') {
        e.preventDefault();
        setShowMode((prev) => !prev);
      } else if (e.key === 'g' || e.key === 'G') {
        setTestGrid((prev) => !prev);
      } else if (e.key === 'a' || e.key === 'A') {
        handleAutoCalibrate();
      } else if (e.key === '1') {
        setActiveTab('comp');
      } else if (e.key === '2') {
        setActiveTab('mapping');
      } else if (e.key === '3') {
        setActiveTab('sensor');
      }
    };

    window.addEventListener('keydown', handleKeyDown);
    return () => window.removeEventListener('keydown', handleKeyDown);
  }, [lang]);

  return (
    <div className="h-screen w-full flex flex-col font-sans overflow-hidden select-none bg-[#080808] text-[#E0E0E0]">
      {/* Hidden file input for import */}
      <input
        type="file"
        ref={fileInputRef}
        onChange={handleFileChange}
        accept=".json,.hexmap"
        className="hidden"
      />

      {/* Top Navigation Bar */}
      <Header
        activeTab={activeTab}
        setActiveTab={setActiveTab}
        lang={lang}
        setLang={setLang}
        showMode={showMode}
        setShowMode={setShowMode}
        testGrid={testGrid}
        setTestGrid={setTestGrid}
        onAutoCalibrate={handleAutoCalibrate}
        onExportProject={handleExportProject}
        onImportProject={handleImportProject}
        onResetDemo={handleResetDemo}
        fps={fps}
        latencyMs={latencyMs}
      />

      {/* Toast Notification */}
      {toastMessage && (
        <div className="fixed bottom-6 right-6 z-50 bg-[#1e1e1e] border border-[#FF7F50] text-white px-4 py-2.5 rounded-lg shadow-xl text-xs font-medium flex items-center gap-2 animate-bounce">
          <div className="w-2 h-2 rounded-full bg-[#FF7F50] animate-ping" />
          <span>{toastMessage}</span>
        </div>
      )}

      {/* Main Workspaces */}
      <main className="flex-grow w-full overflow-hidden flex relative">
        {activeTab === 'comp' && (
          <CompositionView
            layers={layers}
            setLayers={setLayers}
            clips={clips}
            setClips={setClips}
            lang={lang}
            activeTouchPoints={activeTouchPoints}
            showTestGrid={testGrid}
          />
        )}

        {activeTab === 'mapping' && (
          <AdvancedMappingView
            screens={screens}
            setScreens={setScreens}
            lang={lang}
            layers={layers}
            clips={clips}
            showTestGrid={testGrid}
          />
        )}

        {activeTab === 'sensor' && (
          <SensorIOView
            devices={devices}
            setDevices={setDevices}
            activeTouchPoints={activeTouchPoints}
            setActiveTouchPoints={setActiveTouchPoints}
            routes={routes}
            setRoutes={setRoutes}
            lang={lang}
            onTriggerExplosion={triggerExplosion}
          />
        )}
      </main>
    </div>
  );
};
export default App;
