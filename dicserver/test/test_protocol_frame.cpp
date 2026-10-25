/*
变长分帧协议（v2）单元测试。

纯标准库，不依赖 SQLite/网络，独立可执行，纳入 ctest。
覆盖：编解码往返、UTF-8 中文、半包拼包、版本识别、各类非法帧的拒绝。
*/

#include "protocol_frame.hpp"

#include <iostream>
#include <string>
#include <vector>

namespace
{
int g_failed = 0;
int g_passed = 0;

void check(bool cond, const char *expr, int line)
{
    if (cond)
    {
        ++g_passed;
    }
    else
    {
        ++g_failed;
        std::cerr << "FAIL (line " << line << "): " << expr << std::endl;
    }
}
} // namespace

#define CHECK(cond) check((cond), #cond, __LINE__)

using namespace ProtocolFrame;

int main()
{
    // ---- 1. 编解码往返 ----
    {
        std::string buf = encode(4, "alice", "apple");
        // 帧长 = 头 16 + name 5 + text 5 = 26
        CHECK(buf.size() == 26);

        Frame f;
        size_t frameLen = 0;
        CHECK(parse(buf.data(), buf.size(), f, frameLen) == ParseResult::Ok);
        CHECK(frameLen == 26);
        CHECK(f.type == 4);
        CHECK(f.name == "alice");
        CHECK(f.text == "apple");
    }

    // ---- 2. 中文按字节计数，不能按字符 ----
    {
        std::string text = "n. 苹果";  // UTF-8 下是 8 字节，不是 5 个"字符"
        std::string buf = encode(4, "张三", text);
        Frame f;
        size_t frameLen = 0;
        CHECK(parse(buf.data(), buf.size(), f, frameLen) == ParseResult::Ok);
        CHECK(f.name == "张三");
        CHECK(f.text == text);
        // 长度字段按字节算：name "张三" 6 字节 + text 8 字节
        CHECK(readU16(buf.data() + 12) == 6);
        CHECK(readU16(buf.data() + 14) == text.size());
        CHECK(readU32(buf.data() + 4) == 8 + 6 + text.size());
    }

    // ---- 3. 空 name / 空 text ----
    {
        std::string buf = encode(6, "", "");
        CHECK(buf.size() == 16);
        Frame f;
        size_t frameLen = 0;
        CHECK(parse(buf.data(), buf.size(), f, frameLen) == ParseResult::Ok);
        CHECK(f.name.empty());
        CHECK(f.text.empty());
    }

    // ---- 4. 半包：字节不够时返回 NeedMore，补足后能解出 ----
    {
        std::string buf = encode(2, "bob", "secret");
        for (size_t n = 1; n < buf.size(); ++n)
        {
            Frame f;
            size_t frameLen = 0;
            ParseResult r = parse(buf.data(), n, f, frameLen);
            if (n < kHeaderSize)
            {
                CHECK(r == ParseResult::NeedMore);
            }
            else if (n < buf.size())
            {
                CHECK(r == ParseResult::NeedMore);
            }
        }
        Frame f;
        size_t frameLen = 0;
        CHECK(parse(buf.data(), buf.size(), f, frameLen) == ParseResult::Ok);
        CHECK(f.name == "bob");
    }

    // ---- 5. 两帧背靠背：一次解析只能吃一帧，且边界正确 ----
    {
        std::string a = encode(1, "u1", "p1");
        std::string b = encode(2, "u2", "p2");
        std::string both = a + b;

        Frame f1, f2;
        size_t len1 = 0, len2 = 0;
        CHECK(parse(both.data(), both.size(), f1, len1) == ParseResult::Ok);
        CHECK(len1 == a.size());
        CHECK(f1.name == "u1");
        CHECK(parse(both.data() + len1, both.size() - len1, f2, len2) == ParseResult::Ok);
        CHECK(len2 == b.size());
        CHECK(f2.name == "u2");
        CHECK(f2.text == "p2");
    }

    // ---- 6. 版本识别：magic 判 v2，普通 type 判 v1，不足 4 字节未知 ----
    {
        std::string v2 = encode(1, "u", "p");
        CHECK(detectVersion(v2.data(), v2.size()) == kV2Frame);

        // 旧 v1 帧：首 4 字节是网络字节序的 type=1
        char v1[4] = {0, 0, 0, 1};
        CHECK(detectVersion(v1, 4) == kV1Fixed);

        char v1_big[4] = {0, 0, 0, 11};
        CHECK(detectVersion(v1_big, 4) == kV1Fixed);

        char tiny[3] = {0, 0, 0};
        CHECK(detectVersion(tiny, 3) == kUnknown);
    }

    // ---- 7. 非法帧必须被拒绝，而不是按偏移硬读 ----
    {
        // 7a magic 不对
        std::string bad = encode(1, "u", "p");
        bad[0] = 'X';
        Frame f;
        size_t fl = 0;
        CHECK(parse(bad.data(), bad.size(), f, fl) == ParseResult::Invalid);

        // 7b bodyLen 越界（超过上限）
        std::string big = encode(1, "u", "p");
        writeU32(&big[4], kMaxBodyLen + 1);
        CHECK(parse(big.data(), big.size(), f, fl) == ParseResult::Invalid);

        // 7c bodyLen 太小（不足 8）
        std::string small = encode(1, "u", "p");
        writeU32(&small[4], 7);
        CHECK(parse(small.data(), small.size(), f, fl) == ParseResult::Invalid);

        // 7d bodyLen 与 nameLen/textLen 不一致——典型的半包错位，必须拒绝
        std::string mismatch = encode(1, "u", "p");
        writeU16(&mismatch[12], 99);
        CHECK(parse(mismatch.data(), mismatch.size(), f, fl) == ParseResult::Invalid);
    }

    // ---- 8. 超长 text 截断后仍自洽 ----
    {
        std::string longText(kMaxTextLen + 100, 'x');
        std::string buf = encode(4, "u", longText);
        Frame f;
        size_t fl = 0;
        CHECK(parse(buf.data(), buf.size(), f, fl) == ParseResult::Ok);
        CHECK(f.text.size() == kMaxTextLen);
        CHECK(fl == buf.size());
    }

    std::cout << "protocol_frame: " << g_passed << " passed, " << g_failed << " failed" << std::endl;
    return g_failed == 0 ? 0 : 1;
}
