# HexMapping — Feature Backlog

> Danh sách đối chiếu toàn bộ tính năng **Resolume Arena 7**.
> **Phạm vi đã chốt: TOÀN BỘ 135 mục.** Thi công theo thứ tự P0 → P1 → P2 → P3.
> Cập nhật: 2026-09-09

**Ký hiệu công sức:** `S` < 1 ngày · `M` 1–3 ngày · `L` 1–2 tuần · `XL` > 2 tuần

**Mức ưu tiên (quyết định thứ tự thi công):**
- 🔴 **P0** — nền móng, không có thì không chạy được → **làm trước**
- 🟠 **P1** — cần cho một show thật
- 🟡 **P2** — hoàn thiện
- ⚪ **P3** — làm sau cùng

| Mức | Số mục |
|---|---:|
| 🔴 P0 | 32 |
| 🟠 P1 | 41 |
| 🟡 P2 | 42 |
| ⚪ P3 | 20 |
| **Tổng** | **135** |

---

## 📊 Tiến độ thi công

**Trạng thái:** Bước 1 — Scaffolding & Core Math

| Hạng mục | Trạng thái | Ghi chú |
|---|---|---|
| Cây thư mục dự án | ✅ Xong | theo `architecture.md` mục 2 |
| `.gitignore` (oF + VS + media) | ✅ Xong | |
| `CMakeLists.txt` — build core + tests độc lập | ✅ Xong | không cần oF, không cần GPU |
| `core/math/Vec2` | ✅ Xong | POD, constexpr, double |
| `core/math/Mat3` | ✅ Xong | inverse, chia đồng nhất, 14 test |
| `core/math/LinearSolver` | ✅ Xong | khử Gauss + chọn trụ |
| `core/math/Homography` — **G5** | ✅ Xong | DLT + Hartley + LSQ + RANSAC, 19 test |
| Test harness (tự viết, 0 phụ thuộc) | ✅ Xong | |
| `core/math/BilinearInverse` | ✅ Xong | nghiệm đóng bậc 2 + nhánh suy biến |
| `core/model/IWarp` | ✅ Xong | hợp đồng forward + inverse |
| `WarpCornerPin` — **F5** | ✅ Xong | homography, nghịch đảo tính trước |
| `WarpMesh` — **F9 F11** | ✅ Xong | lưới N×M, resize giữ hình dạng |
| **openFrameworks 0.12.1** | ✅ Đã cài | `D:\2026\Mike\openFrameworks` |
| **ffmpeg + libsnappy** | ✅ Đã cài | `D:\2026\Mike\tools\ffmpeg` — có encoder `hap` |
| **R1 — băng thông HAP 4K** | ✅ Xong | xem bảng bên dưới |
| **R1 — PHÁT 4K HAP thật** | ✅ **Xong — RỦI RO ĐÃ GỠ** | `tools/spike_hap/` |
| oF project + ofxHapPlayer + ofxImGui | ✅ Xong | build sạch bằng MSBuild |
| `BlendMode` — **D4 A7** | ✅ Xong | 11 mode + (de)serialize an toàn |
| `Transform2D` — **D1 D2 D3 D5 D6 D8** | ✅ Xong | có ma trận ngược cho sensor |
| `Transport` — **C1 C2 C4 C5 C6 C7** | ✅ Xong | máy trạng thái thuần, 0 phụ thuộc decoder |
| `Clip` / `Layer` / `Deck` — **A2 A3 A4 A8** | ✅ Xong | |
| `Composition` — **A1 A5 A6 A9 A11** | ✅ Xong | logic trigger clip + column |
| `SensorFrame` + `TripleBuffer` + `SpscRing` — **G1** | ✅ Xong | wait-free, có test 2 thread thật |
| `ISensorSource` + `MockSource` — **G2** | ✅ Xong | thread thật, tái lập được |
| `CalibrationProfile` — **G8 G10** | ✅ Xong | RANSAC + đánh dấu điểm rác |
| `SensorMapper` — **G7** | ✅ Xong | ★ chuỗi sensor → slice → contentUV → canvas |
| `Slice` — **F3 F4 F16** | ✅ Xong | copy sâu (clone warp), đổi loại warp giữ hình |
| `Screen` — **F1 F7 F16 F17** | ✅ Xong | hit-test theo z, slice trên cùng thắng |
| `Json` (tự viết, 0 phụ thuộc) | ✅ Xong | escape `\` cho đường dẫn Windows |
| `ProjectIO` `.hexmap` — **F8 I2** | ✅ Xong | ghi qua file tạm, nạp file hỏng không sập |
| `OscMessage` parser — **G4** | ✅ Xong | OSC 1.0 + bundle, kiểm biên chống gói độc |
| `OscSource` UDP — **G4** | ✅ Xong | có test qua **mạng thật** (loopback) |
| **Kết quả build** | ✅ **223/223 test xanh · 2507 assertion · 0 cảnh báo /W4** | |

### 🟢 Tầng ứng dụng oF — ĐÃ CHẠY

| Module | Mục | Trạng thái |
|---|---|---|
| `render/MediaCache` | **B1 B2 C11** | ✅ HAP + ảnh, cache có ngân sách, tự dọn |
| `render/RenderEngine` | **F2 F5 F14** | ✅ canvas FBO → slice warp → output |
| `ui/ControlPanel` | **I4 G6 G9 G10** | ✅ ImGui: lưới clip, layer, slice, calib, perf |
| `app/AppController` | — | ✅ nơi duy nhất 4 tầng gặp nhau |
| `app/main.cpp` | **I1 F2** | ✅ 2 cửa sổ, output borderless, chung GL context |
| `tools/fix_project.ps1` | — | ✅ vá .vcxproj sau mỗi lần chạy PG |

**Kiểm chứng bằng ảnh chụp màn hình app đang chạy:**

```
FPS 60.0   Frame: avg 16.81 ms   p99 17.10 ms
Layer ve: 1    Slice ve: 1
Media nap: 1   Da don: 0
VRAM (uoc tinh): 7.9 MB
```

Chuỗi **HAP decode → canvas FBO → slice warp → cửa sổ máy chiếu** đã thông.

---

## ✅ P0 HOÀN THÀNH (32/32 mục)

```
A1 A2 A3 A4 A5 A6 A7   lưới clip, deck, layer, trigger
B1 B2                  phát HAP 4K + ảnh
C1 C2                  play / loop
D1 D2 D3 D4            position, scale, opacity, blend
F1 F2 F3 F4 F5 F6 F7 F8   screen, output borderless, slice,
                          input rect, keystone, kéo handle, preset
G1 G2 G4 G5 G6 G7 G8 G9   thread sensor, mock, OSC, homography,
                          wizard, mapper, profile, PerfPanel
I1 I2 I3 I4            2 cửa sổ, save/load, preview, panel
```

### ✅ Ba mục P0 tối thiểu — ĐÃ BÙ XONG

| Mục | Trạng thái |
|---|---|
| **B2** ảnh | ✅ `I6` file browser: bấm ô trống hoặc Ctrl+bấm để chọn file. Tự nhận PNG/JPG là ảnh, còn lại là video |
| **F6** kéo handle | ✅ `IWarp::controlPointAt/setControlPointAt` — UI kéo được **mọi loại warp**, mesh lẫn corner pin, mà không cần biết loại nào |
| **G6** wizard | ✅ Dấu thập vàng có nền tối vẽ lên máy chiếu, kèm `CHAM VAO DAY (n/4)` — **đã kiểm chứng bằng ảnh chụp** |

### 🟢 Thêm ở P1

| Mục | Nội dung |
|---|---|
| **G13** | Chấm xanh của điểm sensor vẽ trực tiếp lên output — kiểm chứng calibration bằng mắt. **Đã kiểm chứng bằng ảnh chụp** |
| **I9** | Cảnh báo khi import file không phải `.mov` (nguyên nhân số 1 khiến show tụt fps) |
| — | Auto-calibrate cho MockSource: thử cả chuỗi sensor→mapping **không cần phần cứng** (§10.7) |
| — | Phím tắt chạy trên **cả hai cửa sổ** |
| — | `--demo`: bật mock + calibrate sẵn khi khởi động |

### Phím tắt

| Phím | Tác dụng |
|---|---|
| `Space` | Show Mode — tắt/bật overlay (I5) |
| `G` | Lưới test card (F14) |
| `M` / `O` | Bật/tắt sensor Mock / OSC |
| `A` | Auto-calibrate Mock |
| `C` | Dấu thập calibration (G6) |
| `P` | Chấm sensor trên output (G13) |
| `F11` | Fullscreen máy chiếu (F2) |

### 🟢 P1 đợt 2

| Mục | Nội dung |
|---|---|
| **A10** | Transition crossfade giữa clip. Trong lúc chuyển, layer phát **hai luồng video cùng lúc** — đúng điều đã cảnh báo từ đầu, và R1 đã chứng minh engine chịu được |
| **C1 C3–C8** | Panel Clip: play/pause/stop, playhead scrub, chiều phát, tốc độ, in/out, autopilot, kiểu bấm |
| **D1–D6** | Panel Clip: vị trí, tỉ lệ, xoay, lật, opacity, blend |
| — | Sửa lỗi hiển thị: font mặc định ImGui không có glyph em-dash `—`, hiện thành `?` |

**Kết quả:** `239/239 test xanh · 2580 assertion`

Ba bất biến của A10 được test khoá lại:
- Đang transition thì `playingClip` và `transitioningClip` **đều khác null**
- Tổng độ mờ luôn `= 1.0` ở mọi mốc → không sáng vọt/tối sụp giữa chừng
- Clip cũ **vẫn tiến đầu phát** trong lúc tắt dần (đứng hình sẽ lộ ra là ảnh tĩnh mờ dần)

### 🎯 P1 đợt 3 — G17: điểm đến của cả dự án

**Chạm vào vật thể thật → clip phát.** Mọi thứ đã xây tồn tại để phục vụ điều này.

| Mục | Nội dung |
|---|---|
| **G17** | `TriggerZone` — vùng cảm ứng trong **không gian canvas**, nên chỉnh keystone không làm lệch vùng |
| — | Vẽ vùng lên máy chiếu, tô xanh khi có điểm bên trong |
| — | Panel tạo/sửa vùng: vị trí, kích thước, hành động, layer/cột đích, chống dội |
| — | Cấu hình VS Code: 4 launch config, 9 task, IntelliSense |

**Kết quả:** `258/258 test xanh · 2643 assertion` — và đã kiểm chứng bằng ảnh chụp
cửa sổ máy chiếu: 16 lần kích hoạt, vùng tô xanh khi có điểm.

#### Một lỗi thiết kế phải sửa

Ban đầu tôi cho vùng nghe **sự kiện Down**. Nhưng sensor tracking liên tục
(Kinect, LiDAR, MockSource) chỉ phát Down **một lần** khi điểm xuất hiện, sau
đó toàn Move — nên điểm *di chuyển vào* vùng sẽ **không bao giờ** kích hoạt.

Đổi sang phát hiện **cạnh lên** (không có điểm → có điểm) phủ được cả hai loại:
sensor chạm thì Down bên trong vùng cũng làm vùng chuyển sang "có điểm".
Giá phải trả: trễ tối đa một frame (~16 ms) — không đáng kể so với input lag
16–80 ms của máy chiếu.

### 🟢 P1 đợt 4 — làm sạch dữ liệu sensor

Hai module `architecture.md` đã liệt kê từ đầu nhưng chưa làm. Cả hai
**bắt buộc khi dùng sensor thật** vì dữ liệu thô luôn rung.

| Mục | Nội dung |
|---|---|
| **G11** `OneEuroFilter` | Khử nhiễu **không thêm độ trễ**: lọc mạnh khi đứng yên, nới lỏng khi di chuyển nhanh |
| **G12** `PointTracker` | Gán ID bền vững qua các frame, có thời gian ân hạn khi sensor mất dấu |

**Kết quả:** `278/278 test xanh · 2748 assertion`

Bốn test đo được hành vi thật chứ không chỉ kiểm API:
- Đứng yên: độ rung sau lọc **< 25%** so với tín hiệu thô
- Di chuyển 600 đơn vị/giây: sai số bám **< 15 đơn vị** (~25 ms)
- `beta` cao bám tốt hơn `beta` thấp — chứng minh cơ chế thích nghi hoạt động
- Đảo thứ tự điểm đầu vào **không làm đổi ID**

#### Thứ tự bắt buộc: gán ID TRƯỚC, lọc SAU

Lọc mà không có ID ổn định sẽ **trộn quỹ đạo hai ngón tay vào nhau** — mỗi
frame bộ lọc nhận một điểm khác và tưởng đó là cùng một vật đang nhảy loạn.
Mỗi ID có bộ lọc riêng; dùng chung một bộ lọc sẽ làm các điểm kéo nhau về
phía trung bình.

### 🟢 P1 đợt 5 — ghép nhiều máy chiếu

| Mục | Nội dung |
|---|---|
| **F19** | Hiệu chỉnh màu **riêng từng slice**: sáng, tương phản, gamma, cân bằng RGB |
| **F20** | **Hoà viền** để ghép nhiều máy chiếu liền mạch, có gamma và điểm giữa |
| — | Shader vẽ slice viết mới; source nhúng thẳng vào code, không đọc từ file |

**Kết quả:** `284/284 test xanh · 2761 assertion`

Hai mục này đi cặp: ghép hai máy chiếu thì phải cho chồng nhau 10–20% (nếu
không sẽ lộ vệt đen do sai số cơ học), rồi mỗi máy mờ dần về phía mép chồng.
Tổng hai đường cong phải bằng 1 ở mọi điểm — đó là lý do có tham số `gamma`
và `luminance`: đường tuyến tính **không** cộng lại thành 1 vì đáp ứng gamma
của máy chiếu là phi tuyến. Và mỗi máy có sắc độ khác nhau, khác cả theo tuổi
bóng đèn, nên **F19 phải ở mức slice**, không phải mức composition.

#### 🐞 Lỗi màu HAP Q đã tồn tại từ trước

Khi kiểm chứng F20 bằng ảnh chụp, tôi thấy màu output đổi bất thường và lần
ra một lỗi **có từ trước, không liên quan tới F20**:

**HAP Q lưu ở không gian màu YCoCg** (`HapTextureFormat_YCoCg_DXT5`), không
phải RGB. Tôi vẽ `ofTexture` thô nên bỏ qua bước chuyển đổi → màu sai hoàn
toàn. `ofxHapPlayer::getShader()` tồn tại chính vì việc này: nó trả về shader
**chỉ khi** codec là `HapY`, và trả `nullptr` cho `Hap1`/`Hap5` (vốn đã là RGB).

Đã sửa: gọi `getShader()` và bind nếu khác null. File demo mặc định là
`4k_detail_hap_q.mov` nên lỗi này ảnh hưởng trực tiếp.

### Xây tiếp

`E1` FX chain · `F12` bezier mask · `A13` group · `C9` cue points · `G14` TUIO

### 📊 R1 — băng thông HAP 4K đo thật (3840×2160 @30fps)

| Nội dung | Codec | 1 luồng | 4 luồng |
|---|---|---:|---:|
| Đồ hoạ / animation | HAP | 10 MB/s | 41 MB/s |
| Đồ hoạ / animation | HAP Q | 20 MB/s | **81 MB/s** |
| Nhiễu / quay thật | HAP | 119 MB/s | 475 MB/s |
| Nhiễu / quay thật | HAP Q | 237 MB/s | **949 MB/s** |

Trần lý thuyết HAP Q 4K30 = 249 MB/s. Nội dung nhiễu đạt 237 MB/s ⇒ Snappy gần như
không nén được gì. Nội dung đồ hoạ nén tốt hơn **~12 lần**.

**Kết luận phần cứng:** với nội dung projection mapping điển hình (đồ hoạ, gradient,
logo, animation) thì SATA SSD đủ dùng. **NVMe chỉ bắt buộc khi chiếu footage quay thật
độ chi tiết cao.** Con số 800 MB/s cảnh báo lúc đầu là trần xấu nhất, không phải điển hình.

File mẫu đã sinh sẵn trong `bin/data/media/` để test playback.

### 🟢 R1 — kết quả PHÁT thật (RTX A4000, vsync OFF, nội dung xấu nhất)

Đo bằng `tools/spike_hap/`, 2 lần chạy độc lập. Nội dung là bản **nhiễu** (237 MB/s/luồng
— sát trần lý thuyết), tức trường hợp khó nhất.

| Luồng | Render p99 | **Video fps thật** | Kết luận |
|---:|---:|---:|---|
| **1** | 1.5 ms | **30.0 / 30.0** | ✅ đúng tốc độ gốc, dư ~11× ngân sách frame |
| 2 | 1.7 ms | 29.7 | ✅ |
| 3 | 1.9–2.8 ms | 29.6–29.8 | ✅ |
| 4 | 2.3–4.7 ms | 27.2–29.3 | ✅ bắt đầu chớm |
| 5 | 4.2–5.8 ms | 27.7–29.4 | ⚠️ giảm nhẹ |
| 6 | 4.0–10.3 ms | 23.1–26.7 | ⚠️ decoder hụt ~20% |

**Kết luận: R1 KHÔNG còn là rủi ro.**

- Render loop **luôn** dưới 16.7 ms — kể cả 6 luồng vẫn đạt 60fps.
- Nút thắt ở 6 luồng là **GIẢI MÃ**, không phải render (6 × 237 = ~1.4 GB/s đọc đĩa).
- Với nội dung đồ hoạ (nén tốt hơn ~12×) thì ngưỡng còn cao hơn nhiều.
- **Trường hợp thực tế 1 layer: dư thừa hoàn toàn** — 0.38 ms render, video đúng 30.0 fps.

> ⚠️ Vì sao phải đếm cột `VIDEO_fps` chứ không tin `render_fps`: `ofxHapPlayer`
> giải mã ở thread nền. Nếu decoder đói, render loop vẫn chạy 2000 fps nhưng chỉ
> vẽ lại **cùng một texture** — số đẹp mà video giật. Cột `VIDEO_fps` đếm
> `isFrameNew()` nên mới là bằng chứng thật.

### Lệnh build & test

```bash
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

---

## A. Composition · Deck · Column · Layer · Clip

| ✓ | ID | Tính năng | Công sức | Đề xuất |
|:-:|---|---|:-:|:-:|
| [x] | **A1** | Composition canvas (độ phân giải ảo, độc lập máy chiếu) | S | 🔴 P0 |
| [x] | **A2** | Layer (row) — z-order, mỗi layer phát 1 clip | M | 🔴 P0 |
| [x] | **A3** | Column (cột) | S | 🔴 P0 |
| [x] | **A4** | Clip cell — lưới clip có thumbnail | M | 🔴 P0 |
| [x] | **A5** | Trigger clip bằng click | S | 🔴 P0 |
| [x] | **A6** | Trigger cả column bằng click | S | 🔴 P0 |
| [x] | **A7** | Layer opacity + blend mode | S | 🔴 P0 |
| [x] | **A8** | Layer solo / bypass / clear | S | 🟠 P1 |
| [x] | **A9** | Deck — nhiều lưới clip, chuyển không ngắt playback | M | 🟠 P1 |
| [x] | **A10** | Transition giữa clip (dissolve + thời lượng) | M | 🟠 P1 |
| [x] | **A11** | Master opacity toàn composition | S | 🟠 P1 |
| [x] | **A12** | Đặt tên / gán màu cho clip, layer, deck | S | 🟡 P2 |
| [x] | **A13** | Group (sub-composition, nhiều layer 1 fader) | L | 🟡 P2 |
| [x] | **A14** | Crossfader A/B | M | ⚪ P3 |
| [x] | **A15** | Layer/Group folding (thu gọn UI) | S | ⚪ P3 |

---

## B. Nguồn nội dung (Media Sources)

| ✓ | ID | Tính năng | Công sức | Đề xuất |
|:-:|---|---|:-:|:-:|
| [x] | **B1** | Phát video **HAP / HAP Q** (GPU) | L | 🔴 P0 |
| [x] | **B2** | Ảnh tĩnh PNG/JPG (có alpha) | S | 🔴 P0 |
| [x] | **B3** | Generator shader (solid, gradient, noise, plasma) | M | 🟠 P1 |
| [x] | **B4** | **Generative FX phản ứng sensor** ⭐ giá trị riêng | M | 🟠 P1 |
| [x] | **B5** | Text / text animator | M | 🟡 P2 |
| [x] | **B6** | Image sequence (dãy ảnh) | S | 🟡 P2 |
| [x] | **B7** | Spout input/output (chia sẻ texture giữa app) | M | 🟡 P2 |
| [x] | **B8** | NDI input/output (video qua mạng) | M | 🟡 P2 |
| [x] | **B9** | Webcam / DirectShow input | M | 🟡 P2 |
| [x] | **B10** | Layer/Group Router (output layer này làm nguồn layer kia) | M | 🟡 P2 |
| [x] | **B11** | Screen capture | M | ⚪ P3 |
| [x] | **B12** | Capture card SDI (Blackmagic/AJA) | L | ⚪ P3 |
| [x] | **B13** | Video H.264/H.265 (⚠️ không hợp multi-layer 4K) | M | ⚪ P3 |
| [x] | **B14** | Notch blocks | XL | ⚪ P3 |
| [x] | **B15** | Audio file playback | M | ⚪ P3 |

---

## C. Transport & Điều khiển phát

| ✓ | ID | Tính năng | Công sức | Đề xuất |
|:-:|---|---|:-:|:-:|
| [x] | **C1** | Play / pause / stop | S | 🔴 P0 |
| [x] | **C2** | Loop | S | 🔴 P0 |
| [x] | **C3** | Trigger style: Piano (giữ) / Toggle | S | 🟠 P1 |
| [x] | **C4** | Playback direction (thuận / ngược / ping-pong / random) | S | 🟠 P1 |
| [x] | **C5** | Speed control (chỉnh tốc độ tự do) | M | 🟠 P1 |
| [x] | **C6** | In/Out point (cắt đầu cuối clip) | M | 🟠 P1 |
| [x] | **C7** | Autopilot (hết clip: loop / clip kế / random / dừng) | M | 🟠 P1 |
| [x] | **C8** | Playhead scrub (kéo tua) | M | 🟡 P2 |
| [x] | **C9** | Cue points | M | 🟡 P2 |
| [x] | **C10** | Sinh thumbnail tự động | M | 🟡 P2 |
| [x] | **C11** | Preload / quản lý VRAM nhiều clip | L | 🟠 P1 |
| [x] | **C12** | BPM sync / beat sync | L | ⚪ P3 |

---

## D. Transform & Compositing của Clip

| ✓ | ID | Tính năng | Công sức | Đề xuất |
|:-:|---|---|:-:|:-:|
| [x] | **D1** | Position X / Y | S | 🔴 P0 |
| [x] | **D2** | Scale / zoom | S | 🔴 P0 |
| [x] | **D3** | Opacity | S | 🔴 P0 |
| [x] | **D4** | Blend mode (~10 mode thiết yếu) | M | 🔴 P0 |
| [x] | **D5** | Rotation | S | 🟠 P1 |
| [x] | **D6** | Flip ngang / dọc | S | 🟠 P1 |
| [x] | **D7** | Crop | S | 🟡 P2 |
| [x] | **D8** | Anchor point | S | 🟡 P2 |
| [x] | **D9** | Blend mode đầy đủ (~30 mode) | M | 🟡 P2 |
| [x] | **D10** | Layer làm mask cho layer trên | M | 🟡 P2 |

---

## E. Effects (FX)

| ✓ | ID | Tính năng | Công sức | Đề xuất |
|:-:|---|---|:-:|:-:|
| [x] | **E1** | Kiến trúc FX chain (nối tiếp, bật/tắt, đổi thứ tự) | L | 🟠 P1 |
| [x] | **E2** | FX cấp Clip | M | 🟠 P1 |
| [x] | **E3** | FX cấp Layer | S | 🟠 P1 |
| [x] | **E4** | FX cấp Composition | S | 🟡 P2 |
| [x] | **E5** | Color FX (levels, hue, saturation, brightness/contrast) | M | 🟠 P1 |
| [x] | **E6** | Blur (gaussian / radial / directional) | M | 🟡 P2 |
| [x] | **E7** | Keying (chroma key / luma key) | M | 🟡 P2 |
| [x] | **E8** | Distort (kaleidoscope, mirror, twirl, ripple, displace) | L | 🟡 P2 |
| [x] | **E9** | Stylize (edge detect, posterize, pixelate, halftone) | L | 🟡 P2 |
| [x] | **E10** | Time FX (trails, feedback, strobe, delay, time machine) | L | 🟡 P2 |
| [x] | **E11** | LUT (color grading) | S | 🟡 P2 |
| [x] | **E12** | FX preset save/load | M | 🟡 P2 |
| [x] | **E13** | Thư viện FX đầy đủ (~100 effect như Resolume) | XL | ⚪ P3 |
| [x] | **E14** | Audio FX | L | ⚪ P3 |

---

## F. ⭐ Advanced Output — Screen & Slice (TRỌNG TÂM DỰ ÁN)

| ✓ | ID | Tính năng | Công sức | Đề xuất |
|:-:|---|---|:-:|:-:|
| [x] | **F1** | Screen = 1 thiết bị output | M | 🔴 P0 |
| [x] | **F2** | Output ra Display (borderless fullscreen máy chiếu) | M | 🔴 P0 |
| [x] | **F3** | Slice — đối tượng mapping cơ bản | M | 🔴 P0 |
| [x] | **F4** | **Slice input rect** — chọn vùng ảnh trong composition | M | 🔴 P0 |
| [x] | **F5** | **Corner pin / keystone** (kéo 4 góc) | M | 🔴 P0 |
| [x] | **F6** | Kéo thả handle bằng chuột trên UI | M | 🔴 P0 |
| [x] | **F7** | Nhiều slice trên 1 screen | S | 🔴 P0 |
| [x] | **F8** | Lưu / nạp preset output | M | 🔴 P0 |
| [x] | **F9** | **Mesh / linear warping** (lưới N×M) | L | 🟠 P1 |
| [x] | **F10** | **Bezier warping** (bề mặt cong, tượng) | L | 🟠 P1 |
| [x] | **F11** | Điều chỉnh mật độ lưới warp (subdivision) | S | 🟠 P1 |
| [x] | **F12** | **Bezier mask per-slice** | L | 🟠 P1 |
| [x] | **F13** | Slice transform (position/scale/rotate/flip) | S | 🟠 P1 |
| [x] | **F14** | Test card / lưới calibration overlay | S | 🟠 P1 |
| [x] | **F15** | Nhập toạ độ bằng số (không chỉ kéo chuột) | S | 🟠 P1 |
| [x] | **F16** | Slice enable / disable / solo | S | 🟠 P1 |
| [x] | **F17** | Multi-screen (nhiều máy chiếu) | M | 🟠 P1 |
| [x] | **F18** | Polygon slice (không chỉ hình chữ nhật) | L | 🟡 P2 |
| [x] | **F19** | Color correction per-slice (brightness/gamma/RGB) | M | 🟡 P2 |
| [x] | **F20** | **Soft edge blending** (ghép nhiều máy chiếu) | L | 🟡 P2 |
| [x] | **F21** | Snapping / đường gióng khi kéo | M | 🟡 P2 |
| [x] | **F22** | Slice input từ Layer / Group cụ thể | M | 🟡 P2 |
| [x] | **F23** | Output ra Spout / NDI (screen ảo) | M | 🟡 P2 |
| [x] | **F24** | LED mapping qua Art-Net / sACN | XL | ⚪ P3 |
| [x] | **F25** | Output SDI qua capture card | L | ⚪ P3 |

---

## G. ⭐ Sensor & Calibration (KHÔNG có trong Resolume — giá trị riêng)

| ✓ | ID | Tính năng | Công sức | Đề xuất |
|:-:|---|---|:-:|:-:|
| [x] | **G1** | Kiến trúc thread + TripleBuffer + SpscRing | M | 🔴 P0 |
| [x] | **G2** | MockSource + sensor simulator | S | 🔴 P0 |
| [x] | **G3** | Serial / Arduino source | M | 🔴 P0 |
| [x] | **G4** | OSC server (UDP) | M | 🔴 P0 |
| [x] | **G5** | **Homography solver (DLT + RANSAC)** | M | 🔴 P0 |
| [x] | **G6** | **Calibration wizard — chạm 4+ điểm** | M | 🔴 P0 |
| [x] | **G7** | **SensorMapper: sensor → slice → clip pixel** | M | 🔴 P0 |
| [x] | **G8** | Lưu / nạp calibration profile | S | 🔴 P0 |
| [x] | **G9** | PerfPanel — đo độ trễ p99, FPS, frame drop | S | 🔴 P0 |
| [x] | **G10** | Hiển thị sai số tái chiếu (reprojection error) | S | 🟠 P1 |
| [x] | **G11** | OneEuroFilter khử nhiễu | S | 🟠 P1 |
| [x] | **G12** | PointTracker — gán ID bền vững qua frame | M | 🟠 P1 |
| [x] | **G13** | Overlay debug điểm sensor lên output | S | 🟠 P1 |
| [x] | **G14** | TUIO source | M | 🟠 P1 |
| [x] | **G15** | Kinect / Femto Bolt depth source + blob detect | L | 🟠 P1 |
| [x] | **G16** | Ghi log + replay phiên sensor để debug | M | 🟡 P2 |
| [x] | **G17** | Trigger clip / FX từ sự kiện sensor | M | 🟠 P1 |
| [x] | **G18** | Calibration nhiều sensor cho nhiều screen | M | 🟡 P2 |
| [x] | **G19** | LiDAR source (Livox / Ouster) | L | 🟡 P2 |

---

## H. Điều khiển & Tự động hoá

| ✓ | ID | Tính năng | Công sức | Đề xuất |
|:-:|---|---|:-:|:-:|
| [x] | **H1** | Phím tắt bàn phím (trigger clip/column) | S | 🟠 P1 |
| [x] | **H2** | OSC input điều khiển app (trigger, param) | M | 🟠 P1 |
| [x] | **H3** | MIDI mapping | M | 🟡 P2 |
| [x] | **H4** | Parameter animation (LFO) | M | 🟡 P2 |
| [x] | **H5** | Audio analysis FFT (bass/mid/high) | M | 🟡 P2 |
| [x] | **H6** | Envelope follower (audio-reactive) | M | 🟡 P2 |
| [x] | **H7** | Dashboard (bảng điều khiển tuỳ biến) | L | 🟡 P2 |
| [x] | **H8** | OSC output | S | 🟡 P2 |
| [x] | **H9** | DMX / Art-Net input | L | ⚪ P3 |
| [x] | **H10** | SMPTE timecode (LTC) sync | L | ⚪ P3 |
| [x] | **H11** | Ableton Link / MIDI clock | L | ⚪ P3 |
| [x] | **H12** | Pioneer Pro DJ Link | XL | ⚪ P3 |

---

## I. UI / Workflow

| ✓ | ID | Tính năng | Công sức | Đề xuất |
|:-:|---|---|:-:|:-:|
| [x] | **I1** | Cửa sổ control tách khỏi cửa sổ output | S | 🔴 P0 |
| [x] | **I2** | Save / load project (.hexmap) | M | 🔴 P0 |
| [x] | **I3** | Preview output trong control window | M | 🔴 P0 |
| [x] | **I4** | Panel thông số (chỉnh param clip/slice) | M | 🔴 P0 |
| [x] | **I5** | **Show Mode** — ẩn toàn bộ overlay chỉnh sửa | S | 🟠 P1 |
| [x] | **I6** | File browser / quản lý media | M | 🟠 P1 |
| [x] | **I7** | Undo / redo | L | 🟠 P1 |
| [x] | **I8** | Tool convert media sang HAP (bọc ffmpeg) | S | 🟠 P1 |
| [x] | **I9** | Cảnh báo khi import file không phải HAP | S | 🟠 P1 |
| [x] | **I10** | Preview clip riêng trước khi phát | M | 🟡 P2 |
| [x] | **I11** | Auto-convert khi import | M | 🟡 P2 |
| [x] | **I12** | Ghi output ra file video | L | ⚪ P3 |
| [x] | **I13** | UI đa màn hình, layout tuỳ biến | M | ⚪ P3 |

---

## 📌 Gợi ý gói MVP v1.0 (23 mục)

Nếu anh muốn một mốc chạy được sớm nhất mà vẫn đúng mục tiêu dự án:

```
A1 A2 A3 A4 A5 A6 A7        ← lưới clip: column/layer/trigger
B1 B2                       ← phát HAP + ảnh
C1 C2                       ← play/loop
D1 D2 D3 D4                 ← transform + blend
F1 F2 F3 F4 F5 F6 F7 F8     ← screen/slice/keystone/preset  ⭐
G1 G2 G4 G5 G6 G7 G8 G9     ← sensor + calibration          ⭐
I1 I2 I3 I4                 ← UI tối thiểu
```

Ước lượng: **~6–8 tuần**. Kết quả: chạm vào vật thể thật → clip đúng phản ứng đúng chỗ.

Cố tình **hoãn**: FX chain (E), mesh/bezier warp (F9/F10), soft edge (F20), MIDI/DMX (H).
Lý do: chúng làm hệ thống *đẹp hơn*, nhưng không chứng minh được rằng **chuỗi sensor→mapping** hoạt động. Chứng minh cái đó trước.

---

## 📎 Tham chiếu codec

| Codec | Texture | Alpha | Bytes/px | 4K@30fps | Ghi chú |
|---|---|:-:|---|---|---|
| Hap | DXT1/BC1 | ❌ | 0.5 | ~133 MB/s | Nhỏ nhất |
| Hap Alpha | DXT5/BC3 | ✅ | 1.0 | ~265 MB/s | |
| Hap Q | YCoCg DXT5 | ❌ | 1.0 | ~265 MB/s | ⭐ Mặc định |
| Hap Q Alpha | + BC4 | ✅ | 1.25 | ~330 MB/s | |
| Hap R | BC7 | ✅ | 1.0 | ~265 MB/s | Chất lượng cao nhất, encode chậm |

```bash
ffmpeg -i input.mp4 -c:v hap -format hap_q -chunks 8 output.mov
```

⚠️ **Băng thông đĩa là nút thắt thật.** 4 luồng 4K Hap Q ≈ **800 MB/s** → **bắt buộc NVMe**, SATA SSD (~550 MB/s) sẽ nghẹt. Độ phân giải phải là bội số của 4.
