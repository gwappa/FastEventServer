/*
 * Copyright (C) 2018 Keisuke Sehara
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

/**
*   utils.h -- generic utility definitions
*/
#ifndef __FE_UTILS_H__
#define __FE_UTILS_H__
#include <string>
#include <stdint.h>
#ifdef _WIN32
#include <winsock2.h>
#else
#include <time.h>
#endif

namespace fastevent {
    enum ResultType { Success, Failure };

    template <typename T>
    class Result {
    private:
        ResultType        _type;
        T                 _value;
        const std::string _msg;

        explicit Result(ResultType type):
            _type(type) {}

    public:
        Result(ResultType type, const T& value):
            _type(type), _value(value) {}
        Result(ResultType type, const std::string& msg):
            _type(type), _msg(msg) {}

        const bool failed() const
        {
            return (_type == Failure);
        }

        const bool successful() const
        {
            return (_type == Success);
        }

        T& get()
        {
            return _value;
        }

        const std::string& what() const
        {
            return _msg;
        }

        static Result<T> success(const T& value)
        {
            return Result<T>(Success, value);
        }

        static Result<T> success()
        {
            return Result<T>(Success);
        }

        static Result<T> failure()
        {
            return Result<T>(Failure);
        }

        static Result<T> failure(const std::string& msg)
        {
            return Result<T>(Failure, msg);
        }
    };

    std::string error_message();

    const uint64_t NSEC_IN_SEC = 1000000000ULL;

    /**
    *   platform-specific wrapper for a real-time clock
    */
    class nanostamp
    {
    public:
        nanostamp();
        /**
        *   get the timestamp into `holder`
        *   somehow it did not work by returning a uint64_t value.
        */
        void get(uint64_t *holder);

        /**
        *   tells if the real-time clock is available on the platform
        */
        bool is_available();
    private:
        bool     supported_;
        uint64_t freq_; // used only on Windows
    };

#ifdef _WIN32
    /**
    *   the structure used for precision timer on Windows
    */
    typedef LARGE_INTEGER timerspec_t;
#else
    /**
    *   on *NIX, we only use the timespec as provided
    */
    typedef struct timespec timerspec_t;
#endif

    /**
    *   platform-specific wrapper for a presicion timer
    */
    class nanotimer
    {
    public:
        nanotimer() {}
        /**
        *   sets the value to sleep during each sleep() call.
        *   (actual sleep duration will be `value` nanosec at minimum)
        */
        void set_interval(uint64_t value);
        void sleep();
    private:
        timerspec_t spec_;
    };

    /**
    *   the template class used for averaging lots of samples.
    *   the sum will be reset at a certain limit to avoid overflow.
    */
    template <typename Val, typename Num>
    class averager
    {
    public:
        averager(Val limit): limit_(limit), sum_(0), num_(0) {}
        void add(Val v)
        {
            if (sum_ > limit_)
            {
                sum_ = v;
                num_ = 1;
            } else {
                sum_ += v;
                num_++;
            }
        }

        double get()
        {
            return ((double)sum_)/num_;
        }

        double get_inv(Val nom)
        {
            return ((double)nom)*num_/sum_;
        }

        Val sum() { return sum_; }
        Num num() { return num_; }

    private:
        Val   limit_;
        Val   sum_;
        Num   num_;
    };
}

#endif
