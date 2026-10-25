#ifndef WORD_ENTRY_HPP
#define WORD_ENTRY_HPP

/*
词条结构与词典行解析。

背景
----
早期 dict.txt 一行只有两列：`单词 释义`，服务端按"第一个空格"切一刀，
后半截整体当释义。客户端的单词卡片、收藏、生词本、导出却都是
「单词 / 释义 / 音标 / 例句 / 例句翻译」五字段，音标等三列只能长期留空，
等于协议里定义了字段却没有数据源。

现在词典升到 5 列，字段之间用 3 个空格分隔：

    abandonment␣␣␣n.放弃␣␣␣/ə'bændənmənt/␣␣␣The abandonment ...␣␣␣放弃旧计划...

为什么是"3 个空格"而不是单个空格：释义/例句/翻译内部本身含有空格
（"n. 女乘务员，空姐"），用单空格无法区分字段边界；用制表符又要改
既有词典文件。3 个空格在自然语言文本里几乎不出现，实测 7987 行词典里
没有一个字段内部含连续空格，切分结果稳定。

为什么需要兼容旧格式：线上库是旧词典导入的，运维也可能拿旧 dict.txt
直接覆盖。parse() 识别不到 3 空格分隔时回退到"第一个空格切一刀"，
音标/例句/翻译留空，行为与升级前完全一致，不会把整行塞进释义列。

解析是纯字符串函数、不依赖 SQLite 与网络，因此可以直接进 ctest
（test/test_dict_entry.cpp）。
*/

#include <cstddef>
#include <string>
#include <vector>

// 一条词条的全部字段，对应客户端 WordData 的前五个字段
struct WordEntry
{
    std::string word;        // 单词
    std::string mean;        // 词性 + 释义
    std::string phonetic;    // 音标（含两侧斜杠，如 /ə'bændənmənt/）
    std::string example;     // 英文例句
    std::string translation; // 例句翻译

    bool empty() const
    {
        return word.empty();
    }
};

namespace DictLine
{

// 词典文件的字段分隔符：三个空格
inline const std::string &fieldSeparator()
{
    static const std::string sep(3, ' ');
    return sep;
}

// 去掉首尾的空白与行尾 CR（词典文件是 CRLF，getline 会留下 '\r'）
inline std::string trim(const std::string &s)
{
    const char *ws = " \t\r\n";
    const std::size_t begin = s.find_first_not_of(ws);
    if (begin == std::string::npos)
    {
        return std::string();
    }
    const std::size_t end = s.find_last_not_of(ws);
    return s.substr(begin, end - begin + 1);
}

/*
按分隔符切分，最多切出 maxFields 段。
最后一段不再切分：即使例句翻译里意外出现分隔符，也整体归入翻译，
不会把句子尾巴当成第 6 个字段丢掉。
*/
inline std::vector<std::string> splitFields(const std::string &line, const std::string &sep,
                                            std::size_t maxFields)
{
    std::vector<std::string> out;
    if (maxFields == 0)
    {
        return out;
    }
    std::size_t pos = 0;
    while (out.size() + 1 < maxFields)
    {
        const std::size_t found = line.find(sep, pos);
        if (found == std::string::npos)
        {
            break;
        }
        out.push_back(line.substr(pos, found - pos));
        pos = found + sep.size();
    }
    out.push_back(line.substr(pos));
    return out;
}

/*
解析 dict.txt 的一行。
  - 新格式（3 空格分隔，最多 5 段）：单词/释义/音标/例句/翻译
  - 旧格式（无 3 空格）：退化为"第一个空格前是单词，其余是释义"
返回 false 表示该行是空行或没有单词，调用方应跳过。
*/
inline bool parse(const std::string &rawLine, WordEntry &out)
{
    const std::string line = trim(rawLine);
    if (line.empty())
    {
        return false;
    }

    const std::string &sep = fieldSeparator();
    if (line.find(sep) != std::string::npos)
    {
        const std::vector<std::string> fields = splitFields(line, sep, 5);
        // 字段按位置取值；不足 5 列时（例如只有单词+释义）后面几列留空
        out.word = (fields.size() > 0) ? trim(fields[0]) : std::string();
        out.mean = (fields.size() > 1) ? trim(fields[1]) : std::string();
        out.phonetic = (fields.size() > 2) ? trim(fields[2]) : std::string();
        out.example = (fields.size() > 3) ? trim(fields[3]) : std::string();
        out.translation = (fields.size() > 4) ? trim(fields[4]) : std::string();
    }
    else
    {
        // 旧格式兜底：第一个空格切一刀，整行剩余部分作为释义
        const std::size_t pos = line.find(' ');
        if (pos == std::string::npos)
        {
            return false; // 既无分隔符也无空格，不是合法词条
        }
        out.word = trim(line.substr(0, pos));
        out.mean = trim(line.substr(pos + 1));
        out.phonetic.clear();
        out.example.clear();
        out.translation.clear();
    }

    return !out.word.empty();
}

} // namespace DictLine

#endif // WORD_ENTRY_HPP
