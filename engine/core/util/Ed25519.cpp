#include "core/util/Ed25519.h"

#include "core/util/Sha2.h"

#include <cstring>

namespace mikmap {

namespace {

// ── GF(2^255-19): 16 limbs of 16 bits, held in int64 (TweetNaCl) ─────────
using gf = int64_t[16];

void set(gf r, const gf a) { for (int i = 0; i < 16; ++i) r[i] = a[i]; }

void car(gf o) {
    for (int i = 0; i < 16; ++i) {
        o[i] += int64_t(1) << 16;
        const int64_t c = o[i] >> 16;  // arithmetic shift (defined in C++20)
        o[(i + 1) * (i < 15)] += c - 1 + 37 * (c - 1) * (i == 15);
        o[i] -= c * 65536;
    }
}

// Constant-time: swap p and q when b == 1.
void sel(gf p, gf q, int b) {
    const int64_t c = ~(int64_t(b) - 1);
    for (int i = 0; i < 16; ++i) {
        const int64_t t = c & (p[i] ^ q[i]);
        p[i] ^= t;
        q[i] ^= t;
    }
}

void pack(uint8_t o[32], const gf n) {
    gf m, t;
    set(t, n);
    car(t); car(t); car(t);
    for (int j = 0; j < 2; ++j) {
        m[0] = t[0] - 0xffed;
        for (int i = 1; i < 15; ++i) {
            m[i] = t[i] - 0xffff - ((m[i - 1] >> 16) & 1);
            m[i - 1] &= 0xffff;
        }
        m[15] = t[15] - 0x7fff - ((m[14] >> 16) & 1);
        const int b = int((m[15] >> 16) & 1);
        m[14] &= 0xffff;
        sel(t, m, 1 - b);
    }
    for (int i = 0; i < 16; ++i) {
        o[2 * i]     = uint8_t(t[i] & 0xff);
        o[2 * i + 1] = uint8_t((t[i] >> 8) & 0xff);
    }
}

bool eq(const gf a, const gf b) {
    uint8_t c[32], d[32];
    pack(c, a);
    pack(d, b);
    return std::memcmp(c, d, 32) == 0;
}

int par(const gf a) {
    uint8_t d[32];
    pack(d, a);
    return d[0] & 1;
}

void unpack(gf o, const uint8_t n[32]) {
    for (int i = 0; i < 16; ++i) o[i] = n[2 * i] + (int64_t(n[2 * i + 1]) << 8);
    o[15] &= 0x7fff;
}

void A(gf o, const gf a, const gf b) { for (int i = 0; i < 16; ++i) o[i] = a[i] + b[i]; }
void Z(gf o, const gf a, const gf b) { for (int i = 0; i < 16; ++i) o[i] = a[i] - b[i]; }

void M(gf o, const gf a, const gf b) {
    int64_t t[31] = {};
    for (int i = 0; i < 16; ++i)
        for (int j = 0; j < 16; ++j) t[i + j] += a[i] * b[j];
    for (int i = 0; i < 15; ++i) t[i] += 38 * t[i + 16];
    for (int i = 0; i < 16; ++i) o[i] = t[i];
    car(o); car(o);
}

void S(gf o, const gf a) { M(o, a, a); }

void inv(gf o, const gf i) {  // i^(p-2)
    gf c;
    set(c, i);
    for (int a = 253; a >= 0; --a) {
        S(c, c);
        if (a != 2 && a != 4) M(c, c, i);
    }
    set(o, c);
}

void pow2523(gf o, const gf i) {  // i^((p-5)/8)
    gf c;
    set(c, i);
    for (int a = 250; a >= 0; --a) {
        S(c, c);
        if (a != 1) M(c, c, i);
    }
    set(o, c);
}

// ── Curve constants, derived instead of transcribed ───────────────────────
struct Consts {
    gf zero{}, one{1}, D{}, D2{}, I{};
    gf B[4]{};
    Consts();
};

const Consts& K() {
    static const Consts k;
    return k;
}

// Strict decode: false unless `s` is the canonical encoding of a curve point.
bool decode(gf r[4], const uint8_t s[32], const Consts& k) {
    gf t, chk, num, den, den2, den4, den6;
    set(r[2], k.one);
    unpack(r[1], s);
    uint8_t back[32];
    pack(back, r[1]);
    uint8_t ys[32];
    std::memcpy(ys, s, 32);
    ys[31] &= 0x7f;
    if (std::memcmp(back, ys, 32) != 0) return false;  // y >= p

    S(num, r[1]);
    M(den, num, k.D);
    Z(num, num, r[2]);   // y^2 - 1
    A(den, r[2], den);   // d y^2 + 1
    S(den2, den);
    S(den4, den2);
    M(den6, den4, den2);
    M(t, den6, num);
    M(t, t, den);
    pow2523(t, t);
    M(t, t, num);
    M(t, t, den);
    M(t, t, den);
    M(r[0], t, den);
    S(chk, r[0]);
    M(chk, chk, den);
    if (!eq(chk, num)) M(r[0], r[0], k.I);
    S(chk, r[0]);
    M(chk, chk, den);
    if (!eq(chk, num)) return false;  // not on the curve

    const int sign = s[31] >> 7;
    if (sign && eq(r[0], k.zero)) return false;  // x = 0 with the sign bit set
    if (par(r[0]) != sign) Z(r[0], k.zero, r[0]);
    M(r[3], r[0], r[1]);
    return true;
}

Consts::Consts() {
    gf a{121666}, b{121665};
    inv(a, a);
    M(D, a, b);
    Z(D, zero, D);  // d = -121665/121666
    A(D2, D, D);
    // I = sqrt(-1) = 2^((p-1)/4); (p-1)/4 = 2^253 - 5.
    gf two{2};
    set(I, one);
    for (int bit = 252; bit >= 0; --bit) {
        S(I, I);
        if (bit != 2) M(I, I, two);  // bits of 2^253 - 5: all ones except bit 2
    }
    uint8_t base[32];
    std::memset(base, 0x66, 32);
    base[0] = 0x58;  // y = 4/5, x even
    decode(B, base, *this);
}

// ── Points: extended coordinates (X, Y, Z, T) ────────────────────────────
void add(gf p[4], gf q[4], const Consts& k) {
    gf a, b, c, d, t, e, f, g, h;
    Z(a, p[1], p[0]);
    Z(t, q[1], q[0]);
    M(a, a, t);
    A(b, p[0], p[1]);
    A(t, q[0], q[1]);
    M(b, b, t);
    M(c, p[3], q[3]);
    M(c, c, k.D2);
    M(d, p[2], q[2]);
    A(d, d, d);
    Z(e, b, a);
    Z(f, d, c);
    A(g, d, c);
    A(h, b, a);
    M(p[0], e, f);
    M(p[1], h, g);
    M(p[2], g, f);
    M(p[3], e, h);
}

void cswap(gf p[4], gf q[4], int b) {
    for (int i = 0; i < 4; ++i) sel(p[i], q[i], b);
}

void encode(uint8_t r[32], gf p[4]) {
    gf tx, ty, zi;
    inv(zi, p[2]);
    M(tx, p[0], zi);
    M(ty, p[1], zi);
    pack(r, ty);
    r[31] ^= uint8_t(par(tx) << 7);
}

// p = [s]q (q is clobbered). Ladder over all 256 bits, swaps branch-free.
void scalarmult(gf p[4], gf q[4], const uint8_t s[32], const Consts& k) {
    set(p[0], k.zero);
    set(p[1], k.one);
    set(p[2], k.one);
    set(p[3], k.zero);
    for (int i = 255; i >= 0; --i) {
        const int b = (s[i / 8] >> (i & 7)) & 1;
        cswap(p, q, b);
        add(q, p, k);
        add(p, p, k);
        cswap(p, q, b);
    }
}

void scalarbase(gf p[4], const uint8_t s[32], const Consts& k) {
    gf q[4];
    for (int i = 0; i < 4; ++i) set(q[i], k.B[i]);
    scalarmult(p, q, s, k);
}

bool isIdentity(gf p[4], const Consts& k) {
    return eq(p[0], k.zero) && eq(p[1], p[2]);
}

bool smallOrder(gf p[4], const Consts& k) {
    gf q[4];
    for (int i = 0; i < 4; ++i) set(q[i], p[i]);
    for (int i = 0; i < 3; ++i) {
        gf r[4];
        for (int j = 0; j < 4; ++j) set(r[j], q[j]);
        add(q, r, k);
    }
    return isIdentity(q, k);
}

// ── Scalars mod L ─────────────────────────────────────────────────────────
const int64_t L[32] = {0xed, 0xd3, 0xf5, 0x5c, 0x1a, 0x63, 0x12, 0x58, 0xd6, 0x9c, 0xf7,
                       0xa2, 0xde, 0xf9, 0xde, 0x14, 0,    0,    0,    0,    0,    0,
                       0,    0,    0,    0,    0,    0,    0,    0,    0,    0x10};

void modL(uint8_t r[32], int64_t x[64]) {
    int64_t carry;
    for (int i = 63; i >= 32; --i) {
        carry = 0;
        int j;
        for (j = i - 32; j < i - 12; ++j) {
            x[j] += carry - 16 * x[i] * L[j - (i - 32)];
            carry = (x[j] + 128) >> 8;
            x[j] -= carry * 256;
        }
        x[j] += carry;
        x[i] = 0;
    }
    carry = 0;
    for (int j = 0; j < 32; ++j) {
        x[j] += carry - (x[31] >> 4) * L[j];
        carry = x[j] >> 8;
        x[j] &= 255;
    }
    for (int j = 0; j < 32; ++j) x[j] -= carry * L[j];
    for (int i = 0; i < 32; ++i) {
        x[i + 1] += x[i] >> 8;
        r[i] = uint8_t(x[i] & 255);
    }
}

void reduce(uint8_t r[32], const uint8_t h[64]) {
    int64_t x[64];
    for (int i = 0; i < 64; ++i) x[i] = h[i];
    modL(r, x);
}

bool scalarCanonical(const uint8_t s[32]) {  // s < L, compared from the top byte
    for (int i = 31; i >= 0; --i) {
        if (s[i] < L[i]) return true;
        if (s[i] > L[i]) return false;
    }
    return false;  // s == L
}

void expand(const uint8_t seed[32], uint8_t a[32], uint8_t prefix[32]) {
    const Sha512Digest h = sha512(seed, 32);
    std::memcpy(a, h.data(), 32);
    std::memcpy(prefix, h.data() + 32, 32);
    a[0] &= 248;
    a[31] &= 127;
    a[31] |= 64;
}

} // namespace

void ed25519PublicFromSeed(const uint8_t seed[32], uint8_t pub[32]) {
    const Consts& k = K();
    uint8_t a[32], prefix[32];
    expand(seed, a, prefix);
    gf p[4];
    scalarbase(p, a, k);
    encode(pub, p);
}

void ed25519Sign(uint8_t sig[64], const uint8_t* msg, size_t n, const uint8_t seed[32]) {
    const Consts& k = K();
    uint8_t a[32], prefix[32], pub[32], r[32], h[32];
    expand(seed, a, prefix);
    gf p[4];
    scalarbase(p, a, k);
    encode(pub, p);

    Sha512 hr;
    hr.update(prefix, 32);
    hr.update(msg, n);
    reduce(r, hr.finish().data());
    scalarbase(p, r, k);
    encode(sig, p);  // R

    Sha512 hk;
    hk.update(sig, 32);
    hk.update(pub, 32);
    hk.update(msg, n);
    reduce(h, hk.finish().data());

    int64_t x[64] = {};
    for (int i = 0; i < 32; ++i) x[i] = r[i];
    for (int i = 0; i < 32; ++i)
        for (int j = 0; j < 32; ++j) x[i + j] += int64_t(h[i]) * a[j];
    modL(sig + 32, x);  // S = r + h*a mod L
}

bool ed25519Verify(const uint8_t sig[64], const uint8_t* msg, size_t n, const uint8_t pub[32]) {
    const Consts& k = K();
    if (!scalarCanonical(sig + 32)) return false;

    gf Apt[4], Rpt[4];
    if (!decode(Apt, pub, k) || smallOrder(Apt, k)) return false;
    if (!decode(Rpt, sig, k) || smallOrder(Rpt, k)) return false;

    Sha512 hk;
    hk.update(sig, 32);
    hk.update(pub, 32);
    hk.update(msg, n);
    uint8_t h[32];
    reduce(h, hk.finish().data());

    // [S]B + [h](-A) must encode to exactly R.
    Z(Apt[0], k.zero, Apt[0]);
    Z(Apt[3], k.zero, Apt[3]);
    gf p[4], q[4];
    scalarmult(p, Apt, h, k);
    scalarbase(q, sig + 32, k);
    add(p, q, k);
    uint8_t check[32];
    encode(check, p);
    return std::memcmp(check, sig, 32) == 0;
}

} // namespace mikmap
