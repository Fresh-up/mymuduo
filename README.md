# mymuduo
A muduo lib without boost lib, which is modified through C++11

直接执行脚本autubuild即可完成安装

但是开另外的shell用Telnet与写好的服务器连接时，时间无法打印，而且会打印一大堆INFO
还没找到问题所在

2022.3.7
根据打印的INFO找到了问题所在，无限循环中有一个条件未设置
具体位置在handleEvent函数
