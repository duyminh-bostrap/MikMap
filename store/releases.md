# Phát hành MikMap

Gói cài đặt được đăng làm tệp đính kèm của GitHub Release ở kho công khai `duyminh-bostrap/MikMap-releases`. Cửa hàng đọc từ đó. Kho mã nguồn có thể giữ riêng tư hay công khai tuỳ bạn.

## Quy tắc tag và tệp

- Tag có dạng `vMAJOR.MINOR.PATCH`, có thể kèm hậu tố `-suffix`. Ví dụ `v0.1.0`.
- Release không được là bản nháp hay prerelease, cửa hàng bỏ qua hai loại đó. Workflow tạo bản nháp trong lúc build rồi chuyển sang công bố ở bước cuối.
- Mỗi release có `SHA256SUMS.txt`: mỗi dòng `<64 ký tự hex>  <tên tệp>`, hai dấu cách ở giữa.
- Nội dung release là Markdown, có các mục `### What's new`, `### Fixes`, `### Known issues`, không có ảnh, không có HTML, dưới 20.000 ký tự. Workflow kiểm tra giới hạn độ dài.

## Tên tệp: dự án chưa khớp quy ước của cửa hàng

Cửa hàng quy ước bốn tên cố định (`<Name>-Setup.exe`, `<Name>.exe`, `<Name>-Setup.pkg`, `<Name>.dmg`). **MikMap hiện không build ra tệp nào trong số đó.** Dự án chỉ đóng gói bản nén, nên workflow đang đăng:

| Tệp | Nội dung |
|---|---|
| `MikMap-<tag>-windows-x64.zip` | `mikmap.exe`, thư mục `assets`, `README.md` |
| `MikMap-<tag>-macos-arm64.tar.gz` | `mikmap`, thư mục `assets`, `README.md` |
| `MikMap-<tag>-linux-x64.tar.gz` | `mikmap`, thư mục `assets`, `README.md` |
| `SHA256SUMS.txt` | Mã băm của ba tệp trên |

TODO(owner): quyết định giữa (a) viết trình cài đặt và đặt đúng tên cố định, hoặc (b) mở rộng quy ước của cửa hàng để nhận các tệp `.zip` và `.tar.gz`. Cho đến khi đó, trang Download sẽ không nhận ra các tệp này.

Cũng chưa có bản macOS cho chip Intel, và chưa có tệp `.dmg` hay `.pkg`.

## Ký số

Chưa ký số trên cả ba hệ. Windows SmartScreen và macOS Gatekeeper sẽ cảnh báo lần đầu. Ghi chú phát hành do workflow tạo luôn nói rõ điều này.

## Ngày build

Hằng `BUILD_DATE` (UTC, `YYYY-MM-DD`) được CMake gán lúc configure, trong `src/CMakeLists.txt`, và hiện ở mục **Giới thiệu MikMap**. Đặt biến môi trường `SOURCE_DATE_EPOCH` để dựng lại cùng một ngày.

## Cách cắt một bản phát hành

Workflow nằm ở `.github/workflows/release.yml` (đóng vai trò của `sync-release.yml` trong quy ước chung).

1. Đảm bảo `main` đã xanh: engine test và build ba hệ điều hành đều đạt.
2. Một lần duy nhất: tạo fine-grained token chỉ cho `MikMap-releases`, quyền *Contents: Read and write*, và thêm vào kho mã nguồn dưới tên secret `RELEASES_TOKEN`. Không bao giờ để token trong kho.
3. Tuỳ chọn: viết ghi chú tay ở `store/release-notes/<tag>.md`, gồm các mục `### What's new`, `### Fixes`, `### Known issues`.
4. Gắn tag và đẩy:

   ```bash
   git tag v0.1.0
   git push origin v0.1.0
   ```

5. Mở tab Actions của kho mã nguồn và theo dõi. Workflow chạy test, build, đóng gói ba hệ rồi tạo release nháp ở `MikMap-releases`, đính kèm gói, tạo `SHA256SUMS.txt` và công bố.
6. Nếu một hệ build lỗi, release vẫn được công bố và ghi chú nêu hệ nào thiếu. Sửa rồi cắt tag mới.

Chạy thủ công (nút Run workflow) chỉ build và test, không đăng gì.

## Cài đặt từng nền tảng

Các gói là bản nén, chưa có trình cài đặt.

### Windows

1. Giải nén tệp `.zip` vào một thư mục, giữ `assets` cạnh `mikmap.exe`.
2. Chạy `mikmap.exe`.
3. Nếu SmartScreen hiện "Windows protected your PC", chọn **More info**, rồi **Run anyway**.
4. Nếu tường lửa hỏi quyền mạng thì chưa cần cấp, ứng dụng chưa dùng mạng.

### macOS

1. Giải nén tệp `.tar.gz`, giữ `assets` cạnh `mikmap`.
2. Chạy `mikmap` từ Terminal hoặc bấm đúp.
3. Nếu hệ thống chặn, mở **System Settings**, **Privacy & Security** và chọn **Open Anyway** cho MikMap.
4. TODO(owner): kiểm chứng trên máy macOS thật rằng gói chạy được khi tải từ trình duyệt (thuộc tính cách ly), và xác nhận có cần `chmod +x`.

### Linux

1. Cài GLFW và OpenGL của hệ thống, ví dụ `sudo apt install libglfw3 libgl1`.
2. Giải nén tệp `.tar.gz`, giữ `assets` cạnh `mikmap`.
3. Chạy `./mikmap`.

### Quyền cần cấp

Ứng dụng cần ghi vào `Documents/MikMap` và thư mục cấu hình. Chưa yêu cầu quyền camera, micro hay mạng.
