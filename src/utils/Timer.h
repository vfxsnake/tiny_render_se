#pragma once

#include <chrono>

class Timer
{
public:

    void start()
    {
        startTime_ = std::chrono::steady_clock::now();
    }

    void stop()
    {
        endTime_ = std::chrono::steady_clock::now();
    }

    double elapsedMs() const
    {
        return std::chrono::duration<double, std::milli>(endTime_ - startTime_).count();
    }

    double elapsedUs() const
    {
        return std::chrono::duration<double, std::micro>(endTime_ - startTime_).count();
    }

private:

    std::chrono::steady_clock::time_point startTime_;
    std::chrono::steady_clock::time_point endTime_;
};