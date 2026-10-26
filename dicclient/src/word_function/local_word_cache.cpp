#include "local_word_cache.h"

LocalWordCache *LocalWordCache::s_instance = nullptr;

LocalWordCache *LocalWordCache::instance()
{
    if (!s_instance)
    {
        s_instance = new LocalWordCache();
    }
    return s_instance;
}

LocalWordCache::LocalWordCache(QObject *parent)
    : QObject(parent)
{
}

void LocalWordCache::put(const QString &word, const WordData &data)
{
    QMutexLocker lock(&m_mutex);
    auto it = m_map.find(word);
    if (it != m_map.end())
    {
        // 已存在：更新数据并提到表头
        it->second = data;
        m_lru.remove(word);
        m_lru.push_front(word);
        return;
    }
    m_map.emplace(word, data);
    m_lru.push_front(word);
    // 超出容量：淘汰表尾（最久未使用）
    while (static_cast<int>(m_map.size()) > m_capacity)
    {
        const QString &victim = m_lru.back();
        m_map.erase(victim);
        m_lru.pop_back();
    }
}

bool LocalWordCache::get(const QString &word, WordData &out) const
{
    QMutexLocker lock(&m_mutex);
    auto it = m_map.find(word);
    if (it == m_map.end())
    {
        return false;
    }
    // 命中即视为最近使用，提到表头
    m_lru.remove(word);
    m_lru.push_front(word);
    out = it->second;
    return true;
}

bool LocalWordCache::contains(const QString &word) const
{
    QMutexLocker lock(&m_mutex);
    return m_map.find(word) != m_map.end();
}

int LocalWordCache::size() const
{
    QMutexLocker lock(&m_mutex);
    return static_cast<int>(m_map.size());
}

void LocalWordCache::clear()
{
    QMutexLocker lock(&m_mutex);
    m_map.clear();
    m_lru.clear();
}
