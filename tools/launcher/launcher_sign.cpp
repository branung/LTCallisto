// Release signature checks for the updater. The Ed25519 arithmetic follows TweetNaCl
// (Bernstein, van Gastel, Janssen, Lange, Schwabe, Smetsers; public domain), verification only.
// Nothing here handles a secret, so none of it needs to run in constant time
#include "launcher_sign.h"

#include <cstdint>
#include <cstring>
#include <vector>

namespace ltlaunch {

namespace {

const uint64_t kSha512K[80] = {
    0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL,
    0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL, 0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL,
    0xd807aa98a3030242ULL, 0x12835b0145706fbeULL, 0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
    0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL, 0xc19bf174cf692694ULL,
    0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL, 0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL,
    0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL, 0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
    0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL, 0xb00327c898fb213fULL, 0xbf597fc7beef0ee4ULL,
    0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL, 0x06ca6351e003826fULL, 0x142929670a0e6e70ULL,
    0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL, 0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
    0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL, 0x81c2c92e47edaee6ULL, 0x92722c851482353bULL,
    0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL, 0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL,
    0xd192e819d6ef5218ULL, 0xd69906245565a910ULL, 0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
    0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL, 0x2748774cdf8eeb99ULL, 0x34b0bcb5e19b48a8ULL,
    0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL, 0x5b9cca4f7763e373ULL, 0x682e6ff3d6b2b8a3ULL,
    0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL, 0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
    0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL, 0xbef9a3f7b2c67915ULL, 0xc67178f2e372532bULL,
    0xca273eceea26619cULL, 0xd186b8c721c0c207ULL, 0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL,
    0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL, 0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
    0x28db77f523047d84ULL, 0x32caab7b40c72493ULL, 0x3c9ebe0a15c9bebcULL, 0x431d67c49c100d4cULL,
    0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL, 0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL,
};

uint64_t Rotr(uint64_t x, int n)
{
    return (x >> n) | (x << (64 - n));
}

void Sha512Block(uint64_t h[8], const unsigned char *p)
{
    uint64_t w[80];
    for (int i = 0; i < 16; ++i)
    {
        w[i] = 0;
        for (int j = 0; j < 8; ++j) w[i] = (w[i] << 8) | p[8 * i + j];
    }
    for (int i = 16; i < 80; ++i)
    {
        const uint64_t s0 = Rotr(w[i - 15], 1) ^ Rotr(w[i - 15], 8) ^ (w[i - 15] >> 7);
        const uint64_t s1 = Rotr(w[i - 2], 19) ^ Rotr(w[i - 2], 61) ^ (w[i - 2] >> 6);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint64_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
    for (int i = 0; i < 80; ++i)
    {
        const uint64_t t1 = hh + (Rotr(e, 14) ^ Rotr(e, 18) ^ Rotr(e, 41)) + ((e & f) ^ (~e & g)) + kSha512K[i] + w[i];
        const uint64_t t2 = (Rotr(a, 28) ^ Rotr(a, 34) ^ Rotr(a, 39)) + ((a & b) ^ (a & c) ^ (b & c));
        hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
}

// Field elements mod 2^255 - 19, as sixteen 16-bit limbs that may run over between reductions
typedef int64_t gf[16];

const gf kGf0 = { 0 };
const gf kGf1 = { 1 };
const gf kD = { 0x78a3, 0x1359, 0x4dca, 0x75eb, 0xd8ab, 0x4141, 0x0a4d, 0x0070,
                0xe898, 0x7779, 0x4079, 0x8cc7, 0xfe73, 0x2b6f, 0x6cee, 0x5203 };
const gf kD2 = { 0xf159, 0x26b2, 0x9b94, 0xebd6, 0xb156, 0x8283, 0x149a, 0x00e0,
                 0xd130, 0xeef3, 0x80f2, 0x198e, 0xfce7, 0x56df, 0xd9dc, 0x2406 };
const gf kBaseX = { 0xd51a, 0x8f25, 0x2d60, 0xc956, 0xa7b2, 0x9525, 0xc760, 0x692c,
                    0xdc5c, 0xfdd6, 0xe231, 0xc0a4, 0x53fe, 0xcd6e, 0x36d3, 0x2169 };
const gf kBaseY = { 0x6658, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666,
                    0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666 };
const gf kSqrtM1 = { 0xa0b0, 0x4a0e, 0x1b27, 0xc4ee, 0xe478, 0xad2f, 0x1806, 0x2f43,
                     0xd7a7, 0x3dfb, 0x0099, 0x2b4d, 0xdf0b, 0x4fc1, 0x2480, 0x2b83 };

// The group order, little-endian
const int64_t kL[32] = { 0xed, 0xd3, 0xf5, 0x5c, 0x1a, 0x63, 0x12, 0x58, 0xd6, 0x9c, 0xf7, 0xa2, 0xde, 0xf9, 0xde, 0x14,
                         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x10 };

void Copy(gf o, const gf a)
{
    for (int i = 0; i < 16; ++i) o[i] = a[i];
}

void Carry(gf o)
{
    for (int i = 0; i < 16; ++i)
    {
        o[i] += (int64_t)1 << 16;
        const int64_t c = o[i] >> 16;
        o[(i + 1) * (i < 15)] += c - 1 + 37 * (c - 1) * (i == 15);
        o[i] -= c * 65536;
    }
}

// Swaps p and q when b is 1
void Select(gf p, gf q, int b)
{
    const int64_t c = ~(int64_t)(b - 1);
    for (int i = 0; i < 16; ++i)
    {
        const int64_t t = c & (p[i] ^ q[i]);
        p[i] ^= t;
        q[i] ^= t;
    }
}

void Pack25519(unsigned char *o, const gf n)
{
    gf m, t;
    Copy(t, n);
    Carry(t);
    Carry(t);
    Carry(t);
    for (int j = 0; j < 2; ++j)
    {
        m[0] = t[0] - 0xffed;
        for (int i = 1; i < 15; ++i)
        {
            m[i] = t[i] - 0xffff - ((m[i - 1] >> 16) & 1);
            m[i - 1] &= 0xffff;
        }
        m[15] = t[15] - 0x7fff - ((m[14] >> 16) & 1);
        const int b = (int)((m[15] >> 16) & 1);
        m[14] &= 0xffff;
        Select(t, m, 1 - b);
    }
    for (int i = 0; i < 16; ++i)
    {
        o[2 * i] = (unsigned char)(t[i] & 0xff);
        o[2 * i + 1] = (unsigned char)(t[i] >> 8);
    }
}

bool Differ(const gf a, const gf b)
{
    unsigned char c[32], d[32];
    Pack25519(c, a);
    Pack25519(d, b);
    return std::memcmp(c, d, 32) != 0;
}

int Parity(const gf a)
{
    unsigned char d[32];
    Pack25519(d, a);
    return d[0] & 1;
}

void Unpack25519(gf o, const unsigned char *n)
{
    for (int i = 0; i < 16; ++i) o[i] = n[2 * i] + ((int64_t)n[2 * i + 1] << 8);
    o[15] &= 0x7fff;
}

void Add(gf o, const gf a, const gf b)
{
    for (int i = 0; i < 16; ++i) o[i] = a[i] + b[i];
}

void Sub(gf o, const gf a, const gf b)
{
    for (int i = 0; i < 16; ++i) o[i] = a[i] - b[i];
}

void Mul(gf o, const gf a, const gf b)
{
    int64_t t[31] = { 0 };
    for (int i = 0; i < 16; ++i)
        for (int j = 0; j < 16; ++j) t[i + j] += a[i] * b[j];
    for (int i = 0; i < 15; ++i) t[i] += 38 * t[i + 16];
    for (int i = 0; i < 16; ++i) o[i] = t[i];
    Carry(o);
    Carry(o);
}

void Square(gf o, const gf a)
{
    Mul(o, a, a);
}

void Invert(gf o, const gf in)
{
    gf c;
    Copy(c, in);
    for (int a = 253; a >= 0; --a)
    {
        Square(c, c);
        if (a != 2 && a != 4) Mul(c, c, in);
    }
    Copy(o, c);
}

// in^((p - 5) / 8)
void Pow2523(gf o, const gf in)
{
    gf c;
    Copy(c, in);
    for (int a = 250; a >= 0; --a)
    {
        Square(c, c);
        if (a != 1) Mul(c, c, in);
    }
    Copy(o, c);
}

// Points in extended coordinates (X, Y, Z, T)
void PointAdd(gf p[4], gf q[4])
{
    gf a, b, c, d, t, e, f, g, h;
    Sub(a, p[1], p[0]);
    Sub(t, q[1], q[0]);
    Mul(a, a, t);
    Add(b, p[0], p[1]);
    Add(t, q[0], q[1]);
    Mul(b, b, t);
    Mul(c, p[3], q[3]);
    Mul(c, c, kD2);
    Mul(d, p[2], q[2]);
    Add(d, d, d);
    Sub(e, b, a);
    Sub(f, d, c);
    Add(g, d, c);
    Add(h, b, a);
    Mul(p[0], e, f);
    Mul(p[1], h, g);
    Mul(p[2], g, f);
    Mul(p[3], e, h);
}

void PointSwap(gf p[4], gf q[4], int b)
{
    for (int i = 0; i < 4; ++i) Select(p[i], q[i], b);
}

void PointPack(unsigned char *r, gf p[4])
{
    gf tx, ty, zi;
    Invert(zi, p[2]);
    Mul(tx, p[0], zi);
    Mul(ty, p[1], zi);
    Pack25519(r, ty);
    r[31] ^= (unsigned char)(Parity(tx) << 7);
}

// p = s * q, which leaves q changed
void ScalarMult(gf p[4], gf q[4], const unsigned char *s)
{
    Copy(p[0], kGf0);
    Copy(p[1], kGf1);
    Copy(p[2], kGf1);
    Copy(p[3], kGf0);
    for (int i = 255; i >= 0; --i)
    {
        const int b = (s[i / 8] >> (i & 7)) & 1;
        PointSwap(p, q, b);
        PointAdd(q, p);
        PointAdd(p, p);
        PointSwap(p, q, b);
    }
}

void ScalarBase(gf p[4], const unsigned char *s)
{
    gf q[4];
    Copy(q[0], kBaseX);
    Copy(q[1], kBaseY);
    Copy(q[2], kGf1);
    Mul(q[3], kBaseX, kBaseY);
    ScalarMult(p, q, s);
}

// r = x mod L, from 64 limbs of a byte each
void ModL(unsigned char *r, int64_t x[64])
{
    int64_t carry;
    for (int i = 63; i >= 32; --i)
    {
        carry = 0;
        int j;
        for (j = i - 32; j < i - 12; ++j)
        {
            x[j] += carry - 16 * x[i] * kL[j - (i - 32)];
            carry = (x[j] + 128) >> 8;
            x[j] -= carry * 256;
        }
        x[j] += carry;
        x[i] = 0;
    }
    carry = 0;
    for (int j = 0; j < 32; ++j)
    {
        x[j] += carry - (x[31] >> 4) * kL[j];
        carry = x[j] >> 8;
        x[j] &= 255;
    }
    for (int j = 0; j < 32; ++j) x[j] -= carry * kL[j];
    for (int i = 0; i < 32; ++i)
    {
        x[i + 1] += x[i] >> 8;
        r[i] = (unsigned char)(x[i] & 255);
    }
}

// A 64-byte hash reduced mod L into its first 32 bytes
void Reduce(unsigned char *r)
{
    int64_t x[64];
    for (int i = 0; i < 64; ++i) x[i] = r[i];
    for (int i = 0; i < 64; ++i) r[i] = 0;
    ModL(r, x);
}

// The point a public key encodes, negated. False when it isn't on the curve
bool UnpackNeg(gf r[4], const unsigned char p[32])
{
    gf t, chk, num, den, den2, den4, den6;
    Copy(r[2], kGf1);
    Unpack25519(r[1], p);
    Square(num, r[1]);
    Mul(den, num, kD);
    Sub(num, num, r[2]);
    Add(den, r[2], den);

    Square(den2, den);
    Square(den4, den2);
    Mul(den6, den4, den2);
    Mul(t, den6, num);
    Mul(t, t, den);

    Pow2523(t, t);
    Mul(t, t, num);
    Mul(t, t, den);
    Mul(t, t, den);
    Mul(r[0], t, den);

    Square(chk, r[0]);
    Mul(chk, chk, den);
    if (Differ(chk, num)) Mul(r[0], r[0], kSqrtM1);

    Square(chk, r[0]);
    Mul(chk, chk, den);
    if (Differ(chk, num)) return false;

    if (Parity(r[0]) == (p[31] >> 7)) Sub(r[0], kGf0, r[0]);
    Mul(r[3], r[0], r[1]);
    return true;
}

bool BelowL(const unsigned char *s)
{
    for (int i = 31; i >= 0; --i)
    {
        if (s[i] < kL[i]) return true;
        if (s[i] > kL[i]) return false;
    }
    return false;
}

} // namespace

void Sha512(const unsigned char *pData, size_t nSize, unsigned char out[64])
{
    uint64_t h[8] = { 0x6a09e667f3bcc908ULL, 0xbb67ae8584caa73bULL, 0x3c6ef372fe94f82bULL, 0xa54ff53a5f1d36f1ULL,
                      0x510e527fade682d1ULL, 0x9b05688c2b3e6c1fULL, 0x1f83d9abfb41bd6bULL, 0x5be0cd19137e2179ULL };
    const size_t nFull = nSize / 128 * 128;
    for (size_t i = 0; i < nFull; i += 128) Sha512Block(h, pData + i);

    // The rest, a 1 bit, and the length in bits as 128 big-endian bits
    unsigned char tail[256] = { 0 };
    const size_t nRest = nSize - nFull;
    if (nRest) std::memcpy(tail, pData + nFull, nRest);
    tail[nRest] = 0x80;
    const size_t nTail = nRest + 17 <= 128 ? 128 : 256;
    const uint64_t nBitsLow = (uint64_t)nSize << 3, nBitsHigh = (uint64_t)nSize >> 61;
    for (int i = 0; i < 8; ++i)
    {
        tail[nTail - 1 - i] = (unsigned char)(nBitsLow >> (8 * i));
        tail[nTail - 9 - i] = (unsigned char)(nBitsHigh >> (8 * i));
    }
    Sha512Block(h, tail);
    if (nTail == 256) Sha512Block(h, tail + 128);

    for (int i = 0; i < 8; ++i)
        for (int j = 0; j < 8; ++j) out[8 * i + j] = (unsigned char)(h[i] >> (56 - 8 * j));
}

bool Ed25519Verify(const unsigned char sig[64], const unsigned char *pMsg, size_t nMsg, const unsigned char key[32])
{
    if (!BelowL(sig + 32)) return false;
    gf a[4];
    if (!UnpackNeg(a, key)) return false;

    // h = SHA-512(R, key, message) mod L, then check that S*B - h*A is R
    std::vector<unsigned char> m(64 + nMsg);
    std::memcpy(&m[0], sig, 32);
    std::memcpy(&m[32], key, 32);
    if (nMsg) std::memcpy(&m[64], pMsg, nMsg);
    unsigned char h[64];
    Sha512(m.data(), m.size(), h);
    Reduce(h);

    gf p[4], sb[4];
    ScalarMult(p, a, h);
    ScalarBase(sb, sig + 32);
    PointAdd(p, sb);
    unsigned char r[32];
    PointPack(r, p);
    return std::memcmp(r, sig, 32) == 0;
}

} // namespace ltlaunch
