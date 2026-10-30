// ════════════════════════════════════════════════════════════════════════
//  io/packs/PackStore.h — kho gói sensor trên đĩa
//
//  Cấu trúc thư mục dưới <root>/:
//    installed.json            sổ cái gói đã cài (ghi nguyên tử qua .tmp)
//    pending.json              danh sách thư mục cần xoá ở lần khởi động tới
//    staging/<id>-<ver>/       tải tạm, xoá sạch mỗi lần startup()
//    installed/<id>/<ver>/     gói đã kiểm xong + pack.json
//
//  Chỉ STL + core/util/{Json,Sha2} + PackManifest. Mọi đường dẫn nhận/trả ở
//  dạng chuỗi UTF-8 (chuyển qua char8_t sang std::filesystem::path để chạy
//  đúng với tên thư mục tiếng Việt trên Windows).
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include "io/packs/PackManifest.h"

#include <mutex>
#include <string>
#include <vector>

namespace mikmap::packs {

struct InstalledPack {
    PackInfo    entry;
    std::string dir;                      // đường dẫn tuyệt đối UTF-8 tới installed/<id>/<ver>
    bool        pendingRemoval = false;   // sẽ xoá ở lần khởi động kế tiếp
};

class PackStore {
public:
    explicit PackStore(std::string rootDirUtf8);

    /// Dọn staging/, thực hiện xoá các mục trong pending.json, đọc installed.json.
    /// Nếu installed.json hỏng: đổi tên sang installed.json.bad, coi như trống,
    /// ghi chú vào errOut và vẫn trả true để app không sập khi khởi động.
    bool startup(std::string& errOut);

    std::vector<InstalledPack> list() const;
    const InstalledPack*       find(const std::string& id) const;
    bool isInstalled(const std::string& id, const std::string& minVersion = "") const;

    /// Tạo mới (xoá cũ nếu có) staging/<id>-<version> và trả đường dẫn UTF-8.
    std::string stagingDir(const std::string& id, const std::string& version) const;

    /// Kiểm tra kích thước + SHA-256 từng file trong staging/<id>-<ver>, ghi pack.json,
    /// đổi tên nguyên tử sang installed/<id>/<ver> và cập nhật installed.json.
    bool commit(const PackInfo& entry, std::string& errOut);

    /// loadedInProcess=false: thử xoá ngay; nếu lỗi (file đang bị giữ) hoặc
    /// loadedInProcess=true: ghi vào pending.json và đánh dấu pendingRemoval.
    bool remove(const std::string& id, bool loadedInProcess, std::string& errOut);

    /// Nhanh (lúc khởi động): kiểm tra sự tồn tại và kích thước từng file.
    bool quickVerify(const std::string& id, std::string& errOut) const;

    /// Sâu (nút Verify packs): kiểm tra lại SHA-256 từng file.
    bool deepVerify(const std::string& id, std::string& errOut) const;

    const std::string& rootDir() const { return m_root; }

private:
    bool saveInstalledLocked(std::string& errOut) const;
    bool savePendingLocked(std::string& errOut) const;

    std::string                m_root;
    std::vector<InstalledPack> m_installed;
    std::vector<std::string>   m_pendingDirs;   // đường dẫn UTF-8 cần xoá ở lần startup sau
    mutable std::recursive_mutex m_mu;
};

} // namespace mikmap::packs

namespace mikmap {
using PackKind      = packs::Kind;
using PackFile      = packs::FileEntry;
using PackDep       = packs::Dependency;
using PackEntry     = packs::PackInfo;
using PackCatalog   = packs::Catalog;
using InstalledPack = packs::InstalledPack;
using PackStore     = packs::PackStore;
} // namespace mikmap
