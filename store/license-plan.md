# Kế hoạch triển khai license

Quyết định của chủ sản phẩm (chốt qua buổi hỏi đáp ngày 2026-10-10) và trạng thái triển khai. Yêu cầu gốc nằm ở `store/license.md`; hợp đồng với web nằm ở `store/license-web-contract.md`.

## Quyết định đã chốt

| Mục | Quyết định |
|---|---|
| Phạm vi khoá | Chỉ ba thứ: watermark, gửi sensor ra ngoài, gửi NDI. Mọi thứ khác dùng đủ khi không có license |
| Phiên bản | Một bản: có license hoặc không. Dùng thử, 1 tháng, 1 năm do **web** quyết định khi phát key; app chỉ cần biết "có quyền hay không" |
| Cách nhận biết | **Nhập key** (kiểu Resolume) `MIKP-XXXXX-XXXXX-XXXXX-XXXXX`. Có nút "Lấy key" mở `<STORE_URL>/store/mikmap/license` |
| Số máy | Mỗi key một máy. Kích hoạt và gỡ key đều cần online |
| Kiểm tra | Chỉ trực tuyến. Không có token ký ngoại tuyến, nên **không cần** xác thực chữ ký trong app |
| Cập nhật | `updates_until` do máy chủ trả về. Bản build phát hành sau ngày đó **chạy như không license**, không bị chặn mở |
| Hết hạn | `expires_at` đã qua thì chạy như không license |
| Mất mạng | Tin kết quả lần kiểm tra thành công cuối tối đa 30 ngày và không bao giờ quá `expires_at`. Có mạng lại mà máy chủ nói "không" thì dừng ngay |
| Web | Dùng chung web `Mike-Portfolio`, entitlement **riêng theo sản phẩm** (ADR 0001): không bao giờ gọi `get_entitlement()` của MikMaster |
| Gửi sensor | Qua OSC/UDP, sau khi đã calib trong app. Làm sau (Pha 3) |
| NDI, Spout | Thêm sau |
| Watermark | Logo và tên app |

## Nguyên tắc ràng buộc

1. Render thread không khoá, không cấp phát, không chạm đĩa hay mạng. Trạng thái license là một số nguyên tử mà render thread chỉ đọc.
2. `engine/core` chỉ dùng STL. Quyết định "được làm gì" là hàm thuần ở `engine/core/license`. Mạng và lưu trữ sẽ ở `src/`.
3. Mặc định khi không rõ là **không có license**.
4. Thời gian tính hạn là giờ máy chủ. Giờ máy chỉ dùng trong `evaluateCached()`, lúc mất mạng.

## Trạng thái từng pha

| Pha | Nội dung | Trạng thái |
|---|---|---|
| 1 | Mô hình quyền `engine/core/license` + test | **Xong** (`tests/test_license.cpp`, 16 test) |
| 2 | Watermark ở Live Output, Show Mode, cửa sổ máy chiếu | **Xong**, trừ cửa sổ máy chiếu chưa kiểm bằng ảnh (xem dưới) |
| 3 | Cổng ở đường gửi sensor (OSC/UDP) và NDI | Chưa. Hai đường này chưa tồn tại, viết cùng cổng |
| 4 | Kích hoạt thật: nhập key, gọi web, lưu kết quả, hết hạn, gỡ key | Chưa. Chờ web có hàm (xem hợp đồng) |
| 5 | Giao diện: tab License, nhãn trạng thái | Chưa |

### Pha 1 (đã làm)

- `engine/core/license/License.{h,cpp}`: `State`, `Claims`, `Entitlements`, `evaluate()`, `evaluateCached()`, `ymdFromUnix()`, `parseBuildDate()`.
- Chỉ `Licensed` và `Trial` mở khoá. `Trial` không bị ràng buộc bởi `updates_until`. Có `updates_until` mà không biết ngày build thì không tin.
- `src/license.cpp`: ba bit nguyên tử (`LicenseWatermark()`, `LicenseSensorOut()`, `LicenseNdiOut()`), mặc định tất cả là "không license".
- Cờ thử `--license none|licensed|trial|expired|other_machine|revoked|invalid`, và một đoạn kiểm trong `--roundtrip`.

### Pha 2 (đã làm)

- `DrawWatermark(area)` vẽ logo và chữ "MIKMAP / UNLICENSED" lặp thưa, so le, độ mờ 22%, kích thước theo khung. Vẽ **sau cùng**, không bao giờ trong từng slice (trong slice nó bị warp, bị mask cắt và mất khi slice ẩn).
- Gọi ở ô Live Output (`deck.cpp`), Show Mode (`main.cpp`), cửa sổ máy chiếu (`output.cpp`, sau mọi slice, không vẽ khi Blackout).
- Đã kiểm bằng ảnh `--shot` (có watermark khi không license, không có khi `--license licensed`) và `--roundtrip`.
- **Chưa kiểm bằng ảnh**: cửa sổ máy chiếu (`--outshot` mở cửa sổ viền-không lên màn hình thật), nên mới kiểm bằng đọc code. Cần chạy tay một lần.

### Pha 3 (làm sau)

Mỗi bộ gửi được viết kèm điểm chặn. Khi `!LicenseSensorOut()` thì không mở socket, không gửi gói nào, và giao diện báo "cần license". Dữ liệu gửi ra là toạ độ **đã qua calib** (nội dung), không phải toạ độ thô. TODO(owner): địa chỉ OSC và định dạng gói. NDI và Spout làm sau; chọn "NDI Output" khi chưa có license thì hiện cảnh báo thay vì im lặng.

### Pha 4 (cần web)

- Nút **Lấy key** mở `<STORE_URL>/store/mikmap/license`. `STORE_URL` là hằng cấu hình lúc build, không viết cứng.
- Ô nhập key, nút **Kích hoạt** gọi `activate_mikmap_key(key, machine)`, nút **Gỡ key** gọi `release_mikmap_key(key, machine)`. Cả hai cần mạng.
- Mã máy: băm một chiều trong `src/`, định dạng `XXXX-XXXX-XXXX-XXXX` (cùng định dạng web đang dùng).
- Thread riêng làm việc mạng và đĩa, kết quả đi vào `LicenseApply()`; render thread chỉ đọc bit.
- Lưu key và kết quả kiểm tra gần nhất ở thư mục config OS (cùng chỗ `settings.json`), **không** vào `.mikmap`.
- Chỉ nhúng URL công khai và khoá anon của Supabase, không bao giờ khoá dịch vụ.
- Kiểm tra định kỳ để biết key đã bị thu hồi chưa; khi máy chủ nói "không" thì dừng ngay, kể cả còn trong 30 ngày ân hạn.

### Pha 5

Tab License trong Cài đặt (trạng thái, ngày `updates_until`, nút Lấy key / Kích hoạt / Gỡ key), nhãn "UNLICENSED" ở thanh trạng thái, dòng license ở mục Giới thiệu. Chuỗi mới phải có đủ 5 ngôn ngữ trong `Settings` và đủ ở hai bảng `Localization` để `test_localization` không đỏ.

## Kiểm thử

- Đơn vị: `tests/test_license.cpp`.
- Không cửa sổ: đoạn license trong `mikmap --roundtrip`.
- Ảnh: `mikmap --shot out.png --license none` và `--license licensed`.
- Pha 4 sẽ dùng máy chủ giả, không gọi web thật trong test.

## Rủi ro và giới hạn

- License phía máy khách thì người có kỹ năng vẫn gỡ được watermark. Mục tiêu hợp lý là ngăn dùng thương mại vô ý, đừng hứa hơn thế trong tài liệu cửa hàng.
- Mất output giữa show nặng hơn mất doanh thu, nên mất mạng không được hạ một license hợp lệ trong thời gian ân hạn, và hết hạn/bản quá cũ chỉ thêm watermark và chặn gửi chứ không chặn mở.
- Bộ gửi sensor và NDI là tính năng mới, không chỉ là cổng.
- Hiện **mọi bản build** đều hiện watermark vì chưa có nguồn license thật (Pha 4). Đừng phát hành ra công khai từ nhánh này trước khi Pha 4 xong, nếu không người dùng thật sẽ thấy watermark mà không có cách gỡ.
