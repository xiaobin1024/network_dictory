#ifndef APP_CONFIG_H
#define APP_CONFIG_H
#include<QString>
    //应用程序配置结构
    struct AppConfig
    {
        QString serverIp{"192.168.23.132"};
        int serverPort{8080};
        int heartbeatInterval{1000}; //心跳间隔（毫秒）
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
