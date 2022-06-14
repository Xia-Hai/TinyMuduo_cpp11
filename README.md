**TinyMuduo_cpp11**
========

## **主要组件**
### Channel
- fd, events, revents, callbacks等，主要有两种Channel，listenfd对应的Channel和connfd对应的Channel
### Poller + EPollPoller（事件分发器）
-   Poller 为上层基类，下面可以实现epoll，poll等，本项目只实现了epoll
-   主要包含 unordered_map<int, Channel*> channels, 通过fd找到对应的Channel
### EventLoop（Reactor）
-   Channel和Poller之间的中间桥梁
-   ChannelList 包含所有的Channel
-   wakeupFd 每一个loop都有一个wakeupFd，同样注册了对应的channel，通过写入一定的事件来唤醒相应的loop（eventfd来实现）
-   unique_ptr<Channel> wakeupChannel， unique_ptr<Poller> poller
### Thread + EventLoopThread
### EventLoopThreadPool
-   通过 getNextLoop() 方法来获取下一个subloop，没有设置线程数量就返回baseloop
-   一个thread对应一个loop（one loop per thread）
### Socket
### Acceptor
-   封装listenfd的相关操作， socket bind listen 运行在baseloop
### Buffer
-   应用层缓冲区，应用写数据->缓冲区->TCP发送缓冲区->send
-   | prependable | readidx | writeidx |
### TcpConnection
-   一个连接成功的客户端对应一个TcpConnection
### TcpServer
-   包含Acceptor EventLoopThreadPool等组件