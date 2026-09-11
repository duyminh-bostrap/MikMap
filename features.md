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
| **Cài đặt ứng dụng** — `AppSettings` | ✅ Xong | `settings.json`, tách khỏi file project |
| **Đa ngôn ngữ VI / EN** — `Localization` | ✅ Xong | 214 khoá × 2, đổi ngay không cần khởi động lại |
| **Font Inter** (OFL) | ✅ Xong | `bin/data/fonts/` — đủ dấu tiếng Việt |
| **F12 — mặt nạ bezier** | ✅ Xong | `BezierMask`, nút ở contentUV, mép mờ |
| **Giao diện MikMap** | ✅ Xong | 3 trang cố định, bảng màu mới, `ui/Theme` |

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

### ⚠️ Đính chính bảng F (2026-09-10)

Bảng F trước đây tick `[x]` cho **sáu mục chưa có dòng code nào**. Đã kiểm
lại từng mục bằng cách tìm trong source và bỏ tick:

| Mục | Bằng chứng |
|---|---|
| F18 Polygon slice | `WarpType` chỉ có `CornerPin`, `Mesh`, `Bezier` |
| F21 Snapping | không có gì trong `core/` lẫn `ui/` (chỉ có `PixelSnapH` của font ImGui, không liên quan). **→ đã làm, xem mục F21 bên dưới** |
| F22 Slice input từ Layer | `Slice` chỉ có `inputOrigin`/`inputSize` — một hình chữ nhật trên canvas, không có trường chọn nguồn. **→ đã làm nửa Layer, xem mục F22 bên dưới** |
| F23 Spout / NDI | `ScreenOutputType` có sẵn hai giá trị enum nhưng **không nơi nào dùng tới**; `render/` và `app/` không nhắc đến |
| F24 Art-Net / sACN | không có |
| F25 SDI | không có |

★ Một bảng tick sai còn tệ hơn không có bảng: nó khiến người lập kế hoạch
tin là đã xong và không phân bổ thời gian, rồi vỡ ra vào lúc dựng sân khấu.

### 🟢 F10 — Bezier warping (ĐÃ XONG THẬT, 2026-09-10)

Mặt Bézier song bậc ba 4×4 (16 điểm điều khiển) cho bề mặt cong THẬT:
cột tròn, vòm, tượng, vải rủ.

**Vì sao cần, khi đã có mesh:** mesh nội suy song tuyến tính từng ô nên
đạo hàm gãy ở biên ô — chiếu lên cột tròn thấy rõ vệt gấp chạy dọc đường
lưới. Muốn giấu phải tăng mật độ lưới rất cao, và khi đó người vận hành
phải kéo hàng trăm điểm. Bézier cho bề mặt trơn tuyệt đối với 16 điểm.

**Nghịch đảo** (đường đi của điểm chạm sensor — architecture.md §10.3)
không có dạng đóng, nên làm hai bước: đoán thô trên lưới mẫu 8×8 rồi nắn
bằng Newton với Jacobian giải tích.

| Kiểm chứng | Kết quả |
|---|---|
| Round-trip trên mặt cong, 3721 điểm | **3721/3721**, sai số lớn nhất **8.88e-16** (≈ 4 ULP) |
| Bốn góc sau khi uốn | khớp chính xác (Bernstein nội suy hai đầu) |
| Bề mặt gấp | `isInvertible()` báo false → UI hiện cảnh báo đỏ |
| Lưu / nạp `.hexmap` | 16 điểm sống sót nguyên vẹn |
| Đổi từ CornerPin/Mesh sang Bezier | giữ **nguyên bốn góc** đã căn |

**Kết quả:** `333/333 test xanh · 5250 assertion`

### 🐞 Lỗi có sẵn tìm được khi làm F10

Test `Mesh: round-trip trên lưới ĐÃ BIẾN DẠNG` hỏng từ lâu, hoá ra không
phải sai số vặt mà là **lỗi thật**: `pointInQuad` so dấu tích có hướng với
0 **không dung sai**, nên điểm nằm đúng trên đường chéo ô lưới bị từ chối.
Mà tâm hình bình hành nằm đúng trên đường chéo, và lưới warp phẳng hoặc
uốn đều thì **mọi ô đều là hình bình hành** ⇒ một **vệt chạm chết chạy
chéo qua từng ô**: chạm đúng đó không phản ứng, lệch một pixel lại chạy.

Đo được: `d3 = -6.82e-13` trong khi hai giá trị kia là `+7.5e+03`.

Đã sửa bằng ngưỡng **theo tỉ lệ** độ lớn tam giác (tích có hướng có đơn vị
diện tích nên hằng số tuyệt đối sẽ sai với lưới toạ độ lớn hoặc nhỏ).
Chỉ ảnh hưởng `WarpMesh`; `WarpCornerPin` dùng nghịch đảo homography.

### 🟢 F22 — Slice lấy nội dung từ một Layer riêng (2026-09-10)

`[~]` chứ không phải `[x]`: **nửa Layer đã xong, nửa Group chờ A13** (Group
chưa tồn tại trong model, xem mục "Xây tiếp").

**Vấn đề F22 giải:** trước đây mọi slice đều lấy từ Composition đã trộn —
đúng khi nhiều máy chiếu ghép thành MỘT bề mặt lớn. Nhưng khi các bề mặt
độc lập nhau (màn sau lưng DJ chạy nội dung này, hai cột hai bên chạy nội
dung khác) thì cách duy nhất trước đây là xếp nội dung vào các góc khác
nhau của canvas rồi cắt ra — tức là phải dựng nội dung theo đúng bố cục
sân khấu, và không đổi được nữa.

**Ba quyết định đáng ghi lại:**

★ **Chỉ số trỏ vào layer đã xoá thì LÙI VỀ Composition, không cho ra màn
đen.** Người dùng trỏ slice vào Layer 3 rồi xoá bớt layer là chuyện thường.
Màn đen im lặng là kiểu hỏng tệ nhất trong buổi diễn — không thông báo,
không log, chỉ một máy chiếu tắt ngóm. Lùi về composition vẫn sai ý người
dùng nhưng **sai thấy được**, và UI hiện cảnh báo vàng ngay cạnh ô chọn.

★ **Chỉ nướng FBO cho layer THẬT SỰ có slice dùng tới** (`layersUsedAsSource`).
Mỗi FBO ở 4K là ~32 MB VRAM cộng một lần clear + vẽ mỗi frame; nướng cho
mọi layer là cách chắc chắn làm tụt fps vì một tính năng đa số project
không dùng. Bộ đệm không còn ai dùng được giải phóng ngay trong frame đó.

★ **Slice/screen đang TẮT vẫn giữ bộ đệm.** Bỏ qua cho đỡ tốn nghe hợp lý,
nhưng hậu quả là bật lên giữa buổi diễn thì layer nguồn chưa được nướng ở
frame đó — máy chiếu loé một frame đen rồi mới có hình.

Và trong bộ đệm riêng, clip được vẽ bằng **alpha thường chứ không dùng
blend mode của layer**: blend mode mô tả cách hoà với layer BÊN DƯỚI, mà
trong bộ đệm riêng thì không có gì bên dưới.

**Kiểm chứng:** `345/345 test xanh` — 12 test riêng cho F22, gồm chỉ số
rác, gom layer từ nhiều screen, và file bản cũ thiếu khoá phải ra
Composition chứ không thành Layer với chỉ số rác.

⚠️ Phần `render/` (nướng FBO theo layer) **chưa được biên dịch** — máy đang
làm không có openFrameworks. Phải build trên Windows để xác nhận.

### 🟢 F21 — Hút điểm về đường gióng khi kéo (2026-09-10)

Căn mép slice bằng mắt tới từng pixel là việc vừa lâu vừa không bao giờ
chính xác — mà sai một pixel ở khe ghép hai máy chiếu là một vệt sáng
hoặc vệt tối chạy dọc suốt buổi diễn.

**Mốc hút:** mép và đường giữa khung máy chiếu · điểm điều khiển của các
slice KHÁC (mốc hay dùng nhất khi ghép nhiều máy) · các điểm khác của
chính slice đang kéo.

**Ba quyết định làm nên hay dở của tính năng này:**

★ **Ngưỡng tính theo PIXEL MÀN HÌNH, không phải đơn vị output.** Độ chính
xác của bàn tay là hằng số theo pixel màn hình. Nếu ngưỡng tính theo đơn
vị output thì khi thu nhỏ khung nhìn, ngưỡng "8 đơn vị" chỉ còn 2 pixel
trên màn — hút gần như không bao giờ ăn; phóng to thì nó thành 40 pixel và
hút loạn xạ. `snapThresholdFor(px, zoom)` chia ngược lại cho hệ số phóng.

★ **Hai trục hút độc lập.** Chỉ hút khi cả x lẫn y cùng khớp thì gần như
không bao giờ kích hoạt — người dùng hay muốn "thẳng cột với góc kia" mà
chiều còn lại thì tuỳ ý.

★ **Chọn đường GẦN NHẤT, không phải đường đầu tiên trong ngưỡng.** Khi
nhiều mốc nằm sát nhau, lấy đường đầu tiên nghĩa là kết quả phụ thuộc thứ
tự trong mảng — người dùng không đoán được nó sẽ hút vào đâu.

**Giữ ALT để tạm tắt trong lúc kéo.** Bắt buộc phải có: cố ý để hở một khe
3px giữa hai slice là việc thật, và không có đường thoát thì người dùng
phải tắt hút ở thanh công cụ rồi bật lại — giữa buổi diễn thì không ai làm
vậy. Đường gióng đang hút được vẽ ra màu vàng, để người dùng THẤY mình
thẳng hàng với cái gì.

**Kiểm chứng:** `357/357 test xanh` — 12 test riêng cho F21, gồm mốc NaN
(warp suy biến lọt vào danh sách) không được làm hỏng cả phép hút, và
ngưỡng phải đổi theo hệ số phóng.

⚠️ Phần nối vào thao tác kéo nằm ở `ui/` nên **chưa được biên dịch**.

### ⚠️ Đính chính bảng G (2026-09-10)

Kiểm lại bằng source: trong `src/io/sources/` chỉ có **đúng hai** nguồn —
`MockSource` và `OscSource`. Mọi nhắc tới TUIO / Kinect / Arduino / LiDAR
trước đây **chỉ nằm trong ghi chú**, không có lớp nào cả.

| Mục | Bằng chứng |
|---|---|
| G3 Serial / Arduino | không có lớp nguồn nào; chỉ được nhắc trong ghi chú của `OscSource.h`. **→ đã làm phần giao thức, xem mục G3 bên dưới** |
| G15 Kinect / Femto | không có; `Kinect` chỉ xuất hiện trong ghi chú giải thích của 5 file |
| G16 Ghi log + replay | không có lớp ghi/phát lại phiên sensor. **→ đã làm phần lõi, xem mục G16 bên dưới** |
| G18 Nhiều sensor cho nhiều screen | `AppController` hardcode `calibrations[0]` ở **6 chỗ** — một sensor duy nhất. **→ đã làm, xem mục G18 bên dưới** |
| G19 LiDAR (Livox / Ouster) | không có |

### 🟢 G14 — Nguồn TUIO (2026-09-10)

TUIO là thứ hầu hết phần mềm tracking nói: Community Core Vision, các bộ
theo dõi LiDAR thương mại, khung IR, ứng dụng multitouch. Nói được TUIO
nghĩa là **cắm được vào phần lớn hệ thống có sẵn mà không phải viết driver
riêng cho từng loại**.

**Làm thành CHẾ ĐỘ của `OscSource`, không phải lớp nguồn riêng.** Phần khó
và dễ sai của một nguồn sensor không nằm ở việc đọc message — nó nằm ở
socket, thread, vòng sự kiện wait-free, và cơ chế hết hạn điểm khi UDP
đánh rơi gói. Chép lại toàn bộ khối đó cho TUIO nghĩa là nhân đôi chỗ để
sai, và sửa lỗi ở một bản sẽ quên bản kia.

**Hai điểm chết người của giao thức, đều đã khoá bằng test:**

★ **TUIO không có message "nhấc tay".** Cách duy nhất biết một điểm biến
mất là nó **vắng mặt trong `alive`** của frame kế. Ai chỉ xử lý `set` sẽ
có bộ theo dõi mà điểm không bao giờ chết — chạm rồi nhấc tay, dấu chạm
nằm đó mãi. Trong tác phẩm tương tác đó là hiệu ứng kẹt cứng tới khi khởi
động lại.

★ **UDP không bảo đảm thứ tự.** Gói đến muộn mang trạng thái cũ; áp vào sẽ
làm điểm nhảy giật về sau rồi nhảy tới. Triệu chứng là hiệu ứng "rung" mà
đổi bộ lọc bao nhiêu cũng không hết — vì nguyên nhân không nằm ở nhiễu.
`fseq` cho biết frame nào mới hơn; frame cũ bị bỏ (và `set` của nó cũng bị
dọn, nếu không sẽ rò sang frame kế thành điểm ma).

Thêm hai chi tiết nhỏ nhưng thật: điểm **đứng yên** chỉ xuất hiện trong
`alive` không kèm `set` (bên gửi chỉ gửi khi có thay đổi) nên phải nhớ vị
trí cũ; và id lạ trong `alive` mà chưa từng thấy `set` thì **bỏ qua chứ
không bịa (0,0)** — góc trên-trái là toạ độ hợp lệ, điểm ma ở đó trông y
như cú chạm thật và sẽ kích hoạt trigger zone.

Cổng mặc định **3333** theo quy ước TUIO (khác 9000 của phương ngữ Hexmap):
phần lớn bộ tracking chỉ cho đổi địa chỉ, không cho đổi cổng.

**Kiểm chứng:** `376/376 test xanh` — 19 test cho G14, trong đó có bài
dựng **gói UDP đúng byte** (bundle: set + alive + fseq) chạy qua cả
`parseOscPacket` lẫn `TuioDecoder` lẫn `OscSource`, và bài chứng minh nhấc
tay sinh sự kiện `Up` **ngay** chứ không đợi hết hạn 1 giây.

### 🟢 G16 — Ghi log & phát lại phiên sensor (2026-09-10)

`[~]`: **lõi xong** (định dạng, ghi, đọc, chọn gói tới hạn) — còn phần nối
vào UI để bấm nút ghi/phát lại lúc đang chạy.

**Vấn đề nó giải:** lỗi sensor gần như không bao giờ tái hiện được ở bàn
làm việc. Nó xảy ra lúc 11 giờ đêm, giữa buổi diễn, với đúng cái LiDAR đó,
đúng cách người ta bước qua vùng quét đó. Hôm sau mở máy ra thì mọi thứ
chạy hoàn hảo. Ghi lại gói thô nghĩa là **mang được nguyên hiện trường về**.

★ **Ghi GÓI THÔ, không ghi `SensorFrame` đã xử lý.** Ghi frame thì chỉ
phát lại được phần SAU bộ giải mã — mà phần lớn lỗi nằm đúng ở đó: gói dị
dạng, thứ tự đảo, id trùng, `alive` thiếu. Byte thô giữ được đúng thứ
người thật đã gửi, kể cả những gói mà bản hiện tại còn đang hiểu sai.
Đổi lại file to hơn (~60 MB cho 2 giờ ở 40 Hz), không đáng để đánh đổi.

★ **Mốc thời gian là ĐỘ LỆCH, không phải giờ tuyệt đối** — file phát lại
được ở bất kỳ thời điểm nào, và không vô tình mang theo thông tin lúc nào
ở đâu.

★ **File cụt vì mất điện vẫn đọc được phần lành**, kèm cảnh báo — và đó
chính là loại phiên hay cần xem lại nhất. Vứt cả file là vứt đúng bằng
chứng mình cần.

★ **Trần 64 KB cho một gói.** File hỏng khai độ dài 4 tỉ byte sẽ làm
`resize()` cố cấp phát 4 GB — chương trình chết vì hết bộ nhớ khi người
dùng chỉ định *mở* một file. Đọc file là chỗ dữ liệu KHÔNG đáng tin, kể cả
khi chính mình đã ghi nó ra.

★ **`collectDuePackets` bơm được NHIỀU gói mỗi lần gọi.** Sensor 40 Hz với
render 60 fps thường là một gói mỗi frame, nhưng chỉ cần một lần khựng
(nạp media, đổi cửa sổ) là dồn hàng chục gói. Bơm mỗi lần một gói thì bản
phát lại tụt hậu dần và không bao giờ đuổi kịp — mất đúng tính chất quan
trọng nhất của replay.

**Kiểm chứng:** `386/386 test xanh` — 10 test cho G16, gồm bài chạy **cả
vòng**: ghi một phiên → đọc lại → bơm vào `OscSource` thật đúng mốc thời
gian → ra đúng điểm chạm. (`OscSource::feedPacket` tồn tại sẵn cho việc
này từ đầu, chỉ chưa ai xây phần còn lại.)

### 🟢 G3 — Phương ngữ Serial cho Arduino (2026-09-10)

`[~]`: **phần giao thức xong** (thứ quyết định firmware chạy được hay
không) — còn phần mở cổng serial, cần API riêng của từng hệ điều hành.

**Phương ngữ:** `T <id> <x> <y>` · `U <id>` · `C`

★ **ASCII từng dòng, không phải nhị phân.** Nhị phân gọn hơn, nhưng thứ
quyết định một dự án Arduino chạy được hay không là khả năng **mở Serial
Monitor ra nhìn**. Với ASCII, người làm phần cứng cắm dây vào là thấy ngay
thiết bị đang gửi gì, và gõ tay một dòng để thử phần mềm mà không cần nạp
firmware. Ở 115200 baud, 40 điểm ở 60 Hz chỉ chiếm ~3% băng thông — chỗ
này không phải nút thắt.

★ **Cái bẫy thật của serial: dòng bị cắt ngang.** Serial KHÔNG giao hàng
theo dòng — một lần `read()` trả về đúng những byte vừa tới, có thể là nửa
dòng, có thể là hai dòng rưỡi. Ai giả định "mỗi lần đọc là một dòng" sẽ có
phần mềm chạy hoàn hảo trên bàn (dữ liệu thưa, mỗi dòng tới trọn vẹn) rồi
hỏng ngay khi cắm thiết bị thật gửi nhanh — và hỏng theo kiểu **mất rải
rác vài điểm**, rất khó lần ra. Có test nạp **từng byte một** và đối chiếu
kết quả phải y hệt nạp cả cục.

★ **Rác từ firmware không được thành điểm chạm ma.** `std::atoi` trả 0 cho
chuỗi rác, nên `T abc def` sẽ thành một cú chạm ở (0,0) — mà góc trên-trái
là toạ độ HỢP LỆ, nên nó trông y như chạm thật và sẽ kích hoạt trigger
zone ở đó. Thiếu một trường thì **bỏ cả dòng**, không lấy phần đọc được.

★ **Trần 128 ký tự một dòng.** Thiết bị hỏng gửi rác không có ký tự xuống
dòng sẽ làm bộ đệm phình vô hạn tới khi hết RAM. Thiết bị hỏng thì phải
làm *mất dữ liệu*, không được làm sập phần mềm điều khiển — và phải **đồng
bộ lại được** ở dòng kế tiếp.

Số dòng rác được đếm (`badLines`) để hiện lên PerfPanel: người làm phần
cứng nhìn con số tăng là biết firmware đang gửi lẫn thứ khác.

**Kiểm chứng:** `402/402 test xanh` — 16 test cho G3.

**Còn lại:** mở cổng (`CreateFile` trên Windows, `termios` trên POSIX) +
thread đọc, theo đúng khuôn `OscSource`.

### 🟢 G18 — Nhiều sensor cho nhiều screen (2026-09-11)

Một sân khấu lớn thường có hai hoặc ba cảm biến: LiDAR quét sàn trước,
khung IR trên tường bên, Kinect nhìn xuống bục. Mỗi cái phủ một vùng khác
nhau và chiếu lên một máy chiếu khác nhau.

Model đã có sẵn `CalibrationProfile::sourceId` và `targetScreenId` từ đầu
— thiếu đúng phần **định tuyến**. Trước đây `AppController` gắn cứng
`calibrations[0]` ở sáu chỗ, nên cảm biến thứ hai cắm vào sẽ **đi qua phép
hiệu chỉnh của cảm biến thứ nhất**.

★ **Bảng CHỈ SỐ chứ không phải con trỏ.** `SensorMapper` giữ con trỏ thô
tới `CalibrationProfile` và `Screen`, mà hai thứ đó nằm trong `std::vector`
của Project — chỉ cần thêm một screen là vector cấp phát lại và mọi con
trỏ thành treo. Đây không phải lo xa: đúng lỗi đó đã xảy ra khi làm nút
"Thêm Screen" (F22). Chỉ số sai thì phát hiện được bằng kiểm tra biên; con
trỏ treo thì im lặng cho tới lúc sập.

★ **Khớp theo `Screen::id`, không theo vị trí trong mảng.** Xoá một screen
ở giữa làm mọi vị trí sau nó dịch đi một — và khi đó mọi cảm biến lặng lẽ
chiếu sang **máy chiếu bên cạnh**. Sai theo kiểu trông vẫn "có chạy", nên
rất lâu mới bị phát hiện.

★ **Screen đích không tồn tại → BỎ tuyến, không lùi về screen 0.** Lùi về
screen 0 thì điểm chạm vẫn xuất hiện, chỉ là ở sai máy chiếu — trông như
phần mềm đang chạy, nên người ta đi tìm lỗi ở chỗ khác. Bỏ tuyến thì triệu
chứng khớp với nguyên nhân: cảm biến đó im lặng, kèm cảnh báo nói rõ.

★ **Hai hồ sơ cùng `sourceId` → giữ cái đầu, nói rõ.** Lấy cái sau nghĩa
là hành vi phụ thuộc thứ tự trong file, và người vận hành sẽ thấy hiệu ứng
"tự nhiên nhảy sang chỗ khác" sau khi lưu lại project mà không đổi gì.

★ **Hồ sơ chưa calibrate xong cũng phải cảnh báo.** Người vận hành cắm cảm
biến vào, không thấy gì xảy ra, và không có cách nào biết là vì chưa
calibrate.

**Kiểm chứng:** `412/412 test xanh` — 10 test cho G18, gồm bài ba sensor →
ba screen đi ba đường riêng, và bài một hồ sơ hỏng không được kéo theo các
tuyến còn lại.

⚠️ Phần nối vào `AppController` **chưa được biên dịch** (lớp oF).

### Xây tiếp

`E1` FX chain · `A13` group · `C9` cue points · `G14` TUIO

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
| [ ] | **F18** | Polygon slice (không chỉ hình chữ nhật) | L | 🟡 P2 |
| [x] | **F19** | Color correction per-slice (brightness/gamma/RGB) | M | 🟡 P2 |
| [x] | **F20** | **Soft edge blending** (ghép nhiều máy chiếu) | L | 🟡 P2 |
| [x] | **F21** | Snapping / đường gióng khi kéo | M | 🟡 P2 |
| [~] | **F22** | Slice input từ Layer / Group cụ thể | M | 🟡 P2 |
| [ ] | **F23** | Output ra Spout / NDI (screen ảo) | M | 🟡 P2 |
| [ ] | **F24** | LED mapping qua Art-Net / sACN | XL | ⚪ P3 |
| [ ] | **F25** | Output SDI qua capture card | L | ⚪ P3 |

---

## G. ⭐ Sensor & Calibration (KHÔNG có trong Resolume — giá trị riêng)

| ✓ | ID | Tính năng | Công sức | Đề xuất |
|:-:|---|---|:-:|:-:|
| [x] | **G1** | Kiến trúc thread + TripleBuffer + SpscRing | M | 🔴 P0 |
| [x] | **G2** | MockSource + sensor simulator | S | 🔴 P0 |
| [~] | **G3** | Serial / Arduino source | M | 🔴 P0 |
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
| [ ] | **G15** | Kinect / Femto Bolt depth source + blob detect | L | 🟠 P1 |
| [~] | **G16** | Ghi log + replay phiên sensor để debug | M | 🟡 P2 |
| [x] | **G17** | Trigger clip / FX từ sự kiện sensor | M | 🟠 P1 |
| [x] | **G18** | Calibration nhiều sensor cho nhiều screen | M | 🟡 P2 |
| [ ] | **G19** | LiDAR source (Livox / Ouster) | L | 🟡 P2 |

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


## 🎛️ Giao diện MikMap

Dựng lại toàn bộ vỏ giao diện theo bản thiết kế `mikmap_ui.tsx`.

| Mục | Nội dung |
|---|---|
| Bố cục | **cố định**, ba trang chuyển bằng tab — không còn cửa sổ nổi |
| Bảng màu | cam = đang phát · vàng = đang chờ · lục = dữ liệu sống · lam = phụ trợ |
| Thanh trên cùng | FPS và độ phân giải output **hiện thường trực** |
| Composition | thư viện media · 2 màn hình xem · thuộc tính · lưới layer × cột |
| Mapping | công tắc VÙNG LẤY / ĐƯỜNG RA dùng chung một khung nhìn |
| Sensor | thiết bị · khung nhìn điểm chạm · calibration + vùng cảm ứng |

**Kết quả:** 60.0 fps, `frame_avg` 16.67 ms — không đổi so với giao diện cũ.

### Vì sao bỏ cửa sổ nổi

Cửa sổ nổi tự do nghe thì linh hoạt, nhưng trong phòng tối giữa buổi diễn,
người vận hành không có thời gian sắp lại bàn làm việc — và một bảng trôi ra
ngoài màn hình hoặc bị che mất là chuyện xảy ra thật. Bố cục cố định nghĩa
là mọi thứ **luôn ở đúng chỗ cũ**. Ngoại lệ duy nhất là cửa sổ **Cài đặt**:
nó không thuộc luồng làm việc lúc diễn (mở ra, chỉnh, đóng lại), nên dành
cho nó một chỗ cố định là lấy mất diện tích của những thứ dùng suốt buổi.

### Hai tính năng mới đi kèm

**Thư viện media** — liệt kê `bin/data/media`, bấm là gán vào ô clip đang
chọn. File **không phải `.mov`** hiện màu vàng ngay trên danh sách: cảnh báo
*sau khi* đã kéo vào thì đã muộn, lúc đó fps đã tụt giữa buổi diễn.

**Chế độ VÙNG LẤY** — kéo vùng lấy của slice ngay trên canvas. Trước đây chỉ
chỉnh được đầu ra bằng chuột, còn vùng lấy phải gõ số. Nhưng hai việc đó là
hai nửa của cùng một thao tác — *lấy phần nào của hình* và *đặt nó ở đâu
trên vật thể* — nên chúng dùng chung một khung nhìn và một công tắc.

### Ba cái bẫy đã mắc phải

**1. `near` là macro của Windows.** Đặt tên một lambda là `near` khiến
`windows.h` biến nó thành rỗng, và lỗi báo ra là *"auto: no variable
declared before '='"* — hoàn toàn không gợi ý gì đến nguyên nhân thật.

**2. Một dòng quá rộng làm cắt CẢ bảng.** ImGui không cắt bớt nội dung tràn
ra — nó **nới rộng** vùng nội dung của child. Hậu quả: mọi widget đặt bề
rộng `-FLT_MIN` (hết chỗ còn lại) ăn theo bề rộng đã nới ấy và thành ra rộng
hơn khung nhìn. Sửa một dòng gây ra chuyện đó thì cả bảng vừa lại.

**3. `GetContentRegionAvail()` trong ô của table không trả về bề rộng ô.**
Ba nút S/B/X căn phải theo giá trị đó bị đẩy ra ngoài cột và bị cắt — nhìn
thì tưởng cột quá hẹp, thật ra là phép tính sai. Sửa bằng mốc cố định.

### Điều suýt mất

Thanh menu cũ bị ba tab thay thế — và **suýt kéo theo cả lối Lưu / Mở
project**. Bản thiết kế không vẽ chỗ nào cho việc này, nhưng một phần mềm
không lưu được công việc thì không dùng được; nên nó quay lại dưới dạng nút
`Dự án` bật popup, không chiếm chỗ thường trực.

---

## ✂️ F12 — Mặt nạ bezier

| Mục | Nội dung |
|---|---|
| Không gian | **contentUV** của slice, đi qua đúng phép warp |
| Hình dựng sẵn | chữ nhật, elip (tay nắm chuẩn kappa) |
| Sửa | kéo nút · bấm lên đường = thêm nút · chuột phải = xoá · Ctrl+kéo = uốn cong · Shift+bấm = duỗi thẳng |
| Đảo | cắt phần bên trong — khoét lỗ chừa cửa sổ thật |
| Mép mờ | 0–0.5 theo cạnh ngắn slice, tính lúc lấy mẫu nên đổi là thấy ngay |
| Sensor | `Screen::hitTest` bỏ qua vùng bị cắt — chạm vào chỗ tối thì không kích hoạt gì |

**Kết quả:** `313/313 test xanh · 3686 assertion` · 60.0 fps với mặt nạ elip + mép mờ
(`frame_avg` 16.67 ms, không đổi so với khi tắt mặt nạ).

### Vì sao nút mặt nạ ở contentUV, không phải pixel máy chiếu

Keystone là thứ ánh xạ nội dung slice lên **bề mặt thật**. Đặt mặt nạ ở
contentUV nghĩa là nó đi qua đúng phép warp đó: vẽ xong bóng vật thể một
lần, sau này máy chiếu bị xê dịch thì chỉ cần kéo lại 4 góc — mặt nạ theo
cùng. Đặt ở pixel máy chiếu thì **mỗi lần chỉnh keystone là mỗi lần phải vẽ
lại toàn bộ**. Cùng lý do khiến `TriggerZone` nằm ở không gian canvas.

### Vì sao texture chứ không phải stencil buffer

Stencil là cách quen thuộc để cắt hình, nhưng nó cho **mép sắc** và không
làm mờ được — mà mép mờ mới là thứ giấu được sai số căn chỉnh giữa hình
chiếu và cạnh vật thể thật (cùng nguyên lý với hoà viền F20). Nó cũng phụ
thuộc vào việc cửa sổ output có stencil buffer hay không. Nướng mặt nạ ra
một texture rồi nhân vào **kênh alpha** thì hoà đúng với F20 mà không phải
sắp xếp lại thứ tự vẽ.

Mép mờ được tính **lúc lấy mẫu** (hộp 5×5 trong shader) chứ không blur sẵn
vào texture: nhờ vậy kéo thanh trượt feather là thấy ngay, không phải nướng
lại texture mỗi frame — nướng lại giữa lúc đang kéo sẽ giật.

### Ba cái bẫy đã mắc phải khi làm

**1. Nướng texture trong lúc shader đang bind.** `maskTexture()` vẽ vào một
FBO khác. Gọi nó *sau* `m_sliceShader.begin()` thì `ofPath::draw()` vẽ đường
mặt nạ **bằng chính slice shader** — cho ra texture đen sì, và mặt nạ cắt
sạch toàn bộ nội dung. Triệu chứng nhìn thấy là **màn hình đen hoàn toàn**,
không hề giống "lỗi vẽ mặt nạ".

**2. Copy constructor viết tay bỏ sót trường mới.** `Slice` giữ
`unique_ptr<IWarp>` nên phải tự viết copy constructor liệt kê từng trường.
Thêm `mask` vào lớp mà quên hai hàm sao chép → mặt nạ **lặng lẽ biến mất**
mỗi lần slice bị sao chép (nạp project, thêm slice), không lỗi biên dịch,
không cảnh báo. Sửa tận gốc: bọc phần khó sao chép vào `WarpPtr` (tự gọi
`clone()`), rồi `Slice` dùng `= default` — trường mới từ nay tự động được
sao chép.

**3. Test đo nhầm đại lượng.** Test "chèn nút không làm đổi hình" ban đầu so
**diện tích** đa giác trước/sau. Nhưng sau khi chèn, `flatten()` sinh nhiều
đỉnh hơn nên đa giác xấp xỉ sát đường cong hơn và diện tích tăng — đó là sai
số *làm phẳng*, không phải hình đổi. Viết lại thành so **từng điểm** trên
đường cong, và khi đó phép chia đôi de Casteljau khớp tới `1e-12`.

*(Bên lề, cũng là một kỳ vọng sai của tôi: bezier bậc 3 với hằng số kappa
**phình ra ngoài** đường tròn thật ~0.03%, nên diện tích elip 4 nút lớn hơn
`π/4` chứ không nhỏ hơn.)*

---

## ⚙️ Cài đặt & đa ngôn ngữ

| Mục | Nội dung |
|---|---|
| Cửa sổ **Cài đặt** | ngôn ngữ, màn hình output mặc định, vsync, cache media, log hiệu năng, tự phát clip đầu |
| **Tiếng Việt / English** | đổi có hiệu lực ngay, không khởi động lại |
| Font **Inter** (OFL) | thay ProggyClean của ImGui — đủ dấu tiếng Việt và dễ đọc hơn |

**Kết quả:** `290/290 test xanh · 3462 assertion`

### Vì sao cài đặt tách khỏi file project

File project mô tả **một buổi diễn** và đi theo người: mang sang máy khác,
gửi cho đồng nghiệp. Cài đặt mô tả **máy này và người này**. Nếu nhét ngôn
ngữ giao diện và chỉ số màn hình output vào project, thì mở project của
đồng nghiệp sẽ đổi ngôn ngữ giao diện của bạn và đẩy output ra một màn hình
không tồn tại. Vì vậy `settings.json` nằm riêng và **không vào git**.

### Vì sao chuỗi tra theo khoá, không phải theo tiếng Anh

Cách phổ biến là lấy luôn chuỗi tiếng Anh làm khoá — `TR("Save")`. Nhưng khi
đó sửa một chữ trong bản tiếng Anh sẽ **âm thầm làm mất bản dịch**, và hai
chỗ dùng cùng một từ tiếng Anh với nghĩa khác nhau bị buộc phải dịch giống
nhau. Khoá phân cấp (`clip.stop` vs `sen.stop`) tránh được cả hai. Khoá
thiếu bản dịch thì **trả về chính khoá** — nhìn thấy `adv.edge.gamma` trên
màn hình là biết ngay thiếu ở đâu, tốt hơn ô trống hoặc âm thầm rơi về
tiếng Anh vì cả hai đều lọt qua khâu kiểm tra.

### Cái bẫy mà `tests/test_localization.cpp` chặn

Trước khi có i18n, `ImGui::Text("Ghi %d điểm", n)` được **trình biên dịch**
kiểm tra `%d` có khớp đối số không. Sau khi chuyển sang `TR("cal.recorded")`
thì chuỗi được tra cứu **lúc chạy** — trình biên dịch không thấy nó nữa. Một
bản dịch viết nhầm `%s` chỗ đáng lẽ `%d` sẽ đọc con trỏ rác từ stack, và chỉ
nổ **khi người dùng đổi sang ngôn ngữ đó** — tức gần như không bao giờ gặp
lúc phát triển, chỉ gặp ở buổi diễn.

Test đối chiếu hai bảng: cùng bộ khoá, và cùng chuỗi specifier ở mỗi khoá.
Nó bắt lỗi ngay lần chạy đầu — 8 khoá đã lọt vào bảng tiếng Việt nhưng thiếu
ở bảng tiếng Anh.


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
