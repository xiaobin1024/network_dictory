#include <gtest/gtest.h>
#include "local_word_cache.h"

// 纯逻辑单元测试：不依赖网络/UI，验证 LRU 缓存的写入、读取、淘汰与更新语义。
// 这些语义是「断网兜底」与「弱网重复查询命中」的正确性基础。

TEST(LocalWordCacheTest, PutAndGet)
{
    LocalWordCache *cache = LocalWordCache::instance();
    cache->clear();

    WordData d("hello", "həˈləʊ", "你好", "Hello world.", "你好，世界。");
    cache->put("hello", d);

    WordData out;
    EXPECT_TRUE(cache->get("hello", out));
    EXPECT_EQ(out.word, "hello");
    EXPECT_EQ(out.meaning, "你好");
    EXPECT_EQ(out.phonetic, "həˈləʊ");
    EXPECT_EQ(out.example, "Hello world.");
    EXPECT_EQ(out.translation, "你好，世界。");

    EXPECT_TRUE(cache->contains("hello"));
    EXPECT_FALSE(cache->contains("world"));
}

TEST(LocalWordCacheTest, LRUCapacityEviction)
{
    LocalWordCache *cache = LocalWordCache::instance();
    cache->clear();

    // 写入 250 条，超过默认容量 200，最早写入的应被淘汰
    for (int i = 0; i < 250; ++i)
    {
        WordData d(QString::number(i), "m", "p", "e", "t");
        cache->put(QString::number(i), d);
    }
    EXPECT_EQ(cache->size(), LocalWordCache::kDefaultCapacity);

    // 最久未使用的 0..49 应被淘汰
    EXPECT_FALSE(cache->contains("0"));
    EXPECT_FALSE(cache->contains("49"));
    // 最近写入的应保留
    EXPECT_TRUE(cache->contains("249"));
    EXPECT_TRUE(cache->contains("200"));
}

TEST(LocalWordCacheTest, RecentUsePromotes)
{
    LocalWordCache *cache = LocalWordCache::instance();
    cache->clear();

    for (int i = 0; i < LocalWordCache::kDefaultCapacity; ++i)
    {
        WordData d(QString::number(i), "m", "p", "e", "t");
        cache->put(QString::number(i), d);
    }

    // 满容量时访问 "0" 把它提到表头（最近使用），再写入新词应淘汰 "1" 而非 "0"
    WordData out;
    ASSERT_TRUE(cache->get("0", out));
    WordData d("new", "m", "p", "e", "t");
    cache->put("new", d);

    EXPECT_TRUE(cache->contains("0"));   // 被提到表头，未被淘汰
    EXPECT_FALSE(cache->contains("1"));   // 最久未使用，被淘汰
}

TEST(LocalWordCacheTest, UpdateExistingEntry)
{
    LocalWordCache *cache = LocalWordCache::instance();
    cache->clear();

    cache->put("apple", WordData("apple", "ph", "old-meaning", "e", "t"));
    cache->put("apple", WordData("apple", "ph", "new-meaning", "e", "t"));

    WordData out;
    ASSERT_TRUE(cache->get("apple", out));
    EXPECT_EQ(out.meaning, "new-meaning");  // 数据被更新（释义字段）
    EXPECT_EQ(cache->size(), 1);            // 未新增条目
}
