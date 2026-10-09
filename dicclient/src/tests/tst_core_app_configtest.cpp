#include <gtest/gtest.h>
#include"protocol/app_config.h"

TEST(CoreTests,CoreAppConfigTest)
{
    //测试1，默认配置应该有效
    AppConfig config;
    EXPECT_TRUE(config.isValid());
    //测试2，验证关键默认值
    EXPECT_EQ(config.serverIp,"127.0.0.1");
    EXPECT_EQ(config.serverPort,8080);
    EXPECT_EQ(config.heartbeatInterval,1000);
    //测试3，边界条件（无效配置）
    AppConfig invalidConfig;
    invalidConfig.serverPort=0; //无效端口
    EXPECT_FALSE(invalidConfig.isValid());

    invalidConfig=AppConfig();
    invalidConfig.serverIp="";
     EXPECT_FALSE(invalidConfig.isValid());
}
