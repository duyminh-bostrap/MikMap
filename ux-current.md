# UX hiện tại của MikMap (`src/`, nhánh `new_UI`)

> **Cập nhật 2026-09-24:** kéo được hai khe dọc hai bên monitor để đổi bề rộng
> Browser/Properties (mục 2.7); toàn bộ chữ lớn hơn 20% (`ui::kTextScale`, mục 0).
>
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
- **Chữ lớn hơn bản thiết kế 20%** (từ 2026-09-24): mọi cỡ chữ trong code vẫn ghi
  đúng số px của bản thiết kế, `ui::kTextScale = 1.2` nhân lúc vẽ (`ui::Text`/`TextW`,
  ô nhập liệu qua `ui::TextPx`). Chỉ chữ to lên, khung/hàng/icon giữ nguyên — chỗ
  chật tự cắt bằng "…" (vd tên nhóm dài trên dải ghim: "STAGE …").

---

## 1. Khung chung (mọi trang)

### 1.1 Thanh tiêu đề (cao 40px) — `main.cpp: TitleBar`
| Vị trí | Thành phần | Hành vi |
|---|---|---|
| Trái | Logo + "MIKMAP" + "MikMap Stage 0…" | ✅ Bấm: mở/đóng menu Project. Khi rê chuột hiện chấm xanh |
| | 3 tab **Composition / Advanced Mapping / Sensor I/O** | ✅ Bấm để đổi trang |
| Phải | Nút **Show TestCard** | ✅ Bật/tắt (cờ `testCard`) — **chiếm quyền của deck**: mọi nơi vẽ nội dung composition (Live Output, trang Input, thumbnail Output, cửa sổ máy chiếu) đều hiện thẻ test thay vì clip |
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
> Không còn khái niệm bấm-đơn/bấm-đúp. **Ô trống nay cũng vẽ bar + body
> (2026-09-26), nhưng chỉ là màu** — không tên, không thumbnail, không chữ chân ô —
> để ô trống đang chọn có cùng cấu trúc (bar sáng lên, body ngả màu) như một clip
> thay vì chỉ một viền lẻ loi. **Hành vi bấm không đổi:** vì sau bar/body vẫn không
> có clip nào, ô trống vẫn là **một vùng bấm duy nhất** — bấm bar hay body đều dừng
> layer đó; chuột phải ở bất kỳ đâu vẫn mở popover.

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
| C10 | Hàng nhóm: nút **Cue 1…n** | Chọn cue của nhóm — **và chọn (cue) đúng clip ở cột đó trên MỌI layer thành viên** (đổi màu Selected/LiveSel như bấm bar từng ô, không chỉ đánh dấu nội bộ), Properties tự chuyển sang tab Clip cho layer đầu tiên trong nhóm; bấm tiêu đề nhóm thu gọn/mở. **Sửa 2026-09-22:** glow của ô Cue đang active trước đây tràn (blur 12px) đè lên tên/fader của nhóm bên trái và ô Cue kế bên (khoảng cách giữa hai ô chỉ 4px) — giảm blur xuống 3px; và ô Cue nay vẽ ở lượt cuộn (trước dải ghim, xem §2.2) nên không còn vẽ đè lên ô danh tính của nhóm khi cuộn nửa cột | ✅ |
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
  trái**, không cuộn theo — kể cả hàng tiêu đề nhóm và hàng tiêu đề cột (nhãn
  **LAYERS** cũng ghim). Quy tắc vẽ: **mọi thứ cuộn ngang (ô clip + ô Cue N
  của nhóm + tiêu đề cột) vẽ TRƯỚC và bị cắt cứng bằng clip-rect tại mép phải
  dải ghim; dải ghim vẽ SAU CÙNG.** Trước đây ô Cue N vẽ trong lượt ghim, ngay
  sau ô danh tính của nhóm, nên một cột cuộn **nửa chừng** (chưa bị ẩn hẳn)
  vẽ nguyên ô "Cue N" đè lên tên/fader nhóm — và glow của ô clip cột đầu
  (blur 12px) lem sang thẻ layer ngay cả khi chưa cuộn. Ô clip/Cue/tiêu đề cột
  nào nằm dưới dải ghim cũng **không nhận click** (`mouseUnderPin`), nên bấm
  vào S/M/B hay fader trên dải ghim không kích hoạt nhầm clip đang ẩn phía
  sau. Khi đang cuộn có thêm **viền tối mềm** ở mép dải để thấy rõ nội dung
  chui xuống dưới dải chứ không phải dính vào thẻ layer — verify bằng `--shot`
  (cuộn nửa cột: tên nhóm còn nguyên, ô Cue bị cắt gọn; bấm lên dải ghim chỉ
  trúng điều khiển của layer). ✅

### 2.3 Deck tabs + Run mode (mới, 2026-09-22; dời vị trí 2026-09-24)
> **Vị trí hiện tại:** tab deck nằm ở **hàng 2 của khối trái thanh transport** (dưới hai monitor, cạnh
> đồng hồ SYSTEM TIME — xem §2.5), vẽ kiểu thanh nav trên cùng. Nếu thanh transport thấp hơn 44px hoặc
> quá hẹp thì `Deck()` tự vẽ lại hàng đồng hồ + hàng tab phía trên lưới (dự phòng, để menu deck không biến mất).
> Công tắc **GRID | TIMELINE** nằm góc trên-trái lưới deck.

- **Tab deck:** ✅ bấm = chuyển, bấm đúp = đổi tên. **Không có nút + DECK riêng** — chuột phải vào tab (hoặc
  mũi tên nhỏ của tab đang chọn) mở menu *Deck*: **Add deck** (deck mới, 3 layer trống, tên "Deck B"/"C"...) ·
  Rename · Duplicate · **Move left/right** (đổi thứ tự tab, mờ ở đầu/cuối) · Delete (chặn khi chỉ còn 1 deck) ·
  và 4 thao tác cấu trúc lưới: **Add layer** · **New group from selected layer** · **Add column** ·
  **Sync to beat ON/OFF**.
- **GRID | TIMELINE:** ✅ chuyển kiểu hiển thị vùng diễn; độ rộng mỗi nút tự tính theo chữ. Ở Timeline mode có
  thêm một hàng mảnh phía trên lưới chứa nút `LOOP ON/OFF` (Grid mode không còn hàng nào phía trên lưới).
- Cụm 4 nút `Layer / Group / Column / Sync` cũ (góc phải hàng Run mode) và nút **DECK TOOLS ⌄** trung gian
  đã bỏ — cả 4 chức năng giờ nằm trong menu *Deck* ở trên, không mất chức năng nào.

Mỗi deck có layer/nhóm/cột **riêng biệt hoàn toàn** — chuyển deck là đổi hẳn sang một
bộ layer khác, không ảnh hưởng deck kia. Lưu trong dự án (`decks[]`, `curDeckIdx`).
Undo/redo cũng phủ hành động thêm/xoá/chuyển deck.

Ý nghĩa **Sync to beat**: khi bật (mặc định tắt), trigger clip/cột chờ tới nhịp kế tiếp của BPM mới phát; tắt hoặc đang pause thì phát ngay. Trạng thái lưu trong dự án.

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

### 2.4 Browser (trái, mặc định 200px — kéo khe bên phải để đổi, xem §2.7; hoặc §5 tab Layout)
Cây thư mục: **Media** (✅ ảnh thật trong `~/Documents/MikMap/media`, bấm thư mục để quét lại) · Sources · Generators · Effects · Composition — bốn thư mục sau vẫn là **dữ liệu mẫu cố
định**. Kéo ảnh vào ô để tạo clip ảnh (căn vừa canvas, có alpha, đi qua warp).
- Bấm thư mục: mở/đóng. Bấm mục: chọn. ✅
- **Bấm đúp một Effect**: thêm vào FX chain của clip đang chọn. ✅
- **Chuột phải một Effect**: menu "Add to <clip>" / "Show FX chain". ✅
- **Kéo** mục ra Deck: xem `C5`.

### 2.5 Hai monitor + Timeline (giữa)
- **Preview Cue** (cyan) và **Live Output** (coral, có nhãn COMPOSITE): xem hình
  clip đã cue / toàn bộ composite. Live thay bằng thẻ test khi bật Show TestCard (`DrawTestCard`, `clipart.cpp` — theo thiết kế "MikMap Test Pattern", độ phân giải bằng độ phân giải comp).
- **Thanh dưới hai monitor** (bố cục hiện tại, sau các lần chỉnh 2026-09-22 → 09-24): **khối trái 2 hàng** —
  hàng 1 = **SYSTEM TIME** + giờ hệ thống thật `HH:MM:SS` (cập nhật mỗi khung hình, `localtime_r`/`localtime_s`),
  hàng 2 = **tab deck** (§2.3); **giữa** = cụm nút transport; **khối phải** = nhãn **TIMELINE** + timecode
  (vd `00:00:03:12 / 00:00:16:00`, theo clip trên cùng đang chọn). **Không còn thanh kéo (scrub bar)** ở Grid mode
  — kéo tua playhead chỉ còn ở Timeline run mode (§2.3b, `scrubZone`). Cao của cả thanh chỉnh được trong Cài đặt →
  **Layout** (`Cao thanh timeline`, 0–96px); **0 = ẩn hẳn** thanh và 5 nút transport, nhường chỗ cho hai
  monitor (tab deck khi đó tự chuyển lên phía trên lưới). ✅ Nhánh dự phòng "thanh thấp" **chưa chụp** thật.
- 5 nút transport (ẩn cùng thanh nếu Cao thanh timeline = 0): **▶ Play** (tiếp tục chạy playhead của mọi clip đang chọn/live ✅) · **⏸ Pause** (dừng toàn bộ playhead, không đổi trạng thái Live ✅) · **■ Stop** (dừng chạy + tua playhead về 0 ✅) · **⏮/⏭** (nhảy sang cột trước/sau **và bắn luôn cột đó** — như bấm header cột, có dừng layer nào trống ở cột mới, có theo Sync/quantize nếu bật ✅; khác phím `←`/`→` chỉ di chuyển lựa chọn, không phát).

### 2.6 Properties (phải, mặc định 236px — kéo khe bên trái để đổi, xem §2.7; hoặc §5 tab Layout) — 3 tab **Comp / Layer / Clip**
- **Comp**: bảng chỉ đọc (canvas, số layer/nhóm/cột, BPM, FPS, độ trễ, output).
- **Layer**: slider opacity/audio, 8 chip **blend mode** (cùng danh sách với dropdown hàng layer), công tắc Solo/Mute/Bypass,
  bảng thông tin.
- **Clip**: ảnh xem trước, PLAYHEAD, **PLAY MODE** (LOOP/BOUN/HOLD/ONCE), **SPEED**
  + REV, **TRANSFORM** (X, Y, Scale, Rotation, Opacity + FLIP H/V + RESET) ✅;
  **FX CHAIN** (chỉ **Strobe, Hue Shift, Mirror** tác động lên hình, ở mọi màn hình; các hiệu ứng khác chỉnh được tham số nhưng chưa vẽ): ADD, danh sách hiệu ứng (chuột phải: bật/tắt, lên/xuống,
  nhân đôi, reset, xoá), tham số, mix, cờ BEAT/AUDIO. ✅

### 2.7 Kích thước
Kéo **thanh ngang giữa dải trên và Deck** để đổi chiều cao (tối thiểu 180px). ✅

Kéo **khe dọc 4px giữa Browser và monitor** để đổi bề rộng Browser (140–320px),
**khe giữa monitor và Properties** để đổi bề rộng Properties (180–360px). Cụm hai
monitor không bao giờ hẹp hơn 560px (hẹp hơn thì tiêu đề Preview Cue tự đè lên
nhau), nên trên cửa sổ nhỏ giới hạn trên có thể thấp hơn 320/360. Rê chuột lên khe
→ con trỏ ↔, khe tô coral như thanh ngang; giữa khe có vạch nắm 34px. Giá trị ghi
thẳng vào `prefs.browserW`/`inspectorW` — slider trong Cài đặt → Layout đổi theo
và tự lưu `settings.json`. (`ColumnSplittersInput`/`ColumnSplittersDraw`,
`deck.cpp`) ✅

---

## 3. Trang Advanced Mapping — `mapping.cpp`

Ba vùng: **cây Screen/Slice/Mask** (trái) | **Stage canvas** (giữa) | **Properties**
(phải). Cây có thể thu gọn thành **rail** dọc.

### 3.1 Cây (trái)
| Mã | Thao tác | |
|---|---|---|
| M1 | Bấm dòng Screen/Slice/Mask → chọn | ✅ |
| M2 | ▾ thu gọn Screen; 👁 ẩn/hiện Screen, Slice và **Mask** (2026-09-26: mask ẩn vẫn nằm trong slice nhưng **không cắt output**; dòng mờ đi, ở trang Input viền mask vẽ nhạt; chuột phải mask cũng có Hide/Show mask; lưu trong dự án) | ✅ |
| M3 | **Chuột phải** → menu: Screen (Move up/down, Duplicate, Add slice, Delete) · Slice (Hide/Show, **Solo/Unsolo**, Whole area, Match output to input, Reset warp, Reset mesh warp, Reset all warping, Add mask, Delete) · Mask (Duplicate, Delete) | ✅ |
| M3b | **Chuột phải khung input** (trang Input selection) → menu kiểu Resolume: Center X/Y · Mirror X/Y · Left/Top/Right/Bottom Half · Whole Area · Match Output Shape · Swap Input Output Shape · Bring Forward/Send Backwards · Duplicate/Copy/Cut/Paste (xem §3.2) | ✅ |
| M3c | **Chọn nhiều (2026-09-26):** giữ **Ctrl/Cmd hoặc Shift + bấm** trên cây (hoặc bấm slice trên stage) để thêm/bớt vào vùng chọn. Chọn được **nhiều screen, hoặc nhiều slice, hoặc nhiều mask** — **không trộn loại** (đang chọn slice thì Ctrl-bấm screen/mask bị bỏ qua) và không bỏ được phần tử cuối cùng. Slice thuộc nhiều screen khác nhau chọn chung được. Bấm thường (không phím) ở bất cứ đâu → về chọn 1. Nhãn ở đầu panel đổi thành "N Slices/Screens/Masks"; panel vẫn sửa **phần tử chính** (cái chọn sau cùng); các slice/mask chọn kèm được tô sáng (viền liền trên trang Input, tô cam trên trang Output). Ghi chú macOS: Ctrl+bấm là chuột phải, dùng **Cmd** hoặc **Shift** | ✅ |
| M3d | **Phím tắt trang Mapping (2026-09-26):** `Ctrl/Cmd+C` copy · `Ctrl/Cmd+X` cắt · `Ctrl/Cmd+V` dán · `Ctrl/Cmd+D` nhân đôi · `Delete/Backspace` xoá — áp cho **toàn bộ phần tử đang chọn**. **Copy screen mang theo mọi slice và mask bên trong; copy slice mang theo mask của nó** (dán ra bản mới, id mới, tên thêm " copy" nếu trùng). Dán: screen → ngay sau screen đang chọn, slice → vào screen hiện tại (sau slice đang chọn), mask → vào slice hiện tại (tự sang trang Input). Xoá: luôn còn ≥1 screen và mỗi screen còn ≥1 slice (có báo). Clipboard chỉ trong phiên, không lưu vào dự án | ✅ |
| M3e | **Phím mũi tên (2026-09-26):** ←↑→↓ dịch 1 px, `Shift`+mũi tên 10 px (giữ để lặp; một lần bấm = một bước undo). **Trang Input:** dịch khung input của các slice đang chọn hoặc các mask đang chọn (px canvas); **trang Output:** dịch quad output của các slice đang chọn (lưới mesh đi theo keystone), hoặc mọi slice của các screen đang chọn | ✅ |
| M4 | Nút thêm **Screen / Slice** ở chân cây (nút **Mask** đã chuyển vào Slice Properties → Input Mask, 2026-09-26) | ✅ |
| M5 | Thu gọn cây thành rail; bấm tên Screen trong rail để mở tạm | ✅ |

### 3.2 Stage canvas (giữa)
- **Hai trang** (tab đầu canvas): **Input selection** (chọn vùng lấy từ canvas) và
  **Output routing** (bố trí đầu ra/warp).
- **Khung input sửa như khung transform của Preview Cue (2026-09-26):** kéo **trong khung** = di chuyển (khung
  thẳng bị giữ trong canvas) · **ô vuông ở góc / bất kỳ đâu dọc cạnh** = co giãn (kéo cả đoạn cạnh, không chỉ ô vuông giữa) trong hệ trục của khung (cạnh/góc đối diện
  đứng yên, kể cả khi đã xoay) · **vòng tròn quanh 4 góc** = **xoay** quanh tâm (giữ `Shift` = nhảy 15°). Thanh
  **Rotation** (−180…180°) ở Slice Properties sửa cùng giá trị, bấm đúp để về 0; nhãn giữa khung ghi
  `W × H · góc · flip`. Xoay/Mirror áp lên **output thật** (`WarpMap::Map`), lưu trong dự án và undo được.
  **Chuột phải** trong khung mở menu (M3b): *Match Output Shape* = input lấy hình dạng + góc của quad output,
  *Swap* = đổi chỗ hai hình, *Bring Forward/Send Backwards* = thứ tự chồng slice, *Duplicate/Copy/Cut/Paste* =
  clipboard slice (runtime, không lưu). ✅ Chưa thử kéo ô co giãn khi khung đang xoay bằng chuột thật.
- **Nhiều slice cùng screen (2026-09-26):** slice đang chọn có khung chỉnh sửa như trên nhưng **nền tô chỉ còn ~5%** (trước 15%) để hình
  nguồn bên dưới vẫn rõ; các slice **khác cùng screen** (đang hiện) chỉ vẽ **viền nét đứt** (không tên, không tay nắm) phần input của chúng, để biết
  vùng nào của nguồn đã được lấy khi chuyển sang slice khác. Slice ẩn (👁 tắt) không vẽ. **Bấm vào bên trong khung nét đứt = chọn slice đó** (nhiều khung chồng nhau thì lấy khung trên cùng; khung của slice đang chọn được ưu tiên trước — kéo/xoay/co giãn vẫn như cũ). Con trỏ hiện bàn tay khi rê vào. ✅
- **Không còn lưới nền** (ô 40px mờ) ở cả trang Input lẫn Output routing (2026-09-26) — khung canvas chỉ còn nền tối phẳng.
- **Slice Properties — thông tin theo mẫu Resolume, giao diện giữ nguyên thiết kế MikMap (2026-09-26):** từ trên xuống — tên slice · **Input source** ·
  lưới ô số **Input rectangle (px)**: `X · Y` (tâm khung), `Left · Top` (góc trên-trái chưa xoay), `Width · Height`, `Rotation` (1 số lẻ) — nhãn mono nhỏ phía trên
  mỗi ô như trước đây · công tắc **Soft edge** (nút ENABLED/DISABLED cùng kiểu Edge blending) · mục **INPUT MASK** với 6 nút hình: **tim · vuông · tròn · tam giác · lục giác · bút**.
  Bấm một hình = thêm mask cỡ nửa **khung input** của slice, ngay giữa khung, và chọn mask vừa tạo.
  **Mask nằm trong không gian composition canvas (như khung input), sửa ở trang Input và cắt hình gửi ra output thật** (2026-09-26):
  - **Mask chỉnh y hệt khung input** (khung kiểu Preview Cue): kéo **trong khung** = di chuyển, **ô vuông ở góc / bất kỳ đâu dọc cạnh** = co giãn (kéo cả đoạn cạnh, không chỉ ô vuông giữa), **vòng tròn quanh 4 góc** = xoay (giữ `Shift` = nhảy 15°). Mọi mask — hình có sẵn hay bút — đều là một đường viền đơn vị đặt bằng một hình chữ nhật xoay (`Mask::x/y/w/h/rot` + `shape` hoặc `u`), nên cùng một bộ điều khiển.
  - **Chỉ mask đang được chọn mới hiện** (kèm khung); các mask khác ẩn cho tới khi chọn ở cây bên trái (bấm mask ở cây tự nhảy sang trang Input). Đang sửa mask thì khung slice chỉ còn viền; bấm vào khung slice (ngoài khung mask) = quay về chọn slice.
  - **Panel Mask** (thông tin theo mẫu Resolume, giao diện MikMap): ô tên · **Invert (cut hole)** (bật = khoét lỗ, tắt = chỉ giữ phần bên trong) · Feather · lưới ô số **Mask rectangle (px)** `X · Y · Left · Top · Width · Height · Rotation` (1 số lẻ) · mục **Mask shape** — **bấm một hình = đổi shape của mask đang chọn** (giữ nguyên khung/góc xoay; hình hiện tại được tô sáng), nút **bút** = vẽ lại đường viền mask đang chọn (bấm từng điểm, bấm lại điểm đầu / `Enter` / bấm đúp để đóng, `Esc` huỷ; khung tự vừa với nét vẽ) · **Delete mask**. Feather vẫn ở panel như thiết kế cũ nhưng **mới chỉ lưu, output chưa làm mờ viền**. Khi chọn **slice** (không phải mask), cùng thanh INPUT MASK ở panel slice **thêm** mask mới (nửa khung input, ở giữa khung).
  - Nhiều mask cùng slice: phần được giữ là hợp các mask "chỉ giữ trong" (không có thì cả hình) trừ đi các lỗ.
  - **Ảnh hưởng output:** `output.cpp` đưa từng điểm mask qua cùng phép ánh xạ khung input → keystone/mesh với hình rồi dùng stencil buffer (`MaskBegin/MaskEnd`, `clipart.cpp`) để giới hạn mọi thứ slice đó vẽ (kể cả chỉnh màu của Screen). Đã xem trên cửa sổ output: mask tim khoét lỗ hình tim; đổi sang tam giác và xoay 15° thì lỗ đổi theo. Chỉ có ở cửa sổ máy chiếu, không có ở Live Output trong workspace.
  - File cũ lưu mask theo px output 1920×1080: tự quy đổi sang px canvas khi mở (đúng theo tỉ lệ canvas/1920×1080; ở canvas mặc định số giữ nguyên).
  Hàng X…Rotation chỉ ở trang Input; Soft Edge và Input Mask hiện ở cả hai trang.
  **Soft Edge chỉ là công tắc lưu theo slice — output máy chiếu CHƯA làm mờ viền** (giống công tắc edge blending của Screen). ✅
- **Nền trang Input = nội dung thật của nguồn (2026-09-26):** khung canvas hiện hình đang phát của đúng
  **nguồn mà slice đang chọn nhận** (*Input source*: Composition / Layer / Group — cùng `DrawSliceSource` với
  cửa sổ output, nên solo/bypass/opacity/transform khớp), chạy động cùng nhịp Live Output; đổi Input source là nền đổi theo.
  Chưa chọn slice thì hiện Composition. (Dòng chú thích tên nguồn trên thanh stage đã bỏ ngày 2026-09-26; nguồn xem ở ô Input source bên phải.) Nguồn không có clip đang live thì nền chỉ còn lưới. ✅
- **Hai công cụ kiểu Resolume (2026-09-26, thay nút Corner-pin ⇄ Mesh):** **＋ EDIT POINTS** · **⛶ TRANSFORM**, cạnh tab trang.
  Trang Output luôn có; trang Input chỉ hiện khi đang chọn một **mask**. Là cài đặt xem trong phiên (không lưu), mặc định Transform.
  Mọi slice giờ luôn có **4 góc phối cảnh lớn + lưới điểm warp bên trong** (như Resolume: góc lớn = perspective, điểm nhỏ = linear warp);
  Subdivisions 0×0 = chỉ 4 điểm nhỏ ở góc lưới. Không còn phân biệt "4-key" và "Mesh" trong dữ liệu (file cũ `warp: 0` được đổi thành lưới 1×1 khi mở, nhìn y hệt).
- **Undo / Redo** (nút trên thanh này và `Ctrl/Cmd+Z`) — dùng chung lịch sử toàn app.
- **Zoom (thu gọn 2026-09-26)**: `−` · số % · `+` · bật/tắt chế độ tập trung. Bấm số % = vừa toàn bộ (thay nút "phóng tối đa"); zoom vào slice đã có chip 🔍 cạnh tên slice trên stage. Mọi icon thanh công cụ lớn hơn (nút 28px, icon 16px). Đã **bỏ dòng chữ** bên phải thanh công cụ (tên thiết bị output / nguồn nội dung). **Lăn chuột**
  = **zoom theo con trỏ** (2026-09-26; một nấc = 15%, trackpad mượt); `Shift`+lăn = cuộn dọc, **lăn ngang** = cuộn ngang.
  **Kéo chuột phải** = pan.
- **Hai nút công cụ (2026-09-26, cả Input lẫn Output, nằm cạnh nhóm Undo/Redo):** 🖐 **Bàn tay** — bật thì **kéo chuột trái = pan** khu vực làm việc, mọi thao tác sửa
  (kéo điểm, chọn, marquee, bút) tạm tắt; ⧉ **Nam châm** — bật thì mọi lần kéo **hít vào điểm và cạnh**: điểm của slice khác/của chính slice, đường x/y trùng nhau, rồi tới cạnh
  gần nhất, cùng viền khung 1920×1080 (Output) hoặc canvas (Input) và điểm giữa; **chỉ đúng cạnh đã hít sáng trắng** (cạnh của khung/box hoặc đoạn đường giữa trong khung — không kẻ dài ra ngoài stage) hoặc vòng tròn ở điểm đã hít. Ngưỡng 8px màn hình. Giữ **Alt**
  để đặt tự do. Áp cho kéo góc corner pin, điểm mesh, nhóm điểm, co giãn/di chuyển khung input và khung mask (khi di chuyển thì mép/tâm khung hít vào các đường). Cả hai công tắc
  chỉ trong phiên (không lưu). ✅
- **Kéo thả chọn vùng (marquee, 2026-09-26):** kéo chuột trên vùng trống của stage vẽ khung nét đứt. **Trang Output + Edit Points:** chọn mọi **điểm của slice đang chọn** (4 góc phối cảnh
  + mọi điểm warp, kể cả 4 góc lưới) nằm trong khung — **không chọn điểm của slice khác** (2026-09-26); đổi sang slice khác thì các điểm đã chọn được bỏ. Kéo nhóm / mũi tên: **chỉ điểm đã chọn di chuyển** — điểm lưới không chọn đứng yên trên màn hình kể cả khi nhóm có góc phối cảnh lớn (góc lớn tự khớp lại quanh lưới); nhóm chỉ gồm góc lớn thì là chỉnh phối cảnh, cả lưới đi theo — điểm được chọn hiện ô vuông trắng; **Output + Transform:** chọn các slice mà khung kéo chạm vào; **kéo một điểm đã chọn = kéo cả nhóm** cùng một độ dời (nam châm
  áp cho điểm cầm, không hít vào chính các điểm đang di chuyển); phím mũi tên cũng dịch cả nhóm; bấm vào chỗ trống thả nhóm. **Trang Input:** chọn mọi slice mà khung input chạm vào
  khung kéo (chọn nhiều slice). Ctrl/Cmd/Shift + kéo = thêm vào vùng chọn hiện có. ✅
- Menu **Reset** (theo trang đang xem): Input → Whole area; Output → Reset perspective corners · Match output to input · Reset warp points · Reset all warping (về slice mới: toàn màn, lưới 1×1).

**Trang Input**: kéo **4 góc** của khung cyan để đổi vùng lấy (tối thiểu 20px). Khung input **được kéo/di chuyển ra ngoài canvas** kể cả khi rotation = 0 (phần ngoài canvas là trống; tâm khung vẫn giữ trong canvas khi di chuyển; kéo tới ±4000..8000px) — riêng mask thẳng vẫn giữ trong canvas. ✅

**Trang Output**:
| Mã | Thao tác | |
|---|---|---|
| M6 | **Edit Points** — kéo **ô vuông lớn** ở 4 góc = góc phối cảnh (keystone/homography); cả lưới đi theo | ✅ |
| M7 | **Edit Points** — kéo **ô vuông nhỏ** = điểm warp (linear, không đổi phối cảnh), **kể cả 4 góc của lưới** (trước đây góc lưới dính cứng vào góc phối cảnh). Khi góc nhỏ nằm đè góc lớn: bấm giữa là điểm nhỏ, bấm viền ô lớn là góc phối cảnh. Mũi tên dịch điểm đã chọn. **4 góc lớn luôn bao quanh lưới** (như Resolume): khi điểm lưới thay đổi, 4 góc lớn tự khớp lại thành tứ giác phối cảnh ôm sát lưới — điểm ngoài cùng nằm đúng trên cạnh của nó (điểm ra ngoài → góc lớn nở ra; cả một cạnh lưới vào trong → góc lớn co vào); hình chiếu không xê dịch (`FitCornersToMesh`) | ✅ |
| M7b | **Transform** — **hộp bao quanh tất cả điểm** của slice (hộp có hướng, xoay theo `Slice::orot`): kéo trong hộp = di chuyển, ô vuông góc/cạnh = co giãn, vòng ở góc = xoay. Cả slice (góc phối cảnh + lưới) biến đổi theo **một phép affine** nên **warp giữ nguyên hình** (không còn ép thành hình chữ nhật như bản trước). `Shift` (theo Resolume): kéo = khoá một trục, kéo góc = giữ tỉ lệ, xoay = bước **45°**. Nam châm hít cho cả ba | ✅ |
| M7c | **Chuột phải** trong một slice ở Output = chọn slice đó + menu như Input: Center X/Y, Mirror X/Y, Left/Top/Right/Bottom Half, Whole Area (đặt **hộp Transform** vào vùng đó, giữ nguyên warp), Match Input Shape, Swap Input Output Shape, Bring Forward/Send Backwards, Duplicate/Copy/Cut/Paste. Match / Swap dùng **4 góc nhìn thấy** (góc của lưới warp) chứ không phải góc phối cảnh lớn: *Match Input Shape* đặt đúng 4 góc nhìn thấy vào hình input và tự tính lại góc phối cảnh để phần warp bên trong giữ nguyên; *Match Output Shape* (trang Input) lấy hình nhìn thấy. | ✅ |
| M8 | **Mask** (trang **Input**, chỉ mask đang chọn hiện): **Transform** = khung như khung input (di chuyển / co giãn / xoay); **Edit Points** = ô vuông vàng ở từng điểm viền — kéo từng điểm, **bấm đúp điểm = xoá** (giữ ≥3), **bấm đúp lên viền = thêm điểm** (như Resolume); mask thành hình tự do, giữ nguyên góc xoay | ✅ |
| M9 | Bấm trong slice → chọn slice trên cùng; bấm vào mask → chọn mask | ✅ |
| M10 | **Edit Points** + **+ Add col / + Add row** (nút sáng vàng, panel ghi `CLICK TO PLACE COL/ROW`): một **đường xem trước** (vàng, kèm nhãn `COL 58%` / `ROW 40%`) chạy theo chuột và uốn theo lưới; bấm để đặt đường đó — kể cả khi bấm ngay sát một điểm/đường có sẵn (lúc đang chờ đặt thì không nắm điểm). Vị trí tính qua lưới đã méo (`MeshParamAt`). Thêm cột/hàng **lấy mẫu lại lưới từ bề mặt hiện tại** nên hình không đổi. **Không còn chấm vàng "điểm cắt"** (2026-09-26, theo yêu cầu: đường xem trước đã thay nó) | ✅ |
| M11 | Chip 🔍 cạnh tên slice → zoom vào slice | ✅ |
| M12 | **Thumbnail output của từng slice** (2026-09-26): mỗi slice hiện đúng hình nó gửi ra máy chiếu — sau vùng lấy, keystone/mesh, **mask cắt**, Opacity/màu của Screen — vẽ bằng chính `DrawSliceOutput` (`output.cpp`) mà cửa sổ máy chiếu dùng, cắt theo khung 1920×1080; **không tô nền**: slice đang chọn chỉ có viền coral, slice khác chỉ có viền nét đứt (2026-09-26). Tôn trọng ẩn/Solo. | ✅ |

### 3.3 Properties (phải)
Slice, **trang Output** (2026-09-26, thông tin theo panel slice của Resolume, widget/màu giữ của MikMap; **Input source chỉ chọn ở trang Input**, không hiện ở Output) — hai bộ tuỳ theo **công cụ** đang chọn trên thanh stage:
- **Transform:** **Output rectangle** (X/Y/Left/Top/Width/Height/Rotation — chính là hộp Transform trên stage; sửa số là biến đổi cả slice, warp giữ hình) · Flip · Is key · Black BG ·
  Brightness/Contrast/Red/Green/Blue · Soft edge · Black level compensation. **Không có** Warping.
- **Edit Points:** Flip · Is key · Black BG · màu · Soft edge · Black level compensation · **Warping**: *Point mode* (chỉ Linear) + **Subdivisions X / Y**
  (số đường chia thêm giữa hai biên, 0…15, −/+; đổi số thì các đường về cách đều nhưng điểm mới lấy từ bề mặt đang warp nên hình giữ nguyên) + công cụ lưới riêng của MikMap (Flatten / Uniform / + Add col / + Add row) (**không còn** ô gõ toạ độ 4 góc — chỉ kéo chuột). **Không có** khối X/Y/Width… và **không có Input mask** (khối *Input rectangle* và các nút Input mask chỉ ở trang Input; cả hai chế độ Output đều không hiện chúng).
- **3 nhóm thu gọn / mở rộng** (bấm cả dòng tiêu đề, mũi tên ▾/▸): **Picture** (Flip · Is key · Black BG · Brightness…Blue), **Soft edge** (ô bật nằm ở tiêu đề), **Black level compensation**. Trạng thái mở/đóng chỉ là cài đặt xem — giữ trong lúc app chạy, không lưu vào dự án.
- **Gọn (2026-09-26):** mỗi thuộc tính một dòng 22px (nhãn · thanh trượt · giá trị); Flip và bốn lựa chọn cùng một dòng; *Is key* / *Black BG* cạnh nhau; Soft edge có ô bật ngay trên tiêu đề mục; Output rectangle dùng ô số có nhãn bên trái (4 dòng thay vì 7). Panel Transform vừa gần đủ một màn hình 900px.
- Flip (NONE / X / Y / X+Y) cộng hợp với mirror của khung input. Black BG = nền đen đặc sau hình (nằm trong mask). Màu của slice chồng lên màu của Screen.
- Thật sự chạy trên hình: Flip, Black BG, màu slice. **Lưu nhưng chưa vẽ** (nhãn "NOT RENDERED YET"): Is key, Soft edge và Black level compensation.
Trang Input có các hàng X/Y/Left/Top/Width/Height/Rotation + Input Mask (§3.2); công tắc Soft Edge đã dời sang Output. Mask: đảo, độ mờ, danh sách điểm (≤8 điểm), xoá.

**Screen (thông tin theo mẫu Resolume, giao diện giữ theo thiết kế MikMap, 2026-09-26):** tên · **Output device** (dropdown chọn màn hình xuất) · ô số **Width / Height** ·
các hàng thanh trượt kiểu panel Clip transform (nhãn trái, giá trị mono có màu bên phải, thanh bên dưới): **Opacity** (coral) · **Brightness / Contrast** (vàng) ·
**Red** (đỏ) · **Green** (mint) · **Blue** (cyan); sau đó Edge blending (công tắc), nút **mở/đóng cửa sổ output** (`F11`), Save/Load preset.
- **Màu chỉnh thật trên cửa sổ máy chiếu** (`output.cpp`): *Opacity* nhân vào toàn bộ hình của screen; *Brightness/Contrast/Red/Green/Blue*
  (−100…100, 0 = giữ nguyên) làm bằng các lớp phủ blend GL (nhân / gain `dst·(1+c)` / cộng / trừ, `DrawColorAdjust` trong `clipart.cpp`) chỉ trong
  đường viền từng slice — vùng đen ngoài slice vẫn đen. Contrast xoay quanh xám 50%; RGB nhân kênh đó; Brightness dịch tất cả. Slice **chồng nhau**
  bị chỉnh màu 2 lần ở vùng chồng. Tác động cửa sổ máy chiếu và thumbnail slice trên trang Output routing, **không** đổi Live Output/Preview trong workspace.
- **Output device + độ phân giải (2026-09-26):** dropdown liệt kê các màn hình thật (`1: Built-in Display (1920x1200@60)`…) rồi **NDI Output / Spout Output / Virtual Output**. Chọn **màn hình thật** → Width/Height **không sửa được**, tự đặt đúng độ phân giải thực tế của màn hình đó (`SyncScreenResolutions`, mỗi khung hình — cắm màn khác thì số đổi theo; ô hiện nhãn `DISPLAY`). Chỉ khi chọn **NDI / Spout / Virtual** mới gõ được Width/Height, và khi đó nút *Mở output* (`F11`) bị khoá vì không có cửa sổ hiển thị (chưa có bộ gửi NDI/Spout thật — chỉ là lựa chọn thiết bị). Tên thiết bị lưu ở `Screen::outDev`, độ phân giải ảo lưu ở `w/h`.
- **Width/Height** hiện ở cây và ở Screen, **chưa** đổi hệ toạ độ output (stage vẫn là 1920×1080 kéo giãn ra máy chiếu).
- Kiểm bằng ảnh chụp cửa sổ output (`--outshot`): Opacity 0 → đen hẳn; Brightness +100 → trung bình sáng ~126/255; Contrast −100 → phẳng xám ~50%; Red +100 → các cột đỏ rực hơn rõ rệt. ✅

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
**5 tab** bên trái (thêm tab thứ 5, 2026-09-22): **Ngôn ngữ · Font · Màu · Cỡ
chữ · Layout**. Thay đổi **áp dụng ngay** (có khung xem trước "LIVE OUTPUT" ở
tab Cỡ chữ). Nút **Reset** đưa về mặc định (kể cả `topBandPx`, để không kẹt ở
chiều cao band đã kéo tay trước đó). Cỡ chữ nay có **5 mức** (thêm "X-Large" /
150%, trước chỉ có 4 mức tới 125%).

**Tab Layout (mới):** 4 slider, đọc/ghi `App::Prefs` — kéo áp dụng ngay,
lưu vào `settings.json` (mô tả *máy này*, không phải dự án, đúng nguyên tắc
tách máy/buổi diễn):
| Slider | Field | Khoảng | Ghi chú |
|---|---|---|---|
| Browser width | `prefs.browserW` | 140–320px | Bề rộng cây Browser bên trái (§2.4) — kéo khe dọc bên phải Browser cũng đổi giá trị này (§2.7) |
| Properties width | `prefs.inspectorW` | 180–360px | Bề rộng panel Properties bên phải (§2.6) — kéo khe dọc bên trái Properties cũng đổi giá trị này (§2.7) |
| Top band height | `prefs.bandPct` | 25–70% | % chiều cao cửa sổ dành cho dải trên (Browser/Monitor/Properties) — kéo tay bằng resize handle (đáy dải) cũng cập nhật lại giá trị % này, hai cách chỉnh đồng bộ hai chiều |
| Timeline height | `prefs.timelineH` | 0–96px | Cao thanh SYSTEM TIME/TIMELINE + nút transport (§2.5) — **0 = ẩn hẳn thanh này** |

Cả 4 field lưu trong `settings.json` (`SaveSettings`/`LoadSettings`,
`project.cpp`), clamp lại khi nạp để file chỉnh tay hỏng không kéo méo layout.

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
| Trang **Advanced Mapping**: `Ctrl/Cmd+C/X/V/D`, `Delete`, mũi tên, `Ctrl/Cmd/Shift`+bấm | Copy / cắt / dán / nhân đôi / xoá / dịch / chọn nhiều screen-slice-mask (xem M3c–M3e, có trong bảng Help) |
| `F11` | Mở/đóng cửa sổ output máy chiếu |
| `Esc` / `F11` / `Ctrl+W` (`Cmd+W`) **khi cửa sổ output đang được focus** | **Đóng cửa sổ output** — trước đây `F11` chỉ ăn khi cửa sổ chính có focus, còn cửa sổ máy chiếu (GLFW trần, không qua ImGui) không nhận phím nào. Nay nó có callback phím riêng (`OutputKeyCb`, `output.cpp`); chỉ tác dụng lúc nhấn xuống, phím khác không đóng. Có trong bảng phím tắt (Help) |
| `Esc` | Đóng popover/menu/hộp thoại/ô đổi tên/Cài đặt |
| Lăn chuột (Mapping, trang Input/Output) | **Zoom theo con trỏ** (điểm dưới con trỏ đứng yên); `Shift`+lăn = cuộn dọc; lăn ngang/trackpad = cuộn ngang. Trước đây lăn = cuộn, phải `Alt`+lăn mới zoom |

Phím tắt bị vô hiệu khi đang gõ chữ hoặc khi có menu/hộp thoại mở. Chưa có `Ctrl+C/V` hay phím gán tuỳ ý cho từng clip.

---

## 7. Khoảng trống & điểm lạ

Trạng thái sau đợt sửa 2026-09-21 (✅ đã sửa · ⛔ còn tồn tại):

| Mã | Vấn đề | |
|---|---|---|
| X1 | Không lưu/mở dự án và cài đặt | ✅ Đã có `.mikmap` + settings; **chưa dùng `ProjectIO`/`.mikmap` của engine** |
| X2 | Menu Project và nút Group/Column/Sync là hình | ✅ Đã nối hết |
| X3 | Rename layer/cột, Loop trong popover không làm gì | ✅ Đã sửa |
| X4 | Chip blend mode có tên khác dropdown, chọn Alpha/Additive rơi về Normal | ✅ Đã sửa (đã xác nhận đúng là lỗi thật) |
| X5 | Không phím tắt thật; không undo cho Composition | ✅ Đã có phím tắt (mục 6) và undo/redo toàn app |
| X6 | Timeline không scrub được | ✅ Đã sửa (2026-09-21: tổng thời lượng/tốc độ playhead theo thời lượng thật của clip) — **2026-09-22: bỏ thanh kéo (scrub bar) ở Grid mode** theo yêu cầu thiết kế mới (§2.5); kéo tua playhead vẫn còn ở Timeline run mode (§2.3b) |
| X7 | Edit ROI không kéo được; radar chỉ giả lập 1 điểm chạm | ✅ ROI kéo được — ⛔ radar vẫn chỉ giả lập |
| X8 | Thanh trạng thái hiện MIDI/Art-Net/NDI giả | ✅ Đã thay bằng số thiết bị và trạng thái output thật |
| X9 | Tên dự án không cập nhật tiêu đề | ✅ Đã sửa |
| X10 | Badge `2L`/`1L` ở header cột là số cố định | ✅ Nay đếm layer đang phát thật |

---

## 8. Bản thiết kế tham chiếu

`sampleUI/mikmap_ui.tsx` và `sampleUI/mikmap_UI_pug` (cập nhật lần cuối 11/9) là
mockup gốc. Chưa đối chiếu từng chi tiết với app hiện tại — nếu bạn thấy khác biệt
so với mockup, ghi vào `ux-feedback.md`.
