/*
词典行解析（DictLine::parse）单元测试。

解析是纯字符串逻辑，不依赖 SQLite 与网络，所以能直接进 ctest。
重点覆盖词典升级引入的几种真实输入：
  1. 新格式 5 列（3 个空格分隔）
  2. CRLF 行尾（词典文件是 Windows 换行）
  3. 旧格式 2 列（运维拿旧 dict.txt 覆盖时不能把整行塞进释义列）
  4. 列数不足（只有单词+释义）
  5. 翻译里意外出现分隔符：并入最后一段，而不是多出一个字段
  6. 空行 / 无单词的脏行必须被跳过

    cd <build> && ctest -R dict_entry
*/

#include "word_entry.hpp"

#include <iostream>
#include <string>

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

int main()
{
    // ---- 1. 新格式 5 列 ----
    {
        WordEntry e;
        const bool ok = DictLine::parse(
            "abandonment   n.放弃   /ə'bændənmənt/   The abandonment of the old plan.   放弃旧计划。", e);
        CHECK(ok);
        CHECK(e.word == "abandonment");
        CHECK(e.mean == "n.放弃");
        CHECK(e.phonetic == "/ə'bændənmənt/");
        CHECK(e.example == "The abandonment of the old plan.");
        CHECK(e.translation == "放弃旧计划。");
    }

    // ---- 2. CRLF 行尾：'\r' 不能被算进最后一个字段 ----
    {
        WordEntry e;
        const bool ok = DictLine::parse("ability   n.能力   /ə'bɪləti/   Try it.   试试。\r\n", e);
        CHECK(ok);
        CHECK(e.word == "ability");
        CHECK(e.translation == "试试。");
        CHECK(e.translation.find('\r') == std::string::npos);
        CHECK(e.translation.find('\n') == std::string::npos);
    }

    // ---- 3. 旧格式兜底：第一个空格切一刀，音标/例句/翻译留空 ----
    {
        WordEntry e;
        const bool ok = DictLine::parse("hello n. 你好", e);
        CHECK(ok);
        CHECK(e.word == "hello");
        CHECK(e.mean == "n. 你好");
        CHECK(e.phonetic.empty());
        CHECK(e.example.empty());
        CHECK(e.translation.empty());
    }

    // ---- 4. 只有单词 + 释义（用了新分隔符但列数不足）----
    {
        WordEntry e;
        const bool ok = DictLine::parse("book   n.书", e);
        CHECK(ok);
        CHECK(e.word == "book");
        CHECK(e.mean == "n.书");
        CHECK(e.phonetic.empty());
        CHECK(e.example.empty());
        CHECK(e.translation.empty());
    }

    // ---- 5. 翻译里出现分隔符：整体归入翻译，不产生第 6 个字段 ----
    {
        WordEntry e;
        const bool ok = DictLine::parse("word   n.词   /wɜːd/   Say it.   说它   再补一句", e);
        CHECK(ok);
        CHECK(e.word == "word");
        CHECK(e.mean == "n.词");
        CHECK(e.phonetic == "/wɜːd/");
        CHECK(e.example == "Say it.");
        CHECK(e.translation == "说它   再补一句");
    }

    // ---- 6. 脏行：空行 / 只有单词 / 无分隔符无空格 ----
    {
        WordEntry e;
        CHECK(!DictLine::parse("", e));
        CHECK(!DictLine::parse("   ", e));
        CHECK(!DictLine::parse("onlyword", e));
    }

    // ---- 7. 行首尾多余空格被 trim，单词字段不受影响 ----
    {
        WordEntry e;
        const bool ok = DictLine::parse("  test   n.测试   /test/   A test.   一个测试  ", e);
        CHECK(ok);
        CHECK(e.word == "test");
        CHECK(e.translation == "一个测试");
    }

    std::cout << "dict_entry: " << g_passed << " passed, " << g_failed << " failed" << std::endl;
    return g_failed == 0 ? 0 : 1;
}
