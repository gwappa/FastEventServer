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
*   utils.cpp -- see utils.h for description
*/

#include "utils.h"
#include <iostream>
#include <sstream>

#ifdef _WIN32
// #include <wdm.h>
#else
#include <errno.h>
#endif

namespace fastevent {

    std::string error_message()
    {
#ifdef _WIN32
      DWORD err = GetLastError();
      if( err != 0 ){
        LPSTR buf = 0;
        size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                 NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&buf, 0, NULL);
        std::string msg(buf, size);
        return msg;
      } else {
        return "no error was detected, however";
      }
#else
      std::stringstream ss;
      ss << strerror(errno);
      return ss.str();
#endif
    }

#ifdef _WIN32
    nanostamp::nanostamp()
    {
        LARGE_INTEGER freq;
        int supported = QueryPerformanceFrequency(&freq);
        supported_ = (supported == 0)? false: true;
        if (!supported_)
        {
            std::cerr << "***QPC clock is not available on this platform.";
            std::cerr << "disabling calculation of transaction latency." << std::endl;
            freq_ = 1;
        } else {
            freq_ = (uint64_t)(freq.QuadPart);
        }
    }

    void nanostamp::get(uint64_t *holder)
    {
        if (!supported_) {
            return;
        }

        LARGE_INTEGER count;
        if (QueryPerformanceCounter(&count) == 0) {
            std::cerr << "***failure to get QPC: " << error_message() << std::endl;
            std::cerr << "***disabling latency calculation." << std::endl;
            supported_ = false;
            return;
        }

        uint64_t ucount = (uint64_t)(count.QuadPart);
        *holder = (ucount*NSEC_IN_SEC)/freq_;
    }
#else
    nanostamp::nanostamp()
    {
        struct timespec test;
        if (clock_gettime(CLOCK_REALTIME, &test)) {
            std::cerr << "***real-time clock is not available on this platform.";
            std::cerr << "disabling calculation of transaction latency." << std::endl;
            supported_ = false;
        }
    }

    void nanostamp::get(uint64_t *holder)
    {
        if (!supported_) {
            *holder = 0;
        }

        struct timespec _clock;
        if (clock_gettime(CLOCK_REALTIME, &_clock)) {
            std::cerr << "***failure to get real-time clock: " << error_message() << std::endl;
            std::cerr << "***disabling latency calculation." << std::endl;
            supported_ = false;
            *holder = 0;
        }

        *holder = ((uint64_t)(_clock.tv_sec))*NSEC_IN_SEC + (uint64_t)(_clock.tv_nsec);
    }
#endif

    bool nanostamp::is_available() { return supported_; }

#ifdef _WIN32
    void nanotimer::set_interval(uint64_t value)
    {
        return; // right now we cannot use nanosleep(), and I have not found any alternatives.
    }

    void nanotimer::sleep()
    {
        return; // right now we cannot use nanosleep(), and I have not found any alternatives.
    }
#else
    void nanotimer::set_interval(uint64_t value)
    {
        if (value > NSEC_IN_SEC) {
            spec_.tv_sec  = value / NSEC_IN_SEC;
            spec_.tv_nsec = value % NSEC_IN_SEC;
        } else {
            spec_.tv_sec  = 0;
            spec_.tv_nsec = value;
        }
    }

    void nanotimer::sleep()
    {
        nanosleep(&spec_, NULL);
    }
#endif
}
