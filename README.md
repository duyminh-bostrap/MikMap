# MikMap

Projection mapping engine kết hợp hệ thống calibration sensor.

> **Lưu ý về tên:** repo tên `MikMap`, nhưng mã nguồn dùng tên nội bộ
> **HexMapping** — namespace `hexmap`, `HexMapping.sln`, `bin/HexMapping.exe`.
> Hai tên này chỉ khác nhau ở nhãn, không phải hai thứ khác nhau.

Lưới clip kiểu Resolume (deck · layer · column) → composition canvas ảo →
slice có keystone/mesh warp → máy chiếu. Kèm chuỗi ánh xạ ngược từ sensor
về toạ độ nội dung: **chạm vào vật thể thật, hiệu ứng nổ đúng chỗ đó**.

```
C++20 · openFrameworks 0.12.x · OpenGL · Dear ImGui · Windows / MSVC 2022
```

---

## Trạng thái

| | |
|---|---|
| Unit test | **313 passed, 0 failed** · 3686 assertion · 0 cảnh báo `/W4` |
| Hiệu năng | 60 fps · frame p99 ~17 ms · độ trễ sensor p99 ~9 ms |
| Video | 4K HAP Q — 1–5 luồng giữ đúng tốc độ gốc (đo thật, xem `features.md`) |
| Giao diện | Tiếng Việt / English, đổi ngay trong **Cài đặt** |

Xem [`features.md`](features.md) để biết tiến độ từng tính năng và
[`architecture.md`](architecture.md) để biết vì sao mọi thứ được đặt ở đó.

---

## Bố cục thư mục BẮT BUỘC

openFrameworks phải là **thư mục anh em** của repo này. File `.vcxproj`
trỏ tới `../openFrameworks` bằng đường dẫn tương đối.

```
D:\...\Mike\
├── openFrameworks\        ← tải từ openframeworks.cc (bản vs)
│   └── addons\
│       ├── ofxHapPlayer\  ← git clone --recursive
│       └── ofxImGui\      ← git clone --recursive
├── tools\ffmpeg\          ← bản build có --enable-libsnappy
└── MikMap\                ← repo này (tên thư mục đặt gì cũng được)
```

Tên thư mục repo không quan trọng — điều bắt buộc là `openFrameworks` và
`tools` phải nằm **cùng cấp** với nó, vì `.vcxproj` trỏ tới `../openFrameworks`.

### Cài đặt

```powershell
# 1. openFrameworks (bản vs, ~717 MB)
#    https://openframeworks.cc/download/  → giải nén thành ../openFrameworks

# 2. Addon
cd ../openFrameworks/addons
git clone --recursive --depth 1 https://github.com/bangnoise/ofxHapPlayer.git
git clone --recursive --depth 1 https://github.com/Daandelange/ofxImGui.git

# 3. ffmpeg có encoder HAP (bắt buộc: --enable-libsnappy)
#    https://github.com/BtbN/FFmpeg-Builds/releases → giải nén thành ../tools/ffmpeg
```

---

## Build

Dự án có **hai hệ build** (lý do ở `architecture.md` §7):

### `core/` + unit test — CMake thuần, KHÔNG cần openFrameworks

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
./build/Debug/hexmap_tests.exe
```

Chạy được trên máy trắng, không cần GPU. Toàn bộ toán học mapping và
calibration nằm ở đây.

### App chính — MSBuild

```powershell
& "C:/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe" `
  HexMapping.sln /p:Configuration=Release /p:Platform=x64 /m
./bin/HexMapping.exe
```

> ⚠️ Sau **mỗi lần** chạy oF Project Generator, phải chạy lại
> `tools/fix_project.ps1`. PG chèn hai stub không tồn tại và bỏ sót `src`
> trong include path — script vá cả hai.

### VS Code

Mở thư mục repo rồi:

| Phím | Việc |
|---|---|
| `Ctrl+Shift+B` | Build app (Release) |
| `F5` | Chạy app — chọn cấu hình ở panel Run and Debug |
| `Ctrl+Shift+P` → *Run Task* | Toàn bộ task: build, test, đổi video sang HAP, sinh lại project |

---

## Dùng

```powershell
./bin/HexMapping.exe          # bình thường
./bin/HexMapping.exe --demo   # bật mock sensor + auto-calibrate sẵn
```

### Bố cục

Ba trang, chuyển bằng tab trên thanh trên cùng. **Bố cục cố định, không
phải cửa sổ nổi**: trong phòng tối giữa buổi diễn không ai có thời gian sắp
lại bàn làm việc, và một bảng trôi ra ngoài màn hình là chuyện xảy ra thật.

```
MIKMAP  Dự án │ COMPOSITION │ ADVANCED MAPPING │ SENSOR I/O    FPS 60.0  ● OUTPUT 1: 1920x1080  ...
```

| Trang | Nội dung |
|---|---|
| **COMPOSITION** | thư viện media · hai màn hình xem · thuộc tính clip · lưới layer × cột |
| **ADVANCED MAPPING** | công tắc VÙNG LẤY / ĐƯỜNG RA · khung chỉnh · thiết lập slice |
| **SENSOR I/O** | thiết bị + hiệu năng · khung nhìn điểm chạm · calibration + vùng cảm ứng |

`Dự án` mở menu Lưu / Mở / Tạo mới, đưa output ra màn hình, và các lệnh
sensor. `...` bên phải mở **Cài đặt** (ngôn ngữ, màn hình mặc định, vsync).

### Thao tác

| | |
|---|---|
| Bấm ô clip | **Chọn** ô đó (không phát) |
| Bấm đúp ô clip | Phát clip |
| Bấm `COL n` | Phát cả cột trên mọi layer |
| Bấm ô trống | Mở hộp thoại chọn file |
| Bấm file trong **THƯ VIỆN** | Gán vào ô clip đang chọn |
| **Kéo điểm ở ĐƯỜNG RA** | Keystone / mesh warp |
| **Kéo khung ở VÙNG LẤY** | Đổi phần canvas mà slice lấy |
| Tick **Chỉnh mặt nạ** | Chuyển sang kéo nút mặt nạ bezier (F12) |
| Bấm lên đường mặt nạ | Thêm một nút ngay chỗ bấm |
| Chuột phải lên nút | Xoá nút |
| `Ctrl`+kéo nút / `Shift`+bấm nút | Uốn cong / duỗi thẳng |

Bấm ô clip **chọn** chứ không phát: người vận hành phải chỉnh được clip
*sắp* dùng mà không làm gián đoạn clip đang chiếu — bấm nhầm một ô giữa
buổi diễn là khán giả thấy ngay.

### Đưa hình ra máy chiếu

`Output` → `Đưa output ra màn hình 2` — tự đẩy cửa sổ sang màn hình đó rồi
fullscreen, và đồng bộ độ phân giải screen theo màn hình đích.

Không cần kéo cửa sổ bằng tay: thao tác đó dễ làm cửa sổ rơi vào khe giữa
hai màn hình.

| Phím tắt | |
|---|---|
| `Space` | Show Mode — tắt/bật overlay chỉnh sửa |
| `G` | Lưới test card |
| `M` / `O` | Bật/tắt sensor Mock / OSC (cổng 9000) |
| `A` | Auto-calibrate cho Mock |
| `C` | Dấu thập calibration trên máy chiếu |
| `P` | Chấm sensor trên output |
| `F11` | Fullscreen máy chiếu |

---

## Ngôn ngữ giao diện

**Cài đặt → Ngôn ngữ**: Tiếng Việt hoặc English, đổi có hiệu lực ngay.
Lựa chọn được ghi vào `bin/data/settings.json` sau khi bấm **Lưu cài đặt**.

`settings.json` **cố ý không vào git**. Nó mô tả *máy này và người này* —
ngôn ngữ, màn hình nào làm output, ngân sách cache. File project thì mô tả
*một buổi diễn* và đi theo người. Trộn hai thứ vào nhau nghĩa là mở project
của đồng nghiệp sẽ đổi ngôn ngữ giao diện của bạn và đẩy output ra một màn
hình không tồn tại.

Font đóng gói kèm là **Inter** (SIL Open Font License, xem
`bin/data/fonts/Inter-OFL.txt`). Font mặc định của Dear ImGui chỉ có ASCII
và Latin-1 — đó là lý do giao diện trước đây phải viết không dấu.

Thêm ngôn ngữ mới: chép một bảng trong `src/ui/Localization.cpp`, dịch phần
giá trị, thêm vào `enum class Language`. `tests/test_localization.cpp` sẽ
báo ngay nếu thiếu khoá hoặc lệch chuỗi định dạng `%d`/`%s` giữa các bảng.

---

## ⚠️ Video phải là HAP

Đây là nguyên nhân số một khiến show tụt fps: kéo một file `.mp4` vào và
không hiểu vì sao từ 60 xuống 12 fps.

```powershell
../tools/ffmpeg/bin/ffmpeg.exe -i input.mp4 -c:v hap -format hap_q -chunks 8 output.mov
```

Băng thông đo thật, 4K @30fps:

| Nội dung | Codec | 1 luồng | 4 luồng |
|---|---|---:|---:|
| Đồ hoạ / animation | HAP Q | 20 MB/s | 81 MB/s |
| Quay thật, chi tiết cao | HAP Q | 237 MB/s | 949 MB/s |

Nội dung đồ hoạ nén tốt hơn ~12 lần. **NVMe chỉ bắt buộc khi chiếu footage
quay thật độ chi tiết cao** — nội dung mapping điển hình thì SATA SSD đủ.

---

## Kiến trúc

```
app/      AppController — nơi DUY NHẤT bốn tầng gặp nhau
ui/       Dear ImGui — chỉ đọc model và phát lệnh
render/   OpenGL + oF — chỉ đọc model
io/       Thread sensor — không biết model tồn tại
core/     C++20 THUẦN — math, model, calib. Zero GL / oF / ImGui
```

`core/` không phụ thuộc gì ngoài STL. Nhờ vậy toàn bộ toán học mapping và
calibration test được trong ~2 giây mà không cần GPU hay openFrameworks.

Công thức trung tâm:

```
p_content = H_w⁻¹ · H_s · p_sensor
```

`H_s` (calibration sensor) và `H_w` (keystone của slice) là **hai ma trận
tách rời** — chỉnh lại keystone không làm hỏng calibration. Chi tiết ở
`architecture.md` §4.

---

## Giấy phép

Chưa chọn.
