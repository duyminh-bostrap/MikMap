# Trang Mapping gộp: căn máy chiếu và căn sensor trên cùng một trang

Ngày: 2026-10-29 · Trạng thái: chờ người dùng duyệt · Nhánh nền: `feat/sensor-skeleton-hokuyo` (HEAD `7560b2b`)

## 1. Mục tiêu

Người vận hành dựng một khu vực tương tác theo đúng thứ tự ngoài hiện trường:

1. Căn máy chiếu: slice, keystone, warp (đã có, trang Advanced Mapping).
2. Kết nối sensor và căn sensor cho khớp với hình đang chiếu thật. **Mỗi sensor có thể chỉ phủ một phần vùng chiếu**; nhiều sensor ghép lại thành cả vùng.
3. Toạ độ và tracking gửi ra Unity, UE hoặc TouchDesigner để tạo hiệu ứng.
4. MikMap nhận lại hình theo thời gian thực (NDI, Spout, capture card) vào các column.
5. Các column là nguồn input cho Mapping, khép vòng lặp.

Spec này lo **bước 2** và giao diện của nó. Bước 3 đã có một phần (OSC/UDP). Bước 4 và trang Effect được ghi chú ở §8, làm sau.

Thước đo thành công: người vận hành căn xong một sensor mà **không phải rời trang Mapping**. Họ luôn nhìn thấy sensor nằm ở đâu so với hình đang chiếu, và biết sensor nào còn thiếu bước nào.

## 2. Quyết định đã chốt với người dùng

| # | Quyết định | Lời người dùng / lý do |
|---|---|---|
| D1 | Gộp trang Sensor vào Advanced Mapping; đổi tên trang thành **Mapping** | "dùng chung screen tree, cùng tác vụ kéo thả"; tên "Mapping hoặc Calibration", chọn Mapping (đúng thuật ngữ sản phẩm) |
| D2 | Sensor là **một node trong cây Screen** (như slice) | phương án A |
| D3 | Tab **INPUT / OUTPUT dùng chung** cho slice và sensor, nội dung đổi theo node đang chọn trong cây; **không có tab SENSOR riêng** | xác nhận nguyên văn |
| D4 | Nút thêm sensor mở **popup** add device, không thay cả cột giữa như wizard hiện tại | "hiện pop up để add device thay vì đổi toàn bộ UI cột 2" |
| D5 | Sensor trả về **hai loại dữ liệu**: video, hoặc danh sách điểm (tx, ty) | nguyên văn |
| D6 | **INPUT của sensor**: chỉnh vị trí và góc xoay, hiện các điểm đang detect, **nền là hình live của composition** đang chiếu | nguyên văn |
| D7 | **OUTPUT của sensor**: kéo 4 góc để warp và tạo lưới hàng × cột; nền vẫn là hình live | nguyên văn |
| D8 | Lưới dùng để **bù bề mặt không phẳng** (tường cong, bậc, méo ống kính), mỗi ô ánh xạ riêng như mesh warp của slice | chọn phương án được khuyến nghị |
| D9 | Sensor **đang chọn dùng màu chọn giống slice đang chọn**; các sensor khác **xám mờ** để căn vùng chồng nhau | nguyên văn |
| D10 | Hai sensor chồng vùng: **gộp các điểm gần nhau thành một điểm** | nguyên văn |
| D11 | x, y chuẩn hoá **0–1 trên toàn screen, chia cho độ phân giải của screen** (không phải 1920×1080). Sensor chỉ trả về toạ độ **trong vùng nó phủ** | nguyên văn |
| D12 | Stage Output dùng **đúng độ phân giải của từng screen**; file cũ co một lần từ 1920×1080 khi mở | chọn phương án được khuyến nghị |
| D13 | Sensor có **mask như slice** để cắt vùng dữ liệu nhiễu | nguyên văn |
| D16 | Vùng tính của sensor có hai chế độ loại trừ nhau: **Screen** (tự giới hạn trong screen) hoặc **Zone** tuỳ chỉnh (hình như mask, chỉ tính toạ độ trong zone) | thay nút "ROI = screen" (§6.4b) |
| D14 | **Làm UI/UX và luồng trước**, các hàm xử lý chi tiết sau | nguyên văn |
| D15 | Trang Sensor cũ trở thành trang **Effect** (làm sau, §8) | nguyên văn |

## 3. Bất biến kiến trúc (không đổi)

- `H_s` (sensor → stage) và `H_w` (warp slice) **tách rời mãi mãi** (nguyên tắc #4). Warp của sensor là bản tổng quát hoá của `H_s`, có 4 góc và lưới; slice không biết sensor tồn tại, sensor không đọc warp của slice.
- Sensor thread chỉ gửi **dữ liệu thô** (mm hoặc ảnh). Warp, mask, gộp điểm và chuẩn hoá chỉ áp ở **render thread**, không lock, không cấp phát trong vòng lặp nóng.
- Hình học người vẽ (mask, lưới) lưu trong không gian của **nguồn**, ở đây là không gian sensor, không lưu theo pixel máy chiếu (nguyên tắc #8).
- `engine/core` chỉ dùng STL; `engine/io` không biết Screen, Slice hay ImGui.

## 4. Giai đoạn

| Giai đoạn | Nội dung | Ghi chú |
|---|---|---|
| **S1. UI/UX và luồng** | §5: gộp trang, cây, tab theo ngữ cảnh, popup, nền live, màu chọn, chip tiến độ, Properties của sensor | Dùng lại dữ liệu và hành vi hiện có: đặt vị trí theo mm, 4 góc, OSC/UDP. Chưa có lưới, mask dữ liệu hay gộp điểm. Plan đầu tiên viết cho S1. |
| **S2. Hàm xử lý dữ liệu** | §6: stage theo độ phân giải screen, lưới cover, loại điểm ngoài vùng, mask, gộp điểm, chuẩn hoá | Mỗi mục là một task có test riêng. |
| **S3. Trang Effect** | §8 | Brainstorm và spec riêng. |

## 5. S1: UI/UX và luồng

### 5.1 Thanh tab trên cùng

`Composition · Mapping` (hai tab). Tab "Sensor I/O" bị bỏ (`src/main.cpp:109`). Tab **Effect** chỉ xuất hiện ở S3, để không có trang rỗng giữa buổi diễn. `A.screen == 2` không còn được dùng. Các chỗ đang chuyển sang trang Sensor (bấm sensor trong cây, `OpenSensorWizard`, cờ `--screen 2`) sẽ chuyển sang trang Mapping và chọn sensor đó. File và cài đặt cũ đang lưu `screen = 2` sẽ mở ra trang Mapping.

### 5.2 Cột 1: cây Screen

- Screen → slice (và mask) → sensor, theo thứ tự hiện tại của mode Mapping (`DrawTree`, `TreeMode::Mapping`). Mode `Sensors` của cây không còn cần, nên bỏ.
- Mỗi dòng sensor có **chip tiến độ** gồm 4 chấm theo thứ tự, chấm đầu tiên chưa đạt được tô nổi:
  1. **Đã kết nối:** thiết bị đang chạy (`Running`), không lỗi.
  2. **Đã đặt vị trí:** lidar có `placed` (hoặc đang ở chế độ Custom); với các loại khác thì luôn đạt.
  3. **Đã căn khớp:** `calibrated` là true.
  4. **Đang gửi:** `outOn`, và có ít nhất một gói được gửi trong 2 giây gần nhất.

  Di chuột lên chip hiện tên bước còn thiếu. Bấm vào chip thì chọn sensor và mở đúng tab cho bước đó: bước 1 mở Properties › Connection, bước 2 mở INPUT, bước 3 mở OUTPUT, bước 4 mở Properties › Send out.
- Nút thêm ở chân cây: `Screen` (coral), `Slice`, `Sensor` (mint, viền mint) như hiện tại. `Sensor` mở popup (§5.6).
- Menu chuột phải của sensor giữ nguyên: Enable/Disable · Rename · Move to screen › · Remove.

### 5.3 Giữa: stage, tab INPUT / OUTPUT theo ngữ cảnh

- **Chọn slice, mask hoặc screen:** giữ đúng hành vi Mapping hiện tại.
- **Chọn sensor:** cùng thanh tab và cùng nhóm zoom; nội dung là phần sensor:
  - **INPUT:**
    - Nền là **hình live của composition** (đúng thứ ô LIVE OUTPUT của Deck đang hiện, gồm cả Show TestCard), vẽ bằng `DrawComposite` kéo giãn theo khung screen; không phải đầu ra của các slice (`DrawScreenBackdrop` chỉ còn dùng cho stage Mapping của slice). Người dùng xác nhận 2026-10-29.
    - Trên nền: biểu tượng sensor tại vị trí và góc của nó (kéo thân để dời, kéo chấm xoay để xoay; Shift làm tròn 10 mm và bước 15°), vòng tầm, các điểm scan, các blob kèm `ID:n`.
    - Lidar vẫn dùng mm thật của screen (`realW`, `realH`); vẽ lên nền theo tỉ lệ mm → pixel của screen. Nếu screen chưa có kích thước thật thì hiện ô nhập ngay trên stage, như hiện tại.
    - Đây cũng là chỗ vẽ **mask** của sensor ở S2 (§6.4).
  - **OUTPUT:**
    - Cùng nền live.
    - Tứ giác vùng cover của sensor chỉnh bằng hai công cụ Edit Points / Transform của Mapping. Lưới hàng × cột được thêm ở S2.
    - Touch đã qua `DeviceH` vẽ chồng lên.
- **Màu:**
  - Sensor đang chọn vẽ bằng **đúng màu và kiểu vẽ của slice đang chọn trên trang đó**, tức tone của trang (OUTPUT coral, như `mapping.cpp` đang chọn `tone`). Màu mint giữ cho icon và dòng của sensor trong cây.
  - Các sensor khác cùng screen vẽ **xám mờ** (vùng cover cùng các điểm của chúng), không bấm được trên stage; muốn chọn thì dùng cây.
  - Slice vẫn hiện trên nền live nhưng không chỉnh được khi đang chọn sensor.

### 5.4 Cột 3: Properties

Đổi theo node đang chọn. Với sensor, giữ bố cục lai đã có (`SensorPropSections`): khối đầu luôn hiện, các mục còn lại theo tab.
- **Luôn hiện:** tên, trạng thái + fps/latency, Active, cảnh báo vàng khi screen dự phòng đang được dùng.
- **INPUT:** Connection (host, port, Reconnect, Scan again) · Screen size (mm) · Placement · Blob tracking.
- **OUTPUT:** Calibration (badge Placed/Custom, Re-apply placement, 4 toạ độ số, wizard chạm 4 điểm, RMS) · Send out · Triggers.

### 5.5 Trang Sensor cũ

`DrawSensor` và các view riêng của nó (`SensorInputView`, `SensorOutputView`, `LidarPlacementView`, `Radar`, `SkeletonView`) chuyển thành phần nội dung của stage Mapping khi node sensor được chọn. Không còn trang riêng; logic dữ liệu giữ nguyên.

### 5.6 Popup thêm sensor

- Modal phủ giữa cửa sổ, nền tối mờ. Nội dung là ba bước của wizard hiện tại: chọn loại → kết nối / quét IP (chọn nhiều) → kiểm tín hiệu. Lidar đã có trong dự án hiện mờ "already added". Danh sách kết quả cuộn được.
- Mở popup **không** đổi trang và không đổi stage. Screen đích là screen đang chọn trong cây, hoặc screen của sensor đang chọn.
- `Esc` hoặc nút đóng: huỷ ở bước 1–2. Ở bước 3 thì giữ các thiết bị đã tạo, kể cả thiết bị lỗi.
- `Done`: đóng popup, chọn thiết bị đầu tiên vừa thêm trong cây, mở tab INPUT.
- Quét IP vẫn chạy trên thread riêng. Popup đóng giữa chừng thì kết quả bị bỏ, thread được join ở lần quét sau hoặc khi tắt app, như hiện tại.

### 5.7 Luồng người vận hành (mục tiêu kiểm thử UX)

1. Bấm `Sensor` ở chân cây → popup → chọn Lidar 2D · Hokuyo → Scan → tick 2 lidar → kiểm tín hiệu → Done. Hai dòng sensor xuất hiện dưới screen, chip ở chấm 2.
2. INPUT: kéo lidar tới mép dưới của hình đang chiếu, xoay cho mũi tên hướng lên; chip sang chấm 3.
3. OUTPUT: chạm thử, chỉnh 4 góc nếu lệch; sensor kia hiện xám để thấy vùng chồng nhau.
4. Properties › Send out: bật OSC; chip đủ 4 chấm.

Không bước nào phải rời trang Mapping.

## 6. S2: hàm xử lý dữ liệu

### 6.1 Stage theo độ phân giải screen (D12)

- `StageSpace` trên trang Output trả về `sc.w × sc.h` của screen đang hiện, thay cho hằng 1920×1080. Mọi toạ độ output của slice (`q`, mask) và `calib[i].tx, ty` của sensor được hiểu là **pixel của screen đó**.
- **Đổi độ phân giải:** khi `w`, `h` của một screen đổi (người dùng gõ, hoặc `SyncScreenResolutions` lấy theo màn thật), mọi hình học output của screen đó co theo `(w'/w, h'/h)` bằng một hàm duy nhất. Mesh warp giữ hình nhờ là affine.
- **Di trú:** file cũ chưa có cờ `stageSpace` được co một lần từ 1920×1080 sang `w × h` của từng screen khi mở; file mới ghi cờ đó.
- Chỗ đang tiêu thụ không gian 1920×1080 (ví dụ `output.cpp` co theo `fw/1920`, `fh/1080`) đổi sang `w`, `h` của screen. Phải tìm mọi hằng `1920` / `1080` trong `src/` và rà từng chỗ.

### 6.2 Lưới cover của sensor (D7, D8)

- Mỗi sensor có `Keystone` 4 góc cộng `meshLocal` hàng × cột (mặc định 1×1), **dùng chung kiểu dữ liệu và code** với slice.
- Không gian vào của lưới là **vùng cover của sensor**: hình chữ nhật ROI theo mm với lidar, UV 0–1 của ảnh với camera. Không gian ra là pixel của screen.
- 4 góc ban đầu suy ra từ vị trí đặt sensor (thay cho 4 cặp calib hiện tại). Kéo tay thì sang chế độ Custom, như hành vi đã có.
- Hàm thuận sensor → screen dùng phần thuận của warp. Cần kiểm tra lại hợp đồng `IWarp` (`inverse` trả `bool`) nếu có chỗ phải đi ngược.
- Thêm hàng/cột trên OUTPUT dùng đúng thao tác của slice (Edit Points: ô vuông lớn là góc, ô nhỏ là điểm lưới).

### 6.3 Toạ độ trả về (D11)

- `x = X / sc.w`, `y = Y / sc.h`, với X, Y là pixel trên screen. Giá trị 0–1 tính trên **toàn screen**.
- Điểm có vị trí sau warp nằm **ngoài vùng cover** của sensor thì bị bỏ, không kẹp vào mép.
- **Đệm mép:** một id đã nằm trong vùng chỉ bị bỏ khi ra ngoài quá 8 px của screen, để tránh nhấp nháy ở mép.

### 6.4 Mask của sensor (D13)

- Dùng lại mask bezier của slice. Vẽ trên tab **INPUT** của sensor. Lưu trong không gian sensor: mm với lidar, UV với camera. Kéo lại vị trí sensor hay warp thì mask đi theo, không phải vẽ lại.
- Điểm rơi trong vùng mask cắt bỏ thì bị loại. Với dữ liệu video, mask là alpha.
- Đa giác đã làm phẳng được tính sẵn khi mask đổi, ở render thread, ngoài vòng lặp nóng. Vòng lặp nóng chỉ kiểm tra điểm trong đa giác bằng mảng có sẵn.

### 6.4b Vùng tính của sensor: "Screen" hoặc "Zone" (D16, người dùng 2026-10-29)

- Mỗi sensor có một chế độ **loại trừ nhau** (không giao nhau), chọn ở Properties › Placement thay cho nút "ROI = screen":
  1. **Screen (mặc định cho lidar mới):** chỉ tính dữ liệu trong vùng screen. ROI luôn bằng 4 góc screen trong không gian của sensor và tự tính lại khi vị trí, góc xoay hay kích thước screen đổi.
  2. **Zone (tuỳ chỉnh):** người dùng tạo một hay nhiều zone; **chỉ các toạ độ rơi trong một zone mới được tính**. Zone có thể là chữ nhật, elip, đa giác hoặc bezier, dùng đúng bộ hình và công cụ vẽ của mask (§6.4), vẽ trên INPUT, lưu trong không gian sensor (mm với lidar).
- Zone khác mask ở chiều tác dụng: **mask cắt bỏ** vùng nhiễu, **zone giữ lại** vùng dùng. Mask vẫn áp trong cả hai chế độ.
- Ở chế độ Zone, vùng screen không tự giới hạn thêm: zone có thể vươn ra ngoài screen (điểm ngoài vùng cover vẫn bị bỏ ở §6.3).
- Dữ liệu: cờ chế độ cộng danh sách zone, mảng đa giác đã làm phẳng tính sẵn như mask; `engine/io` cần bộ lọc nhiều đa giác thay cho ROI một tứ giác hiện tại.
- Làm theo hai bước: **(a)** chế độ Screen thành ô chọn, ROI tự tính lại (nhỏ, không cần engine mới); **(b)** Zone cùng mask ở S2.

### 6.5 Gộp điểm giữa các sensor (D10)

- Sau warp và lọc vùng cover, các điểm của **mọi sensor cùng một screen** cách nhau dưới bán kính gộp thì gộp thành một.
- Điểm gộp lấy vị trí trung bình và **id của điểm xuất hiện sớm nhất**, để id ổn định khi người đi qua vùng chồng nhau.
- Bán kính gộp đặt theo screen, đơn vị pixel của screen, mặc định 1,5% chiều rộng screen.
- Dùng mảng kích thước cố định theo `kMaxTouchPoints` của mỗi sensor nhân số sensor, cấp phát sẵn; không cấp phát mỗi khung hình.
- Gửi ra theo screen thay vì theo sensor: OSC `/mikmap/<screen>/touch` (tên screen đã làm sạch như tên thiết bị). Có thể giữ thêm tuỳ chọn gửi riêng từng sensor.

### 6.6 Thứ tự xử lý mỗi khung hình

1. Dữ liệu thô của sensor.
2. Lọc ROI, tầm, blob (đã có).
3. Mask của sensor.
4. Warp (4 góc + lưới) sang pixel screen.
5. Loại điểm ngoài vùng cover (có đệm mép).
6. Gộp điểm giữa các sensor cùng screen.
7. Chia cho độ phân giải screen → 0–1 → gửi OSC/UDP.

### 6.7 Dữ liệu dạng video

Mô hình ở §6.2 áp cho cả ảnh: ảnh camera được warp lên screen như nội dung của slice, có mask alpha. Nguồn video thật của sensor (camera 2D) thuộc plan camera; S2 chỉ cần đường dữ liệu không chặn nó.

## 7. Kiểm thử

- **S1:** self-test trong `--roundtrip`:
  - chọn slice thì tab hiện phần slice; chọn sensor thì hiện phần sensor;
  - file cũ có `screen = 2` mở ra trang Mapping;
  - chip tiến độ khớp trạng thái thiết bị thật (giả lập bằng Sim/Hokuyo fake);
  - popup không đổi trang.

  Ảnh `--shot` cho mỗi tab với slice và với sensor, nhiều sensor (đang chọn và xám), từng bước popup. So ảnh Mapping trước/sau khi chọn slice để chứng minh không đổi hành vi.
- **S2:** test engine thuần cho:
  - co hình học khi đổi độ phân giải;
  - warp có lưới (điểm qua ô cong rơi đúng chỗ);
  - điểm trong đa giác mask;
  - gộp điểm (hai điểm cách 10 px gộp lại, 100 px thì không, id giữ ổn định qua nhiều khung);
  - đệm mép.

  Self-test `--roundtrip` cho di trú file 1920×1080 → 3840×2160 và 1920×1200 (slice và calib co đúng).
- Mọi giai đoạn: 0 cảnh báo mới `/W4`, hai bộ test xanh, ghi rõ phần nào chưa thử với phần cứng thật.

## 8. Làm sau (chỉ ghi chú, ngoài phạm vi spec này)

- **Trang Effect (S3)**, thay chỗ trang Sensor cũ:
  - tạo hiệu ứng phản ứng chạm ngay trong MikMap: hệ particle và một số effect video cơ bản, tuỳ chỉnh được;
  - đầu vào hai dạng: dữ liệu từ sensor (điểm đã gộp, hoặc video), hoặc nguồn video và ảnh từ nơi khác;
  - đầu ra là một clip đưa vào column.
- **Nhận video vào column:** NDI, Spout, capture card, và đọc file `.tox` qua TouchEngine (B7, B8, B12 trong `features.md`).
- Các việc nhỏ còn treo từ plan trước:
  - `features.md:743` ghi sai về calib khi screen bị xoá;
  - toast "Re-apply placement" khi screen bị xoá;
  - kiểm `boundPort()==0` trong hai test;
  - Send-out chỉ nhận IPv4 hoặc `localhost`;
  - Sim bật Send-out mà không gửi gì;
  - chỉnh ROI lidar bằng tay;
  - build Linux/macOS.

## 9. Rủi ro

- **S1 đụng `mapping.cpp` lớn.** Stage và Properties của Mapping phải chừa chỗ cho nội dung sensor mà không đổi hành vi slice. Cách giảm rủi ro: so ảnh trước/sau như khi tách `stage.cpp`, cùng với `MappingSelfTest`.
- **S2 §6.1 là thay đổi toàn cục.** Mọi hình học output đổi đơn vị. Cách giảm rủi ro: một hàm co duy nhất, cờ di trú trong file, test với 3 độ phân giải.
- **Gộp điểm làm id nhảy** nếu hai sensor lệch nhau hơn bán kính gộp. Người vận hành thấy được điều này trên OUTPUT, nhờ sensor khác hiện xám cùng các điểm của nó.
