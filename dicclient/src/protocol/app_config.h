#ifndef APP_CONFIG_H
#define APP_CONFIG_H
#include<QString>
    //应用程序配置结构
    struct AppConfig
    {
        // 服务器地址与端口默认指向生产环境。
        // 测试与 CI 环境可用 DICT_SERVER_IP / DICT_SERVER_PORT 环境变量覆盖：
        // 例如将地址指向 127.0.0.1 的未监听端口，集成测试会因连接失败
        // 确定性地走 GTEST_SKIP 分支，而不是超时失败。
        // 注意：环境变量在 AppConfig 构造时读取，NetworkManager 默认参数
        // 每次调用都会重新构造，因此覆盖即时生效。
        QString serverIp{qgetenv("DICT_SERVER_IP").isEmpty()
                             ? QString("47.100.244.110")
                             : QString::fromLocal8Bit(qgetenv("DICT_SERVER_IP"))};
        int serverPort{[]() {
            const QByteArray envPort = qgetenv("DICT_SERVER_PORT");
            bool ok = false;
            const int port = envPort.toInt(&ok);
            // 非法或越界的端口值回退到默认端口，避免配置静默失效
            return (ok && port > 0 && port < 65536) ? port : 8080;
        }()};
        int heartbeatInterval{10000}; //心跳间隔（毫秒）；配合存活看门狗，断网检测最坏≈15s，远快于被动等 TCP RTO(~20s)
        int reconnectInterval{3000}; //重连间隔（毫秒）
        int maxReconnectAttempts{100};//最大重连次数

        //应用配置
        QString appName{"网络电子词典"};
        QString version{"1.0.0"};
        int queryTimeout{3000}; //查询超时时间（毫秒）

        //用户界面配置
        int maxHistoryRecords{1000}; //最大历史记录数
        int maxCollectRecords{500}; //最大收藏记录数
        bool enableSound{true};     //启动声音
        bool autoPlayPronunciation{false};//自动播放发音

        //验证配置有效性
        bool isValid() const{
            return !serverIp.isEmpty()&&
                   serverPort>0 && serverPort<65536 &&
                   heartbeatInterval>0 &&
                   reconnectInterval>0 &&
                   maxReconnectAttempts>=0;
        }

    };

#endif // APP_CONFIG_H
