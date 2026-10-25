#ifndef WORD_CACHE_HPP
#define WORD_CACHE_HPP

/*
热点词缓存：查词最高频路径的 LRU 内存缓存。

设计要点：
1. LRU 淘汰：unordered_map + 双向链表，O(1) 命中与淘汰；
   容量可配置（默认 4096 条，按词条平均几十字节估算约几百 KB 内存）。
2. 只缓存 dict 表词条（纯读、无用户维度），缓存键即单词本身。
   缓存值是完整的 WordEntry（释义/音标/例句/翻译），不是只有释义：
   一次查词需要 4 个字段，缓存只给释义的话剩下 3 列还是得回表，
   缓存就白做了。
3. 写路径无需失效：dict 表运行时只读（启动导入后不再变更），
   收藏/生词本标志由调用方按用户单独查询，不进入本缓存。
4. 线程安全：内部互斥锁，服务端多工作线程并发查词时安全。
5. 与 DB 层的关系：缓存未命中才走 SQLite，命中直接返回，
   把"一次 SQLite 查询 + 表级锁"降为"一次哈希查找"。

用法：
    WordCache &cache = WordCache::getInstance();
    WordEntry entry;
    if (!cache.get(word, entry)) {
        // 未命中：查 DB 后回填
        cache.put(word, entry);
    }
*/

#include "word_entry.hpp"

#include <string>
#include <list>
#include <unordered_map>
#include <mutex>
#include <cstddef>

class WordCache
{
public:
    static WordCache &getInstance()
    {
        static WordCache instance;
        return instance;
    }

    // 查询缓存；命中返回 true 并通过 out 返回完整词条
    bool get(const std::string &word, WordEntry &out)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!enabled_)
        {
            ++miss_count_;
            return false;
        }
        auto it = index_.find(word);
        if (it == index_.end())
        {
            ++miss_count_;
            return false;
        }
        // 命中：移到链表头（最近使用）
        items_.splice(items_.begin(), items_, it->second);
        out = it->second->second;
        ++hit_count_;
        return true;
    }

    // 写入/更新缓存
    void put(const std::string &word, const WordEntry &entry)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!enabled_)
            return;
        auto it = index_.find(word);
        if (it != index_.end())
        {
            // 已存在：更新值并提到链表头
            it->second->second = entry;
            items_.splice(items_.begin(), items_, it->second);
            return;
        }
        // 容量满：淘汰链表尾（最久未用）
        if (items_.size() >= capacity_)
        {
            const std::string &victim = items_.back().first;
            index_.erase(victim);
            items_.pop_back();
            ++evict_count_;
        }
        items_.emplace_front(word, entry);
        index_[word] = items_.begin();
    }

    // 统计信息（命中率监控）
    size_t size() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return items_.size();
    }
    size_t hitCount() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return hit_count_;
    }
    size_t missCount() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return miss_count_;
    }
    size_t evictCount() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return evict_count_;
    }

    // 命中率，0.0 ~ 1.0；无访问记录时返回 0.0
    double hitRate() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t total = hit_count_ + miss_count_;
        if (total == 0)
            return 0.0;
        return static_cast<double>(hit_count_) / static_cast<double>(total);
    }

    // 容量（条目数上限）
    size_t capacity() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return capacity_;
    }

    // 设置容量；缩小到小于当前条目数时会立即淘汰尾部多余条目。
    // 保留统计计数不变，便于压测前后对比。
    void setCapacity(size_t capacity)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        capacity_ = (capacity == 0) ? 1 : capacity; // 0 容量会让 put 死循环淘汰，兜底为 1
        while (items_.size() > capacity_)
        {
            index_.erase(items_.back().first);
            items_.pop_back();
            ++evict_count_;
        }
    }

    // 压测开关：关闭后 get() 恒未命中、put() 直接丢弃，
    // 用于在同一二进制、同一台机器上做"有缓存 / 无缓存"的 A/B 对比（环境变量控制）。
    void setEnabled(bool enabled)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = enabled;
    }
    bool enabled() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    // 清空缓存与统计。词典重新导入（dict 表内容变化）后调用，避免读到旧释义。
    void clear()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        items_.clear();
        index_.clear();
    }

private:
    WordCache() = default;
    WordCache(const WordCache &) = delete;
    WordCache &operator=(const WordCache &) = delete;

    // 双向链表：front = 最近使用，back = 最久未用（淘汰候选）
    using ItemList = std::list<std::pair<std::string, WordEntry>>;

    static constexpr size_t kDefaultCapacity = 4096;

    ItemList items_;
    std::unordered_map<std::string, ItemList::iterator> index_;
    size_t capacity_{kDefaultCapacity};
    bool enabled_{true};
    size_t hit_count_{0};
    size_t miss_count_{0};
    size_t evict_count_{0};
    mutable std::mutex mutex_;
};

#endif // WORD_CACHE_HPP
