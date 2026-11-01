---
name: cut-release
description: Cắt một bản phát hành MikMap — kiểm điều kiện, gắn tag vX.Y.Z, theo dõi GitHub Actions và xác minh release công khai ở duyminh-bostrap/MikMap-releases (gói, SHA256SUMS.txt, ghi chú). Chỉ chạy khi người dùng gọi tường minh.
argument-hint: <tag, ví dụ v0.1.2>
disable-model-invocation: true
---

# Cắt bản phát hành MikMap: `$ARGUMENTS`

Release ở `MikMap-releases` là **công khai và khó thu hồi** (người khác có thể tải ngay). Vì vậy
quy trình này có một cổng xác nhận trước khi gắn tag, và không bao giờ tự xoá hay dời thứ gì đã
đẩy lên mà không hỏi.

Nguồn sự thật cho các bước build/đóng gói là `.github/workflows/release.yml` và
`store/releases.md`. Đọc hai file này trước nếu nghi ngờ, đừng dựa vào trí nhớ.

## 0. Kiểm tham số

- `$ARGUMENTS` phải đúng dạng `vMAJOR.MINOR.PATCH`, cho phép thêm `-hậu-tố` (`v0.1.2`, `v0.2.0-rc1`).
  Sai dạng → dừng, nói rõ dạng đúng.
- Không có tham số → hỏi người dùng muốn tag gì, đừng tự đoán số phiên bản.

## 1. Điều kiện bắt buộc (chỉ ĐỌC ở bước này; dừng và báo nếu bước nào không đạt)

Chạy từng lệnh, đừng gộp thành một dòng phức tạp (hook `pre-push` từng chặn nhầm lệnh gộp).

1. **Nhánh và cây làm việc:**
   `git rev-parse --abbrev-ref HEAD` phải là `main`; `git status --short` phải trống.
2. **Đồng bộ với remote:** `git fetch origin`, rồi `git rev-list --count main..origin/main` phải là `0`
   và `git rev-list --count origin/main..main` phải là `0` (commit định phát hành đã được push).
   Ghi lại SHA ngắn: `git rev-parse --short HEAD`.
3. **Tag chưa tồn tại:** `git tag -l $ARGUMENTS` trống; `git ls-remote --tags origin $ARGUMENTS` trống;
   `gh release view $ARGUMENTS -R duyminh-bostrap/MikMap-releases` phải báo "release not found".
   Tag/release đã tồn tại → **dừng**, không ghi đè. Đưa ra lựa chọn: dùng số phiên bản mới.
4. **Secret có mặt:** `gh secret list -R duyminh-bostrap/MikMap` phải liệt kê `RELEASES_TOKEN`
   (chỉ thấy tên, không thấy giá trị). Thiếu → nhắc người dùng tự thêm vào tab **Secrets** (không phải
   Variables) rồi dừng. Tuyệt đối không nhận token qua chat, không tự `gh secret set` bằng token người
   dùng gửi; token đã dán vào chat thì khuyên thu hồi.
5. **Repo phát hành không trống:** `gh api repos/duyminh-bostrap/MikMap-releases/branches --jq length`
   phải ≥ 1. Repo trống thì `gh release edit --draft=false` trả HTTP 422 (đã gặp ở v0.1.0).
6. **Engine test xanh ở máy này:**
   `cmake -DBUILD=build -P cmake/configure.cmake`, `cmake --build build --target mikmap_tests -j`,
   `ctest --test-dir build --output-on-failure`. Báo số test đạt. (Hook `pre-push` cũng chạy lại.)
7. **CI trên chính commit này đã xanh:** `gh workflow run release.yml -R duyminh-bostrap/MikMap --ref main`
   (chạy thủ công chỉ build + test + đóng gói, **không đăng gì**), rồi theo dõi đến khi xong và
   mọi job `build` đều `success`. Đây là lần thử trước khi làm việc không đảo ngược.
   Đừng bỏ bước này: các lỗi macOS/Windows thường chỉ lộ trên runner thật.

## 2. Ghi chú phát hành (tuỳ chọn)

Nếu có `store/release-notes/$ARGUMENTS.md` thì workflow dùng nó nguyên văn (các mục `### What's new`,
`### Fixes`, `### Known issues`; Markdown thuần, **không ảnh, không HTML**, dưới 20.000 ký tự).
Không có thì workflow tự sinh bản tối thiểu. Hỏi người dùng có muốn viết ghi chú không; nếu có, tạo file,
commit, push **trước khi** gắn tag (ghi chú phải nằm trong commit được gắn tag).

## 3. Cổng xác nhận, KHÔNG bỏ qua

Trình bày cho người dùng, rồi hỏi một câu rõ ràng và chờ trả lời:

- tag `$ARGUMENTS` sẽ trỏ vào commit `<SHA ngắn> <tiêu đề commit>`;
- release sẽ **công khai** tại `https://github.com/duyminh-bostrap/MikMap-releases/releases`;
- danh sách tệp sẽ có (đọc `matrix` trong `release.yml`: gói `.zip`/`.tar.gz` mỗi hệ, cộng
  `installers` của từng hệ, cộng `SHA256SUMS.txt`);
- các gói **chưa ký số** (Windows SmartScreen, macOS Gatekeeper sẽ cảnh báo).

Chỉ tiếp tục khi người dùng đồng ý rõ ràng cho đúng tag này. "Ừ" cho một việc khác không tính.

## 4. Gắn tag và đẩy

```bash
git tag -a $ARGUMENTS -m "MikMap $ARGUMENTS"
git push origin $ARGUMENTS
```

Chỉ đẩy tag, không `--force`, không `--tags`. Sau khi đẩy, workflow `release` tự chạy:
`draft release` → `build` (3 hệ) → `publish`.

## 5. Theo dõi

`gh run list -R duyminh-bostrap/MikMap --workflow release.yml --limit 1` lấy id, rồi chờ đến `completed`
(dùng Monitor/vòng lặp có `sleep`, đừng gọi dồn dập). Báo từng job. Nếu `gh` mất mạng giữa chừng
(`dial tcp ... timeout`) thì đó là mạng, không phải CI: thử lại, đừng kết luận CI đỏ.

Khi có job đỏ, đọc log lỗi (`gh api --allow-escape-sequences repos/.../actions/jobs/<id>/logs`, lọc mã màu)
và nêu **nguyên nhân thật** trước khi làm gì. Các lỗi đã gặp ở lần phát hành đầu, để nhận ra nhanh:

| Triệu chứng | Nguyên nhân |
|---|---|
| `... is a directory`, HTTP 422 khi upload | glob `MikMap-*.*` khớp cả thư mục `MikMap-vX-<os>`; upload phải gọi đích danh phần mở rộng |
| `Permission denied` ở script `.sh` | commit từ Windows mất bit `+x`; gọi qua `bash script.sh` và `git update-index --chmod=+x` |
| `publish` lỗi HTTP 422 ở `--draft=false` | `MikMap-releases` chưa có commit nào (repo trống) |
| job không khởi động, "payments have failed" | thanh toán/hạn mức tài khoản GitHub (repo private), không phải code |
| smoke test thoát mã 2 trên Windows/macOS | runner không có GPU/OpenGL 3.x, app không tạo được cửa sổ; chỉ render test ở Linux |
| `timeout` exit 2 trên Windows | Git Bash gọi nhầm `timeout.exe` của cmd; dùng `/usr/bin/timeout` |

**Release đã công bố nhưng thiếu gói** (ví dụ một job đỏ): `publish` vẫn công bố và ghi tên nền tảng
thiếu vào ghi chú. Đó là hành vi cố ý. Nói rõ với người dùng và để họ chọn: giữ bản này, hay sửa rồi cắt
số phiên bản mới. **Không tự** xoá release, xoá tag, hay dời tag; mỗi việc đó cần người dùng đồng ý riêng.

## 6. Xác minh sau khi công bố (bắt buộc trước khi báo xong)

1. `gh release view $ARGUMENTS -R duyminh-bostrap/MikMap-releases --json isDraft,isPrerelease,assets,url`:
   `isDraft=false`, `isPrerelease=false` (cửa hàng bỏ qua bản nháp và prerelease), liệt kê đủ tệp đã hứa ở bước 3.
2. Tải về thư mục tạm và kiểm checksum bằng chính tệp thật:
   `gh release download $ARGUMENTS -R duyminh-bostrap/MikMap-releases --dir <tạm>` rồi
   `sha256sum -c SHA256SUMS.txt` → mọi dòng `OK`. Mỗi dòng đúng dạng `<64 hex>  <tên tệp>` (hai dấu cách).
3. Đọc phần thân release: có các mục What's new / Fixes / Known issues, nói rõ các gói chưa ký số,
   không có ảnh hay HTML, và không có dòng "Not included in this release" (nếu có, đó là nền tảng thiếu).
4. Báo người dùng: URL release, SHA commit đã gắn tag, kết quả checksum, và mọi nền tảng còn thiếu.

## Không được làm

- Không phát hành khi cây làm việc bẩn, khi `main` chưa đẩy lên, hay khi CI thử (bước 1.7) chưa xanh.
- Không `git push --force`, không `git push --tags`, không dời/xoá tag đã đẩy nếu người dùng chưa đồng ý.
- Không đưa token vào lệnh, file, log hay câu trả lời. Chỉ kiểm tên secret bằng `gh secret list`.
- Không sửa `store/` hay workflow trong lúc đang cắt release; nếu cần sửa, dừng quy trình, sửa, rồi bắt đầu lại.
- Không tự quyết định số phiên bản.
