#include "EventLoop.h"
#include "Logger.h"
#include "Poller.h"
#include "Channel.h"

#include <sys/eventfd.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <memory>

//防止一个线程创建多个EventLoop thread_local
__thread EventLoop *t_loopInThisThread = nullptr;

//定义默认的Poller IO复用接口的超时时间
const int kPollTimeMS = 10000;//10s
//创建wakeupfd,用来notify唤醒subReactor处理新来的channel
int createEventfd(){
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0){
        // 出错了
        LOG_FATAL("eventfd error:%d \n", errno);
    }
    return evtfd;
}

EventLoop::EventLoop()
:   looping_(false),
    quit_(false),
    callingPendingFunctors_(false),
    threadId_(CurrentThread::tid()),
    poller_(Poller::newDefaultPoller(this)),
    wakeupFd_(createEventfd()),
    wakeupChannel_(new Channel(this, wakeupFd_))
    //currentActiveChannel_(nullptr)
{
    LOG_DEBUG("EventLoop created %p in thread %d \n", this, threadId_);
    if (t_loopInThisThread){
        LOG_FATAL("Another EventLoop %p exists in this thread %d \n", t_loopInThisThread, threadId_);
    }
    else t_loopInThisThread = this;

    //设置wakeupfd的事件类型以及发生事件的回调操作
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
    //每一个eventloop都将监听wakeupchannel的EPOLLIN读事件
    wakeupChannel_->enableReading();
}
EventLoop::~EventLoop(){
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    ::close(wakeupFd_);
    t_loopInThisThread = nullptr;
}


//开启事件循环
void EventLoop::loop(){
    looping_ = true;
    quit_ = false;

    LOG_INFO("EventLoop %p start looping \n", this);
    while (!quit_){
        activeChannels_.clear();
        //监听两类fd，一种是client的fd, 就是正常地跟客户端通信用的fd相应的channel
        // 一种是wakeupfd，就是mainloop跟subloop通信用的fd相应的channel
        pollReturnTime_ = poller_->poll(kPollTimeMS, &activeChannels_);
        for (Channel *channel : activeChannels_){
            //Poller可以监听哪些channel发生事件了，然后上报给EVentLoop,通知channel处理相应的事件
            channel->handleEvent(pollReturnTime_);
        }
        //执行当前EventLoop事件循环需要处理的回调操作
        /*
        IO线程mainLoop 的工作是接受新用户的连接(accept),返回一个fd,
        然后用channel打包这个fd,这个channel分发给subloop来处理读写事件
        mainloop事先注册一个回调cb(需要subloop来执行)，用wakeup唤醒subloop,
        唤醒后，执行下面的方法，执行之前mainloop注册的cb操作，有可能多个cb*/
        doPendingFunctors();
    }
    LOG_INFO("EventLoop %p stop looping. \n", this);
    looping_ = false;
}
//退出事件循环  1.loop在自己的线程中调用quit，不会阻塞在poll那里
//如果在其他线程(非loop线程)调用的quit，例如在一个subloop(worker)中，调用mainloop(IO线程)的quit，
//所以再写一个if语句，详情请看27集后四分之一
void EventLoop::quit(){
    quit_ = true;
    if (!isInLoopThread()){//调用loop的quit，不一定都在自己的线程中quit自己
    //有可能在别的线程(loop)中quit loop。一个线程是一个loop
        wakeup();
    }
}
//在当前loop中执行cb
void EventLoop::runInLoop(Functor cb){
    if (isInLoopThread()){
        //在当前loop线程中，执行cb
        cb();
    }
    else {//在非当前loop线程中执行cb,需要唤醒loop所在线程，执行cb
        queueInLoop(cb);
    }
}
//把cb放入队列中，唤醒loop所在的线程，执行cb
void EventLoop::queueInLoop(Functor cb){
    //并发的访问，需要锁的控制
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(cb);//emplaceback直接构造，pushback是拷贝构造
    }
    
    //唤醒相应的，需要执行上面回调操作的loop线程了
    if (!isInLoopThread() || callingPendingFunctors_){
        //callingpendingfunctors_表示当前loop正在执行回调，没有阻塞在这loop上
        //但是loop又有新的回调了
        wakeup();//唤醒loop所在线程
    }
}

void EventLoop::handleRead(){
    uint64_t one = 1;
    ssize_t n = read(wakeupFd_, &one, sizeof one);
    if (n != sizeof one){
        LOG_ERROR("EventLoop::handleRead() reads %d bytes instead of 8", n);
    }
}


//用来唤醒loop所在的线程 向wakeupfd_写一个数据就唤醒了
//wakeupChannel就发生读事件，当前loop线程就会被唤醒
//写啥读啥无所谓，就是为了唤醒loop线程
void EventLoop::wakeup(){
    uint64_t one = 1;
    ssize_t n = write(wakeupFd_, &one, sizeof one);
    if (n != sizeof one){
        LOG_ERROR("EventLoop::wakeup() writes %lu bytes instead of 8\n", n);
    }
}

//EventLoop的方法 => Poller的方法
void EventLoop::updateChannel(Channel *channel){
    //调用底层的Poller的updateChannel
    poller_->updateChannel(channel);
}
void EventLoop::removeChannel(Channel *channel){
    poller_->removeChannel(channel);
}
bool EventLoop::hasChannel(Channel *channel){
    return poller_->hasChannel(channel);
}
void EventLoop::doPendingFunctors(){//执行回调
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_);
    }//出了这个右括号，锁就没了

    for (const Functor &functor : functors){
        functor();//执行当前loop需要执行的回调操作
    }
    callingPendingFunctors_ = false;
} 