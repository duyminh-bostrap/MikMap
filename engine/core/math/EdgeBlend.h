// ════════════════════════════════════════════════════════════════════════
//  core/math/EdgeBlend.h — toán ghép mép mềm (soft-edge) giữa hai slice
//
//  Hai rect chồng mép nhau → một dải (Band) dọc mép chồng; mỗi phía tự làm
//  mờ theo đường cong Bourke để tổng độ sáng hai phía trong dải bằng 1.
// ════════════════════════════════════════════════════════════════════════
#pragma once

namespace mikmap::edgeblend {

struct Rect { double x = 0, y = 0, w = 0, h = 0; };

enum class Side { Left, Right, Top, Bottom };

// start < end theo trục dốc (x cho Left/Right, y cho Top/Bottom); area = vùng chồng.
struct Band { Side side = Side::Left; double start = 0, end = 0; Rect area; };

// Dải chồng mép của `self` với `other`. false khi không chồng, chỉ chạm cạnh,
// hoặc rect này nằm trọn trong rect kia theo trục dốc (không phải ghép mép).
bool overlapBand(const Rect& self, const Rect& other, Band& out);

// Vị trí trong dải, kẹp [0,1]: 0 = mép của chính self, 1 = đầu trong của dải.
double bandT(const Band& b, double x, double y);

// Đường cong Bourke: t<0.5 → a·(2t)^p, còn lại 1 − (1−a)·(2(1−t))^p.
double ramp(double t, double luminance, double power);

// ramp(...)^(1/gamma); gamma <= 0 hoặc không hữu hạn → coi như 1.
double gammaRamp(double t, double luminance, double power, double gamma);

} // namespace mikmap::edgeblend
