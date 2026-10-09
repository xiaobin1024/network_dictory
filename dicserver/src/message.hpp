#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include<myhead.h>
//操作类型的定义
#define R 1     //用户注册
#define L 2     //用户登录
#define Q 3     //用户退出
#define S 4     //单词查询
#define H 5     //历史记录
#define A 6     //心跳检测
#define C 7     //单词收藏
#define D 8     //单词收藏记录

//定义客户端与服务器进行数据通信的协议
#pragma pack(push, 1) 
struct Msg
{
    //操作类型
    int type;
    //用户名
    char name[20];
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