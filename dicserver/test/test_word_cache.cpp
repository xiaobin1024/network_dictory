/*
LRU 热点词缓存单元测试。

不依赖 gtest / SQLite / 网络，只链接标准库，独立可执行，便于在服务器上直接跑。
覆盖：命中与未命中、LRU 淘汰顺序、重复写更新、容量上限、清空、命中率统计，
以及"缓存值是完整词条（释义/音标/例句/翻译）而不是只有释义"这一条——
词典升级后如果缓存只存释义，查词回包里的音标/例句仍会是空的。
*/

#include "word_entry.hpp"
#include "word_cache.hpp"

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

// 构造一个词条：只填释义时，音标/例句/翻译为空
WordEntry makeEntry(const std::string &mean, const std::string &phonetic = "",
                    const std::string &example = "", const std::string &translation = "")
{
    WordEntry e;
    e.mean = mean;
    e.phonetic = phonetic;
    e.example = example;
    e.translation = translation;
    return e;
}
} // namespace

#define CHECK(cond) check((cond), #cond, __LINE__)

int main()
{
    WordCache &cache = WordCache::getInstance();

    // ---- 1. 未命中 → 写入 → 命中 ----
    cache.clear();
    cache.setCapacity(4096);
    {
        WordEntry entry;
        CHECK(!cache.get("apple", entry)); // 首次查询未命中

        cache.put("apple", makeEntry("n. 苹果", "/ˈæpl/", "An apple a day.", "一天一苹果。"));
        CHECK(cache.get("apple", entry));
        CHECK(entry.mean == "n. 苹果");

        size_t hits_before = cache.hitCount();
        size_t miss_before = cache.missCount();
        CHECK(cache.get("apple", entry));
        CHECK(cache.hitCount() == hits_before + 1); // 命中计数递增
        CHECK(cache.missCount() == miss_before);    // 未命中计数不变
    }

    // ---- 1b. 缓存必须带完整词条：音标/例句/翻译不能丢 ----
    cache.clear();
    cache.setCapacity(16);
    {
        cache.put("ability", makeEntry("n. 能力", "/ə'bɪləti/",
                                       "The test measures your ability.", "这种测试考查能力。"));
        WordEntry entry;
        CHECK(cache.get("ability", entry));
        CHECK(entry.mean == "n. 能力");
        CHECK(entry.phonetic == "/ə'bɪləti/");
        CHECK(entry.example == "The test measures your ability.");
        CHECK(entry.translation == "这种测试考查能力。");
    }

    // ---- 2. LRU 淘汰顺序：最久未用的先被淘汰 ----
    cache.clear();
    cache.setCapacity(3);
    {
        cache.put("a", makeEntry("1"));
        cache.put("b", makeEntry("2"));
        cache.put("c", makeEntry("3"));
        CHECK(cache.size() == 3);

        WordEntry v;
        CHECK(cache.get("a", v)); // 访问 a，a 变为最近使用，b 成为最久未用
        cache.put("d", makeEntry("4")); // 容量满，应淘汰 b

        CHECK(cache.size() == 3);
        CHECK(!cache.get("b", v)); // b 已被淘汰
        CHECK(cache.get("a", v));  // a 因刚被访问而保留
        CHECK(cache.get("c", v));
        CHECK(cache.get("d", v));
        CHECK(cache.evictCount() >= 1);
    }

    // ---- 3. 重复写入同一 key：更新值且不重复占位 ----
    cache.clear();
    cache.setCapacity(16);
    {
        cache.put("k", makeEntry("v1"));
        cache.put("k", makeEntry("v2"));
        CHECK(cache.size() == 1);

        WordEntry v;
        CHECK(cache.get("k", v));
        CHECK(v.mean == "v2");
    }

    // ---- 4. 容量上限：写入量远超容量时条目数不越界 ----
    cache.clear();
    cache.setCapacity(8);
    {
        for (int i = 0; i < 100; ++i)
        {
            cache.put("w" + std::to_string(i), makeEntry("m" + std::to_string(i)));
        }
        CHECK(cache.size() == 8);
        CHECK(cache.capacity() == 8);

        // 最早写入的 w0 必然已被淘汰，最后写入的 w99 必然还在
        WordEntry v;
        CHECK(!cache.get("w0", v));
        CHECK(cache.get("w99", v));
        CHECK(v.mean == "m99");
    }

    // ---- 5. 容量为 0 的兜底：不得进入死循环，且至少能存 1 条 ----
    cache.clear();
    cache.setCapacity(0);
    {
        CHECK(cache.capacity() == 1);
        cache.put("only", makeEntry("one"));
        WordEntry v;
        CHECK(cache.get("only", v));
    }

    // ---- 6. 清空后不再命中 ----
    cache.clear();
    cache.setCapacity(16);
    {
        cache.put("x", makeEntry("y"));
        cache.clear();
        CHECK(cache.size() == 0);
        WordEntry v;
        CHECK(!cache.get("x", v));
    }

    // ---- 7. 命中率统计在 0.0 ~ 1.0 之间 ----
    cache.clear();
    cache.setCapacity(16);
    {
        WordEntry v;
        cache.put("hit", makeEntry("1"));
        cache.get("hit", v);   // 命中
        cache.get("miss", v);  // 未命中
        double rate = cache.hitRate();
        CHECK(rate > 0.0);
        CHECK(rate < 1.0);
    }

    std::cout << "word_cache: " << g_passed << " passed, " << g_failed << " failed" << std::endl;
    return g_failed == 0 ? 0 : 1;
}
