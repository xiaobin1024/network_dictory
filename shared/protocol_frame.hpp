#ifndef PROTOCOL_FRAME_HPP
#define PROTOCOL_FRAME_HPP

/*
变长分帧协议（v2）与旧定长协议（v1）的兼容层。

背景
----
v1 是 sizeof(Msg)=1316 的定长结构：type(4) + name(32) + text(1280)。
问题有两个：
  1. 浪费带宽：一次心跳只有 type 有效，却固定发 1316 字节；
     历史/收藏列表里大量字段是空串，同样按 1280 字节补齐。
  2. 无法扩展：text 被 1280 字节焊死，一条记录超长就会被截断，
     想加字段又必然改变结构体大小，旧客户端直接解不开。

v2 帧格式（全部网络字节序，大端）
--------------------------------
  偏移  长度  字段
  0     4     magic  = 0x44494354（"DICT"）
  4     4     bodyLen = 8 + nameLen + textLen
  8     4     type
  12    2     nameLen
  14    2     textLen
  16    n     name
  16+n  m     text
  帧总长 = 8 + bodyLen

兼容怎么做的
------------
v1 首 4 字节是 type（网络字节序，取值 1~11）；v2 首 4 字节是固定 magic
0x44494354，两者不可能相等。服务端因此只需看前 4 字节就能判定版本，
同一个端口同时服务新旧客户端，旧客户端不升级也能继续用。

版本是"按连接"判定的：握手后的第一帧决定这条连接后续所有帧的格式，
不会中途切换，因此不需要在每一帧里再带版本字段。

用法（服务端）
--------------
  int ver = ProtocolFrame::detectVersion(buf.data(), buf.size());   // -1=数据不够
  size_t frameLen = 0;
  ProtocolFrame::Frame f;
  auto r = ProtocolFrame::parse(buf.data(), buf.size(), f, frameLen);
  // r == ParseResult::NeedMore 表示还要继续 recv
*/

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

namespace ProtocolFrame
{

// "DICT" 的 ASCII。选它做 magic 是因为它既不可能与 v1 的 type(1~11) 撞车，
// 抓包时也能一眼认出这是本协议的帧。
constexpr uint32_t kMagic = 0x44494354u;

// 帧头：magic(4) + bodyLen(4) + type(4) + nameLen(2) + textLen(2)
constexpr size_t kHeaderSize = 16;

constexpr size_t kMaxNameLen = 4096;   // 用户名实际最长 20，留足冗余
constexpr size_t kMaxTextLen = 65535;  // textLen 是 uint16，硬上限
constexpr size_t kMaxBodyLen = 8 + kMaxNameLen + kMaxTextLen;
constexpr size_t kMaxFrameLen = kHeaderSize + kMaxTextLen + kMaxNameLen;

// 协议版本
enum Version
{
    kV1Fixed = 0,   // 旧定长 1316 字节
    kV2Frame = 1,   // 新变长帧
    kUnknown = -1,  // 已收到的字节还不足以判定
};

struct Frame
{
    int type = 0;
    std::string name;
    std::string text;
};

enum class ParseResult
{
    Ok,       // 解出一帧，frameLen 为其在缓冲中的字节数
    NeedMore, // 缓冲里的字节还不够一帧
    Invalid,  // 帧头非法（magic 不对 / 长度越界），该连接只能断开
};

// 用前 4 字节判定协议版本；不足 4 字节返回 kUnknown
inline int detectVersion(const char *data, size_t len)
{
    if (len < 4)
        return kUnknown;
    uint32_t head = 0;
    std::memcpy(&head, data, 4);
    // 缓冲里存的是网络字节序的原始字节，直接按大端解释
    uint32_t be = (static_cast<uint32_t>(static_cast<unsigned char>(data[0])) << 24) |
                  (static_cast<uint32_t>(static_cast<unsigned char>(data[1])) << 16) |
                  (static_cast<uint32_t>(static_cast<unsigned char>(data[2])) << 8) |
                  (static_cast<uint32_t>(static_cast<unsigned char>(data[3])));
    (void)head;
    return (be == kMagic) ? kV2Frame : kV1Fixed;
}

// 读大端 uint32 / uint16（不依赖 htonl，便于在单元测试里脱离 socket 使用）
inline uint32_t readU32(const char *p)
{
    return (static_cast<uint32_t>(static_cast<unsigned char>(p[0])) << 24) |
           (static_cast<uint32_t>(static_cast<unsigned char>(p[1])) << 16) |
           (static_cast<uint32_t>(static_cast<unsigned char>(p[2])) << 8) |
           (static_cast<uint32_t>(static_cast<unsigned char>(p[3])));
}

inline uint16_t readU16(const char *p)
{
    return static_cast<uint16_t>((static_cast<uint16_t>(static_cast<unsigned char>(p[0])) << 8) |
                                 (static_cast<uint16_t>(static_cast<unsigned char>(p[1]))));
}

inline void writeU32(char *p, uint32_t v)
{
    p[0] = static_cast<char>((v >> 24) & 0xFF);
    p[1] = static_cast<char>((v >> 16) & 0xFF);
    p[2] = static_cast<char>((v >> 8) & 0xFF);
    p[3] = static_cast<char>(v & 0xFF);
}

inline void writeU16(char *p, uint16_t v)
{
    p[0] = static_cast<char>((v >> 8) & 0xFF);
    p[1] = static_cast<char>(v & 0xFF);
}

// 解析一帧。成功时 frameLen 返回该帧占用的总字节数（调用方据此从缓冲移除）
inline ParseResult parse(const char *data, size_t len, Frame &out, size_t &frameLen)
{
    frameLen = 0;
    if (len < kHeaderSize)
        return ParseResult::NeedMore;

    if (readU32(data) != kMagic)
        return ParseResult::Invalid;

    uint32_t bodyLen = readU32(data + 4);
    // 上限校验：防止恶意/损坏的 bodyLen 让服务端一次性分配几百 MB
    if (bodyLen < 8 || bodyLen > kMaxBodyLen)
        return ParseResult::Invalid;

    size_t total = static_cast<size_t>(bodyLen) + 8;
    if (len < total)
        return ParseResult::NeedMore;

    out.type = static_cast<int>(readU32(data + 8));
    uint16_t nameLen = readU16(data + 12);
    uint16_t textLen = readU16(data + 14);

    // bodyLen 必须等于 8 + nameLen + textLen，否则是半包错位，不能按偏移硬读
    if (bodyLen != 8u + nameLen + textLen)
        return ParseResult::Invalid;
    if (nameLen > kMaxNameLen || textLen > kMaxTextLen)
        return ParseResult::Invalid;

    out.name.assign(data + kHeaderSize, nameLen);
    out.text.assign(data + kHeaderSize + nameLen, textLen);
    frameLen = total;
    return ParseResult::Ok;
}

// 序列化一帧
inline std::string encode(int type, const std::string &name, const std::string &text)
{
    // 超长截断而非拒绝：受协议字段宽度所限，截断后仍能保持帧自洽
    size_t nameLen = name.size() > kMaxNameLen ? kMaxNameLen : name.size();
    size_t textLen = text.size() > kMaxTextLen ? kMaxTextLen : text.size();

    std::string buf;
    buf.resize(kHeaderSize + nameLen + textLen);
    char *p = &buf[0];

    writeU32(p, kMagic);
    writeU32(p + 4, static_cast<uint32_t>(8 + nameLen + textLen)); // bodyLen
    writeU32(p + 8, static_cast<uint32_t>(type));
    writeU16(p + 12, static_cast<uint16_t>(nameLen));
    writeU16(p + 14, static_cast<uint16_t>(textLen));
    if (nameLen)
        std::memcpy(p + kHeaderSize, name.data(), nameLen);
    if (textLen)
        std::memcpy(p + kHeaderSize + nameLen, text.data(), textLen);
    return buf;
}

} // namespace ProtocolFrame

#endif // PROTOCOL_FRAME_HPP
