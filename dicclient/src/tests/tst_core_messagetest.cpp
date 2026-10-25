#include <gtest/gtest.h>
#include"protocol/message.h"
#include<QDebug>
#include<QDataStream>
#include<QBuffer>
#include<QString>
#include<QtEndian>
using namespace CoreMessage;


TEST(CoreTests,CoreMessageTest)
{
    //消息类型测试，验证每个枚举成员的数值是否符合定义
    EXPECT_EQ(static_cast<int>(MsgType::REGISTER),1);
    EXPECT_EQ(static_cast<int>(MsgType::LOGIN),2);
    EXPECT_EQ(static_cast<int>(MsgType::Quit),3);
    EXPECT_EQ(static_cast<int>(MsgType::SEARCH),4);
    EXPECT_EQ(static_cast<int>(MsgType::HISTORY),5);
    EXPECT_EQ(static_cast<int>(MsgType::HEARTBEAT),6);
    EXPECT_EQ(static_cast<int>(MsgType::COLLECT),7);
    EXPECT_EQ(static_cast<int>(MsgType::COLLECTList),8);
    //验证MsgType类型是否为int
    EXPECT_EQ(sizeof(MsgType),sizeof(int));
}

TEST(CoreTests, CoreMessageOutputTest)
{
    //直接测试operator<<函数
    QString output;
    QDebug debug(&output);
    debug << MsgType::REGISTER;

    // 输出应该包含REGISTER和1
    EXPECT_TRUE(output.contains("REGISTER"));
    EXPECT_TRUE(output.contains("1"));

    debug << MsgType::LOGIN;
    // 输出应该包含LOGIN和2
    EXPECT_TRUE(output.contains("LOGIN"));
    EXPECT_TRUE(output.contains("2"));

    debug << MsgType::Quit;
    // 输出应该包含Quit和3
    EXPECT_TRUE(output.contains("Quit"));
    EXPECT_TRUE(output.contains("3"));

    debug << MsgType::SEARCH;
    // 输出应该包含SEARCH和4
    EXPECT_TRUE(output.contains("SEARCH"));
    EXPECT_TRUE(output.contains("4"));

    debug << MsgType::HISTORY;
    // 输出应该包含HISTORY和5
    EXPECT_TRUE(output.contains("HISTORY"));
    EXPECT_TRUE(output.contains("5"));

    debug << MsgType::HEARTBEAT;
    // 输出应该包含HEARTBEAT和6
    EXPECT_TRUE(output.contains("HEARTBEAT"));
    EXPECT_TRUE(output.contains("6"));

    debug << MsgType::COLLECT;
    // 输出应该包含COLLECT和7
    EXPECT_TRUE(output.contains("COLLECT"));
    EXPECT_TRUE(output.contains("7"));

    debug << MsgType::COLLECTList;
    // 输出应该包含QUERYCOLLECT和8
    EXPECT_TRUE(output.contains("COLLECTList"));
    EXPECT_TRUE(output.contains("8"));
}

TEST(CoreTests,CoreMessageCoreByteOrderTest)
{
    //测试1，创建Msg并测试字节序转换
    Msg msg;
    msg.type=MsgType::REGISTER;
    strncpy(msg.name,"testuser",sizeof(msg.name));
    strncpy(msg.text,"Hello Word",sizeof(msg.text));
    //保存原始值
    MsgType originalType=msg.type;
    //转换为网络字节序
    msg.toNetWorkByteOrder();
    //验证类型没有丢失，只是字节序改变
    EXPECT_TRUE(static_cast<int>(msg.type)!=0);
    //转为主机字节序
    msg.toHostByteOrder();
    //验证类型恢复为原始值
    EXPECT_EQ(msg.type,originalType);
    //验证其他字段没有被修改
    EXPECT_STREQ(msg.name,"testuser");
    EXPECT_STREQ(msg.text,"Hello Word");

}

TEST(CoreTests,CoreMessageMsgQDebugOutputTest)
{
    Msg msg;
    msg.type=MsgType::REGISTER;
    strncpy(msg.name,"testuser",sizeof(msg.name));
    strncpy(msg.text,"Hello Word",sizeof(msg.text));
    //测试debug输出
    QString output;
    QDebug debug(&output);
    debug<<msg;
    //验证输出包含所有字段值
    EXPECT_TRUE(output.contains("Msg{"));
     EXPECT_TRUE(output.contains("type="));
     EXPECT_TRUE(output.contains("REGISTER"));
     EXPECT_TRUE(output.contains("name="));
     EXPECT_TRUE(output.contains("testuser"));
     EXPECT_TRUE(output.contains("text="));
     EXPECT_TRUE(output.contains("Hello Word"));
     EXPECT_TRUE(output.contains("}"));
     //打印实际输出以便于调试
     std::cout<<"Msg输出："<<output.toStdString()<<std::endl;
}
