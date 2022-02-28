#pragma once
#include <iostream>
#include <string>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
class Timestamp
{
    public:
        Timestamp();
        //带参数的构造函数最好都加上explicit
        explicit Timestamp(int64_t microSecondsSinceEpoch);
        static Timestamp now();
        std::string toString() const;
    private:
        int64_t microSecondsSinceEpoch_;
};