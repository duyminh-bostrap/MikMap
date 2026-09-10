# HexMapping — Architecture

> **Projection Mapping Engine + Sensor Calibration System**
> Stack: **C++20 · openFrameworks 0.12.x · OpenGL 4.x · Dear ImGui · OpenCV**
> Target: Windows 10/11 · MSVC 2022 · NVIDIA RTX A4000
> Version: 0.1 (draft) — 2026-09-09

---

## 0. Mục tiêu & Ràng buộc

| # | Ràng buộc | Hệ quả kiến trúc |
|---|---|---|
| C1 | Duy trì **> 60 FPS** ổn định (frame time < 16.6ms, jitter < 2ms) | Render thread không được có lock / alloc / I/O |
| C2 | Tách biệt hoàn toàn **Core / Renderer / UI** | `core/` là C++ thuần, zero GL, zero oF, zero ImGui |
| C3 | Toán ma trận chuẩn xác | Homography DLT + RANSAC, có unit test chạy trên CI không cần GPU |
| C4 | 4K multi-layer video | Codec **HAP** (nén DXT/BC, giải nén trên GPU), không dùng H.264 |
| C5 | Đa sensor: Serial + OSC + Kinect/LiDAR | Mỗi sensor 1 thread riêng, giao tiếp qua lock-free buffer |
| C6 | Độ trễ thấp | Đo được (đóng dấu thời gian), không đoán |

---

## 1. Bản đồ tầng (Layer Map)

```
╔════════════════════════════════════════════════════════════════════╗
║  app/            AppController — nơi DUY NHẤT 4 tầng gặp nhau      ║
╠══════════════╦═══════════════════════╦═════════════════════════════╣
║  ui/         ║  render/              ║  io/                        ║
║  Dear ImGui  ║  OpenGL + oF          ║  Sensor threads             ║
║  (chỉ đọc &  ║  (chỉ đọc model,      ║  (không biết gì về          ║
║   phát lệnh) ║   ghi lên GPU)        ║   model / GL)               ║
╠══════════════╩═══════════════════════╩═════════════════════════════╣
║  core/       C++20 THUẦN — math, model, calib, filter              ║
║              KHÔNG include GL / oF / ImGui / OpenCV-GUI            ║
╚════════════════════════════════════════════════════════════════════╝
```

### Quy tắc phụ thuộc (BẤT KHẢ XÂM PHẠM)

```
core   ──▶ (không phụ thuộc ai)
io     ──▶ core
render ──▶ core
ui     ──▶ core
app    ──▶ core, io, render, ui
```

- ❌ `core/` **không bao giờ** `#include` `ofMain.h`, `<GL/...>`, `imgui.h`.
- ❌ `render/` **không bao giờ** gọi hàm của `ui/`.
- ❌ `io/` **không bao giờ** biết `Slice`, `Layer`, hay ma trận mapping tồn tại.
- ✅ Kiểm chứng tự động bằng `tools/check_layering.ps1` (grep các include bị cấm).

**Lợi ích cụ thể:** `core/` biên dịch được bằng CMake thuần → unit test chạy trong ~2 giây trên CI, không cần GPU, không cần cài openFrameworks.

---

## 2. Cấu trúc thư mục

```
HexMapping/
│
├─ architecture.md                  # ← tài liệu này
├─ README.md
├─ .gitignore  .clang-format  .editorconfig
├─ CMakeLists.txt                   # build core/ + tests (ĐỘC LẬP với oF)
│
├─ HexMapping.sln                   # sinh bởi oF Project Generator
├─ addons.make                      # danh sách addon cho Project Generator
│
├─ addons/                          # oF addons (git submodule)
│   ├─ ofxHapPlayer/                #   4K HAP playback
│   ├─ ofxImGui/                    #   control panel
│   ├─ ofxOsc/                      #   (có sẵn trong oF core)
│   └─ ofxAzureKinect/              #   (tuỳ chọn — chốt sau khi mua phần cứng)
│
├─ src/
│  │
│  ├─ core/                         # ⚙️  C++20 THUẦN — trái tim, testable 100%
│  │  ├─ math/
│  │  │   ├─ Vec2.h                     # POD, constexpr
│  │  │   ├─ Mat3.h / .cpp              # ma trận thuần nhất 3x3, inverse, adjugate
│  │  │   ├─ Homography.h / .cpp        # DLT 4 điểm + N điểm least-squares + RANSAC
│  │  │   ├─ BilinearInverse.h / .cpp   # nghịch đảo ô lưới cong (nghiệm bậc 2 analytic)
│  │  │   └─ Bezier.h / .cpp            # cubic bezier + tessellation + point-in-polygon
│  │  │
│  │  ├─ model/                     # mô hình dữ liệu — thuần dữ liệu, serializable
│  │  │   ├─ IWarp.h                    # ★ interface: forward() VÀ inverse()
│  │  │   ├─ WarpCornerPin.h / .cpp     # impl: homography (keystone 4 điểm)
│  │  │   ├─ WarpMesh.h / .cpp          # impl: lưới NxM (bilinear/bicubic)
│  │  │   ├─ Mask.h / .cpp              # bezier mask, bake ra stencil
│  │  │   ├─ Slice.h / .cpp             # inputRect + outputQuad + IWarp + Mask
│  │  │   ├─ Clip.h / .cpp              # 1 ô trong lưới: media + transport + transform
│  │  │   ├─ Layer.h / .cpp             # 1 hàng: vector<Clip> + activeColumn + blend
│  │  │   ├─ Deck.h / .cpp              # lưới Layer × Column + logic trigger
│  │  │   ├─ Screen.h / .cpp            # 1 máy chiếu = 1 screen + N slice
│  │  │   ├─ Composition.h / .cpp       # gốc cây: canvas + decks + screens
│  │  │   └─ ProjectIO.h / .cpp         # (de)serialize JSON — .hexmap
│  │  │
│  │  ├─ calib/                     # ★ hệ thống calibration
│  │  │   ├─ CorrespondencePair.h       # (điểm sensor, điểm output) + trọng số
│  │  │   ├─ CalibrationProfile.h/.cpp  # H_s + metadata + sai số tái chiếu
│  │  │   ├─ CalibrationSolver.h/.cpp   # giải H_s, RANSAC, đánh giá chất lượng
│  │  │   └─ SensorMapper.h / .cpp      # ★★ sensor → content: H_w⁻¹ · H_s
│  │  │
│  │  ├─ filter/
│  │  │   ├─ OneEuroFilter.h / .cpp     # khử nhiễu KHÔNG thêm độ trễ đáng kể
│  │  │   └─ PointTracker.h / .cpp      # gán ID bền vững qua các frame
│  │  │
│  │  └─ util/
│  │      ├─ Clock.h                    # steady_clock, nanosecond
│  │      ├─ Result.h                   # error handling không dùng exception
│  │      └─ Log.h                      # logging lock-free cho hot path
│  │
│  ├─ io/                           # 🔌 THU THẬP SENSOR — nơi các thread sống
│  │  ├─ SensorFrame.h                  # ★ POD, kích thước cố định, memcpy-able
│  │  ├─ ISensorSource.h                # start / stop / health
│  │  ├─ TripleBuffer.h                 # ★ cầu nối wait-free Sensor → Render
│  │  ├─ SpscRingBuffer.h               # kênh sự kiện (không được rơi)
│  │  ├─ SensorHub.h / .cpp             # sở hữu N source, gộp thành 1 view
│  │  ├─ sources/
│  │  │   ├─ SerialSource.h / .cpp      # COM port — thread riêng
│  │  │   ├─ OscSource.h / .cpp         # UDP listener — thread riêng
│  │  │   ├─ TuioSource.h / .cpp        # TUIO 1.1/2.0 (chồng lên OSC)
│  │  │   ├─ DepthSource.h / .cpp       # Kinect/Femto/LiDAR — thread riêng
│  │  │   └─ MockSource.h / .cpp        # ★ giả lập — phát triển KHÔNG cần phần cứng
│  │  └─ proto/
│  │      ├─ SerialProtocol.h / .cpp    # framing + checksum + parser không alloc
│  │      └─ OscSchema.h                # định nghĩa address pattern
│  │
│  ├─ render/                       # 🎨 GPU — CHỈ render thread được vào đây
│  │  ├─ RenderEngine.h / .cpp           # điều phối các pass
│  │  ├─ LayerCompositor.h / .cpp        # Pass 1: layers → composition FBO
│  │  ├─ SliceRenderer.h / .cpp          # Pass 2: FBO → slice đã warp
│  │  ├─ MaskRenderer.h / .cpp           # Pass 3: bezier mask (stencil)
│  │  ├─ OutputWindow.h / .cpp           # cửa sổ borderless fullscreen máy chiếu
│  │  ├─ WarpGeometry.h / .cpp           # sinh VBO từ IWarp (cache, update khi dirty)
│  │  ├─ GpuResourcePool.h / .cpp        # FBO/texture pool — zero alloc lúc runtime
│  │  ├─ sources/
│  │  │   ├─ IMediaSource.h
│  │  │   ├─ HapVideoSource.h / .cpp     # ★ 4K HAP
│  │  │   ├─ ImageSource.h / .cpp
│  │  │   └─ GenerativeSource.h / .cpp   # hiệu ứng shader phản ứng sensor
│  │  └─ shaders/
│  │      ├─ warp.vert / warp.frag
│  │      ├─ mask.frag
│  │      ├─ blend.frag
│  │      └─ hap_ycocg.frag              # giải mã HAP Q (YCoCg → RGB)
│  │
│  ├─ ui/                           # 🖥️  Dear ImGui — không chạm GL state
│  │  ├─ ControlPanel.h / .cpp
│  │  ├─ panels/
│  │  │   ├─ LayerPanel.*   SliceEditorPanel.*   OutputPanel.*
│  │  │   └─ SensorPanel.*  CalibrationWizard.*  PerfPanel.*
│  │  └─ widgets/
│  │      ├─ QuadHandleEditor.*          # kéo 4 góc keystone
│  │      ├─ MeshGridEditor.*            # kéo điểm lưới
│  │      └─ BezierPathEditor.*          # vẽ mask
│  │
│  └─ app/
│     ├─ main.cpp
│     ├─ AppController.h / .cpp          # ★ nơi DUY NHẤT 4 tầng được nối
│     └─ AppSettings.h / .cpp
│
├─ tests/                          # Catch2 — chỉ test core/, KHÔNG cần GPU
│  ├─ test_mat3.cpp                     # inverse, associativity, identity
│  ├─ test_homography.cpp               # round-trip, degenerate, noise/RANSAC
│  ├─ test_warp_inverse.cpp             # ★ forward(inverse(p)) == p, mọi loại warp
│  ├─ test_sensor_mapper.cpp            # ★ chuỗi H_w⁻¹·H_s
│  ├─ test_one_euro.cpp
│  └─ test_project_io.cpp               # save → load → so sánh sâu
│
├─ tools/
│  ├─ sensor_simulator/                 # app CLI phát OSC giả lập → test không cần HW
│  ├─ encode_hap.ps1                    # ffmpeg: mọi video → HAP / HAP Q
│  └─ check_layering.ps1                # ★ ép quy tắc phụ thuộc ở mục 1
│
├─ bin/data/                       # thư mục runtime của oF
│  ├─ projects/    *.hexmap
│  ├─ media/       *.mov (HAP), *.png
│  ├─ calib/       *.calib.json
│  └─ shaders/
│
└─ docs/
   ├─ math_derivations.md               # dẫn giải DLT & inverse bilinear
   ├─ serial_protocol.md
   └─ operator_manual.md
```

---

## 3. ★ Luồng dữ liệu: Sensor Thread ↔ Render Thread

Đây là phần cốt lõi nhất của kiến trúc.

### 3.1 Toàn cảnh

```
   PHẦN CỨNG                THREAD RIÊNG                 CẦU NỐI              RENDER THREAD (60Hz)
 ─────────────────      ────────────────────       ──────────────────      ────────────────────────

 ┌──────────────┐       ┌─────────────────┐
 │ Arduino      │──USB─▶│ SerialSource    │        ┌───────────────┐
 │ (COM3)       │ 1kHz  │ read() blocking │───────▶│ TripleBuffer  │──┐
 └──────────────┘       │ + parse + stamp │        │ <SensorFrame> │  │
                        └─────────────────┘        └───────────────┘  │
                                                                      │
 ┌──────────────┐       ┌─────────────────┐        ┌───────────────┐  │
 │ IR Frame /   │──UDP─▶│ OscSource       │───────▶│ TripleBuffer  │──┤
 │ TouchDesigner│ 120Hz │ recvfrom() block│        └───────────────┘  │
 └──────────────┘       └─────────────────┘                           │
                                                                      ▼
 ┌──────────────┐       ┌─────────────────┐        ┌───────────────┐ ┌──────────────────┐
 │ Kinect /     │──SDK─▶│ DepthSource     │───────▶│ TripleBuffer  │▶│  SensorHub       │
 │ Femto Bolt   │ 30Hz  │ get_capture()   │        └───────────────┘ │  ::poll()        │
 └──────────────┘       │ + blob detect   │                          │  (wait-free)     │
                        └────────┬────────┘        ┌───────────────┐ └────────┬─────────┘
                                 │                 │ SpscRing      │          │
                                 └── sự kiện ─────▶│ <TouchEvent>  │──────────┤
                                    down / up      └───────────────┘          │
                                                                              ▼
                                                              ┌───────────────────────────┐
                                                              │ PointTracker  (gán ID)    │
                                                              │ OneEuroFilter (khử nhiễu) │
                                                              └─────────────┬─────────────┘
                                                                            ▼
                                                              ┌───────────────────────────┐
                                                              │ ★ SensorMapper            │
                                                              │   p_content =             │
                                                              │     H_w⁻¹ · H_s · p_raw   │
                                                              └─────────────┬─────────────┘
                                                                            ▼
                                                              ┌───────────────────────────┐
                                                              │ RenderEngine::draw()      │
                                                              │  Pass1 Layers → FBO       │
                                                              │  Pass2 Warp   → Slice     │
                                                              │  Pass3 Mask   → Output    │
                                                              └───────────────────────────┘
```

### 3.2 Cấu trúc dữ liệu qua biên thread

```cpp
// io/SensorFrame.h  —  POD thuần: KHÔNG con trỏ, KHÔNG std::vector, memcpy-able.
// Kích thước cố định ⇒ không cấp phát heap trong hot path.

static constexpr int kMaxPoints = 64;

struct TouchPoint {
    uint32_t id;          // ID bền vững qua các frame (do PointTracker gán)
    float    x, y;        // ★ TOẠ ĐỘ SENSOR THÔ (mm / depth-px / ADC) — CHƯA biến đổi
    float    z;           // độ sâu hoặc áp lực (0 nếu không có)
    float    confidence;  // 0..1
    uint8_t  state;       // 0=NONE 1=DOWN 2=MOVE 3=UP
    uint8_t  _pad[3];
};                                          // 24 bytes

struct SensorFrame {
    uint64_t   seq;           // số thứ tự tăng dần — phát hiện frame bị rơi
    int64_t    tCaptureNs;    // ★ steady_clock lúc THU THẬP — để đo độ trễ thật
    uint16_t   sourceId;
    uint8_t    count;
    uint8_t    _pad;
    TouchPoint points[kMaxPoints];
};                                          // ≈ 1552 bytes → 3 slot = 4.6 KB
```

### 3.3 TripleBuffer — vì sao KHÔNG dùng mutex

| | Mutex | **TripleBuffer** |
|---|---|---|
| Sensor 1000Hz ghi, Render 60Hz đọc | Render **có thể bị block** bởi writer → **frame drop** | Cả hai **không bao giờ chờ** |
| Frame cũ | Xếp hàng → độ trễ tích luỹ | **Tự động vứt** — render luôn lấy frame mới nhất ✅ |
| Chi phí | ~20–100 ns + rủi ro context switch | 1 lệnh `compare_exchange` (~5 ns) |

```
       slot[0]        slot[1]        slot[2]
      ┌────────┐    ┌────────┐    ┌────────┐
      │ WRITE  │    │ READY  │    │  READ  │
      └────────┘    └────────┘    └────────┘
           ▲             ▲             ▲
     sensor thread   atomic<int>   render thread
      ghi tự do       + dirty       đọc tự do
                       flag

  Writer: điền slot[write] → atomic swap(write ↔ ready), set dirty
  Reader: nếu dirty → atomic swap(read ↔ ready), clear dirty
  ⇒ Không bên nào block. Không alloc. Không frame bị xé (torn read).
```

### 3.4 Hai kênh truyền — vì sao cần cả hai

| Kênh | Cơ chế | Ngữ nghĩa | Dùng cho |
|---|---|---|---|
| **State** | `TripleBuffer` | "Trạng thái mới nhất" — **được phép rơi** frame cũ | Vị trí ngón tay đang di chuyển. Render chỉ cần frame mới nhất; frame cũ vô giá trị. |
| **Event** | `SpscRingBuffer` (bounded) | "Chuỗi sự kiện" — **KHÔNG được rơi** | TOUCH_DOWN / TOUCH_UP. Rơi 1 sự kiện = hiệu ứng không kích hoạt, hoặc kẹt vĩnh viễn. |

> Ring buffer có biến đếm `overflowCount` hiển thị trên PerfPanel — nếu > 0, render thread đang quá tải và ta biết ngay lập tức.

### 3.5 ★ Quyết định thiết kế: biến đổi toạ độ ở ĐÂU?

**Quyết định: Sensor thread gửi toạ độ THÔ. Render thread mới áp dụng ma trận.**

Lý do:

1. `H_w` (corner-pin) **thay đổi bất cứ lúc nào** người dùng kéo góc trong UI. Nếu sensor thread cần đọc `H_w`, nó phải chia sẻ trạng thái mapping đang bị UI ghi → **bắt buộc phải lock** → phá vỡ ràng buộc C1.
2. Chi phí biến đổi là **không đáng kể**: 64 điểm × 1 phép nhân ma trận 3×3 ≈ 576 phép nhân ≈ **< 1 µs**. So với ngân sách 16 600 µs của một frame, đây là **0.006%**.
3. Toạ độ thô cho phép **calibrate lại mà không khởi động lại sensor**, và cho phép **ghi log / phát lại (replay)** phiên làm việc để debug.

> **Nguyên tắc rút ra:** *dữ liệu thô đi qua biên thread; ngữ nghĩa được áp dụng tại nơi sử dụng.*

### 3.6 Đo độ trễ (không đoán)

Mỗi `SensorFrame` mang `tCaptureNs`. Tại render thread:

```cpp
const double latencyMs = (Clock::nowNs() - frame.tCaptureNs) / 1.0e6;
perfPanel.pushLatency(latencyMs);   // hiển thị min / avg / p99
```

PerfPanel hiển thị realtime: **latency p99, frame time, số frame rơi, ring overflow, VRAM**.
⇒ Tối ưu dựa trên số đo, không dựa trên cảm giác.

---

## 4. Chuỗi biến đổi toạ độ (Transform Chain)

### 4.1 Công thức trung tâm

```
        [SENSOR SPACE]                            [CONTENT SPACE]
      mm / depth-px / ADC                        video texture UV
             │                                          │
             │  H_s  (calibration: chạm 4+ điểm)        │  H_w  (corner-pin thủ công)
             │  3x3, giải bằng DLT + RANSAC             │  3x3, do người dùng kéo góc
             ▼                                          ▼
        ╔═══════════════════════════════════════════════════╗
        ║        [OUTPUT SPACE] — pixel máy chiếu           ║
        ╚═══════════════════════════════════════════════════╝

   ┌──────────────────────────────────────────────────────────┐
   │                                                          │
   │      p_content  =  H_w⁻¹ · H_s · p_sensor                │
   │                                                          │
   │   "Chạm vào vật thể thật → biết ngay pixel nào trên      │
   │    video layer đang bị chạm."                            │
   │                                                          │
   └──────────────────────────────────────────────────────────┘
```

### 4.2 Ba hệ quả thiết kế

**① `H_s` và `H_w` phải là hai ma trận TÁCH RỜI.**

Chỉnh lại keystone ⇒ chỉ `H_w` đổi ⇒ hệ thống tự khớp lại, **không phải calibrate sensor lại**.
Đây là lỗi kiến trúc phổ biến nhất trong các hệ thống loại này — trộn chung hai ma trận khiến mỗi lần tinh chỉnh mapping là phải làm lại toàn bộ calibration.

**② Không cần đo đạc vật lý.**

Không cần biết khoảng cách máy chiếu, góc nghiêng, hay tiêu cự. Chỉ cần ≥ 4 cặp điểm tương ứng → `Homography::solveDLT()`. Với > 4 điểm dùng least-squares + RANSAC để chống nhiễu và loại outlier.

**③ Mesh warp phá vỡ tính khả nghịch dạng đóng.**

Homography 3×3 có nghịch đảo analytic. Lưới mesh thì không.
⇒ `IWarp` **bắt buộc** khai báo cả hai chiều ngay từ đầu:

```cpp
// core/model/IWarp.h
class IWarp {
public:
    virtual ~IWarp() = default;

    virtual Vec2 forward(const Vec2& contentUV) const = 0;          // content → output
    virtual bool inverse(const Vec2& outputPx, Vec2& outUV) const = 0;  // ★ BẮT BUỘC
    virtual bool isInvertible() const = 0;
    virtual void tessellate(int cols, int rows, WarpGeometry& out) const = 0;
};
```

> **Chữ ký `inverse()` đã sửa so với bản đầu.** Ban đầu tôi viết `Vec2 inverse(Vec2)`. Khi cài đặt thật mới thấy sai: phép nghịch đảo **có thể thất bại hợp lệ** — điểm nằm ngoài vùng warp, hoặc ô lưới suy biến. Trả về `Vec2` buộc phải bịa ra một giá trị, thường là `{0,0}` — nhưng `{0,0}` lại là **toạ độ hợp lệ** (góc trên-trái). Người gọi không phân biệt được "chạm vào góc" với "trượt ra ngoài", sinh ra hiệu ứng ma ở góc màn hình. Dạng `bool` + tham số ra biến việc xử lý thất bại thành **bắt buộc**.

- `WarpCornerPin::inverse()` → `Mat3::inverse()`, nghiệm đóng.
- `WarpMesh::inverse()` → tìm ô lưới chứa điểm (spatial hash) rồi giải **inverse bilinear** (phương trình bậc 2, nghiệm đóng — xem `docs/math_derivations.md`).

> Nếu để đến Bước 4 mới thêm `inverse()`, sẽ phải viết lại toàn bộ `render/WarpGeometry` và `model/Slice`. Đây là lý do interface này thuộc **Bước 2**, không phải Bước 4.

### 4.2b Mặt nạ nằm ở đâu trong chuỗi

Mặt nạ bezier (F12) **không phải một phép biến đổi** — nó là một hàm che phủ
`contentUV → [0,1]` nhân vào kênh alpha ở cuối. Vì nút mặt nạ ở `contentUV`,
nó tự động đi theo `H_w`: kéo lại góc keystone thì mặt nạ biến dạng cùng bề mặt.

Chiều ngược lại cũng phải nhất quán: `Screen::hitTest` gọi `Slice::isLit()` chứ
không phải `outputToContent()`, vì một điểm rơi vào vùng bị mặt nạ cắt là điểm
**không có ánh sáng** — báo "chạm trúng" ở đó là sai, và tệ hơn, nó che mất slice
nằm dưới đang thực sự sáng ở chỗ đó.

Nhưng `outputToContent()` thì **cố ý bỏ qua mặt nạ**: đó là phép nghịch đảo hình
học thuần tuý, và wizard calibration dựa vào nó — điểm ngắm hoàn toàn có thể rơi
vào vùng bị cắt, lúc đó vẫn phải calibrate được.

### 4.3 Chuỗi đầy đủ với Composition Canvas

Mục 4.1 là dạng rút gọn. Trong hệ thống thật có thêm một tầng ở giữa: **Composition Canvas** — không gian ảo nơi các layer được trộn lại, **độc lập với độ phân giải máy chiếu**.

```
   ┌─────────────────────────────────────────────────────────────────┐
   │  DECK — lưới Layer × Column                                     │
   │                                                                 │
   │              COL 1     COL 2     COL 3     COL 4                │
   │   LAYER 3  │ clip A │ clip B │   ──   │ clip C │                │
   │   LAYER 2  │ clip D │ clip E │ clip F │   ──   │                │
   │   LAYER 1  │ clip G │ clip H │ clip I │ clip J │                │
   │                ▲         ▲                                      │
   │      click clip│         │click column header                   │
   │      → 1 layer đổi       → CẢ CỘT trigger cùng lúc              │
   └────────────────────────────┬────────────────────────────────────┘
                                │ composite theo z-order + blend
                                ▼
   ┌─────────────────────────────────────────────────────────────────┐
   │        COMPOSITION CANVAS  (vd 3840×2160 — không gian ảo)       │
   │   ┌─────────┐         ┌─────────┐         ┌─────────┐           │
   │   │inputRect│         │inputRect│         │inputRect│           │ ← Slice CHỌN VÙNG
   │   │ Slice 1 │         │ Slice 2 │         │ Slice 3 │           │
   │   └─────────┘         └─────────┘         └─────────┘           │
   └────────┬──────────────────┬───────────────────┬─────────────────┘
            │ IWarp riêng      │ IWarp riêng       │ IWarp riêng
            ▼                  ▼                   ▼
   ╔═════════════════════════════════════════════════════════════════╗
   ║          SCREEN = 1 máy chiếu — [OUTPUT SPACE] px               ║
   ╚═════════════════════════════════════════════════════════════════╝
                                ▲
                                │  H_s (calibration)
                         [SENSOR SPACE] mm / depth-px / ADC
```

**Chuỗi biến đổi ngược đầy đủ** (sensor → biết đang chạm pixel nào của clip):

```
 p_sensor
    │  × H_s                          (calibration, 3×3)
    ▼
 p_output  (pixel máy chiếu)
    │  tìm slice chứa điểm            (duyệt slice, test point-in-quad)
    ▼
 slice     ── slice.warp.inverse() ──▶  UV cục bộ trong slice  [0,1]²
    │  × inputRect (scale + offset)
    ▼
 p_canvas  (toạ độ Composition Canvas)
    │  × layer/clip transform⁻¹
    ▼
 p_clip    (pixel trong texture của clip)
```

**Ba điểm cần lưu ý khi cài đặt:**

1. **Thứ tự duyệt slice quan trọng.** Các slice có thể chồng nhau. Quy ước: duyệt từ trên xuống theo z-order, lấy slice đầu tiên chứa điểm — giống cách xử lý sự kiện chuột.
2. **`inputRect` là phép biến đổi affine đơn giản** (scale + offset), không cần homography. Chỉ `warp` mới cần.
3. **Bốn ma trận trong chuỗi nên được hợp nhất trước** thành một `Mat3` duy nhất cho mỗi slice, tính lại chỉ khi có thứ gì đó `dirty`. Đã kiểm chứng bằng test `★ Ma tran hop nhat cho ket qua giong het tinh tung buoc`: hợp nhất rẻ hơn (1 phép nhân thay vì 4) mà không mất độ chính xác.

---

## 5. Render Pipeline

```
   ┌─ Pass 0 ─ MEDIA UPDATE ──────────────────────────────────┐
   │  HapVideoSource: khối DXT (đã Snappy-decompress ở thread │
   │  decode) → glCompressedTexSubImage2D qua PBO             │
   │  ⇒ CPU gần như không phải giải nén ảnh → đây là lý do    │
   │    4K multi-layer khả thi, còn H.264 thì không.          │
   └──────────────────────────────────────────────────────────┘
                              ▼
   ┌─ Pass 1 ─ LAYER COMPOSITE ───────────────────────────────┐
   │  for each Layer (theo z-order):                          │
   │      bind texture → blend shader → compositionFBO        │
   │  Kết quả: 1 texture "nội dung ảo" (vd 4096x2160)         │
   └──────────────────────────────────────────────────────────┘
                              ▼
   ┌─ Pass 2 ─ SLICE WARP ────────────────────────────────────┐
   │  for each Slice:                                         │
   │      VBO đã bake sẵn từ IWarp (chỉ rebuild khi dirty)    │
   │      sample compositionFBO tại inputRect UV              │
   │      → vẽ vào outputFBO theo hình dạng đã warp           │
   └──────────────────────────────────────────────────────────┘
                              ▼
   ┌─ Pass 3 ─ MASK ──────────────────────────────────────────┐
   │  Bezier mask đã tessellate → stencil buffer (bake 1 lần) │
   │  KHÔNG đánh giá bezier trên từng pixel.                  │
   └──────────────────────────────────────────────────────────┘
                              ▼
   ┌─ Pass 4 ─ PRESENT ───────────────────────────────────────┐
   │  outputFBO → OutputWindow (borderless fullscreen)        │
   │  + Overlay chế độ chỉnh sửa (grid, handle) — CHỈ khi     │
   │    edit mode bật; TẮT HOÀN TOÀN lúc chạy show.           │
   └──────────────────────────────────────────────────────────┘
```

### Ngân sách frame @ 60Hz (16.60 ms)

| Giai đoạn | Ngân sách | Ghi chú |
|---|---:|---|
| Poll sensor + filter + transform | 0.10 ms | wait-free |
| Upload texture HAP (4 layer 4K) | 2.50 ms | qua PBO, bất đồng bộ |
| Pass 1 — layer composite | 1.50 ms | |
| Pass 2 — slice warp | 0.80 ms | geometry đã cache |
| Pass 3 — mask | 0.30 ms | stencil |
| UI ImGui | 1.20 ms | chỉ trên control window |
| **Tổng** | **6.40 ms** | **dư ~10 ms an toàn** |

---

## 6. Mô hình Thread

| Thread | Ưu tiên | Tần suất | Chặn? | Nhiệm vụ |
|---|---|---|---|---|
| **Render** (main) | Normal | 60–120 Hz | ❌ không bao giờ | GL, ImGui, transform, composite |
| **Serial** | Above normal | ~1000 Hz | ✅ chặn ở `read()` | Đọc COM, parse, đóng dấu thời gian |
| **OSC** | Above normal | theo sự kiện | ✅ chặn ở `recvfrom()` | Nhận UDP, giải mã |
| **Depth** | Normal | 30 Hz | ✅ chặn ở SDK | Kinect/LiDAR, blob detect |
| **Video decode** | Normal | 30–60 Hz | ✅ | Do ofxHapPlayer quản lý (DirectShow) |
| **Disk I/O** | Below normal | hiếm | ✅ | Load project, ảnh, ghi log |

> **Luật vàng:** Thread nào có `✅ chặn` thì **không bao giờ** được chia sẻ dữ liệu với Render thread bằng mutex — chỉ qua `TripleBuffer` hoặc `SpscRingBuffer`.

---

## 7. Chiến lược Build

**Vấn đề:** openFrameworks trên Windows dùng **Project Generator → `.vcxproj`**, trong khi `core/` cần build được **độc lập bằng CMake** để chạy unit test trên CI không cần GPU.

**Giải pháp — hai hệ build cùng đọc một tập mã nguồn:**

```
             src/core/*.cpp   (C++20 thuần, chỉ phụ thuộc STL)
                    │
        ┌───────────┴────────────┐
        ▼                        ▼
  CMakeLists.txt           HexMapping.vcxproj
  (tests + core)           (oF Project Generator)
        │                        │
        ▼                        ▼
  hexmapping_tests.exe     HexMapping.exe
  chạy trên CI, ~2 giây    ứng dụng thật
  KHÔNG cần oF / GPU
```

**Ràng buộc để mô hình này hoạt động:** `core/` chỉ được phụ thuộc STL. Nếu cần OpenCV cho RANSAC, đặt sau macro `HEXMAP_USE_OPENCV` kèm một fallback DLT tự viết — để test vẫn build được ở môi trường tối giản.

---

## 8. Sổ rủi ro

| # | Rủi ro | Mức | Giảm thiểu | Xử lý ở bước |
|---|---|---|---|---|
| **R1** | ~~HAP 4K multi-layer trên Windows chưa được kiểm chứng~~ | ✅ **ĐÃ GỠ** | Spike test `tools/spike_hap/` đã chạy: 1–5 luồng 4K HAP Q giữ đúng 30fps gốc, render loop < 16.7ms tới 6 luồng. Nút thắt là decode/đĩa, không phải GPU. Xem `features.md` | Bước 1 ✅ |
| **R2** | `WarpMesh::inverse()` không có nghiệm đóng | 🟡 TB | Inverse bilinear analytic + spatial hash; unit test round-trip | Bước 2 |
| **R3** | Azure Kinect DK đã **EOL (8/2023), SDK ngừng bảo trì** | 🔴 Cao | Trừu tượng hoá sau `DepthSource`. Khuyến nghị **Orbbec Femto Bolt / Mega** (SDK tương thích K4A) | Bước 3 |
| **R4** | Nhiễu / drift sensor làm calibration lệch dần | 🟡 TB | OneEuroFilter + RANSAC + hiển thị sai số tái chiếu cho người vận hành | Bước 4 |
| **R5** | Multi-window vsync lệch → tearing | 🟡 TB | Một GL context chia sẻ; render offscreen FBO rồi blit | Bước 2 |
| **R6** | **Input lag của máy chiếu (16–80 ms) lấn át toàn bộ pipeline phần mềm** | 🟠 TB | Bật "Low Latency / Fast Mode" trên máy chiếu; **tắt keystone nội bộ của máy chiếu** (dùng warp phần mềm thay thế) | Bước 1 |
| **R7** | Phát triển bị chặn vì chưa có phần cứng | 🟢 Thấp | `MockSource` + `tools/sensor_simulator` ngay từ đầu | Bước 1 |

---

## 9. Lộ trình (map với 5 bước đã thống nhất)

| Bước | Nội dung | Đầu ra kiểm chứng được |
|---|---|---|
| **1** | Scaffolding: cây thư mục, oF 0.12.x + VS2022, addons, CMake cho core+tests, `MockSource`, **spike test HAP 4K** | Build sạch cả 2 hệ; cửa sổ output fullscreen ra màn hình 2; **số FPS thật của 4× 4K HAP** |
| **2** | Render & Mapping: `IWarp` (forward+inverse), corner-pin, mesh, mask, layer composite, kéo góc bằng chuột | Kéo 4 góc thấy hình biến đổi realtime, > 60fps; unit test warp round-trip xanh |
| **3** | Sensor Input: `TripleBuffer`, `SpscRing`, Serial + OSC + Depth source, PerfPanel đo độ trễ | Điểm sensor hiện trên overlay; **con số độ trễ p99 hiển thị trên màn hình** |
| **4** | Calibration: DLT + RANSAC, wizard 4 điểm, `SensorMapper`, lưu/nạp profile | Chạm vật thể thật → hiệu ứng xuất hiện đúng vị trí; sai số tái chiếu < 3 px |
| **5** | UI/UX: hoàn thiện panel, quản lý project, phím tắt, chế độ show (ẩn overlay) | Người vận hành dựng được một setup mới từ đầu mà không cần chạm vào code |

---

## 10. Nguyên tắc bất di bất dịch

1. **Render thread không bao giờ lock, alloc, hay chạm đĩa.**
2. **`core/` không bao giờ include GL / oF / ImGui.**
3. **Mọi `IWarp` phải cài đặt được `inverse()`** — nếu không, nó không dùng được cho calibration.
4. **`H_s` (sensor) và `H_w` (mapping) là hai ma trận tách rời, mãi mãi.**
5. **Toạ độ thô đi qua biên thread; ngữ nghĩa áp dụng tại nơi sử dụng.**
6. **Độ trễ phải được ĐO, không được ĐOÁN** — PerfPanel là công dân hạng nhất, không phải tính năng phụ.
7. **Mọi tính năng phải chạy được với `MockSource`** — không có phần cứng vẫn phát triển và test được.
8. **Hình học do người dùng vẽ ra sống ở không gian gắn với NỘI DUNG, không phải với máy chiếu.**
   Mặt nạ bezier (F12) ở `contentUV` của slice; vùng cảm ứng (G17) ở không gian canvas.
   Lý do chung: keystone là thứ ánh xạ nội dung lên bề mặt thật, nên bất cứ thứ gì
   người dùng đã căn theo vật thể phải **đi qua cùng phép warp đó**. Đặt ở pixel máy
   chiếu thì mỗi lần chỉnh lại keystone — việc xảy ra thường xuyên khi máy chiếu bị
   xê dịch — là mỗi lần phải vẽ lại toàn bộ.
9. **Lớp nào có thành viên khó sao chép thì bọc riêng thành viên đó, không viết tay
   copy constructor cho cả lớp.** Copy constructor viết tay liệt kê từng trường, nên
   mỗi trường mới thêm vào lớp là một cơ hội để sót — và cái sót đó không gây lỗi
   biên dịch, chỉ gây mất dữ liệu lặng lẽ. `Slice` từng mất `mask` đúng theo kiểu
   này; nay `WarpPtr` (tự `clone()`) lo phần đa hình và `Slice` dùng `= default`.

---

## Phụ lục A — Tham chiếu

- openFrameworks — https://openframeworks.cc/download/
- ofxHapPlayer (bangnoise) — https://github.com/bangnoise/ofxHapPlayer
- ofxDirectShowDXTVideoPlayer (dự phòng Windows) — https://github.com/59de44955ebd/ofxDirectShowDXTVideoPlayer
- HAP codec — https://hap.video/developers
- Orbbec Femto Bolt (kế nhiệm Azure Kinect) — https://www.orbbec.com/documentation/comparison-with-azure-kinect-dk/
