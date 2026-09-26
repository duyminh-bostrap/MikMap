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
| [x] | **F8** | Lưu / nạp preset output | M | 🔴 P0 |
| [x] | **F9** | **Mesh / linear warping** (lưới N×M) | L | 🟠 P1 |
| [ ] | **F10** | **Bezier warping** (bề mặt cong, tượng) | L | 🟠 P1 |
| [x] | **F11** | Điều chỉnh mật độ lưới warp (subdivision) | S | 🟠 P1 |
| [~] | **F12** | **Bezier mask per-slice** | L | 🟠 P1 |
| [~] | **F13** | Slice transform (position/scale/rotate/flip) | S | 🟠 P1 |
| [x] | **F14** | Test card / lưới calibration overlay | S | 🟠 P1 |
| [x] | **F15** | Nhập toạ độ bằng số (không chỉ kéo chuột) | S | 🟠 P1 |
| [x] | **F16** | Slice enable / disable / solo | S | 🟠 P1 |
| [~] | **F17** | Multi-screen (nhiều máy chiếu) | M | 🟠 P1 |
| [ ] | **F18** | Polygon slice (không chỉ hình chữ nhật) | L | 🟡 P2 |
| [ ] | **F19** | Color correction per-slice (brightness/gamma/RGB) | M | 🟡 P2 |
| [~] | **F20** | **Soft edge blending** (ghép nhiều máy chiếu) | L | 🟡 P2 |
| [ ] | **F21** | Snapping / đường gióng khi kéo | M | 🟡 P2 |
| [x] | **F22** | Slice input từ Layer / Group cụ thể | M | 🟡 P2 |
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
| [x] | **G8** | Lưu / nạp calibration profile | S | 🔴 P0 |
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
| [x] | **I2** | Save / load project (.mikmap) | M | 🔴 P0 |
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
- F1/F17 nhiều screen chỉ là dữ liệu, chưa có output thật · F12 mask đa giác N điểm (vuông/tam giác/lục giác/tròn/tim/bút, đảo/độ mờ), chưa phải bezier và chưa che hình ở output thật · F14 thanh màu test card ở Live Output, chưa phủ lên output · F15 nhập số cho input rect, tọa độ góc chỉ đọc · F16 bật/tắt hiển thị bằng nút mắt, chưa có solo · F20 công tắc edge blending chưa có gamma/độ rộng.
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
- **I2 — lưu/mở dự án `[x]`** (`src/project.cpp`): định dạng `.mikmap` (JSON, dùng `engine/core/util/Json`, ghi qua file tạm rồi rename), thư mục `~/Documents/MikMap`. Có Dự án mới (trống) / Mở (hộp thoại liệt kê, mới nhất trước) / Lưu / Lưu bản sao, phát hiện thay đổi chưa lưu (bỏ qua playhead và trạng thái live) và cảnh báo khi bỏ/thoát, nạp file hỏng hoặc rỗng không làm hỏng trạng thái đang chạy. **Lưu ý:** đây là schema riêng của `src/`, **chưa gọi `ProjectIO`/`.mikmap` của `engine/`**. Kiểm bằng `mikmap --roundtrip <file>` (chạy không cần cửa sổ).
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
- **Sửa 2 lỗi do đợt cập nhật deck vừa rồi (2026-09-22, người dùng báo):**
  - Chữ "TIMELINE" tràn khỏi khung segmented Grid/Timeline — độ rộng mỗi nút trước đó cố định 62px, không đủ cho "TIMELINE" (dài gần gấp đôi "GRID"). Sửa: mỗi nút tự tính độ rộng theo `TextW()` của đúng chữ nó chứa, giống cách các tab khác trong app đã làm.
  - Bấm chọn 1 clip (bar hoặc body, ở Grid lẫn Timeline) không tự chuyển Properties sang tab Clip — người dùng phải tự bấm tab CLIP mới thấy đúng thông tin clip vừa chọn. Sửa: `A.tab = 2` ngay khi cue một clip, ở cả 3 chỗ (bar-release, body-click, block-click trong Timeline). Đã xác nhận bằng ảnh chụp: bắt đầu từ tab COMP, bấm 1 ô clip, Properties tự nhảy sang CLIP hiện đúng "Cyber Hex Grid — LIVE".
- **Ghim cột Layers ngang khi cuộn lưới + Cue nhóm chọn đúng clip + bỏ nút +DECK (2026-09-22, theo yêu cầu người dùng, `src/deck.cpp`):**
  - **Ghim ngang:** cột layer 178px không còn cuộn mất khi lưới tràn khung — dựng lại `DeckGrid()` thành ba lượt (layout-only → ô clip cuộn → dải ghim vẽ đè lên trên, cùng kiểu với hàng tiêu đề cột đã ghim dọc từ trước). **Sửa tiếp một lỗi do chính lượt ghim gây ra** (người dùng báo qua ảnh chụp, 2 vòng): cột cuộn **nửa chừng** (chưa ẩn hẳn) vẽ đè ô "Cue N" của nhóm lên tên/fader nhóm, và glow (blur 12px) của ô clip/Cue đang active tràn qua khe 4px sang thẻ/ô bên cạnh. Sửa dứt điểm bằng một quy tắc vẽ duy nhất: mọi thứ cuộn ngang vẽ TRƯỚC và bị cắt cứng bằng `PushClipRect` tại mép phải dải ghim; dải ghim vẽ SAU CÙNG; hit-test theo đúng ranh giới đó (bấm lên dải ghim không lọt xuống ô đang ẩn phía sau).
  - **C10 — Cue nhóm chọn clip thật:** `selectGroupCue()` trước đây chỉ ghi `selectedCells` để tham khảo nội bộ, không đổi `Clip::st` — bấm Cue N của nhóm không đổi màu ô nào cả (trông như không có tác dụng). Sửa để mô phỏng đúng `cue()` cho từng layer thành viên (Loaded↔Selected, Live↔LiveSel), Properties tự chuyển tab Clip.
  - **Deck tabs:** bỏ nút **+ DECK** riêng; chuột phải vào tab deck bất kỳ mở menu có thêm **Add deck** và **Move left/right** (đổi thứ tự, cùng kiểu "dịch trái/phải" của menu cột C9) — `App::moveDeckTo()` mới, mirror `moveColTo()`.
  - Kiểm bằng `--shot` (cuộn nửa cột, bấm lên dải ghim, bấm Cue nhóm, bấm menu chuột phải tab deck) và `--roundtrip` (thêm scenario cho `selectGroupCue`/`moveDeckTo`). `mikmap_tests` không đổi (không đụng `engine/`); 0 cảnh báo mới Debug lẫn Release.
- **Thanh SYSTEM TIME thay Timeline ở góc trái + Cài đặt có tab Layout chỉnh được panel (2026-09-22, người dùng tự viết trực tiếp trên đĩa — không qua phiên Claude Code này; ghi lại sau khi được yêu cầu "kiểm tra code mới, note lại tiến độ"):**
  - `DrawDeck()`: góc trái thanh dưới 2 monitor đổi từ nhãn "TIMELINE"+timecode sang **SYSTEM TIME** (giờ hệ thống thật `HH:MM:SS`, `localtime_r`/`localtime_s`); nhãn TIMELINE+timecode dời sang góc phải, cỡ nhỏ hơn, chỉ còn để tham khảo. **Bỏ hẳn thanh kéo (scrub bar)** dưới 2 monitor — xem lại C8 bên dưới. Cả thanh (label + 5 nút transport) giờ **ẩn được khi `prefs.timelineH = 0`**.
  - `settings.cpp`/`project.cpp`/`app.h`: thêm tab thứ 5 **Layout** trong Cài đặt — 4 slider chỉnh `browserW`/`inspectorW`/`bandPct`/`timelineH`, áp dụng ngay + lưu `settings.json` (mô tả máy, đúng nguyên tắc tách máy/dự án). Cỡ chữ thêm mức thứ 5 (150%, "X-Large").
  - **Việc phiên này (Claude Code) đã làm khi kiểm tra:** phát hiện `leftW` (biến đo bề rộng chữ giờ hệ thống) được tính nhưng không dùng ở đâu — gây cảnh báo `-Wunused-variable`, phá quy tắc "0 cảnh báo". Đã xoá dòng thừa, build lại sạch cả `-fsyntax-only -Wall -Wextra -Wpedantic` lẫn CMake Debug/Release, chạy `--roundtrip` qua. **Chưa tự sửa gì khác** — xem "Điểm cần xác nhận lại với người dùng" ngay dưới.
  - **Đã giải quyết (2026-09-24):** câu hỏi "TIMELINE nên ở trái hay phải" không còn — người dùng tiếp tục chỉnh bố cục thanh này (khối trái thành 2 hàng SYSTEM TIME + menu deck; TIMELINE + timecode giữ ở góc phải, xem các mục 2026-09-24 bên dưới). Mô tả "góc trái = SYSTEM TIME" ở trên chỉ là bước trung gian; bố cục hiện hành ghi ở `ux-current.md` §2.5.
  - **C8 — Playhead scrub:** vẫn `[x]` vì scrub bằng kéo/bấm **vẫn còn** — nhưng chỉ còn ở màn Timeline run mode (`TimelineView`, `scrubZone`/`A.tlProgress`), không còn ở Grid mode nữa (thanh kéo dưới 2 monitor đã bị bỏ, xem trên). Ghi chú lại để lần sau đừng tưởng nhầm scrub Grid-mode vẫn còn.
- **F8/G8 — Preset output & calibration profile trở thành file riêng, tách khỏi project (2026-09-23):** trước đó cả hai chỉ lưu **cùng** `.mikmap`, không có cách nạp lẫn giữa các dự án (xem ghi chú 2026-09-21 ở trên). Nay:
  - **F8 `[x]`:** `SaveOutputPreset`/`LoadOutputPreset` (`src/project.cpp`) ghi một `Screen` (device/resolution/slices/masks) ra file `.mikmap-preset` riêng, thư mục `~/Documents/MikMap/Presets` (`PresetsDir()`) — tách khỏi `ProjectsDir()` gốc nên không lẫn với danh sách "Mở dự án". Nút **Save preset / Load preset** ở Properties → Screen (`src/mapping.cpp`, ngay dưới khối PROJECTOR OUTPUT): Save ghi đè file theo tên màn hình (giống cách Lưu dự án ghi đè); Load mở menu chuột phải liệt kê `ListPresets()`, chọn 1 preset sẽ nạp vào **đúng screen đang xem** (giữ nguyên `id` gốc để không vỡ tham chiếu `selSc`/`selSl`), không tạo screen mới.
  - **G8 `[x]`:** `SaveCalibProfile`/`LoadCalibProfile` lưu 4 điểm calibration + ROI + noise/blobSize ra file `.mikmap-calib`, thư mục `~/Documents/MikMap/Calibration` (`CalibDir()`) — **không** lưu ma trận `H_s` đã fit, vì `src/calib.cpp` vốn tính lại homography từ điểm gốc mỗi lần dùng (không cache ở đâu cả, xem `.claude/CLAUDE.md`), nên profile giữ đúng điểm gốc để nhất quán với cách `src/` đang xử lý calibration ở mọi chỗ khác. Nút **Save profile / Load profile** ở khung "MATRIX H_s" trong màn Sensor I/O (`src/sensor.cpp`); Load reset `A.wizardStep = 0` để không kẹt giữa chừng một lần chạm wizard cũ.
  - Cả hai dùng chung `WriteAtomic` (ghi file tạm rồi rename) như lưu project, và có `"format"` riêng (không dùng chung `kFormat` của `.mikmap` — đây là 2 định dạng file độc lập).
  - Kiểm tự động (`--roundtrip`, `src/main.cpp`): lưu rồi nạp lại preset/profile phải khớp đúng từng field (kể cả slices/masks của Screen, cả 4 điểm + ROI + noise/blobSize của calib), và một file sai định dạng (thiếu `"screen"`, hoặc `"calib"` không đúng 4 điểm) phải bị từ chối, không được nạp im lặng. Build sạch MSVC Debug/Release, không thêm cảnh báo mới ngoài các `fopen`/`sscanf` deprecation đã có sẵn kiểu cũ trong `main.cpp`.
  - **Giới hạn còn lại:** chưa có hộp thoại nhập tên khi lưu (Save preset dùng luôn tên màn hình hiện tại, Save profile luôn stamp giờ) — nếu cần đặt tên tuỳ ý hoặc xác nhận ghi đè, đó là việc UI thêm, chưa làm ở đợt này.
- **A4 — Thumbnail ô clip là hình thật, không còn gradient tĩnh (2026-09-23):** trước đó cố ý giữ gradient phẳng vì vẽ hình sinh (generator art) trực tiếp vào cả ~40 ô mỗi khung hình làm deck tụt xuống ~16 giây/khung (xem ghi chú "Không làm được trong đợt này" ở trên). Nay mỗi `Clip` có texture thumbnail riêng (`Clip::thumbTex`, `src/app.h`), dựng qua FBO (`RenderClipThumbnail`, `src/clipart.cpp`) và **giới hạn tối đa 3 ô vẽ lại thumbnail mỗi khung hình** (`ResetThumbBudget(3)` gọi ở đầu `DrawDeck`, `src/deck.cpp`) bất kể deck có bao nhiêu ô — nên chi phí không phụ thuộc kích thước deck. Một ô chỉ vẽ lại khi texture còn trống hoặc đã cũ hơn 1 giây; các ô khác dùng lại texture cache của khung trước.
  - Cách dựng: vì `DrawClipContent` vẽ qua `ImDrawList` (không phải OpenGL trực tiếp), thumbnail dùng đúng kỹ thuật chính thức của Dear ImGui để render một `ImDrawList` độc lập ra ngoài khung hình chính — tạo `ImDrawList` riêng qua `ImGui::GetDrawListSharedData()`, gọi `_ResetForNewFrame()`, vẽ vào đó thay vì `g.dl` (tráo tạm con trỏ `g.dl`/`g.warp`/`g.alpha`), rồi tự dựng một `ImDrawData` một-list-duy-nhất và gọi thẳng `ImGui_ImplOpenGL3_RenderDrawData()` trong lúc FBO của thumbnail đang được bind — không đụng gì tới việc vẽ khung hình chính đang dở. Texture 128×72, `lod=0.35` (thấp hơn cả ô Inspector Clip-tab hiện có, `lod=0.4`).
  - Các hàm FBO (`glGenFramebuffers`/`glBindFramebuffer`/`glFramebufferTexture2D`/`glCheckFramebufferStatus`) không có trong header `<GL/gl.h>` 1.1 đời cũ trên Windows, nên nạp lúc chạy qua `glfwGetProcAddress` giống hệt cách `glBlendEquation` đã làm cho blend mode (D4) — cùng một hàm `InitBlendModes()`, không thêm entry-point khởi tạo mới.
  - `Clip::thumbTex`/`thumbAt` là state runtime thuần, **không** vào `ClipJ`/`ReadClip` (không lưu vào `.mikmap`, không tính vào so sánh undo/dirty) — giống nguyên tắc `Layer::fadeFrom`/`fadeT` đã có từ trước. Copy một `Clip` (undo snapshot, `App tmp = A`) sẽ copy luôn giá trị `thumbTex` — vô hại (bản sao trỏ chung 1 texture GL còn sống), không phải double-free, vì không chỗ nào gọi `glDeleteTextures` (giống hạn chế "texture chưa được giải phóng" đã ghi ở B2).
  - **Đã xác nhận bằng ảnh chụp thật** (`--shot --page 0 --frames 200`, máy Windows thật, không phải suy đoán): các ô hiện đúng hình sinh riêng của từng clip (Cyber Hex Grid = lưới lục giác, Particle Vortex = chấm rải rác, Mesh Pulse = vòng tròn...), không bị lật ngược, khớp đúng hình ở Preview Cue/Live Output. PerfPanel ổn định **P99 ~17.7ms** sau 200 khung — đúng mục tiêu `tech-defaults.md` (p99 < ~17ms), không tái diễn regression ~16s/khung.
  - **Giới hạn còn lại:** `Strobe Tunnel` có thể thumbnail tối/đen nếu bắt đúng lúc FX Strobe đang ở pha "tắt" — đây là hành vi đúng theo FX thật (khác gradient tĩnh trước đây không phản ánh FX gì cả), không phải lỗi. Texture không bao giờ được giải phóng (leak nhẹ theo phong cách hiện có của `src/`, xem B2); nếu cần dọn khi xoá/đổi clip thì phải thêm `glDeleteTextures` — chưa làm ở đợt này.
  - **Chỉ ô đang live/preview mới "phát" thumbnail, còn lại là ảnh tĩnh (2026-09-23, theo yêu cầu người dùng):** ban đầu mọi ô có `thumbTex` đều vẽ lại sau mỗi 1 giây bất kể trạng thái — 40 ô cùng "phát" một lúc trông rối, không đúng ý người dùng chỉ muốn thấy ô đang chọn động. Sửa: `bool active = live || preview;` (đúng 2 cờ đã tính sẵn trong `ClipCell` cho màu viền) — chỉ `active` mới coi là cũ sau 0.2s và được vẽ lại; các ô còn lại (`Loaded`) chỉ vẽ **đúng một lần** (lúc `thumbTex==0`) rồi đứng yên vĩnh viễn trong phiên đó, không bao giờ đặt lại `stale=true` nữa. Đã xác nhận: chạy `--shot` cùng kịch bản 3 lần liên tiếp, ô không active cho hình hơi khác nhau **giữa các lần mở app** (vì mỗi lần là tiến trình mới, bắt được một mốc thời gian animation khác nhau lúc lần vẽ đầu tiên) nhưng ô Live thì đổi rõ rệt hơn nhiều theo tổng số khung hình chạy — đúng như kỳ vọng "tĩnh trong 1 phiên, không tiếp tục phát". Debug bật assertion chạy sạch.
  - **Sửa lỗi nghiêm trọng: `abort()` khi chạy bản Debug (2026-09-23, người dùng báo qua F5 VS Code):** bản Release (`--shot`) không lộ ra vì `IM_ASSERT` bị tắt ở Release, nhưng bản Debug thật (build+chạy trực tiếp, không qua `--shot`) crash ngay khi thumbnail đầu tiên render, với thông báo *"ImDrawCmd is referring to ImTextureData that wasn't uploaded to graphics system"*. Nguyên nhân: ở bản Dear ImGui mới (1.92.x, texture quản lý động), font atlas chỉ thật sự upload lên GPU khi có lệnh `RenderDrawData` xử lý `draw_data->Textures` — lệnh vẽ khung hình chính chỉ làm việc đó **một lần, ở CUỐI khung hình**, còn thumbnail vẽ **giữa** khung hình (trong `DrawDeck`), nên ở khung hình đầu tiên atlas chưa kịp upload. Thử sửa lần 1 (trỏ `dd.Textures` vào `ImGui::GetPlatformIO().Textures`) **không đủ** vì list đó bị `UpdateTexturesEndFrame()` dọn rỗng đầu mỗi khung hình và chỉ được đổ lại ở cuối — giữa khung hình nó luôn rỗng. Sửa đúng: tự dựng một `ImVector<ImTextureData*>` một phần tử trỏ thẳng `io.Fonts->TexData` (đã build sẵn ngay sau `NewFrame()`, không phụ thuộc `platformIO.Textures`) làm `dd.Textures`, để lệnh render thumbnail tự upload atlas nếu cần, y hệt lệnh render chính làm — an toàn gọi lại nhiều lần (bỏ qua nếu atlas đã `ImTextureStatus_OK`). Đã xác nhận: chạy trực tiếp bản Debug 30s không còn assert, và `--shot --frames 200` trên **cả 3 màn hình bằng chính bản Debug** (bật assertion, phép thử nghiêm ngặt hơn `--shot` Release trước đó) đều exit 0, 0 dòng stderr.

### F5/F9 — corner-pin và mesh không kéo ra ngoài vùng 1920×1080 được (2026-09-23, người dùng báo)
`src/mapping.cpp` (`Stage()`): toạ độ chuột khi kéo (`mu`) bị `std::clamp` cứng vào đúng `[0,1920]×[0,1080]` — dùng
chung cho **cả 4 loại kéo** (corner pin, input rect, mask, mesh). Vì vậy kéo chuột **không bao giờ** đưa được
điểm corner-pin hay mesh ra ngoài khung, dù kéo chuột xa đến đâu — trong khi ô nhập toạ độ bằng số cho corner pin
(F15, `IntField` cạnh "CORNER PINS") đã luôn cho phép `-4000..8000` từ trước, tức là gõ số thì đi ra ngoài được,
kéo chuột thì không — hai đường không khớp nhau.

Kéo mesh point **bên trong** khung đã hoạt động đúng từ trước (đã xác nhận bằng ảnh chụp `--drag`, lưới biến dạng
thật khi kéo điểm nội bộ) — không phải "mesh hoàn toàn không kéo được", chỉ là không vượt được ra ngoài khung
giống corner-pin.

**Đã sửa:** thêm biến `muOut` (cùng công thức nhưng clamp `-4000..8000`, khớp đúng ô nhập số) dùng riêng cho
`dragKind==1` (corner pin, F5) và `dragKind==4` (mesh, F9). `dragKind==2` (input rect — chọn vùng nguồn trong
composition canvas, không gian khác, phải ở trong canvas) và `dragKind==3` (mask) **giữ nguyên** `mu` cũ (bó
buộc `0..1920`/`0..1080`) — không mở rộng, vì đó là không gian khác/tính năng khác chưa được người dùng báo lỗi.

Đã xác nhận bằng ảnh chụp thật (`--drag`, cả bản Release lẫn Debug bật assertion): kéo góc TL của corner-pin từ
trong khung ra ngoài, số hiện `-299, -201` (âm, vượt hẳn khung) và tứ giác kéo dài ra ngoài canvas thấy rõ; kéo
điểm mesh nội bộ lên trên khung, đỉnh lưới lồi ra ngoài viền cam. `--roundtrip` vẫn pass.

### F5/F9 — kéo thả keystone/warp kiểu Resolume (2026-09-23, người dùng báo tiếp)
Sau bản sửa ở trên, người dùng báo thêm 2 lỗi: (1) điểm keystone **bị che mất** khi kéo ra ngoài vùng màn hình,
(2) **warp không liên kết gì với keystone** — kéo góc thì lưới mesh đứng yên. Cả hai đều có gốc thật trong
`src/mapping.cpp`:
- (1) mọi thứ trên Stage bị `PushClipRect` cắt đúng theo khung 1920×1080, hit-test chỉ nhận click trong khung
  (`cv.Contains(m)`), zoom bị kẹp tối thiểu 100%, và cuộn bị kẹp trong khung canvas — nên điểm ra ngoài vừa
  vô hình vừa không nắm lại được.
- (2) `Slice::meshPts` lưu toạ độ output **tuyệt đối**, và ở chế độ mesh `SliceMapUV` bỏ qua hẳn `q[]` — kéo góc
  chỉ đổi `q`, lưới không hề biết.

**Đã làm (theo mô hình Resolume / engine `WarpCornerPin`):**
- **Keystone là phép phối cảnh thật** (homography unit-square→quad dạng đóng Heckbert, struct `Keystone` trong
  `mapping.cpp`, có cả nghịch đảo) thay cho nội suy bilinear 4 góc trước đây — đường thẳng giữ thẳng, khoảng
  cách co theo phối cảnh như máy chiếu đặt nghiêng. Tứ giác lõm/bắt chéo (không có homography hợp lệ) tự lùi
  về bilinear để không lật nội dung qua vô cực. **Thay đổi hành vi có chủ đích:** slice hình thang cũ sẽ hiển
  thị theo phối cảnh (hình chữ nhật/hình bình hành thì y hệt cũ).
- **Mesh sống trong không gian cục bộ của keystone:** `Slice::meshPts` (tuyệt đối) → `Slice::meshLocal` (toạ độ
  unit-square của keystone); output = `keystone(bilinear lưới cục bộ)`. Kéo góc keystone là cả lưới đi theo,
  giữ nguyên chỗ uốn. Kéo điểm mesh thì chuyển vị trí chuột qua **nghịch đảo keystone** rồi mới lưu. 4 góc
  lưới chính là 4 góc keystone (tay nắm lớn = keystone, chấm nhỏ = warp). Lưới chưa uốn = y hệt corner pin.
- **File cũ tự chuyển đổi khi mở** (`MigrateAbsoluteMesh`, gọi từ `ReadSlice` — áp dụng cho cả `.mikmap` lẫn
  `.mikmap-preset`): nếu có `meshPts` mà không có `meshLocal`, ở chế độ mesh thì lấy 4 góc lưới cũ làm keystone
  (vì renderer cũ vẽ đúng lưới đó, bỏ qua `q`), rồi đổi các điểm còn lại sang toạ độ cục bộ — hình hiển thị giữ
  nguyên. File mới chỉ ghi `meshLocal`.
- **Stage là khung nhìn pan/zoom tự do trên không gian output:** zoom **20%–600%** (trước: 100–600%), pan không
  bị kẹp theo khung canvas (chỉ giữ tâm nhìn trong `-4000..8000`, đúng biên của điểm), mọi thứ vẽ và hit-test
  trên **toàn vùng Stage** chứ không chỉ trong khung 1920×1080. Nút **Fit** (maximize) giờ khung **toàn bộ điểm**
  kể cả điểm ngoài khung (vẫn là 100% như cũ khi mọi điểm nằm trong khung); nút zoom-vào-slice khung theo
  bbox thật của slice (kể cả mesh). Alt+lăn zoom giữ nguyên điểm dưới con trỏ. Kéo điểm sát/qua mép Stage thì
  khung nhìn **tự cuộn** theo, nên điểm không bao giờ trượt xuống dưới panel bên cạnh.
- Tay nắm có **kích thước cố định theo pixel màn hình** (trước co theo zoom), sáng lên khi hover/kéo, giữ khoảng
  lệch lúc nắm (nắm lệch tâm không làm điểm nhảy), và hiện **nhãn toạ độ** cạnh con trỏ khi kéo.
- Ở chế độ mesh, Stage tô theo **từng ô lưới** + vẽ **viền lưới thật** (biên mesh có thể lồi ra ngoài quad),
  khung keystone vẽ mờ phía sau. Chọn slice bằng click dùng viền thật này. Điểm "LAST POINT"/thêm cột-hàng
  tính u,v qua nghịch đảo keystone thay vì bbox xấp xỉ.
- `output.cpp`: khung cắt cửa sổ máy chiếu lấy bbox của **cả lưới** (`SliceOutputBounds`), không chỉ 4 góc —
  trước đây phần mesh lồi ra ngoài quad bị cắt mất trên máy chiếu.

**Kiểm:** `--roundtrip` thêm kịch bản: keystone đi qua đúng 4 góc; tâm unit-square rơi đúng giao điểm hai đường
chéo (tính chất riêng của homography, bilinear không có); lưới chưa uốn trùng corner pin; uốn 1 điểm rồi kéo
góc TR ra ngoài khung → điểm đi theo và vẫn đúng vị trí trong không gian keystone; bbox bao được góc ngoài
khung; `meshLocal` lưu/mở đúng; file preset kiểu cũ (`meshPts` tuyệt đối) mở ra giữ nguyên hình. Ảnh chụp thật
(Release + Debug bật assertion, `--drag`/`--click`/`--outshot`): kéo góc ra ngoài → khung tự cuộn, Fit khung được
cả tay nắm ngoài khung; uốn mesh rồi kéo góc → lưới đi theo phối cảnh; cửa sổ máy chiếu render đúng nội dung
đã uốn. Hiệu năng so với code cũ cùng kịch bản (output mở): 59 FPS · P99 33.8ms mới vs 59 FPS · P99 36.2ms cũ —
không chậm đi.

**Chưa làm (có thể làm tiếp nếu cần giống Resolume hơn):** kéo cả slice bằng cách nắm vào giữa; chọn nhiều điểm
cùng lúc; nudge bằng phím mũi tên; đổi số cột/hàng hoặc thêm cột/hàng hiện vẫn **reset** lưới về chưa uốn (như
cũ) thay vì lấy mẫu lại hình đang uốn; mask (dragKind 3) vẫn ở toạ độ output tuyệt đối, chưa đi theo keystone.

### F22 — Slice input từ Composition / Layer / Group (2026-09-23, theo yêu cầu người dùng)
Mỗi slice có **Input source** (Properties → Slice, ngay dưới tên slice): **Composition** (mặc định, như trước),
**Layer · <tên>** hoặc **Group · <tên>**. Input rectangle vẫn cắt trên nguồn đó (cùng canvas 1920×1080).
- Model: `Slice::srcKind` (`SrcComp/SrcLayer/SrcGroup`) + `Slice::srcRef` (id). Layer trước đây **không có id** —
  thêm `Layer::id` ổn định (`EnsureLayerIds`, gọi ở mọi chỗ tạo/nạp/nhân bản layer), để đổi tên hay trùng tên
  layer không làm gãy routing. File cũ không có id được gán `layer-1..n` khi mở; layer nhân bản nhận id mới.
- Render: `DrawSliceSource` (`clipart.cpp`) — cùng quy tắc blend/opacity/fader nhóm/dissolve với composite, chỉ
  lọc layer thuộc nguồn. Bypass/mute vẫn ẩn layer; **solo chỉ tính trong tập layer của nguồn** (solo một layer
  khác ở main mix không làm tắt slice đang route layer riêng). `DrawComposite` giờ chỉ là nguồn Composition.
  Cửa sổ máy chiếu (`output.cpp`) vẽ mỗi slice theo nguồn riêng của nó.
- Nguồn mất (layer/group đã xoá): slice lùi về Composition, ô chọn hiện đỏ "Missing · showing Composition";
  tham chiếu **không** bị xoá nên undo khôi phục layer là routing trở lại. Lưu trong `.mikmap` và preset output.
- Giới hạn: tham chiếu theo deck hiện tại (mỗi deck có layer riêng; deck nhân bản giữ cùng id nên routing đi
  theo layer tương ứng). Stage/Live Output monitor chưa xem trước được nội dung theo nguồn slice (chỉ cửa sổ
  máy chiếu vẽ nội dung).
- Kiểm: `--roundtrip` (id duy nhất; mặc định Composition; đổi tên layer không gãy; lưu/mở Layer và Group; xoá
  layer → lùi Composition; `EnsureLayerIds` giữ id tốt, sửa id trống/trùng). Ảnh chụp thật `--outshot`: cùng
  slice đổi Composition → Layer · Spectrum thì máy chiếu chỉ còn spectrum, slice bên cạnh vẫn mix đầy đủ; bản
  Debug (bật assertion) chọn Group chạy sạch.

### Reset slice về default + cue group theo column + icon reset + Preview đúng cỡ (2026-09-23, theo yêu cầu người dùng)
- **Reset warp về default thật:** `resetWarp()` trước đây đặt quad output = input rect hiện tại (slice đang crop
  thì "reset" ra hình crop — sai nghĩa reset). Nay về fullscreen mặc định (0,0,1920,1080, khớp `NewBlankProject`);
  mesh đi theo tự động vì sống trong không gian keystone. Hành vi cũ giữ lại thành action riêng
  **`matchOutputToInput()`** (kiểu Resolume "match output to input"), không lẫn với reset.
- **Các hàm reset còn lại cũng về default:** `resetMeshWarp()` (chỉ xả uốn, giữ mật độ/splits lưới),
  `resetAllWarping()` (fullscreen + lưới uniform 4×3 mặc định, không uốn), `resetInputRect()` ("Whole area",
  theo `canvasW/H` thay vì cứng 1920×1080). Menu nút Reset trên toolbar gọi đúng các hàm này (trước là lambda
  rời rạc, "Reset mesh warp" còn xoá cả splits).
- **Menu chuột phải slice** (cây + Stage output): Whole area (input) · Match output to input · Reset warp ·
  Reset mesh warp · Reset all warping (trước chỉ có Reset warp).
- **Chọn column cũng chọn cue của group:** `fireColumn()` set `activeCol` cho mọi group nên ô `Cue N` chạy theo
  column (trước chỉ đổi `activeCol` của app, highlight cue lệch với column đang chọn).
- **Tên cột mặc định tiếng Anh:** `colName()` fallback và `insertCol()` dùng "Column N" thay "Cột N".
- **Icon nút reset (`rotate-ccw`, `ui.cpp`):** cung tròn cũ (-0.6→4.9 rad) hở ở trên-phải, mũi tên ở trên-trái —
  rời nhau nên nhìn như icon hỏng. Đổi cung -1.15→3.56 rad để đầu cung chạm đúng góc mũi tên (3,8), khớp bản
  Lucide gốc. Một chỗ sửa, hết cho toolbar Mapping, menu slice và nút reset FX bên deck.
- **Preview Cue đúng cỡ mặc định:** trước vẽ full `well` với base 480 → to gấp ~2x so với Live (letterbox +
  base 960). Nay letterbox `CanvasRect` + base 960, lưới grid và viền gói trong vùng letterbox.
- Kiểm: build Release sạch (chỉ warning có sẵn); `--roundtrip` pass; ảnh chụp headless trang Mapping (icon
  reset hiện tròn-mũi tên đúng) và Composition (Preview cùng cỡ Live).

### Kéo-thả file từ Explorer vào Browser và vào ô clip (2026-09-24, theo yêu cầu người dùng)
Ảnh / video / âm thanh kéo từ Explorer (Finder) thả thẳng vào cửa sổ:
- **Thả vào panel Browser** → file vào danh sách **Media** (tham chiếu tại chỗ, không copy — video 4K không bị copy
  nghìn MB). Danh sách lưu ở cài đặt máy (`settings.json`, khoá `mediaExtra`), không nằm trong project; file bị
  xoá/di chuyển thì tự biến khỏi Browser. Thư mục `Documents/MikMap/media` vẫn được quét như trước, nay nhận cả
  video/âm thanh.
- **Thả vào một ô clip** (lưới Deck, hoặc làn Timeline) → clip nạp ngay. Thả nhiều file: file đầu vào ô đó, mỗi file
  tiếp theo vào ô **trống** kế tiếp cùng layer (bỏ qua ô đã có clip); hết ô thì chỉ vào Browser.
- Nhận: `.png .jpg .jpeg .bmp .tga` (ảnh), `.mov .mp4 .m4v .avi .mkv .webm .wmv` (video), `.wav .mp3 .ogg .flac .aac
  .m4a .aif .aiff` (âm thanh) — phân loại theo đuôi file (`MediaKindOf`). File khác bị bỏ qua, có thông báo.
- **Giới hạn thật:** chỉ **ảnh** được vẽ (B2). Video/âm thanh được nạp vào ô clip và Browser nhưng **chưa phát**
  (B1 vẫn `[ ]`) — ô clip hiện "VIDEO / AUDIO — playback not available yet" thay vì "MISSING MEDIA" gây hiểu nhầm.
- Kiểm: `--roundtrip` thêm kịch bản (phân loại đuôi, file đầu vào ô đích + file sau vào ô trống kế tiếp, bỏ qua ô đã
  có clip, `.txt` bị loại, nhập trùng không nhân đôi, New project giữ danh sách). Ảnh chụp headless bằng cờ mới
  `--drop x,y <file>` (mô phỏng thả ở toạ độ đó): thả vào Browser hiện đúng 3 file, thả vào ô trống nạp ảnh + video.
  **Chưa thử kéo thả bằng chuột thật từ Explorer** (không tự động hoá được) — chỉ dựa vào việc GLFW đặt con trỏ tại
  điểm thả trước khi gọi callback (Win32 `WM_DROPFILES`), nên nếu ô nhận sai vị trí thì kiểm tra chỗ này đầu tiên.
- Lỗi phát hiện khi làm: chạy `--shot`/`--roundtrip` không nạp `settings.json` nên nếu gọi `SaveSettings()` sẽ ghi đè
  cài đặt thật bằng mặc định — đã chặn bằng `SetSettingsPersistence(false)` ở chế độ headless.

### Properties > Comp: chỉnh độ phân giải + các thông số composition (2026-09-24, theo yêu cầu người dùng)
Tab **Comp** không còn chỉ là bảng đọc: có các mục sửa được, xếp theo bố cục Resolume người dùng gửi. Lưu theo project
(`composition.props` trong `.mikmap`), undo/redo và cờ "chưa lưu" tính cả các giá trị này.
- **COMPOSITION — Resolution:** hai ô W × H (gõ số, áp khi Enter/Tab/bấm ra ngoài — không áp từng phím vì "3840" đi
  qua 3, 38, 384) + 4 nút preset 720p / 1080p / 1440p / 4K; hiện tỉ lệ + megapixel. Đổi độ phân giải thì **input rect
  của mọi slice co giãn theo từng trục** để vẫn phủ cùng một phần canvas (slice "whole area" 1920×1080 vẫn là whole
  area ở 3840×2160); quad/mesh/mask đầu ra ở không gian pixel của screen nên không đổi. Giới hạn 64…16384.
  Trang Advanced Mapping › **Input selection** trước đây cứng 1920×1080 — nay khung sân khấu theo đúng độ phân giải và
  tỉ lệ canvas (đã xem ở 3840×1080 và 1920×1080); trang Output routing vẫn 1920×1080 pixel của screen như cũ.
- **Master** (0–100%) và **Video › Opacity** nhân vào alpha của toàn bộ composite (Live Output + cửa sổ máy chiếu +
  slice lấy nguồn Composition/Layer/Group; Preview Cue chỉ xem một clip nên không đổi) → **A11 chuyển `[x]`**.
- **Speed** (0–400%) nhân tốc độ chạy của mọi clip đang chạy và của playhead Timeline.
- **Transform** (Position X/Y theo pixel canvas, Scale %, Rotation °, Anchor X/Y): áp cho **cả composite** bằng cách
  gộp vào transform riêng của từng clip (clip là art căn giữa nên phép gộp cho kết quả đúng như biến đổi cả ảnh xong);
  nút RESET trả về mặc định.
- **CHƯA có tác dụng (chỉ lưu giá trị, panel ghi rõ):** **Audio** Volume/Pan (chưa có audio engine) và **CrossFader**
  Blend Mode/Behaviour/Curve (A14 crossfader A/B vẫn `[ ]`).
- Kiểm: `--roundtrip` thêm kịch bản (đổi độ phân giải co giãn input rect đúng từng trục, đổi cùng cỡ không trôi, kẹp
  64–16384, mọi thông số lưu/nạp được, đổi thông số làm project "dirty", vừa nạp thì không dirty). Ảnh chụp headless
  bằng cờ test mới `--comp scale=55,rot=25,px=300,...`: Live Output xoay/thu nhỏ/dịch đúng, Preview Cue giữ nguyên;
  Master 35% × Opacity 80% làm Live Output tối đi; preset 4K đổi nhãn Live Output thành 3840×2160.
  **Chưa thử** kéo các thanh trượt bằng chuột thật (chỉ thử qua `--click` preset và `--comp`); các thanh trượt dùng
  đúng widget `Slider` của Properties › Clip nên hành vi giống hệt.

### Properties > Layer: bố cục theo Resolume (2026-09-24, theo yêu cầu người dùng)
Tab **Layer** xếp lại theo ảnh người dùng gửi (Layer / Audio / Video / Transition / Transform), thay cho lưới chip blend cũ.
- **Chạy thật:** **Master** (0–100%, nhân alpha cả layer), **Audio › Volume** (hiện/sửa bằng dB, ghi vào chính thanh A
  trên dải layer — 100% = 0 dB), **Video › Blend Mode** (dropdown, đúng 8 mode đang render), **Opacity**,
  **Transition › Duration** (= `blendTime` của dissolve A10, đồng bộ với ô "ADD 0.5 s" trên dải layer), và **Transform**
  (Position X/Y, Scale, Rotation, Anchor X/Y — áp cho clip của layer đó, xong mới đến transform của composition).
- **Chỉ lưu giá trị, panel ghi rõ:** **Pan** (chưa có audio engine), **Size W×H / Auto Size** (chưa có canvas riêng cho
  layer), **Transition › Blend Mode** (mới có dissolve alpha). Mục Solo/Mute/Bypass vẫn giữ (dạng nút ROUTING).
- Lưu theo project (các khoá mới trong mỗi layer; file cũ thiếu khoá → mặc định trung tính).
- Kiểm: `--roundtrip` thêm kịch bản lưu/nạp mọi thuộc tính layer + không dirty sau khi nạp. Ảnh chụp headless (cờ test mới
  `--layer N,scale=60,rot=20,px=250,master=60`, `--band 70` để panel hiện đủ): Layer 3 thu nhỏ/xoay/dịch/tối đi còn các layer
  khác trong Live Output giữ nguyên; giá trị Volume −6 dB khớp thanh A 50% trên dải layer. **Chưa thử** kéo thanh trượt bằng chuột thật.

### Menu deck lên thanh SYSTEM TIME (2026-09-24, theo yêu cầu người dùng)
Tab **DECK A/B…** và công tắc **GRID | TIMELINE** chuyển từ hai hàng phía trên lưới deck lên **thanh SYSTEM TIME** dưới hai
màn Preview/Live (tab ở dòng trên, GRID|TIMELINE ở dòng dưới, nằm giữa đồng hồ và cụm nút transport). Phía trên lưới còn
đúng một hàng (nút +LAYER/GROUP/COLUMN/SYNC hoặc LOOP ON/OFF của Timeline) nên lưới deck cao thêm 26px. Tab quá nhiều thì
co nhỏ và cắt bớt chữ; menu chuột phải/đổi tên bằng bấm đúp giữ nguyên. **Dự phòng:** nếu thanh bị ẩn/thấp hơn 44px
(Cài đặt › Layout › timeline height) hoặc quá hẹp thì hai hàng cũ tự hiện lại phía trên lưới — menu deck không thể biến mất.
Đã chụp cả chế độ Grid và Timeline (bấm công tắc ở chỗ mới). **Chưa thử** nhánh dự phòng (thanh thấp) và nhiều hơn 1 deck.

### Bỏ đồng hồ SYSTEM TIME + cụm nút +LAYER/GROUP/COLUMN/SYNC → menu "DECK TOOLS" (2026-09-24, theo yêu cầu người dùng)
- **Đồng hồ SYSTEM TIME** (nhãn + giờ hệ thống) đã bỏ khỏi thanh transport; tab deck + GRID|TIMELINE giờ nằm sát mép trái thanh.
- **Cụm 4 nút** phía trên lưới deck đã bỏ. Nhãn ghim "LAYERS ⌄" ở góc trên-trái lưới (trước đây chỉ là chữ) thành nút **DECK TOOLS ⌄**
  mở menu: **Add layer** · **New group from selected layer** · **Add column** · **Sync to beat: ON/OFF** — đủ 4 chức năng cũ, không mất gì.
  Trong Grid mode không còn hàng nào phía trên lưới (lưới cao thêm 34px); Timeline mode giữ một hàng cho nút LOOP ON/OFF.
- Đổi nhỏ kèm theo: "Add layer" tạo layer có **đúng số ô bằng số cột của deck** (trước cứng 8 ô — lệch khi deck có số cột khác 8).
- Kiểm: `--roundtrip` thêm kịch bản cho 3 hàm mới `App::addLayer/groupSelectedLayer/toggleSync`; ảnh chụp nhãn và menu mở ra đủ 4 mục.
  **Chưa bấm thử** từng mục menu bằng chuột trong app.

### Layer thu gọn không hiện thumbnail (2026-09-24, theo yêu cầu người dùng)
Ô clip của layer đã thu gọn (hoặc nằm trong group đóng) chỉ còn **dải tên + chế độ phát/thời lượng**, không vẽ thumbnail nữa
(trước đây là một lát ảnh 16px khó đọc, lại tốn ngân sách vẽ thumbnail mỗi khung hình). Bấm dải tên/thân ô vẫn cue/phát như cũ.
Đã chụp ảnh layer thu gọn cạnh layer mở. Layer mở giữ nguyên thumbnail.

### Properties Comp/Layer: tên lên đầu, bỏ các dòng thông tin thừa (2026-09-24, theo yêu cầu người dùng)
Tab **Comp** và **Layer** mở đầu bằng dòng **tên** (Composition · tên project / Layer · tên layer), rồi tới các điều khiển. Đã bỏ các dòng
đọc-số ở cuối: Comp (Layers, Groups, Columns, BPM, Beat sync, Rate, Latency, Output — trùng với deck và thanh trạng thái), Layer (Group,
Play mode, State). Tab Clip giữ nguyên. Đã chụp cả hai tab; **chưa cuộn tới cuối panel** để xem bằng mắt (chỉ xác nhận qua code là các dòng đã bị xoá).

### Tên layer sửa được trong Properties › Layer (2026-09-24, theo yêu cầu người dùng)
Đầu tab Layer là ô **Name** sửa trực tiếp (ghi vào tên layer ngay khi gõ; tên rỗng/toàn khoảng trắng thì giữ tên cũ, cắt khoảng trắng hai đầu — cùng
quy tắc với popup đổi tên). Bấm đúp tên trên dải layer vẫn đổi tên được như cũ. Tên Composition vẫn chỉ đọc (gắn với tên file project).
Đã chụp ô Name; **chưa thử gõ chữ thật** (script test không gõ phím được).

### Properties > Clip làm lại theo Resolume + mục riêng cho từng effect (2026-09-24, theo yêu cầu người dùng)
Tab **Clip** xếp theo ảnh người dùng gửi: **Name** (sửa được ngay, đổi tên vẫn ghim hình vẽ) · ảnh xem trước · **Transport** · **Autopilot** ·
**Audio** (chỉ hiện với file audio/video) · **Video** · **Transform** · **Effects**.
- **Chạy thật:**
  - **Transport:** thanh timeline có **kéo playhead để tua**, **hai mốc in/out** (kéo được) giới hạn vùng phát — loop/bounce/once/hold đều chạy trong vùng
    này (`AdvanceClip`; mặc định 0–100 phát cả clip như cũ); nút **◀ ‖ ▶** là transport riêng của clip (đảo chiều / tạm dừng / phát; bấm phát lại clip thì tự
    tiếp tục); dropdown **chế độ lặp** (Loop/Bounce/Hold/Once); **Speed** 0–400%; **Duration** hiện giây, nút **/2** và **×2** đổi thời lượng thật.
  - **Video:** kênh **R G B** (tắt kênh nào thì kênh đó biến mất khỏi hình, cả thumbnail/preview/output), **Opacity**, **Blend Mode** riêng của clip
    (Layer Determined = theo layer, hoặc 8 mode ghi đè), thông tin nguồn (ảnh: kích thước thật; generator; video: ghi rõ chưa giải mã được).
  - **Transform:** Position X/Y (hiện theo pixel canvas), Scale %, Rotation, **Anchor X/Y** (tâm xoay/phóng — đã chụp: anchor lệch phải làm hình quay quanh điểm đó), Flip H/V, RESET.
  - **Effects:** mỗi effect trên clip có **một mục riêng** với đủ tham số của nó (slider, lựa chọn, Dry/Wet, BEAT/AUDIO) + nút mắt (bypass) và × (xoá);
    chuột phải tiêu đề mở menu (move/duplicate/reset/remove). Kéo effect từ Browser vào clip → mục của effect đó tự xuất hiện. (Thay cho danh sách chain +
    một bảng chỉnh cho effect đang chọn trước đây.)
- **Chỉ lưu giá trị, panel ghi rõ:** chế độ Transport (BPM Sync), **Autopilot** (Action/Loops — vì hiện chỉ clip đang được CHỌN mới chạy playhead nên
  autopilot chưa có nền để chạy), **Audio** Volume/Pan (chưa có audio engine), **Size W×H**, kênh **A** (xám: clip không có kênh alpha để bật/tắt).
- Đã bỏ các dòng đọc-số cũ (Clip/Source/Duration/Play mode/Speed/Playhead/Resolution/Codec/Beat sync/State) và nhãn giả "TUNNEL_04.MOV".
  Lưu theo project (khoá mới trong mỗi clip; pause là trạng thái runtime, không lưu).
- Kiểm: `--roundtrip` thêm kịch bản (loop/once/bounce trong vùng in-out, pause đứng yên, mặc định vẫn 0–100, lưu/nạp mọi thuộc tính, không dirty sau
  khi nạp). Ảnh chụp headless (cờ test `--clip`, `--fx`, `--inspscroll`): toàn bộ panel kể cả 4 effect, kênh R-only ra hình đỏ, anchor lệch làm hình dịch/quay quanh điểm.
  **Chưa thử** bằng chuột thật: kéo playhead/mốc in-out, bấm ◀ ‖ ▶, /2 ×2, gõ tên clip; **chưa thử** Blend Mode override và clip file ảnh/video trong panel này.

### Giảm glow viền ô clip + màu clip gộp vào popover (2026-09-24, theo yêu cầu người dùng)
- **Glow ô clip:** quầng sáng quanh ô đang chọn giảm từ 0.35 → 0.14 (blur 12 → 5), quanh ô đang live từ 0.40 → 0.20 (blur 12 → 6) — không còn lan sang ô bên cạnh.
  Viền 2px của ô được chọn giữ nguyên nên vẫn nhận ra ô nào đang chọn/đang live.
- **CLIP COLOR:** ô chọn màu trước đây là một hộp thứ hai treo bên dưới popover; nay nằm **trong cùng một popover** của clip (dưới "Clear Slot", ngăn bằng một vạch).
  Đã chụp popover chuột phải trên thanh tên clip. **Chưa bấm thử** đổi màu bằng chuột.

### Màu clip điều khiển cả trạng thái chọn của ô (2026-09-24, theo yêu cầu người dùng)
Ô clip nay lấy **cả ba trạng thái từ màu riêng của clip** (Clip color trong popover) thay vì cố định nâu/cyan/cam: **không chọn** = sắc tối của màu đó (cam → nâu,
xanh dương → xanh đen, xanh lá → lục tối…), **được chọn/cue** = chính màu đó (viền + dải tên sáng lên), **đang live** = màu đó đậm nhất kèm glow. Vòng chọn cũng theo màu clip
(trước là cyan cố định cho ô cue). Màu mặc định (cam) giữ đúng vẻ cũ: nâu khi không chọn, cam khi chọn/live — chỉ ô "được chọn" đổi từ cyan sang cam. Đã chụp
clip xanh dương / xanh lá / tím cạnh clip cam. **Chưa** áp cho khối clip trong Timeline (vẫn dùng cyan làm viền chọn).

### Màu layer đổi được trong Properties › Layer (2026-09-24, theo yêu cầu người dùng)
Tab Layer có hàng **Color** (6 màu, cùng bảng màu với clip) ngay dưới Name. Màu layer là **màu nhấn của dải layer** trong deck: thanh chọn bên trái + quầng sáng khi layer được chọn,
mũi tên khi layer đang live, thanh trượt V; layer không được chọn có thanh trái mờ (35%) để vẫn thấy màu. Màu mặc định (cam) giữ như cũ. Lưu theo project (`color` trong layer,
`--roundtrip` đã kiểm). Màu layer **không** đổi màu các clip trong layer — clip giữ màu riêng (đổi ở popover clip). Đã chụp hai layer đổi sang xanh dương/xanh lá.

### Preview Cue chỉnh transform trực tiếp + menu chuột phải (2026-09-24, theo yêu cầu người dùng)
Khung **Preview Cue** giờ là bảng chỉnh transform của clip đang cue (cùng dữ liệu với Properties › Clip › Transform, nên nếu clip đang live thì Live Output đổi theo):
- **Khung viền + tay nắm** quanh nội dung clip (ảnh: đúng khung ảnh đã fit vào canvas; generator/video: cả canvas), đi theo tịnh tiến/xoay/scale/anchor hiện có.
  **Kéo trong khung = dời**, **kéo ô vuông nhỏ (4 góc + 4 cạnh) = scale**, **kéo vòng tròn quanh góc = xoay**.
- **Chuột phải:** Center X · Center Y · Mirror X · Mirror Y · Left Half · Top Half · Right Half · Bottom Half · Reset. Half = xoay về 0, scale đều để vừa nửa canvas và đặt vào giữa
  nửa đó; Center tính cả anchor; Reset trả mọi thứ (kể cả flip/anchor) về mặc định.
- Menu chuột phải chung nay có **vạch ngăn** và mục không icon (trước chỉ có mục có icon), và tô màu mục theo `toneHex` (mục đang chọn của các dropdown ô chọn).
- **Giới hạn thật:** clip chỉ có **một hệ số scale đều** nên ô vuông ở cạnh giữa cũng scale đều (chưa kéo giãn riêng chiều ngang/dọc); bỏ qua khi có popover/menu/hộp thoại mở;
  khung bị cắt theo viền màn hình Preview nếu clip đã dời ra ngoài.
- Kiểm: `--roundtrip` thêm kịch bản cho cả 9 mục menu + công thức anchor (`ClipEffectivePos`). Ảnh chụp headless: khung + tay nắm; kéo dời 68px làm nội dung dịch (cả Live Output vì clip đang live); menu đúng bố cục ảnh tham chiếu.
  **Chưa thử** kéo scale/xoay bằng chuột (đã thử dời); các phép toán scale/xoay theo tỉ lệ khoảng cách/góc từ tâm khung khi bắt đầu kéo.

### Màu layer: xuống cuối tab, swatch sáng không viền, đổi theo cả clip (2026-09-24, theo yêu cầu người dùng — thay cho ghi chú trước đó "màu layer không đổi màu clip")
- Hàng **COLOR** chuyển xuống **cuối** tab Layer (sau ROUTING). 6 ô là 6 màu cố định ở độ sáng đầy đủ, **không viền**; màu đang dùng có một chấm trắng nhỏ ở giữa.
- **Đổi màu layer thì các clip đang cùng màu với layer (màu cũ) đổi sang màu mới**; clip mà người dùng đã tô màu khác thì giữ nguyên. Logic ở `App::setLayerColor`
  (`--roundtrip` kiểm: chỉ clip cùng màu cũ đổi, chọn lại đúng màu đó thì không đụng gì, chỉ số layer sai bị bỏ qua). Đã chụp cuối tab Layer. **Chưa bấm thử** bằng chuột.

### Preview Cue: khung transform chỉ hiện khi rê chuột, zoom/kéo màn hình (2026-09-24, theo yêu cầu người dùng)
- **Khung chỉnh transform chỉ hiện khi con trỏ nằm trong khung Preview** (hoặc đang kéo dở); rời chuột ra là ẩn. Khi bật công cụ bàn tay thì khung ẩn hẳn.
- **Cuộn chuột giữa = zoom** quanh con trỏ (điểm dưới chuột đứng yên), 10%–1600% so với vừa khít. **Giữ chuột giữa + kéo = kéo màn hình** (cả ngang lẫn dọc).
- **Hai nút góc trên-phải Preview:** dropdown zoom (hiện % kích thước pixel thật của canvas; menu: Fit · 12% · 25% · 50% · 100% · 200% · 400%) và **bàn tay** (bật thì kéo bằng chuột trái để dời màn hình, con trỏ thành bàn tay).
  Trạng thái zoom/vị trí là của phiên làm việc (không lưu vào project); Fit đưa về mặc định. Thêm icon `hand` vào bộ icon vẽ tay.
- Kiểm: ảnh chụp headless (cờ test `--pv zoom,panX,panY,hand`, `--hover x,y`): ẩn khung khi không hover, hiện khi hover; zoom 0.5× + kéo + bàn tay bật; menu zoom. **Chưa thử bằng chuột thật:** cuộn giữa để zoom,
  giữ chuột giữa để kéo, kéo trái với bàn tay, chọn mục % trong menu (các phép tính giống editor Preview đã dùng).

### Thanh tên clip sáng bằng viền (2026-09-24, theo yêu cầu người dùng)
Thanh tên của ô clip nay sáng **bằng đúng màu viền**: ô được chọn/cue và ô live → thanh là chính màu clip (chữ tự chọn tối/trắng theo độ sáng của màu — chữ trắng trên vàng sẽ khó đọc);
ô không chọn → thanh cùng sắc tối với viền (không còn sáng hơn hay tối hơn viền). Đã chụp clip cam / vàng / xanh dương / xanh lá.

### Ô clip: viền = đang phát, thanh = đang chọn (2026-09-24, theo yêu cầu người dùng)
Viền và thanh tên của ô clip giờ trả lời hai câu hỏi khác nhau (trước đây cùng sáng/tối theo một trạng thái):
- **Viền sáng (dày) = clip đang phát.** **Thanh sáng = clip đang được chọn/xem preview.**
- Bấm thân clip → clip vừa phát vừa được chọn: **viền và thanh cùng sáng**. Sau đó cue clip khác trong cùng layer (bấm thanh): clip đó **sáng thanh, viền thường**;
  clip đang phát **giữ viền sáng nhưng thanh tối**. Đã chụp đúng chuỗi này (bấm thân Strobe Tunnel rồi bấm thanh Plasma Waves). Bỏ quầng sáng chung quanh ô chỉ-được-chọn
  (chỉ ô đang phát còn glow). Ô trống được chọn vẫn có viền mảnh để thấy chỗ đang chọn. **Chưa bấm thử** bằng chuột thật.

### GRID|TIMELINE xuống chỗ DECK TOOLS, chức năng deck tools vào menu tab DECK (2026-09-24, theo yêu cầu người dùng — thay cho mục "DECK TOOLS" trước đó)
- Nút **DECK TOOLS** bỏ. Công tắc **GRID | TIMELINE** chuyển xuống đúng chỗ đó (góc trên-trái lưới deck; ở chế độ Timeline thì nằm bên trái hàng có nút LOOP ON/OFF). Thanh transport chỉ còn tab deck.
- **Add layer · New group from selected layer · Add column · Sync to beat: ON/OFF** nay nằm trong **menu của tab DECK**: chuột phải tab, hoặc bấm **mũi tên nhỏ** mới có trên tab đang chọn
  (bấm phần chữ của tab vẫn là chuyển deck, bấm đúp vẫn đổi tên). Nếu mở menu từ một tab không phải deck hiện tại, các thao tác này áp vào deck của tab đó (tự chuyển sang tab đó trước).
- Dự phòng khi thanh transport bị ẩn/quá thấp: hàng tab + hàng công tắc vẫn hiện phía trên lưới như trước. Đã chụp bố cục mới, menu từ mũi tên tab và chế độ Timeline; bốn thao tác dùng lại
  các hàm `App::addLayer/groupSelectedLayer/toggleSync/insertCol` đã có test. **Chưa bấm thử** từng mục menu bằng chuột.

### Timeline: công tắc GRID|TIMELINE thay chỗ nhãn "BAR nn", bỏ nút LOOP ON/OFF (2026-09-24, theo yêu cầu người dùng)
Ở chế độ Timeline, công tắc **GRID | TIMELINE** nằm ở **ô trái của thước** (chỗ nhãn "BAR nn" cũ — thước cao thêm 18 → 26px cho vừa nút); hàng riêng phía trên timeline bỏ hẳn cùng **nút LOOP ON/OFF**.
Khi tab deck không nằm trên thanh transport (thanh bị ẩn/quá thấp) thì hàng phía trên vẫn giữ công tắc và nhãn "BAR nn" vẫn hiện. **Hệ quả cần biết:** không còn chỗ nào bật/tắt lặp playhead Timeline
(`tlLoopOn` vẫn còn trong code, giữ nguyên giá trị hiện có, chỉ mất nút) — nếu cần lại thì báo. Đã chụp chế độ Timeline.

### Hàng SYSTEM TIME phía trên deck (2026-09-24, theo yêu cầu người dùng)
Thêm một hàng mảnh (26px) ở đầu vùng deck, ngay trên lưới/timeline, hiện **SYSTEM TIME** + giờ hệ thống `HH:MM:SS` (font số cố định). Hàng này luôn có (cả Grid lẫn Timeline, cả khi tab deck nằm ở dự phòng); lưới/timeline thấp đi 26px. Đã chụp chế độ Grid.

### Thanh transport bên trái thành 2 hàng: SYSTEM TIME + menu deck kiểu thanh nav (2026-09-24, theo yêu cầu người dùng — thay cho hàng "SYSTEM TIME phía trên deck")
Khối bên trái thanh transport có **2 hàng như khối TIMELINE bên phải**: hàng 1 = **SYSTEM TIME + giờ hệ thống**; hàng 2 = **menu deck** vẽ giống thanh nav trên cùng (Composition / Advanced Mapping / Sensor I/O):
một nhóm bo tròn tối, tab đang chọn viền + glow cam, tab khác chữ xám, tab đang chọn có mũi tên mở menu deck. Hàng SYSTEM TIME riêng phía trên lưới deck đã bỏ (lưới cao lại như trước).
Dự phòng khi thanh transport thấp hơn 44px hoặc quá hẹp: Deck() tự vẽ hàng SYSTEM TIME + hàng tab (kiểu nav) phía trên lưới. Đã chụp bố cục 2 hàng; nhánh dự phòng **chưa chụp**.

### Input selection: khung xoay/di chuyển/co giãn như Preview + menu chuột phải kiểu Resolume (2026-09-26, theo yêu cầu người dùng)
Trang **Input selection** (Advanced Mapping) trước chỉ có 4 tay nắm góc để co giãn hình chữ nhật, không xoay, không kéo cả khối, menu chuột phải chỉ có "Whole area".
Nay khung input sửa giống khung transform của Preview Cue: **kéo trong khung = di chuyển** (khung thẳng thì bị giữ trong canvas), **ô vuông ở 4 góc + 4 điểm giữa cạnh = co giãn** trong hệ trục của chính khung (góc/cạnh đối diện đứng yên, kể cả khi đã xoay), **vòng tròn quanh 4 góc = xoay** quanh tâm (giữ Shift = nhảy 15°). Panel Slice Properties thêm thanh **Rotation** (−180…180°, bấm đúp để về 0).
- Dữ liệu: `Slice::irot`, `iflipX`, `iflipY` (`app.h`), lưu/đọc trong `project.cpp` (nên undo phủ luôn); `ix..ih` vẫn là hình chữ nhật CHƯA xoay nên tâm và các ô số không đổi khi xoay. `WarpMap::Map` (`mapping.cpp`) hoàn tác xoay quanh tâm rồi mới lật, nên **output thật** (cửa sổ máy chiếu) nhận đúng vùng đã xoay/lật.
- **Chuột phải trên khung input** — đúng danh sách trong ảnh mẫu của Resolume: Center X · Center Y · Mirror X · Mirror Y | Left/Top/Right/Bottom Half · Whole Area | **Match Output Shape** (input lấy hình dạng + góc xoay của quad output) · **Swap Input Output Shape** | Bring Forward · Send Backwards (đổi thứ tự slice, mờ ở đầu/cuối) | Duplicate · Copy · Cut · Paste (Paste mờ khi clipboard trống, Cut mờ khi chỉ còn 1 slice). Clipboard slice là runtime, không lưu vào dự án.
- "Match output to input" (menu slice) nay cũng mang theo góc xoay của input, và quy đổi canvas→output theo từng trục (trước đây chép thẳng số px, chỉ đúng khi canvas = 1920×1080). "Whole area" nay đưa xoay về 0.
- **F13 `[ ]` → `[~]`:** position/scale/rotate/flip đã có **cho input rect** (bằng tay + số + menu); chưa có cho **output** — quad output vẫn chỉ là 4 corner pin tự do, không có nút xoay/lật riêng. Không tick `[x]` cho tới khi output cũng có.
- Kiểm bằng ảnh chụp (`--shot --screen 1 --page 0`): vòng xoay kéo ra 6° (nhãn `1720 × 980 · 6°`, thanh Rotation cập nhật, Undo bật); menu hiện đủ 17 mục, "Send Backwards" và "Paste" mờ đúng; Swap sau khi xoay đưa quad output thành chữ nhật nghiêng 6° (chụp trang Output routing); kéo góc/cạnh/cả khối trên khung thẳng ra đúng ô số (giữ trong canvas). `--roundtrip` thêm: ánh xạ `WarpMap` (thẳng/Mirror X/Mirror Y/xoay 180°), lưu-nạp `irot`/flip, `matchOutputToInput` mang góc xoay, Bring Forward/Send Backwards, Duplicate/Copy/Cut/Paste (id không trùng). **Chưa thử:** kéo ô co giãn khi khung đang xoay bằng chuột thật (logic có, chỉ mới kiểm khung thẳng), và phím tắt cho các lệnh menu.

### Slice properties kiểu Resolume + Input Mask nhiều hình (2026-09-26, theo yêu cầu người dùng)
- **Slice Properties (trang Input):** hàng `X/Y` (tâm), `Left/Top`, `Width/Height`, `Rotation` với ô gõ số + nút −/+ (Shift ×10) — `StepRow`/`FloatField` (`mapping.cpp`, `ui.cpp`). Thêm ô **Soft Edge** (`Slice::softEdge`, lưu trong dự án) — **chỉ là công tắc: output chưa làm mờ viền slice** (F20 vẫn `[~]`).
- **Mask thành đa giác N điểm** (`Mask::pts` là `std::vector`, ≥3 điểm; file cũ 4 điểm vẫn đọc được): thanh **Input Mask** có 6 nút — tim (36 điểm), vuông (4), tròn (32), tam giác (3), lục giác (6), **bút** (bấm từng điểm, đóng bằng điểm đầu/Enter/bấm đúp, Esc huỷ). **Nút Mask ở chân cây bên trái đã bỏ**, chuyển vào đây (menu chuột phải "Add mask" vẫn thêm mask vuông). Mask vẫn chỉ vẽ/sửa ở trang Output routing và **vẫn chưa che hình ở output thật** — F12 giữ `[~]` (nay là đa giác thay vì 4 điểm, vẫn chưa bezier).
- Kiểm bằng `--shot`: panel đúng thứ tự mẫu; bấm tim → 36 điểm, tự sang Output và chọn mask; bút 4 điểm + bấm lại điểm đầu → mask 4 điểm. `--roundtrip`: đúng số điểm từng hình, giữ trong khung output, bút <3 điểm bị bỏ, lưu/nạp mask 5 điểm và `softEdge`. **Chưa thử:** kéo điểm của mask tim/tròn bằng chuột thật (vòng lặp đã tổng quát theo số điểm), undo sau khi vẽ bút.
- **Screen properties kiểu Resolume (2026-09-26):** Device · Width · Height · **Opacity · Brightness · Contrast · Red · Green · Blue** (`Screen::opacity/brightness/contrast/red/green/blue`, lưu trong dự án và preset Screen). **Có tác dụng thật lên cửa sổ máy chiếu** (`output.cpp` + `DrawColorAdjust`, `clipart.cpp`): opacity nhân alpha, còn lại là các lớp phủ blend GL nhân/gain/cộng/trừ trong đường viền slice. Đây là color correction **theo screen** — `F19` (theo *slice*) vẫn `[ ]`. Giới hạn: slice chồng nhau bị chỉnh 2 lần ở vùng chồng; Width/Height chỉ là nhãn độ phân giải thiết bị, chưa đổi không gian output; không tác động Live Output trong workspace. Kiểm bằng `--outshot` (opacity 0 → đen, brightness +100 → ~126, contrast −100 → xám phẳng) và `--roundtrip` (lưu/nạp đủ 8 trường).
