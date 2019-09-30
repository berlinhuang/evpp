//
// Created by berli on 2019/9/30.
//

#include "current_thread.h"
#include <pthread.h>
#include <unistd.h>
#include <syscall.h>
#include <stdlib.h>
#include <iostream>

namespace current_thread
{
    __thread int t_cachedTid = 0;
    __thread char t_tidString[32];
    __thread int t_tidStringLength = 6;
    __thread const char * t_threadName = "unknown";
    const bool sameType = std::is_same<int, pid_t>::value;


    pid_t gettid()
    {
        return static_cast<pid_t>(::syscall(SYS_gettid));
    }
    void cachedTid()
    {
        if(t_cachedTid == 0)
        {
            t_cachedTid = gettid();
            t_tidStringLength = snprintf(t_tidString,sizeof t_tidString, "%5d ",t_cachedTid);
        }
    }
    bool isMainThread()
    {
        return tid() == ::getpid();// 主线程的线程id和进程id相同
    }



}