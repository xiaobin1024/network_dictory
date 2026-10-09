#include<gtest/gtest.h>
#include"network/network_manager.h"
#include <QCoreApplication>
#include<QtTest/QTest>
#include<QThread>
#include<QVector>
// 创建全局的 QCoreApplication 实例
class QtTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        int argc = 0;
        char* argv[] = {nullptr};
        app = new QCoreApplication(argc, argv);
    }

    void TearDown() override {
        delete app;
    }

private:
    QCoreApplication* app;
};

// 在 main 函数之前注册环境
::testing::Environment* const qt_env = ::testing::AddGlobalTestEnvironment(new QtTestEnvironment);
// TEST(NetworkManagerTest, NetworkManagerIsConnectedTest)
// {
//     // 直接创建对象，不使用fixture
//     NetworkManager* manager = new NetworkManager();

//     // 测试初始状态应该是未连接
//     EXPECT_FALSE(manager->isConnected());

//     delete manager;
// }

// TEST(NetworkManagerTest,NetworkManagerConnectToServerTest)
// {
//     NetworkManager* manager = new NetworkManager();
//     manager->connectToServer("",8080);
//     EXPECT_FALSE(manager->isConnected());

//     manager->connectToServer("127.0.0.1",-1);
//     EXPECT_FALSE(manager->isConnected());
//     delete manager;
// }

// TEST(NetworkManagerTest,NetworkManagerDisconnectFromServerTest)
// {
//     //测试在未链接状态下断开连接
//     NetworkManager* manager = new NetworkManager();
//     EXPECT_NO_THROW(manager->disconnectFromServer());
//     delete manager;
// }

// TEST(NetworkManagerTest,NetworkManagerConnectTest)
// {
//     // 确保有 QCoreApplication 实例
//     ASSERT_NE(QCoreApplication::instance(), nullptr);
//     //测试连接服务器断开服务器
//     NetworkManager* manager = new NetworkManager();

//     // QObject::connect(manager, &NetworkManager::connectionEstablished, manager, []() {
//     //     qDebug() << "Main: Connection established!";
//     // });

//     // QObject::connect(manager, &NetworkManager::networkError, manager, [](const QString &error) {
//     //     qDebug() << "Main: Network error:" << error;
//     // });

//     // QObject::connect(manager,&NetworkManager::connectionLost,manager,[](){
//     //     qDebug() << "Main: Network disconnected";
//     // });

//     //创建消息结构体
//     CoreMessage::Msg sendMsg;
//     sendMsg.type=CoreMessage::MsgType::REGISTER;
//     strncpy(sendMsg.name,"testusr1",sizeof(sendMsg.name)-1);
//     strncpy(sendMsg.text,"testlogin",sizeof(sendMsg.text)-1);

//     //连接服务器
//     manager->connectToServer("192.168.23.132",8080);
//     //等待建立连接
//     QTest::qWait(1000);
//     //验证连接状态
//     EXPECT_TRUE(manager->isConnected());

//     //测试发送消息
//     bool sent=manager->sendMessage(sendMsg);
//     EXPECT_TRUE(sent);

//     QTest::qWait(1000);  // 等待缓冲区有数据

//     //断开连接
//     manager->disconnectFromServer();
//     //等待断开时间
//     QTest::qWait(1000);
//     //验证断开状态
//     EXPECT_FALSE(manager->isConnected());
//     delete manager;
// }

// TEST(NetworkManagerTest,NetworkManagerMessageQueueTest)
// {
//     // 确保有 QCoreApplication 实例
//     ASSERT_NE(QCoreApplication::instance(), nullptr);
//     //测试连接服务器断开服务器
//     NetworkManager* manager = new NetworkManager();
//     //创建消息结构体
//     CoreMessage::Msg msg1;
//     msg1.type=CoreMessage::MsgType::REGISTER;
//     strncpy(msg1.name,"testusr1",sizeof(msg1.name)-1);
//     strncpy(msg1.text,"testlogin",sizeof(msg1.text)-1);

//     CoreMessage::Msg msg2;
//     msg2.type=CoreMessage::MsgType::REGISTER;
//     strncpy(msg2.name,"testusr2",sizeof(msg2.name)-1);
//     strncpy(msg2.text,"testlogin",sizeof(msg2.text)-1);

//     CoreMessage::Msg msg3;
//     msg3.type=CoreMessage::MsgType::REGISTER;
//     strncpy(msg3.name,"testusr3",sizeof(msg3.name)-1);
//     strncpy(msg3.text,"testlogin",sizeof(msg3.text)-1);

//     CoreMessage::Msg msg4;
//     msg4.type=CoreMessage::MsgType::REGISTER;
//     strncpy(msg4.name,"testusr4",sizeof(msg4.name)-1);
//     strncpy(msg4.text,"testlogin",sizeof(msg4.text)-1);

//     manager->sendMessageAsync(msg1);
//     manager->sendMessageAsync(msg2);
//     manager->sendMessageAsync(msg3);

//     //获取队列中的数量
//     int pendingCount=manager->getPendingMessageCount();
//     qDebug()<<"Pending messages: "<<pendingCount;


//     //连接服务器
//     manager->connectToServer();
//     //等待建立连接
//     QTest::qWait(1000);
//     //验证连接状态
//     EXPECT_TRUE(manager->isConnected());

//     //插入一条新的消息
//      manager->sendMessageAsync(msg4);
//     manager->sendMessageAsync(msg1);
//     manager->sendMessageAsync(msg2);
//     manager->sendMessageAsync(msg3);


//     QTest::qWait(10000);  // 等待缓冲区有数据

//     //断开连接
//     manager->disconnectFromServer();
//     //等待断开时间
//     QTest::qWait(1000);
//     //验证断开状态
//     EXPECT_FALSE(manager->isConnected());
//     delete manager;

// }

//测试测试心跳和消息队列同时触发
// TEST(NetworkManagerTest,NetworkManagerMessageQueueHeartbeatTest)
// {
//     // 确保有 QCoreApplication 实例
//     ASSERT_NE(QCoreApplication::instance(), nullptr);
//     //测试连接服务器断开服务器
//     NetworkManager* manager = new NetworkManager();

//     //建立连接前大量数据在队列中
//     for(int i=0;i<50;i++)
//     {
//         CoreMessage::Msg msg;
//         msg.type=CoreMessage::MsgType::SEARCH;
//         CoreUtils::StringUtils::safeStringCopy(msg.name,QString("user500%1").arg(i).toUtf8().constData(),sizeof(msg.name));
//         CoreUtils::StringUtils::safeStringCopy(msg.text,QString("Message500%1").arg(i).toUtf8().constData(),sizeof(msg.text));

//         manager->sendMessageAsync(msg);
//         QThread::msleep(5);     //稍微延迟
//     }


//     //连接服务器
//     manager->connectToServer();
//     //等待建立连接
//     QTest::qWait(500);
//     //测试1：大量消息发送时触发心跳
//     qDebug()<<"\n=== Test 1: Heartbeart during heavy message sending ===";
//     for(int i=0;i<500;i++)
//     {
//         CoreMessage::Msg msg;
//         msg.type=CoreMessage::MsgType::SEARCH;
//         CoreUtils::StringUtils::safeStringCopy(msg.name,QString("user%1").arg(i).toUtf8().constData(),sizeof(msg.name));
//         CoreUtils::StringUtils::safeStringCopy(msg.text,QString("Message %1").arg(i).toUtf8().constData(),sizeof(msg.text));

//         manager->sendMessageAsync(msg);
//         QThread::msleep(5);     //稍微延迟
//     }

//     //等待一段时间让消息发送完成
//     QTest::qWait(2000);

//     // 统计收到的消息类型
//     int heartbeatCount = 0;
//     int chatMessageCount = 0;
//     for (const auto& type : manager->receivedMessageTypes) {
//         if (type == CoreMessage::MsgType::HEARTBEAT) {
//             heartbeatCount++;
//         } else if (type == CoreMessage::MsgType::SEARCH) {
//             chatMessageCount++;
//         }
//     }

//     qDebug() << "Heartbeat messages received:" << heartbeatCount;
//     qDebug() << "Search messages received:" << chatMessageCount;

//     //断开连接
//     manager->disconnectFromServer();
//     //等待断开时间
//     QTest::qWait(500);
//     delete manager;
// }

//测试当客户端网络断开，能否把发送失败的消息重新发送
TEST(NetworkManagerTest,NetworkManagerMessageNextSendText)
{
    // 确保有 QCoreApplication 实例
    ASSERT_NE(QCoreApplication::instance(), nullptr);
    //测试连接服务器断开服务器
    NetworkManager* manager = new NetworkManager();
    //连接服务器
    manager->connectToServer();
    //等待建立连接
    QTest::qWait(500);

    // 5. 发送一些消息
    qDebug() << "\nStep 2: Sending initial messages...";

    int initialMessages = 5;
    for (int i = 1; i <= initialMessages; ++i) {
        CoreMessage::Msg msg;
        msg.type = CoreMessage::MsgType::SEARCH;

        QString name = QString("user%1").arg(i);
        QString text = QString("Initial message %1").arg(i);

        CoreUtils::StringUtils::safeStringCopy(msg.name,
                                               name.toUtf8().constData(), sizeof(msg.name));
        CoreUtils::StringUtils::safeStringCopy(msg.text,
                                               text.toUtf8().constData(), sizeof(msg.text));

        manager->sendMessageAsync(msg);
        qDebug() << "Sent message:" << text;

        QTest::qWait(10);  // 小延迟，避免发送太快
    }

    // 等待消息发送完成
    QTest::qWait(1000);
    //断开连接
    manager->disconnectFromServer();
    //等待断开时间
    QTest::qWait(500);


    //在断开连接的情况下发送消息
    qDebug() << "断开连接时发送消息（应该进入队列）...";

    int queuedMessages = 5;
    for (int i = 1; i <= queuedMessages; ++i) {
        CoreMessage::Msg msg;
        msg.type = CoreMessage::MsgType::SEARCH;

        QString name = QString("user%1").arg(i + 10);
        QString text = QString("断开连接时发送的消息 %1").arg(i);

        CoreUtils::StringUtils::safeStringCopy(msg.name,
                                               name.toUtf8().constData(), sizeof(msg.name));
        CoreUtils::StringUtils::safeStringCopy(msg.text,
                                               text.toUtf8().constData(), sizeof(msg.text));

        manager->sendMessageAsync(msg);
        qDebug() << "排队消息:" << text;

        QTest::qWait(50);
    }

    // 检查队列中的消息数量
    int pendingCount = manager->getPendingMessageCount();
    qDebug() << "当前队列中的消息数量:" << pendingCount;

    // 重新连接
    qDebug() << "重新连接服务器...";
    manager->connectToServer();
    QTest::qWait(1000);

    //检查队列是否被清空
    int finalQueueSize = manager->getPendingMessageCount();
    if (finalQueueSize == 0) {
        qDebug() << "✓ 测试通过：所有队列中的消息在重新连接后都已发送";
    } else {
        qDebug() << "✗ 测试失败：队列中还有" << finalQueueSize << "条消息未发送";
    }


    manager->disconnectFromServer();
    QTest::qWait(500);
    delete manager;
}
