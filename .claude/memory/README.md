# `.claude/memory/` — bộ nhớ cá nhân của Claude

Thư mục này dành cho ghi chú mà Claude tự lưu lại **giữa các phiên làm việc**
— ví dụ quyết định nhỏ đã chốt, thứ đã thử mà không hoạt động, hay ngữ cảnh
của một task đang dở nhiều phiên. Nội dung ở đây **được theo dõi bằng git**
(`.gitignore` không còn chặn `.claude/memory/*`), nên đừng ghi thông tin
riêng máy/người (đường dẫn cục bộ, thiết bị cá nhân) — những thứ đó thuộc
`CLAUDE.local.md`.

## Khi nào dùng thư mục này thay vì tài liệu chung

| Loại thông tin | Nơi lưu |
|---|---|
| Quyết định kiến trúc/quy ước áp dụng cho cả team | `.claude/rules/*.md` hoặc `architecture.md` |
| Tính năng đã xong, đối chiếu được bằng code thật | `features.md` |
| Ghi chú tạm, việc dở dang nhiều phiên | `.claude/memory/` (thư mục này) |
| Cấu hình máy/thiết bị cá nhân | `.claude/CLAUDE.local.md` |

Nói cách khác: quy ước và tính năng đã chốt đưa vào tài liệu chung ở trên; ghi
chú tạm/việc dở dang để Claude tự nhắc mình ở phiên sau thì để ở đây.
