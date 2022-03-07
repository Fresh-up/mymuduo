#pragma once
//已检查
#include "noncopyable.h"
#include "Timestamp.h"
#include "Logger.h"
#include <functional>
#include <memory>
#include <sys/epoll.h>
class EventLoop;
/*理清楚 EventLoop, Channel, Poller之间的关系 对应 Reactor
模型的Demultiplex
Channel 理解为通道，封装了sockfd和其感兴趣的event,如EPOLLIN,
EPOLLOUT事件，还绑定了poller返回的具体事件
*/
class Channel : noncopyable{
public:
    using EventCallback = std::function<void()>;
    using ReadEventCallback = std::function<void(Timestamp)>;

    Channel(EventLoop *loop, int fd);
    ~Channel();

    //fd得到poller通知后，处理事件
    void handleEvent(Timestamp receiveTime);

    //设置回调函数对象
    void setReadCallback(ReadEventCallback cb){readCallback_ = std::move(cb);}
    void setWriteCallback(EventCallback cb){writeCallback_ = std::move(cb);}
    void setCloseCallback(EventCallback cb){closeCallback_ = std::move(cb);}
    void setErrorCallback(EventCallback cb){errorCallback_ = std::move(cb);}

    //防止当channel被手动remove，channel还在执行回调操作
    void tie(const std::shared_ptr<void>&);

    int fd() const {return fd_;}
    int events() const {return events_;}
    int set_revents(int revt) {revents_ = revt;}

    //设置fd相应的事件状态
    void enableReading() {events_ |= kReadEvent; update();}
    void disableReading() {events_ &= ~kReadEvent; update();}
    void enableWriting() {events_ |= kWriteEvent; update();}
    void disableWriting() {events_ &= ~kWriteEvent; update();}
    void disableAll() {events_ = kNoneEvent; update();}

    //返回fd当前的事件状态
    bool isNoneEvent() const {return events_ == kNoneEvent;}
    bool isWriting() const {return events_ & kWriteEvent;}
    bool isReading() const {return events_ & kReadEvent;}

    int index() {return index_;}
    void set_index(int idx) {index_ = idx;}

    //one loop per thread
    EventLoop* ownerLoop(){return loop_;}
    void remove();
private:
    void update();
    void handleEventWithGuard(Timestamp receiveTime);

    static const int kNoneEvent;
    static const int kReadEvent;
    static const int kWriteEvent;

    EventLoop *loop_;//事件循环
    const int fd_; //fd, Poller监听的对象
    int events_; //注册fd感兴趣的事件
    int revents_; //poller返回的具体发生的事件
    int index_;

    std::weak_ptr<void> tie_;//防止channel被手动remove之后，还在使用这个channel,使用弱智能指针实现跨线程的生存状态的监听
    bool tied_;
    /*shared_ptr与weak_ptr搭配使用，要注意shared_ptr可能会延长对象的生存期以及循环引用问题。
    下面两个例子都用到了弱回调，弱回调是指如果对象还活着，就调用它的成员函数，否则忽略之，
    可以通过尝试将weak_ptr提升为shared_ptr,如果提升成功则表示对象还活着。*/

    //因为只有channel通道里面能获知fd最终发生的具体事件revents
    //是读事件还是写事件，还是监听事件等等....
    //所以它负责调用具体事件的回调操作
    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
};