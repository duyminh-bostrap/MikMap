---
title: Ghi chú phát hành
slug: release-notes
---
Ghi chú của từng bản nằm trong mô tả của bản phát hành tại kho `duyminh-bostrap/MikMap-releases` trên GitHub, và hiển thị ở trang Download của cửa hàng. Mỗi bản có các mục **What's new**, **Fixes** và **Known issues**.

## Kiểm tra tệp tải về

Mỗi bản phát hành có tệp `SHA256SUMS.txt`, mỗi dòng gồm mã SHA-256 và tên tệp. Để kiểm tra trên Linux hoặc macOS:

```bash
shasum -a 256 -c SHA256SUMS.txt
```

Trên Windows PowerShell:

```powershell
Get-FileHash .\MikMap-v0.1.0-windows-x64.zip -Algorithm SHA256
```

So kết quả với dòng tương ứng trong `SHA256SUMS.txt`.

## Chưa ký số

Các gói hiện tại chưa được ký số, nên Windows và macOS có thể hiện cảnh báo lần đầu mở. Xem [Xử lý sự cố](/store/mikmap/docs/troubleshooting).
