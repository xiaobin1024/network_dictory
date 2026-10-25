#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include <arpa/inet.h> // htonl / ntohl
//操作类型的定义
#define R 1     //用户注册
#define L 2     //用户登录
#define Q 3     //用户退出
#define S 4     //单词查询
#define H 5     //历史记录
#define A 6     //心跳检测
#define C 7     //单词收藏
#define D 8     //单词收藏记录
#define U 9     //用户注销（删除账号）
#define V 10    //生词本增删（状态切换）
#define W 11    //生词本记录列表
// 协议支持的最大消息类型，收到超过该值的报文直接丢弃
#define MAX_MSG_TYPE 11

//定义客户端与服务器进行数据通信的协议
#pragma pack(push, 1)
struct Msg
{
    //操作类型
    int type;
    //用户名
    // 注意：缓冲区必须大于客户端允许的最大用户名长度（20 字符）+ 1，
    // 否则 20 字符用户名会恰好填满缓冲区而没有 '\0' 终止符，
    // 导致服务端按 C 字符串读取时越界或被 safeStrcpy 截断。
    char name[32];
    //文本内容(密码/单词)
    char text[1280];

    //定义相关的字节序转换函数
    //主机字节序转换成网络字节序
    void networkByteOrder()
    {
        type = htonl(type);
    }
    //网络字节序转换成主机字节序
    void hostByteOrder()
    {
        type = ntohl(type);
    }
};
#pragma pack(pop)
#endif