#include "Buffer.h"
#include <errno.h>
#include <sys/uio.h>
#include <unistd.h>

/*
从fd上读取数据  Poller工作在LT模式
Buffer缓冲区是有大小的，但是从fd上读取数据时，却不知道tcp数据最终大写
*/
ssize_t Buffer::readFd(int fd, int* saveErrno){
    char extrabuf[65536] = {0};//栈上内存空间
//栈空间分配的效率是很快的，而且随着程序的结束，系统会自动回收栈空间
    struct iovec vec[2];

    //第一块缓冲区
    const size_t writable = writableBytes();//这是Buffer底层缓冲区剩余的可写空间大小
    vec[0].iov_base = begin() + writerIndex_;
    vec[0].iov_len = writable;

    //第二块缓冲区
    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof extrabuf;

    //选择空间
    const int iovcnt = (writable < sizeof extrabuf) ? 2 : 1;
    const ssize_t n = ::readv(fd, vec, iovcnt);
    if (n < 0){
        *saveErrno = errno;
    }
    else if (n <= writable){//Buffer的可写缓冲区已经够存储读出的数据了
        writerIndex_ += n;
    }
    else {
        //extrabuf里面也写入了数据
        writerIndex_ = buffer_.size();
        append(extrabuf, n - writable);//wirterIndex_开始写n-writable大小的数据
    }
    return n;
}

ssize_t Buffer::writeFd(int fd, int* saveErrno){
    ssize_t n = ::write(fd, peek(), readableBytes());
    if (n < 0){
        *saveErrno = errno;
    }
    return n;
}