# Sensor packs (P): cài sensor theo nhu cầu

Ngày: 2026-10-30 · Trạng thái: chờ người dùng duyệt · Nhánh nền: `feat/sensor-skeleton-hokuyo`

## 1. Mục tiêu và bối cảnh

MikMap sắp có sensor chạy mô hình ngay trong App (nhận dạng người bằng ONNX Runtime + CUDA, rồi tay, mặt, cử chỉ). Phần chạy GPU và mô hình rất nặng (ONNX Runtime bản GPU cỡ vài trăm MB, cuDNN có thể gần 1 GB, mỗi mô hình vài tới vài chục MB; đây là **ước lượng, chưa đo**). Không phải máy nào, buổi diễn nào cũng cần.

Yêu cầu của người dùng (2026-10-30): "nếu nặng thì khi install app sẽ có options để chọn sử dụng sensor nào; dùng sensor nào thì sẽ tải DLL hoặc model của sensor đó; sửa cả UI để cài trên app nếu chưa cài nhưng lúc sau muốn cài."

Spec này lo **P: quản lý gói sensor**: nơi lưu, danh mục, tải, kiểm tra, cài, gỡ, nạp lúc chạy, giao diện. Spec **A** (nền chạy mô hình + nhận dạng thân người nhiều người) viết sau và dùng P. B (tay/mặt/chế độ) và C (cử chỉ) sau A.

Thước đo thành công:
- Một máy chưa cài gói nào vẫn mở và chạy App bình thường (Composition, Mapping, Hokuyo, OSC).
- Người vận hành cài một gói từ trong App, thấy dung lượng trước khi tải, không bị đơ giao diện, và gói chưa kiểm tra đúng thì **không bao giờ** được nạp.
- Mở một dự án cần gói mà máy thiếu thì sensor báo "chưa cài", không báo lỗi lạ và không tự tải gì.

## 2. Quyết định đã chốt

| # | Quyết định | Nguồn |
|---|---|---|
| P1 | Mỗi loại sensor nặng là một **gói** riêng; sensor đã có sẵn trong App (Hokuyo, OSC, Sim) không cần gói | người dùng |
| P2 | Chọn gói lúc cài App **và** cài/gỡ bất cứ lúc nào trong App | người dùng |
| P3 | Nguồn tải: **GitHub Releases của repo** (danh mục + tệp gói). Có đường cài từ tệp có sẵn cho máy không mạng | người dùng chọn |
| P4 | Danh mục được **ký Ed25519**; mỗi tệp kiểm SHA-256 | người dùng duyệt phần 2 |
| P5 | Không cập nhật hay gỡ gói khi sensor dùng nó đang chạy | người dùng duyệt phần 2 |
| P6 | Không tự tải khi chưa bấm; mở dự án thiếu gói chỉ báo thiếu | người dùng duyệt phần 3 |
| P7 | Giao diện ở ba chỗ: popup thêm sensor, Cài đặt › Sensor packs, cảnh báo ngay trên sensor thiếu gói | người dùng duyệt phần 3 |

## 3. Bất biến kiến trúc (không đổi)

- Render thread không lock, không cấp phát trong vòng lặp nóng, không chạm đĩa hay mạng. Tải, băm SHA-256 và giải nén chạy ở **thread riêng**; kết quả sang render thread qua atomics hoặc hàng đợi một chiều như `TouchSender`/`NetScan` đã làm.
- `engine/core` chỉ STL. Mã mạng, băm và nạp DLL nằm ở `engine/io` (cùng chỗ `net/` và `plugin/`), không biết Screen, Slice, hay ImGui.
- Gói là dữ liệu của **máy**, không phải của **buổi diễn**: lưu ở thư mục cấu hình (`ConfigDir()` = `%APPDATA%\MikMap`, `src/project.cpp:521`), **không** vào `.mikmap`. Dự án chỉ ghi sensor cần gói nào.
- P không thêm dependency ngoài: băm SHA-256 và Ed25519 là mã tự viết nhỏ gọn kèm test với vector chuẩn (RFC 8032, FIPS 180-4); mạng dùng WinHTTP của Windows. Phải có test thật cho mã mật mã, vì sai ở đây là lỗ hổng bảo mật.

## 4. Mô hình gói

```
%APPDATA%\MikMap\packs\
  installed.json                     danh sách gói đã cài (id, phiên bản, thời điểm)
  <id>\<version>\
    pack.json                        manifest (bên dưới)
    <các tệp DLL / .onnx ...>
%APPDATA%\MikMap\packs\.tmp\         vùng tải dở (*.part)
```

`pack.json`:

```json
{ "id": "pose-onnx", "name": "Body tracking (ONNX)", "version": "1.0.0",
  "kind": "runtime | plugin | model", "sizeBytes": 123456789,
  "files": [ {"path": "onnxruntime.dll", "sha256": "…", "size": 12345} ],
  "requires": [ {"id": "onnx-runtime-cuda", "min": "1.18.0"} ],
  "provides": ["sensor.pose"] }
```

- `kind = plugin`: DLL sensor theo ABI C hiện có (`engine/io/plugin`); thư mục của gói được thêm vào đường quét plugin. `SensorPlugins()` hiện chỉ quét **một lần** từ `<exe>/plugins/sensors` (`src/sources.cpp:189`); P phải cho phép quét lại sau khi cài hoặc gỡ.
- `kind = runtime`: DLL dùng chung (ONNX Runtime, CUDA provider, cuDNN). `kind = model`: tệp `.onnx` của A/B/C.
- Phụ thuộc cài theo thứ tự. Phiên bản cố định theo danh mục; không có nâng cấp tự động.
- **Danh mục** `catalog.json` + `catalog.sig` (chữ ký Ed25519) ở GitHub Releases: danh sách gói (manifest rút gọn, địa chỉ từng tệp, SHA-256, dung lượng).
- Dự án ghi nhu cầu bằng `Device::needsPack` (chuỗi id gói, lưu thành khoá `"needsPack"` trong `.mikmap`). Giá trị do loại sensor quyết định, người dùng không sửa. Rỗng hoặc không có khoá nghĩa là sensor không cần gói; file cũ nạp bình thường.

## 5. Tải, kiểm tra, cài

1. **Tải danh mục** → kiểm chữ ký bằng khoá công khai nhúng trong App; sai chữ ký hoặc không có chữ ký thì **từ chối toàn bộ**, không tải gì.
2. **Tải từng tệp** qua WinHTTP (HTTPS) ở thread nền, ghi vào `.tmp\<id>\*.part`; tiến độ và trạng thái trong atomics; huỷ được.
3. **Kiểm** SHA-256 và kích thước từng tệp so với danh mục; sai bất kỳ tệp nào → xoá `.tmp`, báo lỗi, **không** cài.
4. **Cài**: đổi tên nguyên khối sang `packs\<id>\<version>\`, ghi `installed.json` (ghi qua tệp tạm rồi đổi tên, như `ProjectIO`). Chỉ lúc này gói mới nhìn thấy được.
5. **Nạp**: chỉ nạp DLL bằng đường dẫn tuyệt đối trong thư mục gói (`LoadLibraryExW`, không tìm theo `PATH`), sau khi gói đã ở trạng thái đã cài. Mỗi lần khởi động chỉ kiểm lại `pack.json` và kích thước tệp; băm lại toàn bộ chỉ khi người dùng bấm **Verify packs**.
6. **Gỡ**: xoá thư mục gói và dòng trong `installed.json`; từ chối nếu có sensor đang chạy dùng gói đó.
7. **Cài từ tệp** (`.mikpack`, tệp nén chứa `pack.json` + tệp + chữ ký của danh mục tương ứng): kiểm tra y như bản tải về.
8. **An toàn tên tệp:** mọi `path` trong manifest phải là đường dẫn tương đối không chứa `..`, tên ổ đĩa, hay đường dẫn tuyệt đối; vi phạm là gói hỏng và bị từ chối (chống ghi ra ngoài thư mục gói).
9. **Không tự tải.** Chỉ tải sau khi người dùng bấm Cài đặt, hiện dung lượng trước.

## 6. Giao diện

- **Popup thêm sensor, bước 1:** thẻ sensor chưa cài hiện "Chưa cài · <dung lượng>" và nút **Cài đặt**; tải ngay trong popup có thanh tiến độ và nút Huỷ; xong thì thẻ chuyển sang đã cài và popup đi tiếp sang bước 2. Đóng popup khi đang tải thì tải tiếp tục ở nền.
- **Cài đặt › Sensor packs:** danh sách mọi gói trong danh mục: tên, phiên bản, dung lượng, trạng thái (chưa cài, đang tải n%, đã cài, lỗi, cần cập nhật); nút Cài đặt, Gỡ, Cập nhật, **Install from file…**, **Verify packs**. Gói đang bị sensor chạy dùng thì Gỡ và Cập nhật bị khoá kèm ghi chú.
- **Sensor trong dự án nhưng thiếu gói:** dòng sensor trong cây chip chấm 1 màu cam, tooltip "Cần gói <tên> — Cài đặt"; Properties › Connection hiện dải "Gói chưa cài" cùng nút Cài đặt; tải xong thì sensor tự kết nối.
- Lúc **cài App**: bộ cài là phần việc riêng, chưa có trong repo (chỉ có `release.yml`); màn hình trong App là đường chính nên bộ cài không bắt buộc phải làm trước. Spec riêng cho bộ cài sau.

## 7. Kiểm thử

- **Engine (`tests/`):** SHA-256 và Ed25519 với vector chuẩn (RFC 8032, FIPS 180-4); phân tích manifest và danh mục; từ chối đường dẫn có `..`; thứ tự phụ thuộc; chữ ký sai bị từ chối; tải từ máy chủ HTTP giả ở loopback có độ trễ và lỗi giữa chừng (huỷ, mã kiểm sai, mất mạng, tệp dở không để lại gói); trạng thái gói là hàm thuần.
- **App (`--roundtrip`):** thiếu gói thì sensor báo "chưa cài" và không tự tải; `Device::needsPack` khứ hồi; file cũ không có khoá nạp bình thường; cài và gỡ không làm `ProjectDirty()` bật.
- **Ảnh `--shot`:** popup chưa cài, Cài đặt › Sensor packs, dòng sensor thiếu gói. Tải thật qua GitHub, bấm bằng chuột thật và cài trên máy sạch chưa kiểm được từ môi trường phát triển; phải ghi rõ trong tài liệu.

## 8. Ngoài phạm vi

- Bộ cài (installer) và việc chọn gói lúc cài App.
- Quy trình phát hành gói (dựng, ký, đẩy lên Releases): cần một mục riêng trong CI và hướng dẫn giữ khoá ký. Ghi vào kế hoạch của P nhưng là việc của người vận hành repo.
- Build Linux/macOS của mã mạng mới (như các mã mạng trước, chưa kiểm).
- Nội dung của gói (A, B, C): ONNX Runtime, mô hình, cử chỉ.
- Cập nhật tự động, giảm dung lượng bằng delta, tải tiếp từ chỗ dừng.

## 9. Rủi ro

- **Giữ khoá ký:** mất khoá thì không phát hành được gói mới; lộ khoá thì mất bảo vệ. Cần quy trình và sao lưu; không đưa khoá vào repo.
- **Mã mật mã tự viết:** chỉ chấp nhận khi có test vector chuẩn đầy đủ; nếu thấy khó, phương án dự phòng là dùng thư viện đã kiểm (libsodium/BearSSL) kèm bảo vệ `MIKMAP_USE_*` như quy tắc của repo. Quyết định khi lập plan.
- **WinHTTP và proxy/tường lửa ở nơi diễn:** tải có thể lỗi; vì vậy luôn có đường cài từ tệp.
- **Dung lượng cuDNN:** gói GPU có thể gần 1 GB; phải hiện dung lượng trước và cho huỷ.
