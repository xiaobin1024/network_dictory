#ifndef LOCAL_WORD_CACHE_H
#define LOCAL_WORD_CACHE_H

#include <QObject>
#include <QString>
#include <QMutex>
#include <list>
#include <unordered_map>
#include "word_data.h"

// 客户端本地查询缓存：成功查到的词条按 LRU 保留最近 N 条。
// 用途：
//   ① 断网兜底——未连接服务器时，若本地缓存已有该词，直接回退展示，避免用户完全不可用；
//   ② 弱网/重复查询即时命中，减少服务器压力。
// 设计边界：缓存只是服务端数据的本地副本。联网时 searchWord 仍以服务端返回为准，
// 不会跳过服务器用缓存（保证数据时效），仅在离线且命中缓存时回退。
class LocalWordCache : public QObject
{
    Q_OBJECT
public:
    static LocalWordCache *instance();

    // 成功查到的词条写入缓存（已存在则更新数据并提到表头）
    void put(const QString &word, const WordData &data);
    // 取缓存；命中返回 true 并把数据写入 out（并视为最近使用提到表头）
    bool get(const QString &word, WordData &out) const;
    bool contains(const QString &word) const;
    int size() const;
    void clear();

    static constexpr int kDefaultCapacity = 200;

private:
    explicit LocalWordCache(QObject *parent = nullptr);

    mutable QMutex m_mutex;
    int m_capacity{kDefaultCapacity};
    mutable std::list<QString> m_lru;                  // 表头 = 最近使用
    std::unordered_map<QString, WordData> m_map;
    static LocalWordCache *s_instance;
};

#endif // LOCAL_WORD_CACHE_H
