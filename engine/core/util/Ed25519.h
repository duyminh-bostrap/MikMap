// ════════════════════════════════════════════════════════════════════════
//  core/util/Ed25519.h — Ed25519 (RFC 8032), STL only
//
//  Verify decides whether a downloaded sensor pack is trusted, so it is
//  STRICT: false for S >= L, for a public key or R that does not decode, is
//  not canonically encoded (y >= p, or x = 0 with the sign bit set) or has
//  small order (8P = identity). The check itself is the cofactorless
//  [S]B == R + [H(R||A||M)]A, compared on the canonical encoding of R.
//  Cofactorless (RFC 8032 permits it; same policy as libsodium), so a
//  mixed-order public key is accepted: harmless for a fixed embedded publisher key.
//  The `y >= p` and `x = 0 with sign bit` decode checks are defence in depth:
//  not reachable through the public API without the key's discrete log.
//
//  Sign/PublicFromSeed exist for the packaging tool (mikmap_packtool) and the
//  tests, not for general-purpose signing: the scalar multiplication swaps
//  points branch-free on secret bits, but nothing else (memory wiping, side
//  channels beyond that) is attempted. The App only calls verify.
//
//  Field arithmetic follows TweetNaCl (GF(2^255-19) in 16 signed 64-bit
//  limbs, extended twisted-Edwards coordinates).
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include <cstddef>
#include <cstdint>

namespace mikmap {

void ed25519PublicFromSeed(const uint8_t seed[32], uint8_t pub[32]);
/// `msg` may be nullptr when n == 0.
void ed25519Sign(uint8_t sig[64], const uint8_t* msg, size_t n, const uint8_t seed[32]);
/// `msg` may be nullptr when n == 0. Strict, see above.
bool ed25519Verify(const uint8_t sig[64], const uint8_t* msg, size_t n, const uint8_t pub[32]);

} // namespace mikmap
