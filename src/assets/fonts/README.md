# Hướng dẫn Đổi & Thêm Font Giao diện (`fonts.json`)

Bạn có thể đổi hoặc thêm font mới vào MikMap **mà không cần biên dịch lại code C++** bằng cách chỉnh sửa file [`fonts.json`](fonts.json) trong thư mục này và chép file `.ttf` vào `assets/fonts/`.

## 1. Cấu trúc `fonts.json`

```json
{
  "text_scale": 1.2,
  "ui": [
    {
      "name": "Inter",
      "prefix": "Inter"
    }
  ],
  "mono": [
    {
      "name": "JetBrains Mono",
      "prefix": "JetBrainsMono"
    }
  ]
}
```

## 2. Ba cách khai báo một bộ Font mới

1. **Khai báo nhanh theo `prefix` (Khuyên dùng)**:
   ```json
   { "name": "Be Vietnam Pro", "prefix": "BeVietnamPro" }
   ```
   Phần mềm sẽ tự động tìm `BeVietnamPro-Regular.ttf`, `BeVietnamPro-SemiBold.ttf`, `BeVietnamPro-Bold.ttf`, `BeVietnamPro-ExtraBold.ttf`. Nếu bạn chỉ có 1 hoặc 2 file (ví dụ chỉ có `BeVietnamPro-Regular.ttf`), phần mềm sẽ tự động dùng file đó cho các độ đậm còn thiếu mà không báo lỗi.

2. **Khai báo 1 file `.ttf` duy nhất**:
   ```json
   { "name": "Custom Font", "file": "CustomFont.ttf" }
   ```

3. **Khai báo tường minh từng độ đậm**:
   ```json
   {
     "name": "Inter",
     "regular": "Inter-Regular.ttf",
     "semibold": "Inter-SemiBold.ttf",
     "bold": "Inter-Bold.ttf",
     "extrabold": "Inter-ExtraBold.ttf"
   }
   ```
   *(Với nhóm `"mono"`, 3 độ đậm là `"regular"`, `"medium"`, `"bold"`).*

> **Lưu ý về Variable Font (`-Var.ttf`)**: Bộ dựng chữ `stb_truetype` của Dear ImGui chỉ đọc đường viền tĩnh (`glyf`). Nếu bạn tải một Variable Font, hãy dùng các file tĩnh (`static/*.ttf`) để hiển thị đúng độ đậm `Bold` / `SemiBold`.
