# UX hiện tại của MikMap (`src/`, nhánh `new_UI`)

> **Cập nhật 2026-09-22:** ô clip tách bar/body theo yêu cầu người dùng — xem mục 2.1.
>
> **Cập nhật 2026-09-21 (lần 2):** đã sửa các mục X1–X4, X6–X8, X10 (xem mục 7) — nội dung dưới đã khớp code hiện tại.
>
> **Nguồn:** đọc code `src/*.cpp` ngày 2026-09-21 — mô tả **những gì code làm**,
> không phải những gì app "định làm". App đã build và mở được trên macOS nhưng
> tài liệu này **chưa được đối chiếu bằng cách bấm thử từng thao tác**.
>
> **Cách dùng để góp ý:** mỗi mục có mã (vd `C4`, `M7`). Ghi vào
> `ux-feedback.md` kiểu "C4: muốn bấm một lần là phát luôn" — Claude sẽ biết
> đúng chỗ trong code. Ký hiệu: ✅ chạy thật · 🟡 chạy một phần · ⛔ chỉ là hình
> (bấm không có tác dụng).

---

## 0. Nguyên tắc chung

- **Ba trang cố định** qua tab trên cùng: Composition · Advanced Mapping ·
  Sensor I/O. Không có cửa sổ nổi, ngoài cửa sổ Cài đặt (modal) và cửa sổ output
  máy chiếu.
- **Vẽ thủ công bằng ImGui** (immediate-mode): không dùng widget chuẩn của ImGui,
  nên **không có** điều hướng bằng Tab/mũi tên, không có focus ring.
- **Chuột là chính**, nay có thêm một số phím tắt (xem mục 6).
- Con trỏ đổi thành bàn tay trên mọi thứ bấm được; popover/menu đóng khi bấm ra
  ngoài hoặc nhấn `Esc`.
- Màu: coral = live/đang chọn, cyan = preview, mint = audio/kết nối, vàng = cảnh
  báo/kéo-thả, đỏ = nguy hiểm.
- **Đã lưu được** (`src/project.cpp`): dự án lưu thành file `.mikmap` (JSON) trong
  `~/Documents/MikMap`; cài đặt máy (ngôn ngữ, font, màu, cỡ chữ, màn hình output)
  lưu riêng ở thư mục config của hệ điều hành và tự lưu khi đổi. Thay đổi chưa lưu
  hiện dấu `*` ở tiêu đề, và app cảnh báo trước khi bỏ/thoát (bấm lại lần nữa để xác nhận).
- Cỡ chữ giao diện chỉnh được 4 mức trong Cài đặt (scale toàn bộ workspace).

---

## 1. Khung chung (mọi trang)

### 1.1 Thanh tiêu đề (cao 40px) — `main.cpp: TitleBar`
| Vị trí | Thành phần | Hành vi |
|---|---|---|
| Trái | Logo + "MIKMAP" + "MikMap Stage 0…" | ✅ Bấm: mở/đóng menu Project. Khi rê chuột hiện chấm xanh |
| | 3 tab **Composition / Advanced Mapping / Sensor I/O** | ✅ Bấm để đổi trang |
| Phải | Nút **Show TestCard** | ✅ Bật/tắt (cờ `testCard`) |
| | Nút **Blackout** | ✅ Bật/tắt; badge đổi Live ⇄ Blackout |
| | Badge **Live/Blackout** | Chỉ hiển thị |
| | Tên file dự án (`<tên>.mikmap`, thêm `*` nếu chưa lưu, `(unsaved)` nếu chưa có file) | ✅ Đổi theo dự án |
| | Nút ⚙ (bánh răng) | ✅ Mở cửa sổ Cài đặt |

### 1.2 Thanh trạng thái (cao 22px) — `StatusBar`
- Trái: chấm nhịp (nhấp nháy theo BPM của dự án) + **`xxx.x BPM`** (✅ bấm = tap tempo, lăn chuột = chỉnh ±1, chuột phải = về 128; lưu trong dự án) + `SENSORS n/m` (đếm thiết bị đang
  "kết nối") + `OUTPUT OPEN/CLOSED` (cửa sổ máy chiếu) — ✅ phản ánh trạng thái thật.
- Khi có thông báo (lưu, mở, cảnh báo chưa lưu…), dòng gợi ý bên phải hiện thông báo
  đó trong vài giây.
- Phải: `FPS · P99 ms · DROP` ✅ **số đo thật** (vàng khi P99 > 20ms).
- Giữa-phải: dòng gợi ý thao tác `CLICK NAME TO CUE · CLICK ART TO PLAY ·
  RIGHT-CLICK NAME FOR ACTIONS` (đổi thành cảnh báo khi Blackout, hoặc hiện
  thông báo gần nhất trong vài giây).

### 1.3 Menu Project (bấm logo) — `ProjectMenu`
| Mục | Hành vi |
|---|---|
| Dự án mới `Ctrl+N` | ✅ Tạo dự án **trống** (4 layer × 8 cột, 1 screen + 1 slice); nếu có thay đổi chưa lưu thì bấm lần hai để xác nhận |
| Mở dự án `Ctrl+O` / Mở gần đây | ✅ Hộp thoại liệt kê file `.mikmap` trong `~/Documents/MikMap`, mới nhất trước; bấm một dòng để mở |
| Lưu dự án `Ctrl+S` | ✅ Ghi đè file hiện tại (lần đầu tạo `<tên>.mikmap`) |
| Lưu bản sao `Ctrl+Shift+S` | ✅ Ghi thêm bản có ngày giờ, không đổi dự án đang mở |
| Cài đặt hệ thống | ✅ Mở Cài đặt |
| Trợ giúp & Phím tắt | ✅ Hộp thoại bảng phím tắt |
| Giới thiệu | ✅ Thông báo phiên bản |
| Nạp lại mẫu Demo | ✅ Thay bằng dự án mẫu (cũng hỏi xác nhận nếu chưa lưu) |
| Chế độ Show `Tab` | ✅ Ẩn toàn bộ giao diện, chỉ còn composite toàn màn hình; `Esc`/`Tab` để thoát |

Trên macOS phím `Ctrl` trong bảng trên là **Cmd**. Khối "Dự án hiện tại" hiện tên dự án thật,
kích thước canvas, giờ hiện tại và badge **Đã lưu / Chưa lưu** thật.

---

## 2. Trang Composition — `deck.cpp`

Bố cục: **dải trên** (cao ~42% màn hình, kéo đổi được) = Browser | 2 monitor +
Timeline | Properties; **dải dưới** = Deck (lưới layer × cột).

### 2.1 Deck — lưới clip

> **Đổi 2026-09-22:** mỗi ô clip **có clip** tách thành hai vùng bấm độc lập —
> **bar** (dải tên phía trên, cao 22px) và **body** (vùng gradient phía dưới).
> Không còn khái niệm bấm-đơn/bấm-đúp. **Ô trống thì KHÔNG tách vùng** (không
> có gì vẽ khác biệt để người dùng biết ranh giới) — toàn bộ ô trống là một
> vùng "body" duy nhất, bấm ở bất kỳ đâu trong ô cũng dừng layer đó; chuột
> phải ở bất kỳ đâu vẫn mở popover.

| Mã | Thao tác | Kết quả | |
|---|---|---|---|
| C1 | **Bấm bar** (nhả chuột, không kéo) | *Cue* — chọn ô, đưa lên Preview; **không phát**, không đổi clip đang chạy của layer. Properties tự chuyển sang tab **Clip** để hiện đúng clip vừa chọn | ✅ |
| C1b | **Bấm body** (ô có clip) hoặc **bấm bất kỳ đâu** (ô trống) | *Cue + Trigger* — phát ngay ở cả Preview lẫn Live Output, ô đó trở thành clip đang chạy của layer. Properties tự chuyển sang tab **Clip**. **Nếu ô đang trống: dừng CHÍNH layer đó** (các layer khác không đụng tới — mỗi layer độc lập, xác nhận với người dùng); cắt cứng, chưa có dissolve khi tắt | ✅ |
| C1c | Preview Cue khi bấm **cột** | Hiện clip của **layer cao nhất** (trên cùng) có nội dung trong cột đó; cột trống thì giữ nguyên Preview cũ | ✅ |
| C2 | ~~Bấm đúp để phát~~ | Không còn cần thiết — bấm đơn vào body đã phát ngay | — |
| C3 | **Chuột phải bar** | Popover *Clip*: Trigger · Cue to Preview · Loop (đặt chế độ LOOP) · **Rename** · Clear Slot + 6 ô màu clip. **Chuột phải body không có tác dụng.** | ✅ |
| C4 | **Kéo bar sang ô khác** (>5px) | Di chuyển clip; ô đích viền vàng nét đứt. **Kéo bắt đầu từ body không di chuyển clip** (chỉ phát, xem C1b) | ✅ |
| C5 | **Kéo từ Browser thả vào ô** | Nạp clip vào ô (`loadClip`); nếu là Effect thì cue ô đó + thêm FX | ✅ |
| C6 | **Bấm header cột** | *Chọn + bắn cả cột* — mọi clip không rỗng ở cột đó (mỗi layer 1 clip) phát ngay; **layer nào có ô trống ở cột này bị dừng luôn** (không giữ nguyên clip cũ) — bấm vào cột trống toàn bộ sẽ tắt hết Live Output | ✅ |
| C6b | Header cột **đang chọn nhưng không có gì live** (vd `activeCol` mặc định trên dự án mới/trống) | Chỉ viền coral nhạt + badge `TRIG` — **không** glow/chấm nhấp nháy/badge `nL` giả (trước đây hiện y hệt cột đang live dù trống) | ✅ |
| C7 | ~~Bấm đúp để bắn cột~~ | Không còn cần thiết — bấm đơn đã bắn cả cột | — |
| C8 | **Kéo header cột** | Đổi thứ tự cột | ✅ |
| C9 | **Chuột phải header cột** | Menu *Column*: chèn trước/sau, dịch trái/phải, **Rename** (ô nhập nổi), Clear, **Set/Clear auto-start on open** (⚡), Delete | ✅ |
| C14 | **Setting: auto-start column** | Đặt qua menu chuột phải header cột. Mặc định **tắt** (không cột nào). Cột được đặt hiện biểu tượng ⚡ trước tên, lưu trong dự án. Khi **mở lại** dự án đó (Ctrl+O / menu Mở dự án), cột này tự bắn ngay (không chờ nhịp dù Sync đang bật) | ✅ |
| C10 | Hàng nhóm: nút **Cue 1…n** | Chọn cue của nhóm — **và chọn (cue) đúng clip ở cột đó trên MỌI layer thành viên** (đổi màu Selected/LiveSel như bấm bar từng ô, không chỉ đánh dấu nội bộ), Properties tự chuyển sang tab Clip cho layer đầu tiên trong nhóm; bấm tiêu đề nhóm thu gọn/mở. **Sửa 2026-09-22:** glow của ô Cue đang active trước đây tràn (blur 12px) đè lên tên/fader của nhóm bên trái và ô Cue kế bên (khoảng cách giữa hai ô chỉ 4px) — giảm blur xuống 3px để glow không vượt quá khoảng cách đó nữa | ✅ |
| C13 | **Kéo fader trên tiêu đề nhóm** | Độ mờ tổng của nhóm, nhân vào mọi layer thành viên (bấm vào fader không thu gọn nhóm) | ✅ |
| C12 | **Chuột phải tiêu đề nhóm** | Menu: Rename group · Change color · Ungroup (bỏ nhóm, giữ nguyên các layer) | ✅ |
| C11 | Bấm ô cuối của cột cuối | **Tự thêm cột mới** khi có clip ở cột cuối | ✅ |

Ô clip hiển thị: tên, chế độ phát (LOOP/BOUN/HOLD/ONCE), thời lượng, thanh tiến
trình (chỉ ô đang chọn). **Thumbnail là gradient tĩnh** (cố ý, để deck không tụt
fps).

**Màu theo 3 trạng thái (2026-09-22, khớp bản thiết kế `MikMap Workspace.dc.html`):**
| Trạng thái | Bar/Body | Viền chọn |
|---|---|---|
| Đã nạp, chưa chọn (Loaded) | Cam ấm (`#2e1a0e`/`#150b05`) | — |
| Đang cue/preview (Selected) | **Xanh lạnh** (`#0e2430`/`#0b141b`) | Cyan |
| Đang live (Live/LiveSel) | Cam cháy đậm (`#8a3c14`/`#2a1408`) | Coral |

Trước đây chỉ có 2 kiểu (trống/nâu ấm hoặc cam thuần khi được chọn — kể cả lúc chỉ
đang *cue* chứ chưa phát), khiến "sắp phát" và "đang phát" trông giống hệt nhau.

### 2.2 Hàng layer (cột trái 178px)
- Bấm hàng → chọn layer **và tự chuyển sang tab Layer** ở Properties. ✅
- **Bấm đúp hàng layer** = đổi tên (ô nhập nổi; Enter xác nhận, Esc huỷ). ✅
- ▾ thu gọn/mở hàng. ✅ Nút ⚙ (sliders) mở menu *Layer*: Move up/down · Duplicate ·
  Rename · Clear clips · Delete layer. ✅
- Nút **S / M / B** (Solo/Mute/Bypass). ✅
- Slider **V** (opacity) và **A** (audio) — kéo được ✅ (chưa kiểm chứng slider A có tác động lên âm thanh/hiển thị nào không).
- Ô blend: bấm mở danh sách 8 chế độ. ✅
- Ô **blend time**: số giây **cross-dissolve** khi đổi clip trên layer (0 = cắt cứng). ✅
- Ô **blend time** nhận số gõ trực tiếp (ô nhập thật của ImGui). ✅
- **Ghim ngang (mới, 2026-09-22):** khi lưới có nhiều cột hơn chiều rộng khung
  (thanh cuộn ngang xuất hiện), cột layer 178px này **luôn đứng yên ở mép
  trái**, không cuộn theo — kể cả hàng tiêu đề nhóm (Cue N của cột đã cuộn qua
  bị ẩn hẳn, không vẽ đè lên tên/fader nhóm) và hàng tiêu đề cột (nhãn
  **LAYERS** cũng ghim, không bị chữ "Cột N" của cột cuộn tới vẽ đè lên). Ô
  clip/tiêu đề cột nào bị ghim che hoàn toàn cũng **không nhận click** nữa
  (tránh bấm trúng vùng ghim mà lại kích hoạt nhầm ô đang ẩn phía sau) —
  verify bằng `--shot` thêm cột tới khi tràn khung, kéo thanh cuộn, chụp ảnh
  đối chiếu. ✅

### 2.3 Deck tabs + Run mode (mới, 2026-09-22)
| Hàng | Nội dung |
|---|---|
| Tab deck (26px) | Danh sách deck (✅ bấm=chuyển, bấm đúp=đổi tên). **Không còn nút + DECK riêng (đổi 2026-09-22)** — chuột phải vào bất kỳ tab nào mở menu *Deck*: **Add deck** (thêm deck mới, 3 layer trống, tên "Deck B"/"C"...) · Rename deck · Duplicate deck · **Move left/right** (đổi thứ tự tab, mờ khi ở đầu/cuối) · Delete deck (chặn khi chỉ còn 1 deck). |
| Run mode (34px) | Segmented **GRID**/**TIMELINE** (✅ chuyển đổi kiểu hiển thị vùng diễn; độ rộng mỗi nút tự tính theo chữ, không còn tràn chữ "TIMELINE" ra ngoài). Bên phải: ở Grid hiện `Layer`/`Group`/`Column`/`Sync` (xem dưới); ở Timeline hiện nút `LOOP ON/OFF`. |

Mỗi deck có layer/nhóm/cột **riêng biệt hoàn toàn** — chuyển deck là đổi hẳn sang một
bộ layer khác, không ảnh hưởng deck kia. Lưu trong dự án (`decks[]`, `curDeckIdx`).
Undo/redo cũng phủ hành động thêm/xoá/chuyển deck.

Thanh công cụ Grid (góc phải hàng Run mode):
`Layer` ✅ thêm layer mới (8 ô trống) · `Group` ✅ đưa layer đang chọn vào nhóm mới ·
`Column` ✅ thêm cột trống ở cuối · `Sync` ✅ **quantize theo nhịp**: khi bật (mặc định tắt), trigger clip/cột chờ tới nhịp kế tiếp của BPM mới phát; tắt hoặc đang pause thì phát ngay. Trạng thái lưu trong dự án.

### 2.3b Timeline run mode (mới, 2026-09-22)
Một cách hiển thị **khác của cùng dữ liệu lưới** — không phải dữ liệu riêng. Mỗi
layer là 1 lane; các clip không rỗng của layer đó (theo đúng thứ tự cột) xếp nối
tiếp nhau, độ rộng tỉ lệ theo **thời lượng thật** của từng clip, trải trên một
thanh playhead chung 0–100%. Bấm ▶ cho playhead tự chạy (100% mỗi 10 giây); tại
mỗi khung hình, clip nào đang nằm dưới playhead ở mỗi lane sẽ tự chuyển Live —
tức là *chính* `Clip::st` mà Grid mode dùng, nên chuyển qua lại Grid ⇄ Timeline
vẫn thấy đúng thứ đang phát. Kéo/bấm trên thanh tick (%) để tua; nút `LOOP ON`
giới hạn playhead trong một khoảng cho trước (`tlIn`/`tlOut`, chưa có UI kéo
khoảng — chỉ bật/tắt qua nút). Kéo clip từ Browser thả vào 1 lane để nạp vào ô
trống đầu tiên của layer đó (hoặc tự thêm cột mới nếu layer đã đầy). Bấm 1 block
= cue (chọn/preview, không phát — giống bấm bar ở Grid mode). Transport đổi
thành 7 nút: ⏮ (về 0%) · ◀ (lùi 1 bar, −6.25%) · ▶/⏸ · ■ (dừng + về 0%) · ▶
(tiến 1 bar, +6.25%) · ⏭ (tới cuối, 99.9%). Playhead/vòng lặp **không lưu vào
dự án** (chỉ trạng thái runtime, reset khi mở lại).

### 2.4 Browser (trái, 200px)
Cây thư mục: **Media** (✅ ảnh thật trong `~/Documents/MikMap/media`, bấm thư mục để quét lại) · Sources · Generators · Effects · Composition — bốn thư mục sau vẫn là **dữ liệu mẫu cố
định**. Kéo ảnh vào ô để tạo clip ảnh (căn vừa canvas, có alpha, đi qua warp).
- Bấm thư mục: mở/đóng. Bấm mục: chọn. ✅
- **Bấm đúp một Effect**: thêm vào FX chain của clip đang chọn. ✅
- **Chuột phải một Effect**: menu "Add to <clip>" / "Show FX chain". ✅
- **Kéo** mục ra Deck: xem `C5`.

### 2.5 Hai monitor + Timeline (giữa)
- **Preview Cue** (cyan) và **Live Output** (coral, có nhãn COMPOSITE): xem hình
  clip đã cue / toàn bộ composite. Live có TestCard khi bật.
- **Timeline**: hiển thị timecode theo clip trên cùng đang chọn (tổng = thời lượng thật của clip, vd `16s`; generator `∞` lặp mỗi 10s) và
  có **thanh kéo (scrub)** ở đáy — bấm/kéo để đổi playhead. ✅
- 5 nút transport: **▶ Play** (tiếp tục chạy playhead của mọi clip đang chọn/live ✅) · **⏸ Pause** (dừng toàn bộ playhead, không đổi trạng thái Live ✅) · **■ Stop** (dừng chạy + tua playhead về 0 ✅) · **⏮/⏭** (nhảy sang cột trước/sau **và bắn luôn cột đó** — như bấm header cột, có dừng layer nào trống ở cột mới, có theo Sync/quantize nếu bật ✅; khác phím `←`/`→` chỉ di chuyển lựa chọn, không phát).

### 2.6 Properties (phải, 236px) — 3 tab **Comp / Layer / Clip**
- **Comp**: bảng chỉ đọc (canvas, số layer/nhóm/cột, BPM, FPS, độ trễ, output).
- **Layer**: slider opacity/audio, 8 chip **blend mode** (cùng danh sách với dropdown hàng layer), công tắc Solo/Mute/Bypass,
  bảng thông tin.
- **Clip**: ảnh xem trước, PLAYHEAD, **PLAY MODE** (LOOP/BOUN/HOLD/ONCE), **SPEED**
  + REV, **TRANSFORM** (X, Y, Scale, Rotation, Opacity + FLIP H/V + RESET) ✅;
  **FX CHAIN** (chỉ **Strobe, Hue Shift, Mirror** tác động lên hình, ở mọi màn hình; các hiệu ứng khác chỉnh được tham số nhưng chưa vẽ): ADD, danh sách hiệu ứng (chuột phải: bật/tắt, lên/xuống,
  nhân đôi, reset, xoá), tham số, mix, cờ BEAT/AUDIO. ✅

### 2.7 Kích thước
Kéo **thanh ngang giữa dải trên và Deck** để đổi chiều cao (tối thiểu 180px). ✅

---

## 3. Trang Advanced Mapping — `mapping.cpp`

Ba vùng: **cây Screen/Slice/Mask** (trái) | **Stage canvas** (giữa) | **Properties**
(phải). Cây có thể thu gọn thành **rail** dọc.

### 3.1 Cây (trái)
| Mã | Thao tác | |
|---|---|---|
| M1 | Bấm dòng Screen/Slice/Mask → chọn | ✅ |
| M2 | ▾ thu gọn Screen; 👁 ẩn/hiện | ✅ |
| M3 | **Chuột phải** → menu: Screen (Move up/down, Duplicate, Add slice, Delete) · Slice (Hide/Show, **Solo/Unsolo**, … Reset warp, Add mask, Delete) · Mask (Duplicate, Delete) | ✅ |
| M4 | Nút thêm **Screen / Slice / Mask** | ✅ |
| M5 | Thu gọn cây thành rail; bấm tên Screen trong rail để mở tạm | ✅ |

### 3.2 Stage canvas (giữa)
- **Hai trang** (tab đầu canvas): **Input selection** (chọn vùng lấy từ canvas) và
  **Output routing** (bố trí đầu ra/warp).
- **Chế độ warp**: nút Corner-pin (khung) ⇄ Mesh (lưới).
- **Undo / Redo** (nút trên thanh này và `Ctrl/Cmd+Z`) — dùng chung lịch sử toàn app.
- **Zoom**: 5 nút icon (phóng to, thu nhỏ, tìm/vừa vùng, phóng tối đa, bật/tắt chế độ tập trung — tên chính xác chưa kiểm chứng). **Alt + lăn chuột**
  = zoom theo con trỏ. **Lăn chuột** = cuộn dọc, **lăn ngang** = cuộn ngang.
  **Kéo chuột phải** = pan.
- Menu **Reset**: Reset 4 corner pins · Reset mesh warp · Reset all warping.

**Trang Input**: kéo **4 góc** của khung cyan để đổi vùng lấy (tối thiểu 20px). ✅

**Trang Output**:
| Mã | Thao tác | |
|---|---|---|
| M6 | Kéo **góc slice** (vùng bấm ~16px) → keystone | ✅ |
| M7 | Kéo **điểm mesh** (viền vàng = biên, coral = trong) | ✅ |
| M8 | Kéo **4 điểm mask** | ✅ |
| M9 | Bấm trong slice → chọn slice trên cùng; bấm vào mask → chọn mask | ✅ |
| M10 | Với slice đang chọn ở chế độ mesh: bấm lại để đặt "điểm cắt", rồi dùng nút thêm cột/hàng | ✅ |
| M11 | Chip 🔍 cạnh tên slice → zoom vào slice | ✅ |

### 3.3 Properties (phải)
Slice: **toạ độ 4 góc nhập được bằng số** (X/Y, đồng bộ với kéo chuột), số cột/hàng mesh (+/−), reset lưới, thêm cột/hàng ở vị trí đã chọn. Mask:
đảo, độ mờ, xoá. Screen: **Edge blend** (công tắc), chọn **màn hình xuất** (bấm
xoay vòng qua các display), nút **mở/đóng cửa sổ output** (`F11`).

---

## 4. Trang Sensor I/O — `sensor.cpp`

Ba cột: **Devices + Calibration** | **Radar view** | **Parameter routing**.

| Mã | Thành phần | Hành vi | |
|---|---|---|---|
| S1 | Thẻ thiết bị | Nút **CONNECT/DISCONNECT** đổi cờ; hiện FPS/LATENCY/PACKETS khi "kết nối" | 🟡 dữ liệu mẫu, **không có thiết bị thật** |
| S2 | **Calibration wizard** | *Start wizard* → 4 bước (TOP-LEFT → BOTTOM-LEFT): **bấm vào radar** để đặt từng điểm; có *Cancel*. Hiện RMS và ma trận H_s 3×3 | ✅ (DLT thật, chưa RANSAC) |
| S3 | **Radar** | Bấm radar = **giả lập một điểm chạm** (thay điểm cũ, chỉ có 1 điểm). Điểm này được chiếu qua calibration lên sân khấu Mapping (trang Output, Screen đầu) và kích hoạt các dây `touch.down` đang bật | 🟡 giả lập nguồn, chuỗi phía sau chạy thật |
| S7 | Nút **Output overlay** (thanh Radar) | Bật/tắt vẽ điểm chạm lên cửa sổ output máy chiếu (sau H_s; Screen đầu; mặc định tắt) | ✅ |
| S4 | **Edit ROI** | Đổi màu sang vàng, hiện 4 chấm — **kéo được** (giới hạn trong đĩa radar); trong lúc sửa ROI, bấm radar không tạo điểm chạm giả | ✅ |
| S5 | Blob tracking | Slider *Noise threshold* và *Blob size* | ✅ đổi số, chưa có nguồn thật để tác động |
| S6 | Parameter routing | Bấm dây để bật/tắt. Dây `touch.down → "<layer> · <clip>"` đang bật sẽ trigger clip đó khi chạm radar | 🟡 chỉ nguồn `touch.down`; các dây khác chưa nối |

---

## 5. Cửa sổ Cài đặt — `settings.cpp`

Modal ở giữa màn hình, đóng bằng **Done**, nút ✕, `Esc`, hoặc bấm ra ngoài.
4 tab bên trái: **Ngôn ngữ · Font · Màu · Cỡ chữ**. Thay đổi **áp dụng ngay** (có
khung xem trước "LIVE OUTPUT"). Nút **Reset** đưa về mặc định.

---

## 6. Bàn phím & thao tác ẩn

| Phím | Tác dụng |
|---|---|
| `Ctrl/Cmd + N / O / S` | Dự án mới / Mở / Lưu |
| `Ctrl/Cmd + Shift + S` | Lưu bản sao có ngày giờ |
| `Ctrl/Cmd + Z`, `Ctrl/Cmd + Shift + Z` hoặc `Y` | Undo / Redo **toàn app** (60 bước; trigger/playhead không vào lịch sử) |
| `Tab` | Bật/tắt Show Mode |
| `Space` | Play / pause (trang Composition) |
| `Enter` | Trigger clip đang chọn |
| `←` / `→` | Cột trước / sau |
| `L` | Clip đang chọn: chế độ LOOP |
| `Delete` / `Backspace` | Xoá clip đang chọn |
| `F11` | Mở/đóng cửa sổ output máy chiếu |
| `Esc` | Đóng popover/menu/hộp thoại/ô đổi tên/Cài đặt |
| Alt + lăn chuột (Mapping) | Zoom theo con trỏ |

Phím tắt bị vô hiệu khi đang gõ chữ hoặc khi có menu/hộp thoại mở. Chưa có `Ctrl+C/V` hay phím gán tuỳ ý cho từng clip.

---

## 7. Khoảng trống & điểm lạ

Trạng thái sau đợt sửa 2026-09-21 (✅ đã sửa · ⛔ còn tồn tại):

| Mã | Vấn đề | |
|---|---|---|
| X1 | Không lưu/mở dự án và cài đặt | ✅ Đã có `.mikmap` + settings; **chưa dùng `ProjectIO`/`.hexmap` của engine** |
| X2 | Menu Project và nút Group/Column/Sync là hình | ✅ Đã nối hết |
| X3 | Rename layer/cột, Loop trong popover không làm gì | ✅ Đã sửa |
| X4 | Chip blend mode có tên khác dropdown, chọn Alpha/Additive rơi về Normal | ✅ Đã sửa (đã xác nhận đúng là lỗi thật) |
| X5 | Không phím tắt thật; không undo cho Composition | ✅ Đã có phím tắt (mục 6) và undo/redo toàn app |
| X6 | Timeline không scrub được | ✅ Đã sửa; tổng thời lượng và tốc độ playhead nay theo thời lượng thật của clip |
| X7 | Edit ROI không kéo được; radar chỉ giả lập 1 điểm chạm | ✅ ROI kéo được — ⛔ radar vẫn chỉ giả lập |
| X8 | Thanh trạng thái hiện MIDI/Art-Net/NDI giả | ✅ Đã thay bằng số thiết bị và trạng thái output thật |
| X9 | Tên dự án không cập nhật tiêu đề | ✅ Đã sửa |
| X10 | Badge `2L`/`1L` ở header cột là số cố định | ✅ Nay đếm layer đang phát thật |

---

## 8. Bản thiết kế tham chiếu

`sampleUI/mikmap_ui.tsx` và `sampleUI/mikmap_UI_pug` (cập nhật lần cuối 11/9) là
mockup gốc. Chưa đối chiếu từng chi tiết với app hiện tại — nếu bạn thấy khác biệt
so với mockup, ghi vào `ux-feedback.md`.
