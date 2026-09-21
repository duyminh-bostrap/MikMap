# `.claude/memory/` — bộ nhớ cá nhân của Claude

Thư mục này dành cho ghi chú mà Claude tự lưu lại **giữa các phiên làm việc**
trên máy này — ví dụ quyết định nhỏ đã chốt, thứ đã thử mà không hoạt động,
hay ngữ cảnh của một task đang dở nhiều phiên. Đây là bộ nhớ *cá nhân/máy
này*, cùng nhóm với `CLAUDE.local.md` và `settings.local.json` — nội dung bên
trong (trừ file này) **không vào git** (xem `.gitignore` gốc, mục
`.claude/memory/*`).

## Khi nào dùng thư mục này thay vì tài liệu chung

| Loại thông tin | Nơi lưu |
|---|---|
| Quyết định kiến trúc/quy ước áp dụng cho cả team | `.claude/rules/*.md` hoặc `architecture.md` |
| Tính năng đã xong, đối chiếu được bằng code thật | `features.md` / `UI_UPDATE_PROGRESS.md` |
| Ghi chú tạm, việc dở dang, ngữ cảnh riêng máy này | `.claude/memory/` (thư mục này) |
| Cấu hình máy/thiết bị cá nhân | `.claude/CLAUDE.local.md` |

Nói cách khác: nếu ghi chú có ích cho người khác mở repo ra đọc, đưa nó vào
tài liệu chung ở trên. Nếu chỉ có ích cho Claude tự nhắc mình ở phiên sau
**trên máy này**, để ở đây.
