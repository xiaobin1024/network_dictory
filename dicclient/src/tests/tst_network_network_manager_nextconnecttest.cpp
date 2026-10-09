#include<gtest/gtest.h>
#include"network/network_manager.h"
#include <QCoreApplication>
#include<QtTest/QTest>
#include<QThread>

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


TEST(NetworkManagerTest,NetworkManagerNextConnectTest)
{
    // 确保有 QCoreApplication 实例
    ASSERT_NE(QCoreApplication::instance(), nullptr);
    //测试连接服务器断开服务器
    NetworkManager* manager = new NetworkManager();

    //建立连接前大量数据在队列中
    for(int i=0;i<500;i++)
    {
        CoreMessage::Msg msg;
        msg.type=CoreMessage::MsgType::SEARCH;
        CoreUtils::StringUtils::safeStringCopy(msg.name,QString("user500%1").arg(i).toUtf8().constData(),sizeof(msg.name));
        CoreUtils::StringUtils::safeStringCopy(msg.text,QString("Message500%1").arg(i).toUtf8().constData(),sizeof(msg.text));

        manager->sendMessageAsync(msg);
        QThread::msleep(5);     //稍微延迟
    }

    //连接服务器
    manager->connectToServer();
    //等待建立连接
    QTest::qWait(500);


    int initialMessages = 500;
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
    QTest::qWait(2000);
    //断开连接
    manager->disconnectFromServer();
    //等待断开时间
    QTest::qWait(500);

   delete manager;
}
