#include "render/GeneratorBank.h"

namespace hexmap {

ofShader* GeneratorBank::shaderFor(const std::string& id) {
    const auto it = m_shaders.find(id);
    if (it != m_shaders.end()) {
        return it->second ? it->second.get() : nullptr;
    }

    // Một lần thử duy nhất. Nạp hỏng thì ghi nullptr vào bảng để những
    // frame sau không thử lại — shader hỏng thử lại 60 lần/giây sẽ ngập
    // log và giết fps, mà kết quả vẫn hỏng.
    auto sh = std::make_unique<ofShader>();
    const std::string frag = "shaders/gen_" + id + ".frag";
    if (!sh->load("shaders/generator.vert", frag)) {
        ofLogError("GeneratorBank") << "khong nap duoc shader: " << frag;
        m_shaders[id] = nullptr;
        return nullptr;
    }

    ofShader* raw = sh.get();
    m_shaders[id] = std::move(sh);
    return raw;
}

void GeneratorBank::render(const std::string& id, ofFbo& target, float timeSec) {
    if (!target.isAllocated()) return;

    ofShader* sh = shaderFor(id);

    target.begin();
    ofClear(5, 5, 5, 255);
    if (sh != nullptr) {
        const float w = static_cast<float>(target.getWidth());
        const float h = static_cast<float>(target.getHeight());

        sh->begin();
        sh->setUniform2f("uRes", w, h);
        sh->setUniform1f("uTime", timeSec);
        ofDrawRectangle(0.0f, 0.0f, w, h);
        sh->end();
    }
    target.end();
}

} // namespace hexmap
