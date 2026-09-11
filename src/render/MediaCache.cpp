#include "render/MediaCache.h"

#include <algorithm>
#include <vector>

namespace hexmap {

ofTexture* MediaCache::Entry::texture() {
    if (video) {
        ofTexture* t = video->getTexture();
        return (t && t->isAllocated()) ? t : nullptr;
    }
    if (image && image->isAllocated()) return &image->getTexture();
    if (fbo && fbo->isAllocated()) return &fbo->getTexture();
    return nullptr;
}

MediaCache::Entry* MediaCache::peek(const MediaRef& ref) {
    if (ref.isEmpty() || ref.path.empty()) return nullptr;
    const auto it = m_entries.find(ref.path);
    return (it == m_entries.end()) ? nullptr : it->second.get();
}

MediaCache::Entry* MediaCache::acquire(const MediaRef& ref) {
    if (ref.isEmpty() || ref.path.empty()) return nullptr;

    const auto it = m_entries.find(ref.path);
    if (it != m_entries.end()) {
        it->second->lastUsedFrame = m_frameCounter;
        return it->second.get();
    }
    return load(ref);
}

MediaCache::Entry* MediaCache::load(const MediaRef& ref) {
    auto entry = std::make_unique<Entry>();
    entry->lastUsedFrame = m_frameCounter;

    const std::string path = ofToDataPath(ref.path, true);

    if (ref.type == MediaType::Video) {
        auto player = std::make_unique<ofxHapPlayer>();
        if (player->load(ref.path) || player->load(path)) {
            player->setLoopState(OF_LOOP_NORMAL);
            entry->size = Vec2{static_cast<double>(player->getWidth()),
                               static_cast<double>(player->getHeight())};
            entry->durationSec = static_cast<double>(player->getDuration());
            entry->video = std::move(player);
            entry->loaded = true;
        } else {
            entry->failed = true;
            // Thông báo cụ thể: nguyên nhân số 1 khiến show bị tụt fps là
            // người vận hành kéo nhầm file .mp4 vào (mục I9).
            entry->errorText = "Khong phat duoc: " + ref.path
                             + "  — file co phai HAP khong? Dung tools/encode_hap.ps1";
        }

    } else if (ref.type == MediaType::Generator) {
        // Generator không có file để nạp — chỉ cần một FBO để vẽ vào.
        //
        // 1280×720 chứ không phải 4K: đây là nội dung SINH RA, phóng lên
        // bao nhiêu cũng không vỡ thành pixel như video, trong khi một
        // FBO 4K tốn 33 MB VRAM và phải tô lại toàn bộ mỗi frame.
        if (!isKnownGenerator(ref.path)) {
            entry->failed = true;
            entry->errorText = "Khong co generator ten: " + ref.path;
        } else {
            auto fbo = std::make_unique<ofFbo>();
            fbo->allocate(1280, 720, GL_RGBA);
            fbo->begin();
            ofClear(5, 5, 5, 255);
            fbo->end();

            entry->size = Vec2{1280.0, 720.0};
            entry->generatorId = ref.path;
            entry->fbo = std::move(fbo);
            entry->loaded = true;
        }

    } else if (ref.type == MediaType::Image) {
        auto img = std::make_unique<ofImage>();
        if (img->load(ref.path) || img->load(path)) {
            entry->size = Vec2{static_cast<double>(img->getWidth()),
                               static_cast<double>(img->getHeight())};
            entry->image = std::move(img);
            entry->loaded = true;
        } else {
            entry->failed = true;
            entry->errorText = "Khong mo duoc anh: " + ref.path;
        }
    }

    Entry* raw = entry.get();
    m_entries[ref.path] = std::move(entry);
    return raw;
}

void MediaCache::update() {
    ++m_frameCounter;

    const float now = static_cast<float>(ofGetElapsedTimef());

    for (auto& kv : m_entries) {
        Entry& e = *kv.second;

        // ★ Chỉ vẽ lại generator ĐANG ĐƯỢC DÙNG ở frame này. Generator
        //   còn nằm trong cache nhưng không clip nào phát thì vẽ lại chỉ
        //   tốn GPU — và với tám generator trong cache thì cái giá đó
        //   không hề nhỏ.
        if (e.fbo && !e.generatorId.empty()
            && e.lastUsedFrame >= m_frameCounter - 1) {
            m_generators.render(e.generatorId, *e.fbo, now);
            continue;
        }

        if (!e.video) continue;

        e.video->update();

        // ofxHapPlayer tra ve kich thuoc = 0 ngay sau load(), truoc khi
        // giai ma xong frame dau. Doc lai khi da co so that — neu khong,
        // PerfPanel se bao "VRAM 0 KB" du dang phat video 4K, va mot
        // bang so lieu sai la bang so lieu vo dung.
        if (e.size.x <= 0.0 && e.video->getWidth() > 0.0f) {
            e.size = Vec2{static_cast<double>(e.video->getWidth()),
                          static_cast<double>(e.video->getHeight())};
            e.durationSec = static_cast<double>(e.video->getDuration());
        }
    }
}

void MediaCache::syncTransport(const Clip& clip) {
    Entry* e = peek(clip.media);
    if (e == nullptr || !e->video) return;

    ofxHapPlayer& p = *e->video;

    // core/ quyết định trạng thái; ở đây chỉ áp xuống player thật.
    const bool shouldPlay = clip.transport.isPlaying();
    if (shouldPlay && p.isPaused())      p.setPaused(false);
    if (!shouldPlay && !p.isPaused())    p.setPaused(true);

    p.setSpeed(static_cast<float>(clip.transport.speed));

    // Chỉ seek khi lệch đáng kể. Seek mỗi frame làm decoder phải nạp lại
    // buffer liên tục và giết sạch hiệu năng — đây là bẫy dễ dính nhất
    // khi nối máy trạng thái vào player thật.
    constexpr float kSeekThreshold = 0.02f;   // 2% độ dài clip
    const float want = static_cast<float>(clip.transport.position);
    const float have = p.getPosition();
    if (std::abs(want - have) > kSeekThreshold) {
        p.setPosition(want);
    }
}

void MediaCache::collectGarbage() {
    if (static_cast<int>(m_entries.size()) <= m_budget) return;

    // Xếp theo lần dùng gần nhất, giải phóng những cái cũ nhất.
    std::vector<std::pair<int, std::string>> byAge;
    byAge.reserve(m_entries.size());
    for (const auto& kv : m_entries) {
        byAge.emplace_back(kv.second->lastUsedFrame, kv.first);
    }
    std::sort(byAge.begin(), byAge.end());

    const int toRemove = static_cast<int>(m_entries.size()) - m_budget;
    for (int i = 0; i < toRemove; ++i) {
        // Không dọn thứ vừa dùng ở frame này — nó đang được vẽ.
        if (byAge[static_cast<size_t>(i)].first >= m_frameCounter) break;
        m_entries.erase(byAge[static_cast<size_t>(i)].second);
        ++m_evictions;
    }
}

void MediaCache::clear() {
    m_entries.clear();
}

int MediaCache::loadedCount() const {
    int n = 0;
    for (const auto& kv : m_entries) {
        if (kv.second->loaded) ++n;
    }
    return n;
}

size_t MediaCache::estimatedVramBytes() const {
    size_t total = 0;
    for (const auto& kv : m_entries) {
        if (!kv.second->loaded) continue;
        // HAP giữ texture nén DXT/BC ~1 byte/pixel; ảnh và FBO RGBA là 4.
        const double px = kv.second->size.x * kv.second->size.y;
        total += static_cast<size_t>(px * (kv.second->video ? 1.0 : 4.0));
    }
    return total;
}

} // namespace hexmap
