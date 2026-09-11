#version 150

// ════════════════════════════════════════════════════════════════════════
//  gen_plasma.frag — "Plasma Waves"
//
//  Chuyển thẳng từ lớp nền của bản thiết kế tham khảo (MikMap_Web,
//  LiveCanvas.tsx — Layer 1): một vầng sáng toả tròn cam → lam → trong
//  suốt, phủ lên một lưới ô 40px rất mờ.
//
//  Bản web vẽ vầng sáng bằng createRadialGradient với ba chặn màu cố
//  định. Ở đây nó được nội suy lại bằng smoothstep trên khoảng cách tới
//  tâm — cùng ba chặn màu, cùng bán kính (0.7 × bề rộng).
//
//  Khác một điểm CÓ CHỦ Ý: bản web đứng yên, chỉ động khi clip bị xoay/
//  phóng. Ở đây tâm sáng trôi chậm theo thời gian, vì đây là một nguồn
//  hình PHÁT LIÊN TỤC — một nền hoàn toàn tĩnh trên máy chiếu trông như
//  máy bị treo.
// ════════════════════════════════════════════════════════════════════════

uniform vec2  uRes;    // kích thước khung, px
uniform float uTime;   // giây kể từ lúc bật

out vec4 fragColor;

// Ba chặn màu lấy đúng từ bản thiết kế.
const vec3 kInner = vec3(1.0,   0.498, 0.314);   // #FF7F50
const vec3 kMid   = vec3(0.067, 0.541, 0.698);   // #118AB2
const vec3 kBg    = vec3(0.0196);                // #050505

void main() {
    vec2 px = gl_FragCoord.xy - uRes * 0.5;

    // Tâm vầng sáng trôi theo hình Lissajous — biên độ nhỏ (8% bề rộng)
    // để nhìn ra là ĐANG CHẠY mà không thành hiệu ứng lắc lư gây mệt.
    vec2 drift = vec2(sin(uTime * 0.23), cos(uTime * 0.17)) * uRes.x * 0.08;

    float r = length(px - drift) / (uRes.x * 0.7);

    vec3  col = kBg;
    float a;

    // Chặn 0.0 → 0.5: cam 0.35 alpha nhạt dần sang lam 0.2 alpha.
    // Chặn 0.5 → 1.0: lam nhạt dần về trong suốt.
    if (r < 0.5) {
        float k = smoothstep(0.0, 0.5, r);
        col = mix(kInner, kMid, k);
        a   = mix(0.35, 0.20, k);
    } else {
        col = kMid;
        a   = 0.20 * (1.0 - smoothstep(0.5, 1.0, r));
    }

    vec3 outCol = kBg + col * a;

    // ── Lưới ô 40px, rgba(255,255,255,0.04) ────────────────────────────
    //
    // Lưới được vẽ ở TOẠ ĐỘ THAM CHIẾU 1080p rồi mới nhân theo khung
    // thật, nên ô lưới giữ nguyên tỉ lệ dù khung 720p hay 4K. Lấy độ dày
    // nét theo fwidth để nét không vỡ thành răng cưa khi phóng to.
    float scale = uRes.y / 1080.0;
    vec2  g     = px / (40.0 * scale);
    vec2  gd    = abs(fract(g) - 0.5) / fwidth(g);
    float line  = 1.0 - min(min(gd.x, gd.y), 1.0);

    outCol += vec3(line * 0.04);

    fragColor = vec4(outCol, 1.0);
}
