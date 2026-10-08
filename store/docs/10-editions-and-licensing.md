---
title: Phiên bản và license
slug: editions-and-licensing
---
MikMap được bán theo license. Bài này nói rõ quy định, và tách riêng điều ứng dụng đã thực thi với điều chưa.

## Quy định

| | Không có license | Có license |
|---|---|---|
| Watermark trên hình | Có | Không |
| Gửi tín hiệu sensor ra ngoài | Không | Có |
| Gửi NDI | Không | Có |

TODO(owner): bảng tính năng đầy đủ cho từng phiên bản (tên các phiên bản, thời gian dùng thử, số máy mỗi key). Chi tiết ở `store/license.md` trong kho mã nguồn.

## Ứng dụng đã thực thi đến đâu

Bản hiện tại **chưa thực thi** quy định trên. Ứng dụng chưa có watermark, chưa có màn hình đăng nhập hay kiểm tra license, và chưa có tính năng gửi sensor hay NDI để mà chặn. Khi các phần này được thêm, trang này sẽ được cập nhật theo.

## Cập nhật và ngày build

Mỗi bản phát hành ghi ngày build. Dự kiến, ngày này được so với hạn cập nhật của license: bản build sau hạn cập nhật sẽ không dùng được với license đó (chưa thực thi). Xem ngày build ở mục **Giới thiệu MikMap** trong menu dự án.
