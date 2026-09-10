#include "core/util/AppSettings.h"

#include "core/util/Json.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

namespace hexmap {

const char* languageCode(Language l) {
    switch (l) {
    case Language::English: return "en";
    default:                return "vi";
    }
}

Language languageFromCode(const char* code) {
    if (code != nullptr && std::strcmp(code, "en") == 0) return Language::English;
    return Language::Vietnamese;
}

bool AppSettings::load(const std::string& path, std::string& outWarning) {
    outWarning.clear();

    std::ifstream f(path, std::ios::binary);
    if (!f) return true;   // chưa có file = lần chạy đầu, dùng mặc định

    std::ostringstream ss;
    ss << f.rdbuf();

    JsonValue root;
    std::string err;
    if (!JsonValue::parse(ss.str(), root, err)) {
        // Cấu hình hỏng không đáng để chặn người dùng mở phần mềm.
        outWarning = "settings.json hong (" + err + "), dung gia tri mac dinh";
        return false;
    }

    language              = languageFromCode(root["language"].asString("vi").c_str());
    defaultOutputDisplay  = root["defaultOutputDisplay"].asInt(-1);
    vsync                 = root["vsync"].asBool(true);
    mediaCacheBudget      = root["mediaCacheBudget"].asInt(12);
    warnNonHapMedia       = root["warnNonHapMedia"].asBool(true);
    perfLogEnabled        = root["perfLogEnabled"].asBool(true);
    perfLogIntervalSec    = root["perfLogIntervalSec"].asNumber(5.0);
    autoPlayFirstClip     = root["autoPlayFirstClip"].asBool(true);
    lastProjectPath       = root["lastProjectPath"].asString();

    // Kẹp về khoảng dùng được: file sửa tay có thể chứa giá trị vô lý,
    // và cache budget = 0 sẽ làm mọi clip bị dọn ngay sau khi nạp.
    if (mediaCacheBudget < 1)   mediaCacheBudget = 1;
    if (mediaCacheBudget > 256) mediaCacheBudget = 256;
    if (perfLogIntervalSec < 0.5) perfLogIntervalSec = 0.5;

    return true;
}

bool AppSettings::save(const std::string& path, std::string& outError) const {
    outError.clear();

    JsonValue root = JsonValue::object();
    root.set("language",             JsonValue(languageCode(language)));
    root.set("defaultOutputDisplay", JsonValue(defaultOutputDisplay));
    root.set("vsync",                JsonValue(vsync));
    root.set("mediaCacheBudget",     JsonValue(mediaCacheBudget));
    root.set("warnNonHapMedia",      JsonValue(warnNonHapMedia));
    root.set("perfLogEnabled",       JsonValue(perfLogEnabled));
    root.set("perfLogIntervalSec",   JsonValue(perfLogIntervalSec));
    root.set("autoPlayFirstClip",    JsonValue(autoPlayFirstClip));
    root.set("lastProjectPath",      JsonValue(lastProjectPath));

    // Ghi qua file tạm rồi đổi tên — giống ProjectIO. Mất điện giữa lúc
    // ghi thì settings cũ vẫn còn, thay vì thành file cụt khiến lần sau
    // mở lên mất hết tuỳ chỉnh.
    const std::string tmp = path + ".tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) { outError = "Khong mo duoc " + tmp; return false; }
        const std::string text = root.dump(2);
        f.write(text.data(), static_cast<std::streamsize>(text.size()));
        if (!f) { outError = "Loi khi ghi " + tmp; return false; }
    }

    std::remove(path.c_str());
    if (std::rename(tmp.c_str(), path.c_str()) != 0) {
        outError = "Khong doi ten duoc " + tmp;
        return false;
    }
    return true;
}

} // namespace hexmap
