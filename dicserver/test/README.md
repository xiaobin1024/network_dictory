# 服务端协议验证脚本

用 Python 标准库模拟客户端，不依赖 Qt，直接验证服务端定长协议与连接健壮性。

## 构建与运行（Linux）



```
\# 1. 构建服务端

cmake -S dicserver -B dicserver/build

cmake --build dicserver/build

cp dicserver/dict.txt dicserver/build/

\# 2. 启动服务端（终端 A）

cd dicserver/build

./server 0.0.0.0 8080

\# 3. 跑验证（终端 B，需在 build 目录下，脚本会读取 ../dict.txt）

cd dicserver/build

cp ../test/test\_dict.py ../test/test\_kick.py .

python3 test\_dict.py     # 10 项协议/分片/CPU 测试

python3 test\_kick.py     # 约 90s：死连接超时踢除，需耐心等待
```

## 覆盖项

test\_dict.py：



* 心跳 type=6 回 ALIVE

* 注册包 100 字节分片、1/400 / 剩余 三段刁钻分片

* 同一连接分片注册后继续登录（拼包缓冲多帧复用）

* 错误密码返回 PASSWORD\_ERROR

* 登录后查询单词返回释义

* 2 条空闲连接时 top 采样 CPU 应 < 5%（实测 0.0%）

* 两条报文一次性发送（粘包）应解析为两帧、各回一个 ACK

test\_kick.py：



* 登录后不再发任何字节（模拟掉网无 FIN），服务端应在约 90s 后关闭连接

* 踢除后立即新建连接并心跳，验证工作线程已释放、服务正常

## 最近一次实测结果（阿里云 Linux 3，g++ 10.2.1）



* test\_dict.py：PASS=10 FAIL=0

* test\_kick.py：死连接 92.4s 被踢，踢除后新连接正常