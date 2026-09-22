# MikMap — Bảng tính năng mới (bỏ tích)

> **Kiểm tra lần này áp dụng cho bản prototype C++ / Dear ImGui (`mikmap-cpp`)** — chỉ phần UI + mô hình dữ liệu trong bộ nhớ; chưa có engine oF, HAP, output ra máy chiếu hay I/O thật.
> Tích theo code thực tế (đã đối chiếu source), không theo mong muốn.
>
> - `[x]` = có hành vi thật trong prototype (chạy được, tương tác được).
> - `[~]` = mới có UI / dữ liệu / một phần hành vi (xem "Ghi chú kiểm tra" cuối file).
> - `[ ]` = chưa làm.

**Ký hiệu công sức:** `S` < 1 ngày · `M` 1–3 ngày · `L` 1–2 tuần · `XL` > 2 tuần
**Ưu tiên:** 🔴 P0 · 🟠 P1 · 🟡 P2 · ⚪ P3

---

## A. Composition · Deck · Column · Layer · Clip

| ✓ | ID | Tính năng | Công sức | Đề xuất |
|:-:|---|---|:-:|:-:|
| [x] | **A1** | Composition canvas (độ phân giải ảo, độc lập máy chiếu) | S | 🔴 P0 |
| [x] | **A2** | Layer (row) — z-order, mỗi layer phát 1 clip | M | 🔴 P0 |
| [x] | **A3** | Column (cột) | S | 🔴 P0 |
| [~] | **A4** | Clip cell — lưới clip có thumbnail | M | 🔴 P0 |
| [x] | **A5** | Trigger clip bằng click | S | 🔴 P0 |
| [x] | **A6** | Trigger cả column bằng click | S | 🔴 P0 |
| [x] | **A7** | Layer opacity + blend mode | S | 🔴 P0 |
| [x] | **A8** | Layer solo / bypass / clear | S | 🟠 P1 |
| [x] | **A9** | Deck — nhiều lưới clip, chuyển không ngắt playback | M | 🟠 P1 |
| [x] | **A10** | Transition giữa clip (dissolve + thời lượng) | M | 🟠 P1 |
| [ ] | **A11** | Master opacity toàn composition | S | 🟠 P1 |
| [x] | **A12** | Đặt tên / gán màu cho clip, layer, deck | S | 🟡 P2 |
| [x] | **A13** | Group (sub-composition, nhiều layer 1 fader) | L | 🟡 P2 |
| [ ] | **A14** | Crossfader A/B | M | ⚪ P3 |
| [x] | **A15** | Layer/Group folding (thu gọn UI) | S | ⚪ P3 |

---

## B. Nguồn nội dung (Media Sources)

| ✓ | ID | Tính năng | Công sức | Đề xuất |
|:-:|---|---|:-:|:-:|
| [ ] | **B1** | Phát video **HAP / HAP Q** (GPU) | L | 🔴 P0 |
| [x] | **B2** | Ảnh tĩnh PNG/JPG (có alpha) | S | 🔴 P0 |
| [~] | **B3** | Generator shader (solid, gradient, noise, plasma) | M | 🟠 P1 |
| [ ] | **B4** | **Generative FX phản ứng sensor** ⭐ giá trị riêng | M | 🟠 P1 |
| [ ] | **B5** | Text / text animator | M | 🟡 P2 |
| [ ] | **B6** | Image sequence (dãy ảnh) | S | 🟡 P2 |
| [ ] | **B7** | Spout input/output (chia sẻ texture giữa app) | M | 🟡 P2 |
| [ ] | **B8** | NDI input/output (video qua mạng) | M | 🟡 P2 |
| [ ] | **B9** | Webcam / DirectShow input | M | 🟡 P2 |
| [ ] | **B10** | Layer/Group Router (output layer này làm nguồn layer kia) | M | 🟡 P2 |
| [ ] | **B11** | Screen capture | M | ⚪ P3 |
| [ ] | **B12** | Capture card SDI (Blackmagic/AJA) | L | ⚪ P3 |
| [ ] | **B13** | Video H.264/H.265 (⚠️ không hợp multi-layer 4K) | M | ⚪ P3 |
| [ ] | **B14** | Notch blocks | XL | ⚪ P3 |
| [ ] | **B15** | Audio file playback | M | ⚪ P3 |

---

## C. Transport & Điều khiển phát

| ✓ | ID | Tính năng | Công sức | Đề xuất |
|:-:|---|---|:-:|:-:|
| [x] | **C1** | Play / pause / stop | S | 🔴 P0 |
| [x] | **C2** | Loop | S | 🔴 P0 |
| [ ] | **C3** | Trigger style: Piano (giữ) / Toggle | S | 🟠 P1 |
| [x] | **C4** | Playback direction (thuận / ngược / ping-pong / random) | S | 🟠 P1 |
| [x] | **C5** | Speed control (chỉnh tốc độ tự do) | M | 🟠 P1 |
| [ ] | **C6** | In/Out point (cắt đầu cuối clip) | M | 🟠 P1 |
| [ ] | **C7** | Autopilot (hết clip: loop / clip kế / random / dừng) | M | 🟠 P1 |
| [x] | **C8** | Playhead scrub (kéo tua) | M | 🟡 P2 |
| [ ] | **C9** | Cue points | M | 🟡 P2 |
| [ ] | **C10** | Sinh thumbnail tự động | M | 🟡 P2 |
| [ ] | **C11** | Preload / quản lý VRAM nhiều clip | L | 🟠 P1 |
| [~] | **C12** | BPM sync / beat sync | L | ⚪ P3 |

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
| [ ] | **D7** | Crop | S | 🟡 P2 |
| [ ] | **D8** | Anchor point | S | 🟡 P2 |
| [ ] | **D9** | Blend mode đầy đủ (~30 mode) | M | 🟡 P2 |
| [ ] | **D10** | Layer làm mask cho layer trên | M | 🟡 P2 |

---

## E. Effects (FX)

| ✓ | ID | Tính năng | Công sức | Đề xuất |
|:-:|---|---|:-:|:-:|
| [~] | **E1** | Kiến trúc FX chain (nối tiếp, bật/tắt, đổi thứ tự) | L | 🟠 P1 |
| [~] | **E2** | FX cấp Clip | M | 🟠 P1 |
| [ ] | **E3** | FX cấp Layer | S | 🟠 P1 |
| [ ] | **E4** | FX cấp Composition | S | 🟡 P2 |
| [~] | **E5** | Color FX (levels, hue, saturation, brightness/contrast) | M | 🟠 P1 |
| [~] | **E6** | Blur (gaussian / radial / directional) | M | 🟡 P2 |
| [ ] | **E7** | Keying (chroma key / luma key) | M | 🟡 P2 |
| [~] | **E8** | Distort (kaleidoscope, mirror, twirl, ripple, displace) | L | 🟡 P2 |
| [~] | **E9** | Stylize (edge detect, posterize, pixelate, halftone) | L | 🟡 P2 |
| [~] | **E10** | Time FX (trails, feedback, strobe, delay, time machine) | L | 🟡 P2 |
| [ ] | **E11** | LUT (color grading) | S | 🟡 P2 |
| [ ] | **E12** | FX preset save/load | M | 🟡 P2 |
| [ ] | **E13** | Thư viện FX đầy đủ (~100 effect như Resolume) | XL | ⚪ P3 |
| [ ] | **E14** | Audio FX | L | ⚪ P3 |

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
| [~] | **F8** | Lưu / nạp preset output | M | 🔴 P0 |
| [x] | **F9** | **Mesh / linear warping** (lưới N×M) | L | 🟠 P1 |
| [ ] | **F10** | **Bezier warping** (bề mặt cong, tượng) | L | 🟠 P1 |
| [x] | **F11** | Điều chỉnh mật độ lưới warp (subdivision) | S | 🟠 P1 |
| [~] | **F12** | **Bezier mask per-slice** | L | 🟠 P1 |
| [ ] | **F13** | Slice transform (position/scale/rotate/flip) | S | 🟠 P1 |
| [x] | **F14** | Test card / lưới calibration overlay | S | 🟠 P1 |
| [x] | **F15** | Nhập toạ độ bằng số (không chỉ kéo chuột) | S | 🟠 P1 |
| [x] | **F16** | Slice enable / disable / solo | S | 🟠 P1 |
| [~] | **F17** | Multi-screen (nhiều máy chiếu) | M | 🟠 P1 |
| [ ] | **F18** | Polygon slice (không chỉ hình chữ nhật) | L | 🟡 P2 |
| [ ] | **F19** | Color correction per-slice (brightness/gamma/RGB) | M | 🟡 P2 |
| [~] | **F20** | **Soft edge blending** (ghép nhiều máy chiếu) | L | 🟡 P2 |
| [ ] | **F21** | Snapping / đường gióng khi kéo | M | 🟡 P2 |
| [ ] | **F22** | Slice input từ Layer / Group cụ thể | M | 🟡 P2 |
| [ ] | **F23** | Output ra Spout / NDI (screen ảo) | M | 🟡 P2 |
| [ ] | **F24** | LED mapping qua Art-Net / sACN | XL | ⚪ P3 |
| [ ] | **F25** | Output SDI qua capture card | L | ⚪ P3 |

---

## G. ⭐ Sensor & Calibration (KHÔNG có trong Resolume — giá trị riêng)

| ✓ | ID | Tính năng | Công sức | Đề xuất |
|:-:|---|---|:-:|:-:|
| [ ] | **G1** | Kiến trúc thread + TripleBuffer + SpscRing | M | 🔴 P0 |
| [x] | **G2** | MockSource + sensor simulator | S | 🔴 P0 |
| [ ] | **G3** | Serial / Arduino source | M | 🔴 P0 |
| [ ] | **G4** | OSC server (UDP) | M | 🔴 P0 |
| [x] | **G5** | **Homography solver (DLT + RANSAC)** | M | 🔴 P0 |
| [x] | **G6** | **Calibration wizard — chạm 4+ điểm** | M | 🔴 P0 |
| [ ] | **G7** | **SensorMapper: sensor → slice → clip pixel** | M | 🔴 P0 |
| [~] | **G8** | Lưu / nạp calibration profile | S | 🔴 P0 |
| [x] | **G9** | PerfPanel — đo độ trễ p99, FPS, frame drop | S | 🔴 P0 |
| [x] | **G10** | Hiển thị sai số tái chiếu (reprojection error) | S | 🟠 P1 |
| [ ] | **G11** | OneEuroFilter khử nhiễu | S | 🟠 P1 |
| [ ] | **G12** | PointTracker — gán ID bền vững qua frame | M | 🟠 P1 |
| [x] | **G13** | Overlay debug điểm sensor lên output | S | 🟠 P1 |
| [ ] | **G14** | TUIO source | M | 🟠 P1 |
| [ ] | **G15** | Kinect / Femto Bolt depth source + blob detect | L | 🟠 P1 |
| [ ] | **G16** | Ghi log + replay phiên sensor để debug | M | 🟡 P2 |
| [~] | **G17** | Trigger clip / FX từ sự kiện sensor | M | 🟠 P1 |
| [ ] | **G18** | Calibration nhiều sensor cho nhiều screen | M | 🟡 P2 |
| [ ] | **G19** | LiDAR source (Livox / Ouster) | L | 🟡 P2 |

---

## H. Điều khiển & Tự động hoá

| ✓ | ID | Tính năng | Công sức | Đề xuất |
|:-:|---|---|:-:|:-:|
| [~] | **H1** | Phím tắt bàn phím (trigger clip/column) | S | 🟠 P1 |
| [ ] | **H2** | OSC input điều khiển app (trigger, param) | M | 🟠 P1 |
| [ ] | **H3** | MIDI mapping | M | 🟡 P2 |
| [ ] | **H4** | Parameter animation (LFO) | M | 🟡 P2 |
| [ ] | **H5** | Audio analysis FFT (bass/mid/high) | M | 🟡 P2 |
| [ ] | **H6** | Envelope follower (audio-reactive) | M | 🟡 P2 |
| [ ] | **H7** | Dashboard (bảng điều khiển tuỳ biến) | L | 🟡 P2 |
| [ ] | **H8** | OSC output | S | 🟡 P2 |
| [ ] | **H9** | DMX / Art-Net input | L | ⚪ P3 |
| [ ] | **H10** | SMPTE timecode (LTC) sync | L | ⚪ P3 |
| [ ] | **H11** | Ableton Link / MIDI clock | L | ⚪ P3 |
| [ ] | **H12** | Pioneer Pro DJ Link | XL | ⚪ P3 |

---

## I. UI / Workflow

| ✓ | ID | Tính năng | Công sức | Đề xuất |
|:-:|---|---|:-:|:-:|
| [x] | **I1** | Cửa sổ control tách khỏi cửa sổ output | S | 🔴 P0 |
| [x] | **I2** | Save / load project (.hexmap) | M | 🔴 P0 |
| [x] | **I3** | Preview output trong control window | M | 🔴 P0 |
| [x] | **I4** | Panel thông số (chỉnh param clip/slice) | M | 🔴 P0 |
| [x] | **I5** | **Show Mode** — ẩn toàn bộ overlay chỉnh sửa | S | 🟠 P1 |
| [~] | **I6** | File browser / quản lý media | M | 🟠 P1 |
| [x] | **I7** | Undo / redo | L | 🟠 P1 |
| [ ] | **I8** | Tool convert media sang HAP (bọc ffmpeg) | S | 🟠 P1 |
| [ ] | **I9** | Cảnh báo khi import file không phải HAP | S | 🟠 P1 |
| [x] | **I10** | Preview clip riêng trước khi phát | M | 🟡 P2 |
| [ ] | **I11** | Auto-convert khi import | M | 🟡 P2 |
| [ ] | **I12** | Ghi output ra file video | L | ⚪ P3 |
| [ ] | **I13** | UI đa màn hình, layout tuỳ biến | M | ⚪ P3 |

---

---

## Ghi chú kiểm tra (prototype `mikmap-cpp`)

**Tổng kết:** 49 mục `[x]` · 18 mục `[~]` · 68 mục `[ ]` (trên tổng 135 mục; đếm từ các dòng bảng ở trên, 2026-09-21). Các danh sách `[x]`/`[~]`/"Chưa làm" ngay dưới là bản chụp trước đợt bổ sung P0 — đợt đó (mục kế tiếp) đã làm thêm A1, A6, C2/C4/C5, D1–D6, F2, I1, G9 và thay G5 bằng DLT. G5 nay đã `[x]`: `src/calib.cpp` có DLT + Hartley và RANSAC (≥6 điểm, seed cố định, loại điểm hiệu chuẩn lệch rồi fit lại trên inlier).

**Đã có hành vi thật (`[x]`)**
- A2/A3/A5/A8/A15 — layer xếp chồng (layer trên đè lên), cột động (chèn/xoá/đổi chỗ/tự thêm), click = cue, đúp = trigger, solo/mute/bypass + xoá clip, thu gọn layer/group. Nguồn: `deck.cpp`.
- F3–F7, F9, F11 — slice, input rect, corner pin, kéo handle, nhiều slice/screen, mesh N×M có kéo điểm, thêm cột/hàng lưới. Nguồn: `mapping.cpp`.
- G2, G6, G10 — chạm giả lập trên radar, wizard 4 điểm, hiện RMS. Nguồn: `sensor.cpp`.
- I3, I4, I10 — Live Output composite, panel thông số Comp/Layer/Clip, Preview Cue riêng.

**Mới ở mức UI / một phần (`[~]`)**
- A1 hiển thị kích thước canvas nhưng chưa có canvas render độc lập · A4 thumbnail là gradient, chưa sinh từ video · A6 bấm header cột mới *chọn* (chưa trigger cả cột) · A7/D4 blend mode chọn được 8 mode nhưng chỉ `Add`/`Screen` được cộng sáng · A10 chỉ có ô nhập blend time · A12 màu clip đổi được; đổi tên layer/cột chưa làm · A13 group có header/cue nhưng chưa có 1 fader chung.
- B3 generator bằng vẽ CPU (`clipart.cpp`, 9 kiểu), chưa phải shader GLSL.
- C1 có play/pause, chưa có stop · C2/C4/C5 nút LOOP/BOUN/HOLD/ONCE và slider tốc độ chưa tác động phát · C12 chỉ có chỉ báo nhịp 128 BPM.
- D3 opacity theo layer, chưa theo clip.
- E1/E2 chuỗi FX gắn riêng từng clip (thêm/xoá/nhân đôi/đổi thứ tự/bypass, kéo từ Browser thả vào clip) · E5/E6/E8/E9/E10 có tham số nhưng preview chỉ vẽ tác dụng của Strobe.
- F1/F17 nhiều screen chỉ là dữ liệu, chưa có output thật · F12 mask 4 điểm (đảo/độ mờ), chưa phải bezier · F14 thanh màu test card ở Live Output, chưa phủ lên output · F15 nhập số cho input rect, tọa độ góc chỉ đọc · F16 bật/tắt hiển thị bằng nút mắt, chưa có solo · F20 công tắc edge blending chưa có gamma/độ rộng.
- G5 dùng affine 2 tỉ lệ + dịch (`FitAffine`), chưa phải DLT/RANSAC · G13 chấm sensor hiện trên radar, chưa phủ lên output · G17 bảng patch cord bật/tắt được nhưng chưa kích hoạt gì.
- I6 danh sách Browser là dữ liệu mẫu, chưa đọc thư mục media · I7 undo/redo chỉ cho Advanced Mapping.

**Chưa làm:** toàn bộ nhóm nguồn thật (HAP, ảnh, Spout/NDI, webcam…), transform clip (D1/D2/D5–D10), mọi mục H (MIDI/OSC/phím tắt/LFO/audio), save/load project và preset (F8, G8, I2), output ra máy chiếu/cửa sổ tách rời (F2, I1), thread + ring buffer sensor (G1), nguồn sensor thật (G3/G4/G14/G15/G19), PerfPanel (G9).

### Đợt bổ sung P0 (bản C++)
- **F2 + I1 + F1 — output ra máy chiếu:** cửa sổ riêng không viền, mở trên màn hình được chọn (`F11` hoặc nút trong Screen properties). Nó chỉ vẽ các slice đã warp, không có giao diện chỉnh sửa. Nội dung đi thẳng qua phép warp (corner pin hoặc mesh) nên không cần texture trung gian. Giới hạn: cắt theo hình chữ nhật bao quanh quad, nên quad xoay nhiều sẽ lòi một ít ở góc.
- **A1 — canvas ảo:** composition có độ phân giải riêng (1920×1080), Live Output đóng khung đúng tỉ lệ đó thay vì giãn theo panel.
- **D1–D6 — transform clip:** vị trí X/Y, scale, xoay, lật ngang/dọc, opacity, có nút RESET. Áp dụng cả ở preview lẫn output.
- **C2/C4/C5 — transport:** LOOP / BOUN / HOLD / ONCE chạy thật, có nút REV đảo chiều và slider tốc độ 0–200%. ONCE tự dừng clip khi hết.
- **D4/A7 — blend mode:** 8 mode đổ bằng hàm trộn OpenGL thật (Add, Screen, Multiply, Lighten, Darken, Difference…). Overlay chưa có công thức cố định nên tạm dùng Screen.
- **A6 — trigger cột:** bấm đúp header cột để bắn toàn bộ clip trong cột.
- **G5 — homography:** thay phép affine cũ bằng DLT có chuẩn hoá Hartley, giải bằng phương trình chuẩn. RMS với 4 điểm mẫu = 0.00px, ma trận có thành phần phối cảnh thật.
- **G9 — PerfPanel:** thanh trạng thái hiện FPS, P99 và số khung rớt; tab Comp và monitor Live lấy số thật thay cho 59.94 cố định.

**Không làm được trong đợt này:** A4 thumbnail động — vẽ hình sinh trong cả 40 ô làm deck tụt xuống ~16 giây/khung (kiểu STARS nặng nhất), nên ô clip giữ nền gradient; hình thật chỉ hiện ở tab Clip và hai monitor.


### Đợt bổ sung UX & lưu/mở dự án (2026-09-21)
Đã grep `src/*.cpp` xác nhận từng mục:
- **I2 — lưu/mở dự án `[x]`** (`src/project.cpp`): định dạng `.mikmap` (JSON, dùng `engine/core/util/Json`, ghi qua file tạm rồi rename), thư mục `~/Documents/MikMap`. Có Dự án mới (trống) / Mở (hộp thoại liệt kê, mới nhất trước) / Lưu / Lưu bản sao, phát hiện thay đổi chưa lưu (bỏ qua playhead và trạng thái live) và cảnh báo khi bỏ/thoát, nạp file hỏng hoặc rỗng không làm hỏng trạng thái đang chạy. **Lưu ý:** đây là schema riêng của `src/`, **chưa gọi `ProjectIO`/`.hexmap` của `engine/`**. Kiểm bằng `mikmap --roundtrip <file>` (chạy không cần cửa sổ).
- **Cài đặt máy tách khỏi dự án:** ngôn ngữ/font/màu/cỡ chữ/màn hình output lưu ở thư mục config của hệ điều hành (`settings.json`), tự lưu khi đổi.
- **F8, G8 `[~]`:** output (screen/slice/mask) và calibration được lưu **cùng dự án**, chưa có preset/profile riêng để nạp lẫn giữa các dự án.
- **H1 `[~]`:** Space play/pause, Enter trigger clip đang chọn, ←/→ đổi cột, L loop, Delete xoá clip, Ctrl/Cmd+N/O/S/Shift+S/Z/Y. Chưa có phím gán tuỳ ý cho từng clip/cột.
- **A12 `[~]`:** đổi tên layer (bấm đúp hàng layer hoặc menu) và cột (menu chuột phải) chạy thật; đổi tên clip/deck chưa.
- **Sửa lỗi:** chip blend mode ở tab Layer dùng tên "Alpha"/"Additive" không khớp danh sách chuẩn nên rơi về Normal — nay dùng chung 8 chế độ; ROI ở Sensor I/O kéo được; nút Group/Column trên Deck chạy; Timeline kéo (scrub) được; thanh trạng thái không còn hiển thị MIDI/Art-Net/NDI giả.
- **I7 — undo/redo `[x]`** (`project.cpp`: `UndoTick`/`UndoStep`): toàn app (Deck, Mapping, Sensor), không chỉ Mapping. Snapshot tự động khi chuột/bàn phím ngừng thao tác (kéo slider/góc = một bước), tối đa 60 bước, `Ctrl/Cmd+Z`, `Ctrl/Cmd+Shift+Z`/`Y` và nút trên thanh Mapping. Hành động trình diễn (trigger, playhead) **không** vào lịch sử, và clip đang phát không bị cắt khi undo. Kiểm bằng `mikmap --roundtrip`.
- **I5 — Show Mode `[x]`:** phím `Tab` hoặc menu Project → chỉ hiện composite toàn màn hình, không có gì để bấm nhầm; `Esc`/`Tab` để thoát.
- **A12** thêm đổi tên clip (popover chuột phải → Rename); mục vẫn `[~]` vì chưa đổi tên deck. Tên clip không còn quyết định hình vẽ (`Clip::style` được cố định khi đổi tên).
- **F2/I1 trên macOS đã kiểm chứng:** cửa sổ output máy chiếu mở và vẽ đúng các slice đã warp (chụp bằng `--outshot`).
- **G13 `[~]` (tiến thêm):** điểm chạm từ radar được chiếu qua homography calibration (`FitHomography` → `ApplyH`) và hiện trên sân khấu Mapping ở trang Output (chấm + gợn sóng + toạ độ px, chỉ với Screen đầu tiên vì đích calibration nằm ở không gian 1920×1080 của nó). **Chưa phủ lên cửa sổ output máy chiếu thật.** Toạ độ sensor vẫn thô cho tới lúc vẽ (nguyên tắc #5).
- **G17 `[~]` (tiến thêm):** dây `touch.down → "<layer> · <clip>"` đang bật sẽ trigger clip có tên đó khi chạm radar (thanh trạng thái báo "Route fired"). Mới hỗ trợ nguồn `touch.down`; các nguồn khác (`touch.x`, `blob.count`, `touch.velocity`) vẫn chưa nối vào tham số nào.
- **A10 — dissolve `[x]`:** đổi clip trên layer có `blend time` > 0 sẽ cross-dissolve từ clip cũ sang clip mới trong đúng khoảng thời gian đó (áp dụng cả Live Output lẫn cửa sổ máy chiếu; bắn cả cột cũng dissolve). Kiểm logic bằng `mikmap --roundtrip`.
- **C12 `[~]` (tiến thêm):** BPM chỉnh được — bấm vào `xxx.x BPM` ở thanh trạng thái để tap tempo, lăn chuột để tinh chỉnh, chuột phải về 128; lưu trong dự án. Chưa đồng bộ nguồn ngoài (MIDI clock / Link).
- **Timeline / playhead:** tổng thời lượng và tốc độ chạy playhead nay theo thời lượng thật của clip (`16s`); generator `∞` lặp mỗi 10s.
- **A13 — fader nhóm `[x]`:** mỗi nhóm có fader tổng trên header (kéo được; bấm vào fader không làm thu gọn nhóm), nhân vào độ mờ của mọi layer trong nhóm khi vẽ composite; lưu trong dự án.
- **C1 — Stop `[x]`:** nút ■ trên transport dừng và tua playhead của clip trên cùng về 0 (cùng Play/Pause).
- **F15 `[x]`:** toạ độ 4 góc corner-pin nhập được bằng số (X/Y cho từng góc, đơn vị px không gian output), đồng bộ hai chiều với kéo chuột trên sân khấu.
- **G13 `[x]`:** nút **Output overlay** ở thanh Radar bật hiển thị điểm chạm (chấm + gợn sóng) lên **cửa sổ output máy chiếu thật** sau khi chiếu qua H_s; mặc định tắt vì là công cụ gỡ lỗi; chỉ với Screen đầu tiên (chỗ calibration nhắm tới). Đã chụp cửa sổ output xác nhận trên macOS.
- **F16 `[x]`:** chuột phải slice → **Solo/Unsolo**; khi một screen có slice solo, cửa sổ output chỉ vẽ các slice solo (cây hiện nhãn `SOLO`), lưu trong dự án. Đã có sẵn ẩn/hiện bằng nút mắt.
- **FX thật (E5/E8/E10 vẫn `[~]`):** trước đây chỉ Strobe vẽ thật và **chỉ ở monitor Preview**. Nay `DrawClipContent` (đường vẽ chung của Preview, Live Output và cửa sổ máy chiếu) áp dụng: **Strobe** (mọi nơi), **Hue Shift** (xoay màu chủ đạo của clip + độ bão hoà) và **Mirror** (H / V / QUAD, vẽ 2–4 vùng lật). Chưa làm được vì cần framebuffer: Blur, Pixelate, Trails/feedback, Kaleidoscope, RGB Shift, Twirl/Ripple, Levels. Cờ test: `mikmap --fx <kind>` thêm FX vào clip đang chọn để chụp ảnh.
- **Sync / quantize (bổ sung, không có mã riêng):** nút `Sync` trên Deck làm trigger clip/cột chờ nhịp kế tiếp theo BPM (`App::pending` + `flushPending` ở cạnh lên của nhịp trong vòng lặp chính). Mặc định tắt để giữ hành vi cũ. Kiểm bằng `--roundtrip`.
- **Hiệu năng undo:** snapshot lịch sử tốn ~5 ms (Release), nên chỉ chụp sau khi có thao tác của người dùng rồi im lặng ≥0,2 s, không chụp định kỳ khi để yên (tránh giật khung hình khi trình diễn). `MIKMAP_BENCH=1 mikmap --roundtrip f` in chi phí này.
- **B2 — ảnh tĩnh `[x]`** (`clipart.cpp`, `Clip::media`): Browser có thư mục **Media** liệt kê PNG/JPG/BMP/TGA trong `~/Documents/MikMap/media` (quét lại khi bấm thư mục, không quét mỗi khung hình). Kéo vào ô để tạo clip ảnh: căn vừa canvas, có alpha, đi qua cùng transform/opacity/blend/FX và **warp** (vẽ lưới ô khi qua slice nên keystone/mesh uốn ảnh đúng) như clip generator; lưu đường dẫn trong dự án. **Giới hạn:** ảnh cần còn ở đúng đường dẫn (thiếu thì hiện `MISSING MEDIA`), chưa thu nhỏ ảnh quá lớn, texture chưa được giải phóng, thumbnail ô clip vẫn là gradient. Đây **chưa phải video/HAP** (`B1`).
- **A5 — tách vùng bấm ô clip (2026-09-22, theo yêu cầu người dùng):** ô clip (`ClipCell`/`CellOut` trong `src/deck.cpp`) nay có hai vùng độc lập thay vì bấm-đơn/bấm-đúp trên toàn ô — **bar** (dải tên, 22px trên) chỉ *cue* (chọn/preview, không phát), kéo để di chuyển clip, chuột phải mở popover; **body** (vùng gradient dưới) bấm 1 lần là *cue+trigger* (phát ngay cả Preview lẫn Live Output), không kéo được và không có chuột phải. Xem `ux-current.md` §2.1 và `.claude/rules/design.md` bảng "Thao tác chuẩn". Đã xác nhận bằng ảnh chụp headless cho cả 5 trường hợp (bấm bar, bấm body, kéo từ bar, kéo từ body, chuột phải body).
- **A6 — bấm đơn bắn cả cột (2026-09-22, theo yêu cầu người dùng):** `ColumnHeader` trong `src/deck.cpp` trước đây bấm đơn chỉ chọn cột (`selectColumn`), phải bấm đúp mới bắn (`fireColumn`). Nay bấm đơn (nhả chuột, không kéo) gọi thẳng `fireColumn` — vừa chọn vừa bắn mọi clip không rỗng trong cột, không cần bấm đúp nữa. Khớp với badge `TRIG` đã có sẵn trên header cột chưa active. Đã xác nhận bằng ảnh chụp headless: 1 click vào cột chưa active làm clip trong cột đó chuyển Live ngay và Live Output đổi hình lập tức.
- **Sửa lỗi hiển thị header cột "trống mà trông như đang live" (2026-09-22, người dùng báo):** `ColumnHeader` (`src/deck.cpp`) trước đây vẽ glow cam + badge `nL` + chấm mint nhấp nháy cho **bất kỳ cột nào đang là `activeCol`**, kể cả khi cột đó không có clip nào đang phát (vd cột đầu tiên của một dự án mới/trống luôn là `activeCol` mặc định). Kết quả: mở dự án trống, chưa bấm gì, đã thấy Cột 1 trông y hệt "đang live" trong khi Live Output hoàn toàn đen — gây hiểu lầm output bị hỏng. Nay glow/badge/chấm chỉ hiện khi cột đó **thật sự có ≥1 layer đang live** (`layerCount > 0`); cột đang chọn nhưng trống chỉ có viền coral nhạt + badge `TRIG` như cột chưa chọn. Đã xác nhận bằng ảnh chụp: dự án trống (Cột 1 trống), dự án demo cột đang chọn nhưng trống (Cột 2), và cột thật sự live (Cột 3, badge `2L`) — cả ba đúng như kỳ vọng.
- **Sửa lỗi: bấm cột/ô trống không dừng nội dung đang chạy (2026-09-22, người dùng báo tiếp):** sau khi sửa phần hiển thị header cột ở trên, Live Output **vẫn** giữ nguyên nội dung cũ khi bấm vào cột/ô trống — vì `App::trigger`/`App::fireColumn` (`src/deck.cpp`) trước đây coi ô trống là no-op, bỏ qua hoàn toàn thay vì dừng layer. Nay bấm vào ô/cột trống sẽ **dừng layer đó** (mọi clip Live trong layer chuyển về Loaded, `l.live=false`) — một cột được bắn phản ánh đúng nội dung của nó, không còn giữ lại nội dung cũ từ cột khác. Đây là cắt cứng, **chưa có dissolve khi tắt** (khác với chuyển sang clip mới, đã có dissolve) vì `DrawComposite` bỏ qua hẳn layer không còn clip nào Live — để dissolve-khi-tắt cần sửa thêm đường vẽ đó. Đã xác nhận bằng ảnh chụp: bấm cột trống trong lúc có clip Live ở cột khác → Live Output tắt hẳn, Properties đổi `LIVE`→`CUED`.
- **Sửa lỗi gốc: `App::cue` thiếu kiểm tra `Clip::Armed` (2026-09-22, người dùng báo "tự tạo clip mới"):** dữ liệu mẫu dùng `Ar()` để tạo ô "trông trống nhưng không phải `Clip::Empty`" (dùng ở Layer 3/Cột6 và vài chỗ khác). `ClipCell`, `trigger()`, `fireColumn()`, đường vẽ FX đều coi Armed = trống, nhưng `cue()` chỉ kiểm tra `Empty` — bấm vào ô Armed khiến `cue()` đổi nó thành `Selected`, rồi `trigger()` (không còn thấy Armed nữa) phát nó như clip thật dù không tên/không nội dung → ô hiện màu "đang live" giả kèm `LOOP`. Đây là **lỗi có sẵn từ trước** (cả `cue()` lẫn `trigger()` bản gốc đều thiếu check Armed), chỉ lộ ra dễ khi không còn cần bấm đúp. Đã sửa `cue()` coi Armed = trống, giống mọi nơi khác. Kiểm bằng `--roundtrip`: quét toàn bộ ô Armed trong dự án demo, xác nhận cue+trigger không đổi state và không bật `l.live`.
- **Preview Cue theo layer cao nhất khi chọn cột (2026-09-22, theo yêu cầu người dùng):** `fireColumn()` nay cập nhật `selLi`/`selCi` theo layer đầu tiên (trên cùng) có clip thật trong cột vừa bắn, để Preview Cue phản ánh đúng "cái gì đang lên đầu" thay vì giữ nguyên lựa chọn cũ không liên quan. Cột hoàn toàn trống thì không đổi Preview. Đã xác nhận bằng ảnh chụp: bấm Cột 1 (5 layer đều có clip) → Preview đổi sang "Cyber Hex Grid" (layer trên cùng).
- **Sửa lỗi UX gốc: ô trống không có vùng bấm rõ ràng (2026-09-22, người dùng báo tiếp lần 3):** sau 2 lần sửa trước, `trigger()`/`fireColumn()` đã dừng đúng layer về mặt logic (xác nhận bằng test quét toàn bộ layer) — nhưng **ô clip trống không vẽ gì phân biệt bar/body** (toàn bộ chỉ là 1 khối phẳng), nên người dùng không có cách nào biết bấm ở đâu mới vào đúng vùng "body" (phát/dừng) thay vì vùng "bar" (chỉ chọn). Hệ quả: bấm vào phần trên ô trống trúng vùng bar cũ → chỉ cue, không dừng gì → tưởng nhầm là lỗi. Đã hỏi lại người dùng để xác nhận phạm vi mong muốn (chỉ dừng layer chứa ô đó, giữ mô hình nhiều-layer-độc-lập — **không** đổi thành "1 ô trống tắt hết mọi layer"). Sửa: **ô trống không tách vùng nữa** — toàn bộ ô là một vùng bấm duy nhất (tương đương "body"), bấm ở bất kỳ đâu trong ô đều dừng layer đó; chuột phải ở bất kỳ đâu vẫn mở popover như cũ. Đã xác nhận bằng ảnh chụp: bấm sát mép trên ô trống (vùng bar cũ) vẫn dừng đúng layer; chuột phải vẫn mở popover bình thường.
- **Setting mới: auto-start column khi mở dự án (2026-09-22, theo yêu cầu người dùng):** chuột phải header cột → "Set as auto-start on open" (biểu tượng ⚡). Lưu trong dự án dưới dạng `autoStartCol` (mặc định `-1` = tắt, đúng yêu cầu "default là không bật gì"). Khi `LoadProject()` mở dự án có đặt giá trị này, nó gọi `fireColumn()` ngay lập tức (bỏ qua Sync/quantize để không phải chờ nhịp). Giá trị lệch phạm vi (vd cột đã bị xoá từ lần lưu trước) tự rơi về tắt (`-1`) thay vì đọc tràn mảng. **Phạm vi:** áp dụng khi MỞ một file `.mikmap` đã lưu (Ctrl+O), không áp dụng cho "Dự án mới"/"Nạp lại mẫu Demo" (luôn tắt) — vì bản thân app hiện tại luôn khởi động bằng dự án demo, chưa có tính năng "nhớ và mở lại project cuối cùng lúc khởi động ứng dụng"; nếu cần đúng nghĩa "khi mở app" thì cần thêm tính năng đó riêng. Đã xác nhận bằng ảnh chụp: đặt Cột 1 làm auto-start, lưu, mở lại → cả 5 layer tự Live đúng nội dung Cột 1 ngay khi mở, không cần bấm gì thêm. Kiểm tự động: mặc định tắt, lưu/nạp đúng giá trị, tự bắn đúng cột khi mở, và giá trị tràn phạm vi tự rơi về tắt.
- **Nút transport ⏮/⏭ giờ phát thật, không chỉ đổi lựa chọn (2026-09-22, theo yêu cầu người dùng):** trước đây `A.stepSel()` chỉ di chuyển `activeCol`/`selectedCells`, không tác động phát/dừng gì — bấm ⏭ liên tục không đổi Live Output. Thêm `App::stepFireColumn(dir)` (`src/deck.cpp`): di chuyển cột rồi gọi thẳng `fireColumn()` — vừa chọn vừa phát đúng cột mới (dừng layer nào trống ở đó, theo Sync/quantize nếu bật), y hệt bấm vào header cột. Hai nút ⏮/⏭ trên Timeline nối vào hàm mới; phím tắt `←`/`→` vẫn dùng `stepSel()` cũ (chỉ duyệt, không phát) để không đổi bất ngờ hành vi phím tắt đã ghi tài liệu. ▶/⏸/■ đã hoạt động đúng từ trước (`A.playing`), không cần sửa. Đã xác nhận bằng ảnh chụp: bấm ⏭ từ Cột 2 nhảy đúng sang Cột 3, Particle Vortex chuyển Live, Live Output đổi hình ngay. Kiểm tự động: `stepFireColumn` di chuyển đúng cột, thật sự phát nội dung, và kẹp đúng biên (không vượt cột đầu/cuối).
- **Cập nhật UI phần deck theo `MikMap Workspace.dc.html` (2026-09-22, theo yêu cầu người dùng, đối chiếu file thiết kế thật trong `mikmap-pro-vj-interface/`):**
  - **Màu ô clip đúng 3 trạng thái** (token thiết kế §3.2): đã nạp (cam ấm), đang cue/preview (xanh lạnh — trước đây KHÔNG có, trộn lẫn với "đang live"), đang live (cam cháy đậm `#8a3c14`, khác màu cam thuần trước đây). Viền chọn cũng đổi: coral cho live, cyan cho preview.
  - **A9 — Multi-deck `[x]`:** tab deck ở đầu vùng Deck (chuyển/đổi tên/nhân bản/xoá), mỗi deck có layer/nhóm/cột hoàn toàn riêng, lưu trong dự án (`decks[]`/`curDeckIdx`), undo/redo phủ luôn hành động thêm/xoá/chuyển deck. **Lưu ý:** chuyển deck đổi ngay Live Output sang deck mới (không giữ deck cũ chạy nền) — khớp đúng hành vi của chính bản thiết kế tham chiếu (`switchDeck` trong `MikMap Workspace.dc.html`), không phải "chuyển deck mà không ngắt phát" theo nghĩa 2 deck cùng phát song song.
  - **C8 — Playhead scrub `[x]`:** đã có từ trước (thanh kéo dưới 2 monitor, sửa ở đợt 2026-09-21 nhưng quên tích) và nay thêm cả thanh tick % trong Timeline mode.
  - **Timeline run mode (tính năng mới, không có mã Resolume-parity riêng vì đây không phải tính năng Resolume):** toggle GRID/TIMELINE cạnh tab deck. Đọc lại CHÍNH dữ liệu lưới (không có cấu trúc block riêng) — mỗi layer là 1 lane, clip không rỗng xếp nối tiếp theo thời lượng thật, chạy trên playhead chung 0–100%. Advancing playhead tự set `Clip::st` giống hệt trigger() nên chuyển Grid⇄Timeline luôn nhất quán. Kéo clip từ Browser vào lane = nạp vào ô trống đầu layer đó. Transport đổi thành 7 nút khi ở Timeline (thêm 2 nút "lùi/tiến 1 bar"). Playhead/loop **không lưu vào dự án** (runtime-only).
  - Đã kiểm bằng ảnh chụp: 3 màu ô clip, tab deck + menu chuột phải, chuyển Grid→Timeline (5 lane đúng thời lượng, playhead 0%), playhead tự chạy sau ~5s (đổi đúng clip live theo từng lane, Live Output đổi hẳn). Kiểm tự động (`--roundtrip`): tlLayout không có khoảng hở, tlSync bật/tắt đúng ô theo playhead, addDeck tạo deck trống độc lập, switchDeck khôi phục đúng nội dung từng deck, lưu/nạp đa-deck round-trip đúng, xoá deck cuối cùng bị chặn.
