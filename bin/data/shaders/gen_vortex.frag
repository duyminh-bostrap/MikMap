#version 150

// ════════════════════════════════════════════════════════════════════════
//  gen_vortex.frag — "Particle Vortex"
//
//  Chuyển từ Layer 3 của bản thiết kế tham khảo (LiveCanvas.tsx): ba
//  dải sóng chạy ngang (cam / vàng / lục) cộng 24 đốm sáng quay quanh
//  tâm.
//
//  Công thức sóng giữ NGUYÊN của bản web:
//      y = sin(x·0.008 + t·2 + offset) · cos(x·0.003 + t) · (40 + 15·i)
//  Hai hàm nhân nhau là thứ làm dải sóng phình ra thắt lại dọc theo
//  chiều ngang thay vì chạy đều — bỏ cos đi là mất hẳn nét riêng.
//
//  Đốm sáng: góc = p·π/12 + t·0.5, bán kính = 120 + sin(t·2 + p)·60.
// ════════════════════════════════════════════════════════════════════════

uniform vec2  uRes;
uniform float uTime;

out vec4 fragColor;

const vec3 kBg     = vec3(0.0196);                // #050505
const vec3 kSpark  = vec3(1.0,   0.820, 0.400);   // #FFD166

vec3 waveColor(int i) {
    if (i == 0) return vec3(1.0,   0.498, 0.314);   // #FF7F50
    if (i == 1) return vec3(1.0,   0.820, 0.400);   // #FFD166
    return             vec3(0.024, 0.839, 0.627);   // #06D6A0
}

void main() {
    float scale = uRes.y / 1080.0;
    vec2  px    = (gl_FragCoord.xy - uRes * 0.5) / scale;   // toạ độ 1080p

    vec3 col = kBg;

    // ── Ba dải sóng ────────────────────────────────────────────────────
    for (int i = 0; i < 3; ++i) {
        float fi  = float(i);
        float amp = 40.0 + fi * 15.0;
        float y   = sin(px.x * 0.008 + uTime * 2.0 + fi * 0.8)
                  * cos(px.x * 0.003 + uTime)
                  * amp;

        float d = abs(px.y - y) - 1.25;              // lineWidth 2.5
        float a = 1.0 - smoothstep(0.0, fwidth(d) + 0.8, d);

        col += waveColor(i) * a;
    }

    // ── 24 đốm sáng quay quanh tâm ─────────────────────────────────────
    for (int p = 0; p < 24; ++p) {
        float fp   = float(p);
        float ang  = fp * 0.2617994 + uTime * 0.5;   // π/12
        float dist = 120.0 + sin(uTime * 2.0 + fp) * 60.0;

        vec2  c = vec2(cos(ang), sin(ang)) * dist;
        float d = length(px - c) - 2.5;
        float a = 1.0 - smoothstep(0.0, fwidth(d) + 1.0, d);

        // Quầng mềm quanh đốm: trên nền gần đen, một chấm 2.5px đặc
        // trông như bụi bẩn màn hình chứ không như hạt sáng.
        float halo = exp(-length(px - c) / 14.0) * 0.35;

        col += kSpark * (a + halo);
    }

    fragColor = vec4(col, 1.0);
}
