#include"database_manager.hpp"
#include"server.hpp"
#include"connectionPool.hpp"
#include"word_cache.hpp"
#include <iostream>
#include <memory>
#include <string>
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

    // 压测开关：DICT_DISABLE_WORD_CACHE=1 关闭热点词缓存，
    // 用于在同一二进制、同一台机器上做"有缓存/无缓存"的 A/B 对比。
    const char *no_cache = getenv("DICT_DISABLE_WORD_CACHE");
    if (no_cache != nullptr && string(no_cache) == "1")
    {
        WordCache::getInstance().setEnabled(false);
        cout << "[config] 热点词缓存已关闭（DICT_DISABLE_WORD_CACHE=1）" << endl;
    }
    else
    {
        cout << "[config] 热点词缓存已开启，容量=" << WordCache::getInstance().capacity() << " 条" << endl;
    }

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
        //启动服务器（阻塞运行，直到收到 SIGINT/SIGTERM 触发优雅关闭）
        server.start();
        // 优雅关闭收尾：先停止接收新任务并唤醒空闲工作线程，再等待在途任务执行完毕。
        // 空闲线程立即退出；正在处理连接的线程最多再等一个收发超时周期（10s）后退出，
        // 故整体等待有上界。此时 server 与 db_manager 仍在作用域内，避免析构顺序竞态。
        thread_pool->shutdown();
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return -1;
    }
    
    return 0;
}