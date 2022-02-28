#pragma once
#include "noncopyable.h"
#include "Timestamp.h"
#include "Channel.h"
#include <vector>
#include <unordered_map>
class Channel;
class EventLoop;
//muduo库中多路事件分发器核心IO复用模块
class Poller : noncopyable{
public:
    using ChannelList = std::vector<Channel*>;
    //不用typedef而用using来定义新的类型
    Poller(EventLoop *loop);
    virtual ~Poller() = default;

    //给所有IO复用保留统一的接口
    virtual Timestamp poll(int timeoutMS, ChannelList *activeChannels) = 0;
    virtual void updateChannel(Channel *channel) = 0;
    virtual void removeChannel(Channel *channel) = 0;
    
    //判断参数channel是否在当前Poller当中
    virtual bool hasChannel(Channel* channel) const;

    //EventLoop可以通过该接口获取默认的IO复用的具体实现
    //因为抽象基类里面最好不包含其他派生类，所以.cc文件内没有newDefaultPoller函数的实现
    //最好创建一个独立的文件来实现它
    static Poller* newDefaultPoller(EventLoop *loop);
protected:
    // 键值为sockfd, 值为sockfd所属的channel通道类型
    using ChannelMap = std::unordered_map<int, Channel*>;
    ChannelMap channels_;
private:
    EventLoop *ownerLoop_;//定义Poller所属的事件循环EventLoop
};