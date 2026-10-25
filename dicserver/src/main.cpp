#include"database_manager.hpp"
#include"server.hpp"
#include"connectionPool.hpp"
#include <iostream>
#include <memory>
#include <cstdlib>
#include <csignal>

using namespace std;

int main(int argc, const char *argv[])
{
    if(argc<3)
    {
        cerr<<"请输入IP地址和端口号"<<endl;
    }
    // 向已关闭的连接写数据时只返回 EPIPE，避免 SIGPIPE 直接终止进程
    signal(SIGPIPE, SIG_IGN);
    try
    {
        //初始化连接池(参数1：数据库路径，参数2：初始连接数，参数3：最大空闲连接数)
        ConnectionPool::getInstance().init("database.db",10,30);
        // 使用单例模式获取数据库管理对象（使用引用，注意访问语法）
        DatabaseManager& db_manager = DatabaseManager::getInstance();
        //创建线程池
        auto thread_pool = make_shared<threadPool>(4);
        //初始化数据库
        if(!db_manager.initalizeDatabase())
        {
            cerr<<"数据库初始化失败，请检查："<<endl;
            cerr<<"1. dict.txt文件是否存在"<<endl;
            cerr<<"2. 当前目录是否具备有写权限"<<endl;
            return -1;
        }
        //构造一个服务器
        Server server(db_manager,thread_pool,argv[1],atoi(argv[2]));
        //启动服务器
        server.start();
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return -1;
    }
    
    return 0;
}