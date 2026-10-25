#ifndef PASSWORD_HASH_HPP
#define PASSWORD_HASH_HPP

/*
密码哈希模块：SHA-256 + 16 字节随机盐，存储格式 "salt_hex:hash_hex"。

设计要点：
1. 不引第三方库，SHA-256 按 FIPS 180-4 自实现——协议层不依赖 OpenSSL，
   部署时无需额外链接，也便于审计核心算法。
2. 每个密码独立随机盐：相同密码的哈希值不同，抵御彩虹表与字典攻击。
3. 恒定时间比较：校验时不因前缀匹配提前返回，抵御时序侧信道。
4. 兼容旧明文库：isHashedFormat 为 false 时表示旧数据，由调用方走明文
   兼容路径并在登录成功后立即升级为哈希（平滑迁移）。

线程安全：本模块无共享状态，可被多线程并发调用。
*/

#include <string>
#include <cstdint>
#include <cstring>
#include <random>
#include <atomic>
#include <chrono>
#include <ctime>

namespace pw
{

// 字节流转小写十六进制字符串（定义在后，类内先用）
inline std::string toHex(const uint8_t *data, size_t len);

// ============================================================================
// SHA-256（FIPS 180-4）
// ============================================================================
class Sha256
{
public:
    Sha256() { reset(); }

    void reset()
    {
        // 初始哈希值：前 8 个素数的平方根小数部分
        h_[0] = 0x6a09e667u;
        h_[1] = 0xbb67ae85u;
        h_[2] = 0x3c6ef372u;
        h_[3] = 0xa54ff53au;
        h_[4] = 0x510e527fu;
        h_[5] = 0x9b05688cu;
        h_[6] = 0x1f83d9abu;
        h_[7] = 0x5be0cd19u;
        buf_len_ = 0;
        total_len_ = 0;
        finalized_ = false;
    }

    void update(const uint8_t *data, size_t len)
    {
        if (finalized_)
        {
            return;
        }
        total_len_ += len;
        while (len > 0)
        {
            size_t take = 64 - buf_len_;
            if (take > len)
            {
                take = len;
            }
            memcpy(buf_ + buf_len_, data, take);
            buf_len_ += take;
            data += take;
            len -= take;
            if (buf_len_ == 64)
            {
                processBlock(buf_);
                buf_len_ = 0;
            }
        }
    }

    void update(const std::string &s)
    {
        update(reinterpret_cast<const uint8_t *>(s.data()), s.size());
    }

    // 返回 32 字节摘要（调用后对象状态即终结，不可再 update）
    std::string digest()
    {
        if (!finalized_)
        {
            // 填充：0x80 + 0x00... + 64 位大端比特长度
            uint64_t bit_len = total_len_ * 8;
            uint8_t pad = 0x80;
            update(&pad, 1);
            uint8_t zero = 0x00;
            while (buf_len_ != 56)
            {
                update(&zero, 1);
            }
            uint8_t len_bytes[8];
            for (int i = 0; i < 8; i++)
            {
                len_bytes[i] = static_cast<uint8_t>(bit_len >> (56 - 8 * i));
            }
            update(len_bytes, 8);
            finalized_ = true;
        }
        std::string out;
        out.resize(32);
        for (int i = 0; i < 8; i++)
        {
            for (int j = 0; j < 4; j++)
            {
                out[i * 4 + j] = static_cast<uint8_t>(h_[i] >> (24 - 8 * j));
            }
        }
        return out;
    }

    std::string hexDigest()
    {
        std::string raw = digest();
        return toHex(reinterpret_cast<const uint8_t *>(raw.data()), raw.size());
    }

private:
    static uint32_t rotr(uint32_t x, uint32_t n)
    {
        return (x >> n) | (x << (32 - n));
    }

    void processBlock(const uint8_t *p)
    {
        static const uint32_t K[64] = {
            0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
            0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
            0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
            0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
            0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
            0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
            0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
            0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};

        uint32_t w[64];
        for (int i = 0; i < 16; i++)
        {
            w[i] = (static_cast<uint32_t>(p[i * 4]) << 24) |
                   (static_cast<uint32_t>(p[i * 4 + 1]) << 16) |
                   (static_cast<uint32_t>(p[i * 4 + 2]) << 8) |
                   static_cast<uint32_t>(p[i * 4 + 3]);
        }
        for (int i = 16; i < 64; i++)
        {
            uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }

        uint32_t a = h_[0], b = h_[1], c = h_[2], d = h_[3];
        uint32_t e = h_[4], f = h_[5], g = h_[6], h = h_[7];
        for (int i = 0; i < 64; i++)
        {
            uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            uint32_t ch = (e & f) ^ (~e & g);
            uint32_t temp1 = h + S1 + ch + K[i] + w[i];
            uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t temp2 = S0 + maj;
            h = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }
        h_[0] += a;
        h_[1] += b;
        h_[2] += c;
        h_[3] += d;
        h_[4] += e;
        h_[5] += f;
        h_[6] += g;
        h_[7] += h;
    }

    uint32_t h_[8];
    uint8_t buf_[64];
    size_t buf_len_;
    uint64_t total_len_;
    bool finalized_;
};

// 字节流转小写十六进制字符串
inline std::string toHex(const uint8_t *data, size_t len)
{
    static const char hexchars[] = "0123456789abcdef";
    std::string out;
    out.reserve(len * 2);
    for (size_t i = 0; i < len; i++)
    {
        out.push_back(hexchars[data[i] >> 4]);
        out.push_back(hexchars[data[i] & 0x0f]);
    }
    return out;
}

// 生成 n 字节加密随机盐（hex 编码，2n 字符）
// 注意：std::random_device 在 Windows/MinGW 运行时上并非真随机源（实测同进程内
// 连续调用返回相同序列），因此混合多熵源后经 SHA-256 压缩取前缀：
// random_device 多次采样 + 纳秒时间戳 + 秒级时间戳 + 栈地址 + 进程内调用计数器。
inline std::string randomSalt(size_t n = 16)
{
    uint64_t seed_material[6];
    std::random_device rd;
    seed_material[0] = (static_cast<uint64_t>(rd()) << 32) | static_cast<uint64_t>(rd());
    seed_material[1] = static_cast<uint64_t>(rd());
    seed_material[2] = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    seed_material[3] = static_cast<uint64_t>(time(nullptr));
    seed_material[4] = reinterpret_cast<uintptr_t>(&seed_material);
    static std::atomic<uint64_t> call_counter{0};
    seed_material[5] = call_counter.fetch_add(1);

    Sha256 sha;
    sha.update(reinterpret_cast<const uint8_t *>(seed_material), sizeof(seed_material));
    std::string mixed = sha.digest();
    if (n > mixed.size())
    {
        n = mixed.size();
    }
    return toHex(reinterpret_cast<const uint8_t *>(mixed.data()), n);
}

// 生成密码哈希：返回 "salt_hex:hash_hex"
inline std::string hashPassword(const std::string &password)
{
    std::string salt = randomSalt();
    Sha256 sha;
    sha.update(salt);
    sha.update(password);
    return salt + ":" + sha.hexDigest();
}

// 判断存储值是否为哈希格式（"hex:hex"），false 表示旧明文数据
inline bool isHashedFormat(const std::string &stored)
{
    size_t colon = stored.find(':');
    if (colon == std::string::npos || colon == 0 || colon + 1 == stored.size())
    {
        return false;
    }
    for (char c : stored)
    {
        if (c != ':' && !((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
        {
            return false;
        }
    }
    return true;
}

// 恒定时间字符串比较：不因首个不同字节提前返回，抵御时序侧信道
inline bool constantTimeEqual(const std::string &a, const std::string &b)
{
    if (a.size() != b.size())
    {
        return false;
    }
    volatile uint8_t diff = 0;
    for (size_t i = 0; i < a.size(); i++)
    {
        diff |= static_cast<uint8_t>(a[i] ^ b[i]);
    }
    return diff == 0;
}

// 校验密码：stored 为 hashPassword 的返回值
inline bool verifyPassword(const std::string &password, const std::string &stored)
{
    if (!isHashedFormat(stored))
    {
        return false;
    }
    size_t colon = stored.find(':');
    std::string salt = stored.substr(0, colon);
    std::string expect = stored.substr(colon + 1);
    Sha256 sha;
    sha.update(salt);
    sha.update(password);
    return constantTimeEqual(sha.hexDigest(), expect);
}

} // namespace pw

#endif // PASSWORD_HASH_HPP
