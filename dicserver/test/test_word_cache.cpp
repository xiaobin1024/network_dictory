/*
LRU 热点词缓存单元测试。

不依赖 gtest / SQLite / 网络，只链接标准库，独立可执行，便于在服务器上直接跑。
覆盖：命中与未命中、LRU 淘汰顺序、重复写更新、容量上限、清空、命中率统计。
*/

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
} // namespace

#define CHECK(cond) check((cond), #cond, __LINE__)

int main()
{
    WordCache &cache = WordCache::getInstance();

    // ---- 1. 未命中 → 写入 → 命中 ----
    cache.clear();
    cache.setCapacity(4096);
    {
        std::string meaning;
        CHECK(!cache.get("apple", meaning)); // 首次查询未命中

        cache.put("apple", "n. 苹果");
        CHECK(cache.get("apple", meaning));
        CHECK(meaning == "n. 苹果");

        size_t hits_before = cache.hitCount();
        size_t miss_before = cache.missCount();
        CHECK(cache.get("apple", meaning));
        CHECK(cache.hitCount() == hits_before + 1); // 命中计数递增
        CHECK(cache.missCount() == miss_before);    // 未命中计数不变
    }

    // ---- 2. LRU 淘汰顺序：最久未用的先被淘汰 ----
    cache.clear();
    cache.setCapacity(3);
    {
        cache.put("a", "1");
        cache.put("b", "2");
        cache.put("c", "3");
        CHECK(cache.size() == 3);

        std::string v;
        CHECK(cache.get("a", v)); // 访问 a，a 变为最近使用，b 成为最久未用
        cache.put("d", "4");      // 容量满，应淘汰 b

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
        cache.put("k", "v1");
        cache.put("k", "v2");
        CHECK(cache.size() == 1);

        std::string v;
        CHECK(cache.get("k", v));
        CHECK(v == "v2");
    }

    // ---- 4. 容量上限：写入量远超容量时条目数不越界 ----
    cache.clear();
    cache.setCapacity(8);
    {
        for (int i = 0; i < 100; ++i)
        {
            cache.put("w" + std::to_string(i), "m" + std::to_string(i));
        }
        CHECK(cache.size() == 8);
        CHECK(cache.capacity() == 8);

        // 最早写入的 w0 必然已被淘汰，最后写入的 w99 必然还在
        std::string v;
        CHECK(!cache.get("w0", v));
        CHECK(cache.get("w99", v));
        CHECK(v == "m99");
    }

    // ---- 5. 容量为 0 的兜底：不得进入死循环，且至少能存 1 条 ----
    cache.clear();
    cache.setCapacity(0);
    {
        CHECK(cache.capacity() == 1);
        cache.put("only", "one");
        std::string v;
        CHECK(cache.get("only", v));
    }

    // ---- 6. 清空后不再命中 ----
    cache.clear();
    cache.setCapacity(16);
    {
        cache.put("x", "y");
        cache.clear();
        CHECK(cache.size() == 0);
        std::string v;
        CHECK(!cache.get("x", v));
    }

    // ---- 7. 命中率统计在 0.0 ~ 1.0 之间 ----
    cache.clear();
    cache.setCapacity(16);
    {
        std::string v;
        cache.put("hit", "1");
        cache.get("hit", v);   // 命中
        cache.get("miss", v);  // 未命中
        double rate = cache.hitRate();
        CHECK(rate > 0.0);
        CHECK(rate < 1.0);
    }

    std::cout << "word_cache: " << g_passed << " passed, " << g_failed << " failed" << std::endl;
    return g_failed == 0 ? 0 : 1;
}
