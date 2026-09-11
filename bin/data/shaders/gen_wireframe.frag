#version 150

// ════════════════════════════════════════════════════════════════════════
//  gen_wireframe.frag — "Geometric Wireframe"
//
//  Chuyển từ Layer 2 của bản thiết kế tham khảo (LiveCanvas.tsx): năm
//  lục giác lồng nhau, bán kính 40·i + sin(t·1.5 + i)·15, vòng chẵn xoay
//  thuận 0.3·t và vòng lẻ xoay ngược — chính sự ngược chiều đó tạo cảm
//  giác lồng vào nhau chứ không phải một khối nở ra co vào.
//
//  Bản web dùng ctx.stroke() nên có nét sẵn. Trong fragment shader phải
//  tự dựng: hàm khoảng cách có dấu tới lục giác, nét = dải quanh mức 0.
// ════════════════════════════════════════════════════════════════════════

uniform vec2  uRes;
uniform float uTime;

out vec4 fragColor;

const vec3 kStroke = vec3(0.067, 0.541, 0.698);   // #118AB2
const vec3 kBg     = vec3(0.0196);                // #050505

// Khoảng cách có dấu tới lục giác đều bán kính r (đỉnh hướng lên).
float sdHexagon(vec2 p, float r) {
    const vec3 k = vec3(-0.8660254, 0.5, 0.5773503);
    p = abs(p);
    p -= 2.0 * min(dot(k.xy, p), 0.0) * k.xy;
    p -= vec2(clamp(p.x, -k.z * r, k.z * r), r);
    return length(p) * sign(p.y);
}

void main() {
    float scale = uRes.y / 1080.0;
    vec2  px    = (gl_FragCoord.xy - uRes * 0.5) / scale;   // toạ độ 1080p

    vec3  col = kBg;
    float lw  = 2.0;   // lineWidth = 2 như bản thiết kế

    for (int i = 1; i <= 5; ++i) {
        float fi  = float(i);
        float rad = 40.0 * fi + sin(uTime * 1.5 + fi) * 15.0;

        // Vòng chẵn xoay một chiều, vòng lẻ xoay chiều ngược lại.
        float ang = uTime * (mod(fi, 2.0) == 0.0 ? 0.3 : -0.3);
        float c   = cos(ang), s = sin(ang);
        vec2  q   = mat2(c, -s, s, c) * px;

        float d = abs(sdHexagon(q, rad)) - lw * 0.5;

        // fwidth cho nét mảnh đều ở mọi độ phân giải mà không răng cưa.
        float a = 1.0 - smoothstep(0.0, fwidth(d) + 0.5, d);

        // Vòng ngoài mờ dần: chiều sâu đọc ra ngay, không cần thêm gì.
        col += kStroke * a * (1.0 - 0.10 * fi);
    }

    fragColor = vec4(col, 1.0);
}
