// ════════════════════════════════════════════════════════════════════════
//  render/GeneratorBank.h — nguồn hình SINH BẰNG SHADER (MediaType::Generator)
//
//  ── Vì sao cần ────────────────────────────────────────────────────────
//  Lưới clip vô dụng nếu chưa ai bỏ file vào. Muốn thử toàn bộ chuỗi
//  composition → mapping → máy chiếu, người dùng phải đi kiếm cho ra vài
//  file HAP trước — trong khi thứ họ cần chỉ là "có cái gì đó đang chạy".
//
//  Generator là nội dung KHÔNG CẦN FILE: một fragment shader vẽ thẳng vào
//  FBO, dùng được ngay như một clip bình thường.
//
//  Danh mục (mã + nhãn) nằm ở core/model/Generators.h để ui/ cũng đọc
//  được mà không phải phụ thuộc tầng render.
//
//  ── Ràng buộc luồng ──────────────────────────────────────────────────
//  CHỈ render thread được gọi lớp này. Nó chạm vào GL context.
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include "core/model/Generators.h"

#include "ofMain.h"

#include <map>
#include <memory>
#include <string>

namespace hexmap {

class GeneratorBank {
public:
    /// Vẽ một frame của generator vào `target`.
    ///
    /// `target` phải đã allocate. Shader được nạp lần đầu khi cần và giữ
    /// lại — biên dịch lại mỗi frame thì mỗi generator tốn vài ms.
    /// Không nạp được shader thì vẽ nền đen, KHÔNG để nguyên rác của FBO
    /// (rác GPU trên máy chiếu là thứ khán giả sẽ thấy).
    void render(const std::string& id, ofFbo& target, float timeSec);

private:
    ofShader* shaderFor(const std::string& id);

    std::map<std::string, std::unique_ptr<ofShader>> m_shaders;
};

} // namespace hexmap
