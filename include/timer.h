/******************************************************************************
 Copyright© HITwh HERO-Robomaster2020 Group

 Author: Zhou Yuxin on 2018.10.10

 Detail: 计时器。
 *****************************************************************************/

#ifndef HERORM2020_TIMER_H
#define HERORM2020_TIMER_H

#include <time.h>


class Timer {
private:
    enum {
        STANDBY,
        RUNNING
    };

    clock_t time_start_;
    clock_t time_end_;
    bool status_;

public:
    Timer();
    ~Timer();
    void start();
    double restart();
    void stop();
    double getTime();

};


#endif  // HERORM2020_TIMER_H