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
*   leonardo.cpp -- see leonardo.h for description
*/
#include <iostream>
#include <sstream>
#include "leonardo.h"

#ifdef __FE_PROFILE_IO__
#include "utils.h"

const uint64_t MAX_LATENCY = 10000000000000000000ULL;
#endif

namespace fastevent {
    namespace driver {
        namespace leonardo {
            // placeholders for commands to be sent

            char SYNC_ON   = '1';
            char SYNC_OFF  = '2';
            char EVENT_ON  = 'A';
            char EVENT_OFF = 'B';
            char FLUSH     = 'F';
            char CLEAR     = 'O';

        }

        const std::string LeonardoDriver::_identifier("leonardo");

        const std::string& LeonardoDriver::identifier()
        {
            return _identifier;
        };

        Result<OutputDriver *> LeonardoDriver::setup(Config& cfg)
        {
            std::cout << "setting up LeonardoDriver" << std::endl;

            try {
                std::string path = json::get<std::string>(cfg, "port");
                std::cout << "port=" << path << std::endl;
                Result<serial_t> portsetup = serial::open(path);
                if (portsetup.failed()) {
                    std::stringstream ss;
                    ss << "error setting up serial port: " << portsetup.what();
                    return Result<OutputDriver *>::failure(ss.str());
                }
                return Result<OutputDriver *>::success(new LeonardoDriver(portsetup.get()));

            } catch (const std::runtime_error& e) {
                std::stringstream ss;
                ss << "parse error in 'options/port': " << e.what() << ".";
                ss << " (set the path to your Arduino in 'options/port' key of 'service.cfg')";
                return Result<OutputDriver *>::failure(ss.str());
            }
        }

        LeonardoDriver::LeonardoDriver(const serial_t& port):
            port_(port), sync_(false), event_(false), counter_(0), closed_(false)
#ifdef __FE_PROFILE_IO__
            , latency(MAX_LATENCY)
#endif
        {
            std::cout << "initializing LeonardoDriver." << std::endl;
            switch (serial::put(port_, &leonardo::CLEAR))
            {
            case serial::Success:
                break;
            case serial::Error:
            default:
                std::cerr << "***error sending serial command: " << error_message() << std::endl;
                shutdown();
            }
        }

        LeonardoDriver::~LeonardoDriver()
        {
            // do nothing
        }

        void LeonardoDriver::sync(const bool& value)
        {
            if (sync_ != value)
            {
                send(value? &leonardo::SYNC_ON : &leonardo::SYNC_OFF);
            }
        }

        void LeonardoDriver::event(const bool& value)
        {
            if (event_ != value)
            {
                send(value? &leonardo::EVENT_ON : &leonardo::EVENT_OFF);
            }
        }

        void LeonardoDriver::send(char *c)
        {
            if (closed_) {
                std::cerr << "***port already closed" << std::endl;
            }

#ifdef __FE_PROFILE_IO__
            uint64_t start, stop;
            clock_.get(&start);
#endif
            switch (serial::put(port_, c))
            {
            case serial::Success:
                break;
            case serial::Error:
            default:
                std::cerr << "***error sending serial command: " << error_message() << std::endl;
                shutdown();
                return;
            }

            if ((++counter_) == 4)
            {
                counter_ = 0;
                char buf;
                switch (serial::get(port_, &buf))
                {
                case serial::Success:
                    break;
                case serial::Error:
                    std::cerr << "***error receiving the response: " << error_message() << std::endl;
                    // fallthrough
                case serial::Closed:
                default:
                    shutdown();
                    return;
                }
                // TODO: check the last state

#ifdef __FE_PROFILE_IO__
                clock_.get(&stop);
                latency.add(stop-start);
#endif
            }
        }

        void LeonardoDriver::shutdown()
        {
            if (!closed_)
            {
                std::cout << "shutting down LeonardoDriver." << std::endl;
                serial::put(port_, &leonardo::CLEAR);
                serial::close(port_);
                closed_ = true;

#ifdef __FE_PROFILE_IO__
                double lat = latency.get();
                // std::cout << "latency sum -- " << latency.sum() << "/" << latency.num() << std::endl;
                std::cout << "------------------------------------------------" << std::endl;
                std::cout << "average response latency: " << lat/1000 << " usec/transaction" << std::endl;
                std::cout << "(negative latency means there was no transaction)" << std::endl;
                std::cout << "------------------------------------------------" << std::endl;
#endif
            }
        }
    }
}
