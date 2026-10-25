#include <gtest/gtest.h>
#include"protocol/app_config.h"

TEST(CoreTests,CoreAppConfigTest)
{
    //测试1，默认配置应该有效
    AppConfig config;
    EXPECT_TRUE(config.isValid());
    //测试2，验证关键默认值（只校验不变量，不写死会随环境变化的具体值）
    EXPECT_FALSE(config.serverIp.isEmpty()) << "默认服务器地址不应为空";
    //端口默认 8080；若外部通过 DICT_SERVER_PORT 覆盖，则以覆盖值为准
    if (qgetenv("DICT_SERVER_PORT").isEmpty()) {
        EXPECT_EQ(config.serverPort,8080);
    } else {
        EXPECT_EQ(config.serverPort,qgetenv("DICT_SERVER_PORT").toInt());
    }
    EXPECT_GT(config.heartbeatInterval,0) << "心跳间隔必须为正";
    //测试3，边界条件（无效配置）
    AppConfig invalidConfig;
    invalidConfig.serverPort=0; //无效端口
    EXPECT_FALSE(invalidConfig.isValid());

    invalidConfig=AppConfig();
    invalidConfig.serverIp="";
     EXPECT_FALSE(invalidConfig.isValid());
}

// 环境变量覆盖：DICT_SERVER_IP / DICT_SERVER_PORT 可覆盖默认服务器地址，
// 供 CI（指向不可达地址使集成测试跳过）与本地联调使用。
// 每个测试文件是独立可执行文件，且本用例结束前恢复环境，不影响其他用例。
TEST(CoreTests,CoreAppConfigEnvOverrideTest)
{
    const QByteArray oldIp = qgetenv("DICT_SERVER_IP");
    const QByteArray oldPort = qgetenv("DICT_SERVER_PORT");
    // 用例结束时恢复原始环境
    struct EnvRestore {
        const QByteArray& ip;
        const QByteArray& port;
        ~EnvRestore() {
            if (ip.isEmpty()) { qunsetenv("DICT_SERVER_IP"); }
            else { qputenv("DICT_SERVER_IP", ip); }
            if (port.isEmpty()) { qunsetenv("DICT_SERVER_PORT"); }
            else { qputenv("DICT_SERVER_PORT", port); }
        }
    } restore{oldIp, oldPort};

    // 合法覆盖值应即时生效
    qputenv("DICT_SERVER_IP", QByteArray("127.0.0.1"));
    qputenv("DICT_SERVER_PORT", QByteArray("19099"));
    AppConfig overridden;
    EXPECT_EQ(overridden.serverIp, QString("127.0.0.1"));
    EXPECT_EQ(overridden.serverPort, 19099);
    EXPECT_TRUE(overridden.isValid());

    // 非法端口值回退到默认端口，而不是静默变成 0 使配置失效
    qputenv("DICT_SERVER_PORT", QByteArray("not-a-port"));
    AppConfig invalidPort;
    EXPECT_EQ(invalidPort.serverPort, 8080);
    EXPECT_TRUE(invalidPort.isValid());

    // 空 IP 覆盖视为未设置，回退到默认地址
    qputenv("DICT_SERVER_IP", QByteArray(""));
    AppConfig emptyIp;
    EXPECT_FALSE(emptyIp.serverIp.isEmpty());
}
