#ifndef MESSAGE_H
#define MESSAGE_H
#include<QtEndian>
#include<QtDebug>

namespace CoreMessage {

//消息枚举类型
enum class MsgType:int{
    REGISTER=1,
    LOGIN=2,
    Quit=3,
    SEARCH=4,
    HISTORY=5,
    HEARTBEAT=6,
    COLLECT=7,
    COLLECTList=8,
    UNREGISTER=9,
    VOCABULARY=10,
    VOCABULARYLIST=11

};
//QDebug输出支持
inline QDebug operator<<(QDebug debug,MsgType type)
{
    const char* typeStr=nullptr;
    switch (type) {
    case MsgType::REGISTER: typeStr="REGISTER"; break;
    case MsgType::LOGIN: typeStr="LOGIN"; break;
    case MsgType::Quit: typeStr="Quit"; break;
    case MsgType::SEARCH: typeStr="SEARCH"; break;
    case MsgType::HISTORY: typeStr="HISTORY"; break;
    case MsgType::HEARTBEAT: typeStr="HEARTBEAT"; break;
    case MsgType::COLLECT: typeStr="COLLECT"; break;
    case MsgType::COLLECTList: typeStr="COLLECTList"; break;
    case MsgType::UNREGISTER: typeStr="UNREGISTER"; break;
    case MsgType::VOCABULARY: typeStr="VOCABULARY"; break;
    case MsgType::VOCABULARYLIST: typeStr="VOCABULARYLIST"; break;
    default: typeStr="UNKNOWN"; break;
    }
    debug<<typeStr<<"("<<static_cast<int>(type)<<")";
    return debug;
}
#pragma pack(push,1)
struct Msg
{
    MsgType type;
    char name[20];
    char text[1280];
    //主机字节序->网络字节序
    void toNetWorkByteOrder()
    {
        type = static_cast<MsgType>(qToBigEndian(static_cast<quint32>(type)));
    }
    //网络字节序->主机字节序
    void toHostByteOrder()
    {
        type = static_cast<MsgType>(qFromBigEndian(static_cast<quint32>(type)));
    }
};
#pragma pack(pop)

//Msg结构体的QDebug输出
inline QDebug operator <<(QDebug debug,const Msg& msg)
{
    debug.nospace()<<"Msg{"
                    <<"type="<<msg.type
                    <<", name="<<msg.name
                    <<", text="<<msg.text
                    <<"}";
    return debug.space();
}
}


#endif // MESSAGE_H
