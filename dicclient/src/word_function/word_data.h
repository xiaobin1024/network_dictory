#ifndef WORD_DATA_H
#define WORD_DATA_H
/*
 * 单词数据结构
 *
 * 用于存储单词的基本信息
 */

#include<QString>
#include"QTimer"

struct WordData{
    QString word;           // 单词
    QString meaning;     // 词性与释义
    QString phonetic;       // 音标
    QString example;        // 例句
    QString translation;    // 句意
    bool isCollected;    //收藏标识
    bool isVocabulary;  //生词标识

    // 默认构造函数
    WordData() = default;

    // 常用构造函数
    WordData(const QString& w, const QString& p, const QString& d,
              const QString& e = "",const QString& t="",bool c=false,bool v=false)
        : word(w)
        , phonetic(p)
        , meaning(d)
        , example(e)
        ,translation(t)
        ,isCollected(c)
        ,isVocabulary(v)
    {}

    // 检查数据是否有效
    bool isValid() const {
        return !word.isEmpty() && !meaning.isEmpty();

    }
};

#endif // WORD_DATA_H
