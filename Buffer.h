#pragma once

#include <vector>
#include <unistd.h>
#include <string>
#include <algorithm>


//网络库底层的缓冲器类型定义
class Buffer{
public:
    static const size_t kCheapPrepend = 8;
    static const size_t kInitialSize = 1024;

    explicit Buffer(size_t initialSize = kInitialSize)
        :buffer_(kCheapPrepend + kInitialSize),
         readerIndex_(kCheapPrepend),
         writerIndex_(kCheapPrepend){}
    
    size_t readableBytes() const{
        return writerIndex_ - readerIndex_;
    }
    size_t writableBytes() const{
        return buffer_.size() - writerIndex_;
    }
    size_t prependableBytes() const{
        return readerIndex_;
    }

    //返回缓冲区中可读数据的起始地址
    const char* peek() const{
        return begin() + readerIndex_;
    }

    //onMessage string <- Buffer
    void retrieve(size_t len){
        if (len < readableBytes()){
            readerIndex_ += len;//应用只读取了可读缓冲区数据的一部分，就是len,还剩下
            //len~writerindex_部分没读
        }
        else{//len == readableBytes()
            retrieveAll();
        }
    }

    void retrieveAll(){
        readerIndex_ = writerIndex_ = kCheapPrepend;
    }

    //把onMessage函数上报的BUffer数据，转成string类型的数据返回
    std::string retrieveAllAsString(){
        return retrieveAsString(readableBytes());
    }

    std::string retrieveAsString(size_t len){
        std::string result(peek(), len);
        retrieve(len);//上面一句把缓冲区中可读的数据，已经读取出来，这里肯定要对缓冲区复位
        return result;
    }
    //buffer_size - writerIndex_ 
    void ensureWriteableBytes(size_t len){
        if (writableBytes() < len){
            makeSpace(len);//扩容函数
        }
    }

    //把[data, data+len]内存上的数据，添加到writable缓冲区中
    void append(const char *data, size_t len){
        ensureWriteableBytes(len);
        std::copy(data, data+len, beginWrite());
        writerIndex_ += len;
    }
    char* beginWrite(){//之前写错了函数名benginWrite,多了一个n,导致上面copy调用了const char* beginWrite,stl里面没找到这个函数
        return begin() + writerIndex_;
    }
    const char* beginWrite() const{
        return begin() + writerIndex_;
    }

    //从fd上读取数据
    ssize_t readFd(int fd, int*saveErrno);
    ssize_t writeFd(int fd, int* saveErrno);

private:
    char* begin(){//返回数组的起始地址
        return &*buffer_.begin();
    }
    const char* begin() const{
        return &*buffer_.begin();
    }
    void makeSpace(size_t len){
        //无法理解的话，看38集最后5~7分钟
        if (writableBytes() + prependableBytes() < len + kCheapPrepend)
            buffer_.resize(writerIndex_ + len);
        else{
            size_t readable = readableBytes();
            std::copy(begin() + readerIndex_,
                        begin() + writerIndex_,
                        begin() + kCheapPrepend);
            readerIndex_ = kCheapPrepend;
            writerIndex_ = readerIndex_ + readable;
        }
    }
    std::vector<char> buffer_;
    size_t readerIndex_;
    size_t writerIndex_;
};