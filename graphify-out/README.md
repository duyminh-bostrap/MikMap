# MikMap: bản đồ cho agent

Bắt đầu ở [LIBRARY_TREE.md](LIBRARY_TREE.md) để tìm file, rồi dùng `graph.json` để truy quan hệ và mở nguồn kiểm chứng. `GRAPH_REPORT.md` ghi các nhóm, độ liên kết và giới hạn trích xuất; `graph.html` dùng để xem tương tác.

Graph mô tả checkout gốc `main` tại commit `5a25401`, không quét các worktree. Tài liệu có thể mô tả kiến trúc đích hoặc hành vi cũ: đọc `source_file` và `source_location`, xác nhận ở `src/` trước khi kết luận tính năng đang chạy. Quan hệ `INFERRED`/`AMBIGUOUS` không phải bằng chứng gọi hàm.

Chạy từ gốc repo trong PowerShell:

```powershell
$py = Get-Content graphify-out/.graphify_python
& $py -m graphify explain 'SensorFrame'
& $py -m graphify query 'Hokuyo calibration' --budget 1500
& $py -m graphify path 'SensorFrame' 'SensorMapper' --undirected
& $py -m graphify update .
```

Chọn từ khoá theo label thật trong JSON; ưu tiên tên symbol tiếng Anh. Khi đường dẫn Python lưu ở `.graphify_python` không còn hợp lệ, tìm lại Python có cài `graphifyy`. `update` cập nhật code; khi tài liệu đổi, chạy lại skill `/graphify . --update` để trích xuất ngữ nghĩa.

Giới hạn AST: header ABI `mikmap_sensor_plugin.h` không lấy được symbol; hai header `stb` chỉ được phân tích một phần; sáu file khai báo không sinh symbol. Cây file vẫn giữ đường dẫn để đọc trực tiếp. Bản đồ không thay thế build/test hoặc xác minh phần cứng.

Chi phí token của các agent không được runtime cung cấp. Các số 0 trong JSON là giá trị chưa đo, không có nghĩa là không tốn token.

Chẩn đoán hiện tại: 613 cạnh thiếu đầu mút, 178 cạnh cùng cặp đầu mút bị gộp ở graph vô hướng. `relations.json` giữ extraction trước gộp. Quan hệ suy luận `InputRectMenu() → PoseModel.box` có khả năng là trùng tên symbol; cần kiểm nguồn trước khi dùng. Benchmark 68,7× chỉ là ước tính token của công cụ.
