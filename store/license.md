# License của MikMap

Tệp này ghi yêu cầu về license và mức ứng dụng đã thực thi. Không có tệp hợp đồng license trong kho mã nguồn (`store/` chỉ có tệp này), nên **chưa có gì được triển khai** ở phía ứng dụng. Mọi con số và quy tắc dưới đây hoặc do chủ sản phẩm nêu, hoặc là câu hỏi mở.

## Quy định của chủ sản phẩm

| Khả năng | Không có license | Có license |
|---|---|---|
| Watermark trên hình | Có | Không |
| Gửi tín hiệu sensor ra ngoài | Không | Có |
| Gửi NDI | Không | Có |

Mọi tính năng khác: TODO(owner): chưa nói rõ có bị giới hạn khi không có license hay không.

## Phiên bản (edition)

TODO(owner): tên các phiên bản (ví dụ Free, Pro) và từng phiên bản làm được gì. Bảng trên mới chỉ phân biệt "có license" và "không có license".

## Thông số chưa có số liệu

- TODO(owner): thời gian dùng thử, và dùng thử tính theo tài khoản, theo máy hay cả hai.
- TODO(owner): một key dùng được trên bao nhiêu máy.
- TODO(owner): tiền tố của key và định dạng key.
- TODO(owner): có bản license ngoại tuyến (key ký sẵn) hay chỉ kiểm tra qua mạng.
- TODO(owner): hạn cập nhật (`updates_until`): nếu hết hạn, ứng dụng được dùng tiếp ở các bản build cũ hơn hạn đó, nhưng không dùng được bản build mới hơn.

## Ứng dụng làm gì hiện tại

| Hành vi | Trạng thái |
|---|---|
| Hiện watermark khi không có license | **Chưa có.** Không có mã watermark trong ứng dụng |
| Chặn gửi tín hiệu sensor | **Chưa có.** Ứng dụng chưa gửi tín hiệu sensor, chỉ giả lập đầu vào |
| Chặn gửi NDI | **Chưa có.** NDI chỉ là nhãn thiết bị trong dữ liệu mẫu, chưa có bộ gửi |
| Đăng nhập, kiểm tra license, mã máy | **Chưa có** |
| Hết hạn, bản cũ hơn, ngoại tuyến, bị thu hồi | **Chưa có** hành vi nào |
| Ngày build nhúng vào bản build | **Có.** Hằng `BUILD_DATE` (UTC, `YYYY-MM-DD`) được CMake gán lúc configure và hiện trong mục Giới thiệu |

## Việc cần làm ở ứng dụng khi hợp đồng được cung cấp

1. Đọc hợp đồng license và triển khai đúng phía ứng dụng, không thiết kế cơ chế thứ hai.
2. Dùng `BUILD_DATE` so với `updates_until` do máy chủ trả về, dùng giờ máy chủ, không dùng đồng hồ máy.
3. Vẽ watermark ở cả khung Live Output và cửa sổ output máy chiếu khi không có license.
4. Chặn đường gửi sensor và NDI khi chưa có license. Hai đường này chưa tồn tại, nên cần được viết kèm điểm chặn.
5. Chỉ để URL công khai và khoá anon của máy chủ trong ứng dụng, không để khoá dịch vụ.

## Ghi chú cho cửa hàng

Chưa biết MikMap có phải sản phẩm thứ hai dùng cơ chế license của cửa hàng hay không (cửa hàng hiện giữ một dòng quyền lợi cho mỗi người dùng). Nếu có, cần tổng quát hoá cửa hàng trước. Phần này chỉ ghi yêu cầu, không tạo bảng, khoá hay thay đổi gì ở máy chủ.
