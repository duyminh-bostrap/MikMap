# Sensor packs (P) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Sensor nặng được cài theo nhu cầu từ trong App: tải từ GitHub Releases, kiểm chữ ký Ed25519 và SHA-256, cài vào thư mục cấu hình của máy, gỡ được, nạp lúc chạy; máy chưa cài gói nào vẫn chạy bình thường.

**Architecture:**
- Lõi không phụ thuộc giao diện nằm ở `engine/`: băm và chữ ký trong `engine/core/util`; manifest, kho gói, tải, bộ cài và container `.mikpack` trong `engine/io/packs`. Tất cả chạy ở thread riêng, trạng thái qua atomics.
- `src/` chỉ ghép: một `PackManager` toàn cục, trường `Device::needsPack`, quét lại plugin sau khi cài, và ba chỗ giao diện (Cài đặt › Sensor packs, popup thêm sensor, cảnh báo trên sensor).
- Gói là dữ liệu của máy (`%APPDATA%\MikMap\packs`), không vào `.mikmap`.

**Tech Stack:** C++20, WinHTTP (Windows), `engine/core/util/Json`, `tests/TestHarness.h`, Dear ImGui (`src/`). Không thêm dependency ngoài.

**Spec:** `docs/superpowers/specs/2026-10-30-sensor-packs-design.md`

**Nền:** nhánh `feat/sensor-skeleton-hokuyo`, HEAD `94ba54e`.

## Global Constraints

- Render thread không lock, không cấp phát trong vòng lặp nóng, không chạm đĩa hay mạng: tải, băm, giải container, ghi đĩa đều ở thread nền.
- `engine/core` chỉ STL; `engine/io` không biết Screen, Slice, ImGui.
- P không thêm dependency ngoài; mạng dùng WinHTTP (Windows). Build Linux/macOS của mã mạng mới **chưa kiểm**: phần không phải Windows chỉ cần biên dịch được và trả lỗi "unsupported" khi tải.
- Mã mật mã tự viết (SHA-256, SHA-512, Ed25519) **bắt buộc** có test vector chuẩn (FIPS 180-4, RFC 8032) trước khi dùng. Nếu một task không đạt đủ vector thì dừng và báo, không "gần đúng".
- Nạp DLL chỉ bằng đường dẫn tuyệt đối trong thư mục gói, sau khi gói ở trạng thái đã cài; không tìm theo `PATH`.
- Mọi `path` trong manifest là đường dẫn tương đối, không chứa `..`, tên ổ đĩa, dấu `:` hay bắt đầu bằng `/` hoặc `\`; vi phạm = gói hỏng, từ chối.
- Chữ ký sai, thiếu, hoặc khoá công khai chưa cấu hình ⇒ từ chối toàn bộ, không tải gì, không cài gì.
- Không tự tải khi chưa bấm; mở dự án thiếu gói chỉ báo thiếu.
- Không cập nhật hay gỡ gói khi sensor dùng nó đang chạy. Thư viện đã nạp **không bao giờ được gỡ** trong tiến trình (`PluginLoader.h`): gỡ gói đã nạp = đánh dấu "gỡ khi khởi động lại".
- Thư mục gói và dự án: gói không vào `.mikmap`; chỉ `"needsPack"` (chuỗi id) được lưu theo từng thiết bị, thiếu khoá = không cần gói.
- Đường dẫn là UTF-8 (đường dẫn Việt Nam có dấu); dùng `std::filesystem::u8path`/`u8string`.
- MSVC `/W4 /permissive- /fp:precise /utf-8`, 0 cảnh báo mới. Dùng helper `ui::` có sẵn; hex literal tách chuỗi sau escape nếu ký tự kế tiếp là chữ số hex.
- Build/test:
  ```
  cmake --build build --config Release --target mikmap_tests -j && ctest --test-dir build -C Release --output-on-failure
  cmake --build src/build --config Release -j && ctest --test-dir src/build -C Release --output-on-failure
  ```
- Commit không có dòng attribution / Co-Authored-By.

## Review Focus

1. **Gói giả hoặc bị sửa** (chữ ký danh mục sai, SHA-256 một tệp sai, kích thước lệch, `path` có `..`/tuyệt đối/ADS `a:b`, manifest nhắc tới tệp ngoài danh sách): không tệp nào được nạp và không để lại gì ngoài `.tmp` đã dọn. Test ở Task 3, 6, 7.
2. **Mạng đứt giữa chừng, người dùng huỷ, App tắt khi đang cài:** không có gói nửa vời; lần khởi động sau dọn `.tmp\` và hoạt động bình thường. Test ở Task 4, 5, 6.
3. **Gỡ hoặc cập nhật gói đã nạp trong tiến trình này** (DLL không gỡ được): từ chối nếu có sensor đang chạy dùng nó; nếu chỉ đã nạp thì ghi "gỡ khi khởi động lại" và thực hiện lúc khởi động sau, trước mọi lần nạp. Test ở Task 4, 8.
4. **Dự án cần gói nhưng máy thiếu** (và máy chưa cấu hình khoá công khai): không crash, sensor báo "Chưa cài", không tự tải, `ProjectDirty()` không đổi. Test ở Task 8.
5. **Đĩa đầy, thư mục cấu hình chỉ đọc, đường dẫn có chữ Việt** trong lúc cài: báo lỗi rõ, không để lại gói dở, `installed.json` không hỏng (ghi qua tệp tạm rồi đổi tên). Test ở Task 4.

---

### Task 1: SHA-256 (và SHA-512)

**Files:**
- Create: `engine/core/util/Sha2.h`, `engine/core/util/Sha2.cpp`
- Test: `tests/test_sha2.cpp`

**Interfaces:**
- Produces (namespace `mikmap`):
  ```cpp
  using Sha256Digest = std::array<uint8_t, 32>;
  using Sha512Digest = std::array<uint8_t, 64>;
  class Sha256 { public: Sha256(); void update(const void* data, size_t n); Sha256Digest finish(); };
  class Sha512 { public: Sha512(); void update(const void* data, size_t n); Sha512Digest finish(); };
  Sha256Digest sha256(const void* data, size_t n);
  Sha512Digest sha512(const void* data, size_t n);
  std::string toHex(const uint8_t* d, size_t n);          // lowercase
  bool fromHex(const std::string& hex, uint8_t* out, size_t n);   // false on wrong length / non-hex
  bool sha256File(const std::string& utf8Path, Sha256Digest& out, std::atomic<bool>* cancel = nullptr);   // streaming, 64 KiB chunks
  ```

- [ ] **Step 1: Failing tests** (`test_sha2.cpp`): FIPS 180-4 vectors — `sha256("")` = `e3b0c442…b855`, `sha256("abc")` = `ba7816bf…15ad`, the 448-bit message `abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq` = `248d6a61…0ecf`, one million `a` = `cdc76e5c…0ad0`; `sha512("abc")` = `ddaf35a1…a54ca49f` and `sha512("")` = `cf83e135…927da3e`. Streaming equals one-shot when fed in chunks of 1, 63, 64, 65 bytes. `fromHex`/`toHex` round trip and rejects odd length. `sha256File` of a temp file equals one-shot; a set `cancel` returns false.
- [ ] **Step 2:** Run `ctest` build, expect FAIL (missing header).
- [ ] **Step 3:** Implement per the signatures above (standard block functions; no external code).
- [ ] **Step 4:** Run, expect PASS.
- [ ] **Step 5:** Commit `core: SHA-256 and SHA-512 with the standard test vectors`.

### Task 2: Ed25519 (verify + sign)

**Files:**
- Create: `engine/core/util/Ed25519.h`, `engine/core/util/Ed25519.cpp`
- Test: `tests/test_ed25519.cpp`

**Interfaces:**
- Consumes: `mikmap::sha512` (Task 1).
- Produces:
  ```cpp
  namespace mikmap {
  void ed25519PublicFromSeed(const uint8_t seed[32], uint8_t pub[32]);
  void ed25519Sign(uint8_t sig[64], const uint8_t* msg, size_t n, const uint8_t seed[32]);
  bool ed25519Verify(const uint8_t sig[64], const uint8_t* msg, size_t n, const uint8_t pub[32]);   // strict: rejects non-canonical S (S >= L) and small-order public keys
  }
  ```
  Sign exists for tests and `mikmap_packtool` (Task 7); the App only calls verify.

- [ ] **Step 1: Failing tests:** all five RFC 8032 §7.1 vectors (empty message; 1 byte `72`; 2 bytes `af82`; the 1023-byte vector; SHA-512 of "abc" 64-byte vector): `ed25519PublicFromSeed` gives the listed public key, `ed25519Sign` gives the listed signature byte-for-byte, `ed25519Verify` accepts. Rejections: flipping any single bit of the signature, of the message, or of the public key (loop over all 512 signature bits for vector 1) → false; `S` replaced by `S + L` → false (non-canonical); the all-zero public key → false.
- [ ] **Step 2:** Run, expect FAIL.
- [ ] **Step 3:** Implement using a TweetNaCl-style field arithmetic (GF(2^255−19) as 16×int64 limbs) and extended-coordinate point ops; constant-time not required for verify (public data) but required for the signing path used by tooling — keep it branch-free on secret scalars where cheap, and say so in a header comment. If any RFC vector fails, STOP and report (see Global Constraints).
- [ ] **Step 4:** Run, expect PASS.
- [ ] **Step 5:** Commit `core: Ed25519 sign/verify with the RFC 8032 vectors`.

### Task 3: Manifest, catalog, versions, dependency order

**Files:**
- Create: `engine/io/packs/PackManifest.h`, `engine/io/packs/PackManifest.cpp`
- Test: `tests/test_packmanifest.cpp`

**Interfaces:**
- Consumes: `JsonValue` (`core/util/Json.h`).
- Produces (namespace `mikmap::packs`):
  ```cpp
  enum class Kind { Runtime, Plugin, Model };
  struct FileEntry { std::string path, sha256Hex, url; uint64_t size = 0; };
  struct Dependency { std::string id, minVersion; };
  struct PackInfo { std::string id, name, version; Kind kind = Kind::Plugin; uint64_t sizeBytes = 0;
                    std::vector<FileEntry> files; std::vector<Dependency> needs;   // JSON key is "requires" (a C++20 keyword, so the C++ field is `needs`)
                    std::vector<std::string> provides; };
  struct Catalog { std::vector<PackInfo> packs; };
  bool isSafeRelPath(const std::string& p);                                   // rules in Global Constraints
  bool parsePackInfo(const JsonValue& v, PackInfo& out, std::string& err);    // validates ids ([a-z0-9._-]{1,64}), versions (N.N.N), sha256Hex 64 hex, sizes, safe paths, sizeBytes == sum of sizes
  bool parseCatalog(const std::string& text, Catalog& out, std::string& err);
  int  compareVersions(const std::string& a, const std::string& b);            // -1/0/1, numeric per component
  bool resolveInstallOrder(const Catalog& c, const std::string& id, std::vector<std::string>& order, std::string& err);  // dependencies first; unknown id, missing dependency, version too low, or a cycle = false
  const PackInfo* findPack(const Catalog& c, const std::string& id);
  }
  ```

- [ ] **Step 1: Failing tests:** `isSafeRelPath` true for `a.dll`, `models/pose.onnx`; false for ``, `../x`, `a/../b`, `/abs`, `\\abs`, `C:\x`, `a:stream`, `a//b`, a path containing NUL, `.`; `compareVersions("1.10.0","1.9.0") == 1`, `"1.0.0" < "1.0.1"`; a valid catalog with 3 packs parses; resolveInstallOrder puts the runtime before the pose pack and the model after; a cycle, a missing dependency and `min` above the available version each give false with a clear message; a pack whose file lists `../evil.dll` fails `parsePackInfo`; `sizeBytes` not equal to the sum of file sizes fails; duplicate paths fail; duplicate ids in a catalog fail.
- [ ] **Step 2-4:** RED → implement → GREEN.
- [ ] **Step 5:** Commit `io/packs: manifest and catalog parsing with path-safety and dependency order`.

### Task 4: PackStore (kho gói trên đĩa)

**Files:**
- Create: `engine/io/packs/PackStore.h`, `engine/io/packs/PackStore.cpp`
- Test: `tests/test_packstore.cpp`

**Interfaces:**
- Consumes: `PackInfo` (Task 3), `sha256File` (Task 1).
- Produces:
  ```cpp
  struct InstalledPack { std::string id, version, dir; };
  class PackStore {
  public:
    explicit PackStore(std::string rootDirUtf8);                     // tests pass a temp dir; the App passes ConfigDir()/"packs"
    void startup();                                                  // 1) applyPending() 2) clear .tmp\ 3) drop installed.json entries whose directory is gone
    std::vector<InstalledPack> list() const;
    const InstalledPack* find(const std::string& id) const;          // copy-safe: returns pointer into an internal snapshot valid until the next mutating call
    bool isInstalled(const std::string& id, const std::string& minVersion = "") const;
    std::string stagingDir(const std::string& id) const;             // <root>/.tmp/<id>  (created)
    bool commit(const PackInfo& info, const std::string& stagedDir, std::string& err);   // verifies every file's size AND sha256 against `info` again, then atomically renames to <root>/<id>/<version>/ and rewrites installed.json via temp+rename; on any failure leaves no new dir
    bool remove(const std::string& id, bool inUse, bool loadedInProcess, std::string& err);   // inUse -> refuse; loadedInProcess -> write pending.json and report "removes on next start"; else delete now
    bool quickVerify(const std::string& id, std::string& problem) const;   // pack.json present, listed files exist with listed sizes
    bool deepVerify(const std::string& id, std::string& problem, std::atomic<bool>* cancel = nullptr) const;   // + sha256 of every file
  };
  ```
  `installed.json` = `{"packs":[{"id","version"}]}`; `pending.json` = `{"remove":["id"]}`. A corrupt `installed.json` is treated as empty and renamed `installed.json.bad` (never crash).

- [ ] **Step 1: Failing tests** (temp dir per test, UTF-8 root with Vietnamese letters, e.g. `Gói cài đặt`): commit of a valid staged pack → `isInstalled`, `quickVerify` ok; a staged file with one flipped byte → `commit` false, no `<id>` dir, `installed.json` unchanged; staged file missing/extra size → false; `remove` with `inUse` → false and files intact; `remove` with `loadedInProcess` → true, files still there, `pending.json` lists the id; a new `PackStore` + `startup()` deletes the pending pack; `startup()` removes stale `.tmp\*`; corrupt `installed.json` → empty list and `.bad` file; commit when the root is read-only (set the dir read-only attribute / use an unwritable path) → false with a message and no residue; two versions of one id: committing 1.1.0 over 1.0.0 keeps one active version and removes the old dir after success (or at next start if its files are loaded — covered by the same pending mechanism).
- [ ] **Step 2-4:** RED → implement (`std::filesystem` with `u8path`; write files via temp+`rename`) → GREEN.
- [ ] **Step 5:** Commit `io/packs: PackStore — atomic commit, remove, pending removal, quick and deep verify`.

### Task 5: HTTP tải (WinHTTP) có tiến độ và huỷ

**Files:**
- Create: `engine/io/packs/HttpGet.h`, `engine/io/packs/HttpGet.cpp`, `tests/FakeHttpServer.h`
- Modify: `CMakeLists.txt` (root, line ≈96: `target_link_libraries(mikmap_io PUBLIC ws2_32 iphlpapi winhttp)` under `WIN32`)
- Test: `tests/test_httpget.cpp`

**Interfaces:**
- Produces:
  ```cpp
  namespace mikmap::packs {
  struct HttpOptions { bool allowLoopbackHttp = false; int timeoutMs = 15000; uint64_t maxBytes = 2ull << 30; };
  struct HttpResult { bool ok = false; int status = 0; std::string err; };
  bool httpSupported();                                                       // false off Windows
  HttpResult httpGetToString(const std::string& url, std::string& out, std::atomic<bool>& cancel, const HttpOptions& o = {});   // maxBytes for catalogs is 4 MiB — caller passes it
  HttpResult httpGetToFile(const std::string& url, const std::string& destUtf8, std::atomic<uint64_t>& doneBytes, std::atomic<bool>& cancel, const HttpOptions& o = {});
  }
  ```
  URL rules: scheme must be `https`; `http` only when `allowLoopbackHttp` and the host is `127.0.0.1`/`localhost`; redirects only to `https` (or the same loopback); any other scheme (`file:`, `ftp:`) is rejected before any request.
  `tests/FakeHttpServer.h`: a tiny one-thread loopback HTTP/1.1 server (uses `io/net/Socket.h`) serving a map path→(body, delay per chunk, drop-after-N-bytes, status).

- [ ] **Step 1: Failing tests** (guard with `#ifdef _WIN32`; other platforms assert `!httpSupported()` and an "unsupported" error): GET of a 300 KiB body to string/file with exact bytes and `doneBytes` reaching the size; `cancel` set mid-transfer stops within 500 ms and leaves no destination file (`*.part` removed by the caller — assert the function itself deletes its partial destination on failure); a server that drops the connection after 100 KiB → `ok == false` with an error and no destination file; 404 → `ok == false`, `status == 404`; body larger than `maxBytes` → false; `https` to a loopback server with `allowLoopbackHttp` false and `http://127.0.0.1` → rejected without connecting; `file:///C:/x`, `ftp://…`, `http://example.com` → rejected before any connection.
- [ ] **Step 2-4:** RED → implement with WinHTTP (`WinHttpOpen`, `WinHttpConnect`, `WinHttpOpenRequest`, read loop in 64 KiB chunks checking `cancel`; secure flags default; no certificate-ignore flags) → GREEN.
- [ ] **Step 5:** Commit `io/packs: WinHTTP download with progress, cancel and a strict URL policy`.

### Task 6: PackInstaller (danh mục → tải → kiểm → cài)

**Files:**
- Create: `engine/io/packs/PackInstaller.h`, `engine/io/packs/PackInstaller.cpp`
- Test: `tests/test_packinstaller.cpp`

**Interfaces:**
- Consumes: Tasks 1–5 (`ed25519Verify`, `parseCatalog`, `resolveInstallOrder`, `PackStore`, `httpGetToString/File`).
- Produces:
  ```cpp
  namespace mikmap::packs {
  enum class InstallPhase { Idle, FetchingCatalog, Downloading, Verifying, Committing, Done, Failed, Cancelled };
  struct InstallSnapshot { InstallPhase phase = InstallPhase::Idle; std::string packId, currentFile, error;
                           uint64_t doneBytes = 0, totalBytes = 0; int filesDone = 0, filesTotal = 0; };
  class PackInstaller {
  public:
    PackInstaller(PackStore& store, const uint8_t publicKey[32], std::string catalogUrl, std::string catalogSigUrl, HttpOptions http = {});
    ~PackInstaller();                                   // cancels and joins the worker
    bool keyConfigured() const;                         // false when the key is all zeros / not set → every operation fails with "Pack signing key is not configured"
    void refreshCatalogAsync();                         // fetch catalog.json + catalog.sig, verify, parse; result in catalog()
    bool catalog(Catalog& out) const;                   // false until a verified catalog was loaded
    void installAsync(const std::string& packId);       // dependencies first (resolveInstallOrder); one worker thread; refused while another install runs
    void installFromFileAsync(const std::string& mikpackPath);   // Task 7 container; same verification path
    void cancel();
    InstallSnapshot snapshot() const;                   // lock-free read of atomics + a mutex-guarded string copy; safe from the render thread (copies two short strings)
  };
  }
  ```
  Order of operations for one pack: download each file to `store.stagingDir(id)/<path>.part` → size check → SHA-256 → rename `.part` → `store.commit(info, stagedDir)`. The signature is checked over the **exact bytes** of `catalog.json` before parsing. `catalog.sig` is the raw 64-byte signature (or its 128 lowercase hex characters — accept both).

- [ ] **Step 1: Failing tests** (use `FakeHttpServer`, a test keypair from a fixed 32-byte seed, `PackStore` in a temp dir; `HttpOptions.allowLoopbackHttp = true`): happy path installs a runtime + a plugin pack that requires it (order: runtime first, `store.isInstalled` both, files byte-exact, phase `Done`); wrong signature (one bit of `catalog.sig` flipped) → `Failed`, nothing downloaded (server request count for pack files == 0); unsigned catalog (404 on `.sig`) → `Failed`; catalog changed by one byte after signing → `Failed`; one file with a wrong SHA-256 → `Failed`, `store.isInstalled` false, `.tmp` cleaned; wrong size → `Failed`; connection dropped mid-file → `Failed`, no pack dir; `cancel()` during download → `Cancelled` within 1 s, `.tmp` cleaned, destructor joins without hanging; two `installAsync` calls at once → the second is refused (not queued silently) with a message; `keyConfigured() == false` (zero key) → every call `Failed("… not configured")` and no network request; progress: `doneBytes` is monotonic and ends at `totalBytes`; `snapshot()` hammered from another thread during an install is race-free (run under the normal test, no TSAN required, but no shared non-atomic reads).
- [ ] **Step 2-4:** RED → implement → GREEN.
- [ ] **Step 5:** Commit `io/packs: PackInstaller — verified catalog, ordered install with progress and cancel`.

### Task 7: Container `.mikpack` và `mikmap_packtool`

**Files:**
- Create: `engine/io/packs/MikPack.h`, `engine/io/packs/MikPack.cpp`, `tools/packs/packtool.cpp`
- Modify: `CMakeLists.txt` (target `mikmap_packtool` linking `mikmap_io`; not part of `ctest`)
- Modify: `engine/io/packs/PackInstaller.cpp` (`installFromFileAsync`)
- Test: `tests/test_mikpack.cpp`

**Interfaces:**
- Produces:
  ```cpp
  // .mikpack = "MIKPACK1" | u32le entryLen | entry.json | 64-byte Ed25519 signature of entry.json | u32le fileCount |
  //            per file: u32le pathLen | path | u64le size | bytes ...      (uncompressed on purpose: no zip dependency)
  // entry.json is a PackInfo (Task 3) with files[].url omitted; the signature is made with the same key as the catalog.
  bool writeMikPack(const std::string& outPath, const PackInfo& info, const std::string& entryJson, const uint8_t sig[64], const std::string& sourceDir, std::string& err);
  struct MikPackReader { bool open(const std::string& path, std::string& err); const std::string& entryJson() const; const uint8_t* signature() const;
                         bool extractTo(const std::string& dir, std::atomic<bool>* cancel, std::string& err); };   // validates every path with isSafeRelPath BEFORE writing; stops at 2 GiB total; entry order must match files in entry.json
  ```
  `mikmap_packtool` subcommands: `keygen <out-seed-file> <out-pubkey-hex-file>`; `sign <seed-file> <file> <out-sig-file>`; `pack <seed-file> <pack-dir-with-pack.json> <out.mikpack>` (computes SHA-256/sizes into the entry, signs, writes); `catalog <seed-file> <catalog.json> <out-sig>` (signs an existing catalog file). It never prints the seed.

- [ ] **Step 1: Failing tests:** write then read a `.mikpack` with 2 files → extracted bytes equal; a truncated file, bad magic, `fileCount` mismatch, an entry path of `../x` or `C:\x`, an oversized declared `size`, and a zero-length entry all fail `open`/`extractTo` without writing outside `dir`; `installFromFileAsync` with a correctly signed pack installs it (phase `Done`), with one flipped byte in a payload file → `Failed` and nothing installed, with a wrong signature → `Failed`, with a zero key → `Failed`.
- [ ] **Step 2-4:** RED → implement → GREEN. Build `mikmap_packtool` once and run `keygen` + `pack` + the installer on its output in a test (call the library functions the tool uses; do not spawn the process).
- [ ] **Step 5:** Commit `io/packs: .mikpack container for offline install, and mikmap_packtool (keygen, sign, pack)`.

### Task 8: Ghép vào App (PackManager, `needsPack`, nạp plugin)

**Files:**
- Create: `src/packs.cpp`
- Modify: `src/app.h` (`Device::needsPack`, `PackManager` declarations), `src/project.cpp` (serialize `"needsPack"`; `ConfigDir()` is `static` at ≈521 — expose `std::string PacksDir()`), `src/sources.cpp` (`SensorPlugins()` ≈189 is a once-only static scan; `SensorsTick` marks missing packs), `src/main.cpp` (startup/shutdown calls), `src/CMakeLists.txt` (pass two CMake cache variables as compile definitions, both default empty: `MIKMAP_PACK_PUBKEY_HEX` — the 64-hex-char Ed25519 public key — and `MIKMAP_PACK_CATALOG_URL`, e.g. `https://github.com/<owner>/<repo>/releases/latest/download/catalog.json`; the signature URL is the catalog URL with `.sig` appended. Either one empty ⇒ `keyConfigured()`/the URL check fails and every pack operation reports "Pack signing key or catalog URL is not configured")
- Test: `SensorSelfTest` in `src/sources.cpp`

**Interfaces:**
- Consumes: `PackStore`, `PackInstaller`, `Catalog` (Tasks 3–7); `scanPluginDir` (`engine/io/plugin`).
- Produces (`src/app.h`):
  ```cpp
  enum class PackState { NotInstalled, Downloading, Installed, Failed, UpdateAvailable, Unavailable /* no key or no catalog yet */ };
  struct PackRow { std::string id, name, version, installedVersion, error; uint64_t sizeBytes = 0; PackState state = PackState::Unavailable;
                   bool inUse = false, loaded = false, pendingRemoval = false; float progress = 0; };
  void PacksStartup();                                 // PackStore::startup(), apply pending, construct installer, load installed packs' plugin dirs
  void PacksShutdown();
  std::vector<PackRow> PackRows();                     // from the verified catalog + installed list + installer snapshot; no I/O (cached)
  void PackRefreshCatalog();                           // user-initiated only
  void PackInstall(const std::string& id);             // user-initiated; refused with a toast while another install runs
  void PackInstallFromFile(const std::string& path);
  void PackCancel();
  bool PackRemove(const std::string& id);              // InUse (a running device with needsPack == id, or a device whose plugin comes from that pack) → toast + false; loaded → pending removal
  bool PackAvailableForDevice(const Device& d);        // d.needsPack empty || isInstalled(d.needsPack)
  PackState PackStateFor(const std::string& id);
  void RescanSensorPlugins();                          // adds plugins from newly installed pack dirs; already-loaded paths are never loaded twice
  ```
  `Device::needsPack` is set by the factory for the sensor kind (none yet in P: only the unit tests set it); serialized `"needsPack"` (absent = ""); config, part of undo snapshots; ticks never touch it.

- [ ] **Step 1: Failing self-tests** (use a temp `PackStore`, a fake catalog server and a test keypair through the same install path): (a) `needsPack` round trips and an old file without the key loads `""`; (b) a device with `needsPack = "pose-onnx"` on a machine without the pack: `SensorsTick` sets status to "not installed" (a dedicated `d.error` text `"Needs pack: <id>"`), starts no source, makes no network request, and `ProjectDirty()` stays false across two ticks (**Review Focus 4**); (c) with an empty `MIKMAP_PACK_PUBKEY_HEX` every `PackInstall*` fails with the "not configured" text and `PackRows()` shows `Unavailable`; (d) install through `PackInstall` → `PackAvailableForDevice` true and the device starts connecting on the next tick without a restart; (e) `PackRemove` while a device using it is enabled/running → false and files intact; remove while idle but with a plugin already loaded → `pendingRemoval` true and the files stay until `PacksStartup()` runs again (**Review Focus 3**); (f) `RescanSensorPlugins` after installing a plugin pack makes it appear in `SensorPlugins()` once, a second call does not duplicate it; (g) no `PackInstall` call happens anywhere without an explicit user action (grep-level guard in the test: `PacksStartup` and `SensorsTick` make zero `httpGet*` calls — count via the fake server's request counter).
- [ ] **Step 2-4:** RED → implement (`PackManager` is a singleton holding `PackStore` + `PackInstaller`; `PackRows()` builds from a snapshot refreshed when the installer's phase changes, never by scanning disk per frame) → GREEN.
- [ ] **Step 5:** Commit `packs: App-side PackManager, Device::needsPack, plugin rescan and pending removal`.

### Task 9: Giao diện — Cài đặt › Sensor packs

**Files:**
- Modify: `src/settings.cpp` (new section/tab "Sensor packs"), `src/ui.h`/`src/ui.cpp` only if a progress-bar helper does not exist (check first; reuse any existing bar)
- Test: `SensorSelfTest` for the pure row-state helper; `--shot`

**Interfaces:**
- Consumes: `PackRows`, `PackRefreshCatalog`, `PackInstall`, `PackInstallFromFile`, `PackCancel`, `PackRemove` (Task 8).
- Produces: `const char* PackActionLabel(const PackRow&)` (pure): `NotInstalled → "Install"`, `Downloading → "Cancel"`, `Installed → "Remove"`, `UpdateAvailable → "Update"`, `Failed → "Retry"`, `Unavailable → ""`; `bool PackActionEnabled(const PackRow&)` (false for Remove/Update when `inUse`, with the reason text from `PackActionHint(const PackRow&)`: `"Turn the sensor off first"`, `"Removes when the app restarts"` for pending).

**Layout:** a list with one row per pack: name, version, size (`"180 MB"` with one decimal, from `sizeBytes`), state chip (not installed / `n%` / installed / error / update), the action button, and an error line in red; under the list: `Refresh`, `Install from file…` (a native open-file dialog if the app already has one — check `src/` for the existing file-dialog helper and reuse it; if none exists, a path text field), `Verify packs` (runs `deepVerify` on a thread and reports per pack). The size is always visible **before** any download starts. A refused action (another install running, key not configured) shows its reason inline, never silently. Not configured: a single dim line "Pack signing key is not configured in this build — packs cannot be installed".

- [ ] **Step 1: Failing self-tests:** `PackActionLabel`/`PackActionEnabled`/`PackActionHint` for every state × (`inUse`, `pendingRemoval`) combination; a size formatter `FormatBytes(uint64_t)` (`999 → "999 B"`, `1536 → "1.5 KB"`, `1.5e9 → "1.4 GB"`, `0 → "0 B"`).
- [ ] **Step 2-4:** RED → implement → GREEN.
- [ ] **Step 5:** Screenshots with `--shot` of the Settings section in three states using fake rows (add a test flag `--packs-demo` that seeds `PackRows` with: not installed 180 MB, downloading 42 %, installed, failed with an error, update available, and the not-configured case); LOOK at them (Read) and report. Real downloads, native dialogs and clicks are not exercised by hand.
- [ ] **Step 6:** Commit `settings: Sensor packs page — install, cancel, remove, update, verify, install from file`.

### Task 10: Giao diện — popup thêm sensor và cảnh báo trên sensor

**Files:**
- Modify: `src/sensor.cpp` (add-sensor popup step 1 cards; Properties › Connection band), `src/mapping.cpp` (`TreeRow` chip step 1 for a missing pack; tooltip)
- Test: `SensorSelfTest`; `--shot`

**Interfaces:**
- Consumes: `PackStateFor`, `PackRows`, `PackInstall`, `PackAvailableForDevice` (Task 8), `PackActionLabel` (Task 9).
- Produces: `const char* SensorPackHint(const Device&, PackState)` (pure; `nullptr` when the sensor needs no pack or the pack is installed; otherwise `"Needs pack: <name> — Install (<size>)"`, `"Installing <name> 42%"`, or `"Pack install failed — Retry"`).

**Behaviour:**
- Popup step 1: a sensor card whose pack is not installed shows "Not installed · <size>" and an **Install** button; pressing it starts `PackInstall` and shows a progress bar on the card with **Cancel**; when `Installed` the card becomes a normal card and **Next** is enabled; the card can't be chosen while `NotInstalled`/`Downloading`. Closing the popup mid-download keeps downloading (progress still visible in Settings).
- Tree chip: a device whose `needsPack` is not installed gets chip step 1 forced to "not reached" with an orange first dot (`pal::coral`-adjacent existing orange token — reuse the token already used for warnings; do not add a new colour) and the tooltip from `SensorPackHint`; clicking the chip opens Settings › Sensor packs.
- Properties › Connection: a banner at the top of the section while the pack is missing: text from `SensorPackHint` + **Install** button; when the install completes the sensor connects on its own (Task 8 (d)).
- No auto-download: nothing in these views calls `PackInstall` except the buttons.

- [ ] **Step 1: Failing self-tests:** `SensorPackHint` for no-pack / installed / not installed / downloading / failed; `SensorProgress` (existing function) reports 0 for a device whose pack is missing even when it is `Running`-like (guard against a stale status); the tree node's `prog`/`err` fields for a pack-missing device (`BuildTree` output) show step 0 and a flag the row uses for the orange dot.
- [ ] **Step 2-4:** RED → implement → GREEN.
- [ ] **Step 5:** `--shot`s (use `--packs-demo` plus a fake device with `needsPack`): popup step 1 with a not-installed card and a downloading card; a tree row with the missing-pack chip; Properties › Connection banner. LOOK at them and report; clicks, real downloads and Esc/Cancel behaviour are covered by tests only.
- [ ] **Step 6:** Commit `sensor: missing-pack cards, chip and banner; install straight from the add-sensor popup`.

### Task 11: Tài liệu

**Files:** `features.md`, `README.md`, `.claude/CLAUDE.md`, `.claude/rules/design.md`, `docs/superpowers/specs/2026-10-30-sensor-packs-design.md` (only if a decision changed)

- [ ] **Step 1:** Grep `src/*.cpp` and `engine/io/packs/*` to confirm each of `PacksStartup`, `PackInstall`, `PackRows`, `RescanSensorPlugins`, `PackActionLabel`, `SensorPackHint` is called on the app path.
- [ ] **Step 2:** Add a features row for the pack system and an honest "Ghi chú kiểm tra": **chưa kiểm** với GitHub Releases thật, máy sạch, WinHTTP qua proxy/tường lửa, native file dialog và bấm bằng chuột; build Linux/macOS (WinHTTP bị stub); khoá ký chưa được cấu hình trong build mặc định nên cài gói bị tắt cho tới khi chủ repo chạy `mikmap_packtool keygen` và đặt `MIKMAP_PACK_PUBKEY_HEX`; quy trình phát hành gói và bộ cài là việc sau. Ghi rõ kiểu mã mật mã tự viết đã qua vector chuẩn nào.
- [ ] **Step 3:** `design.md` "Thao tác chuẩn": Cài đặt › Sensor packs; thẻ chưa cài trong popup thêm sensor; chip cam khi thiếu gói.
- [ ] **Step 4:** Commit `docs: sensor packs`.

---

## Kiểm cuối

- Hai bộ test xanh, chạy mỗi bộ hai lần; 0 cảnh báo mới `/W4`.
- Vector chuẩn SHA-256/512 và Ed25519 đều đạt (Task 1–2); không có thử nghiệm mật mã nào bị bỏ qua.
- Một vòng thật trên loopback: `mikmap_packtool keygen` → `pack` → `catalog` → fake server → `PackInstaller` cài và nạp được plugin giả (`mikmap_fake_scan_plugin`) từ thư mục gói, rồi sensor `Plugin` kết nối.
- Agent `reviewer` của repo duyệt toàn nhánh; nêu rõ phần chưa kiểm với mạng và máy thật.
