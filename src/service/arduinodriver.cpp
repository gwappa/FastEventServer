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
#include "arduinodriver.h"

#ifdef __FE_PROFILE_IO__
#include "utils.h"

const uint64_t MAX_LATENCY = 10000000000000000000ULL;
#endif

#define LOG_OUTPUT_ARDUINO

namespace fastevent {
    namespace driver {
        namespace arduino {
            // placeholders for commands to be sent

            char SYNC_ON   = '1';
            char SYNC_OFF  = '2';
            char EVENT_ON  = 'A';
            char EVENT_OFF = 'B';
            char FLUSH     = 'F';
            char CLEAR     = 'O';

            char LINE_END  = '\n';

        }

        ArduinoDriver::ArduinoDriver(const serial_t& port):
            port_(port), sync_(false), event_(false), counter_(0), closed_(false)
#ifdef __FE_PROFILE_IO__
            , latency(MAX_LATENCY)
#endif
        {
            // do nothing
        }

        ArduinoDriver::~ArduinoDriver()
        {
            // do nothing
        }

        void ArduinoDriver::clear()
        {
            switch (serial::put(port_, &arduino::CLEAR))
            {
            case serial::Success:
                break;
            case serial::Error:
            default:
                std::cerr << "***error sending serial command: " << error_message() << std::endl;
                shutdown();
            }
        }

        void ArduinoDriver::waitForLine()
        {
            char buf;
            while (true) {
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

                if (buf == arduino::LINE_END)
                {
                    break;
                }
            }
            std::cout << "--- Arduino is ready." << std::endl;
        }

        void ArduinoDriver::sync(const bool& value)
        {
            if (sync_ != value)
            {
                send(value? &arduino::SYNC_ON : &arduino::SYNC_OFF);
                sync_ = value;
#ifdef LOG_OUTPUT_ARDUINO
                std::cout << "sync->" << value << std::endl;
#endif
            }
        }

        void ArduinoDriver::event(const bool& value)
        {
            if (event_ != value)
            {
                send(value? &arduino::EVENT_ON : &arduino::EVENT_OFF);
                event_ = value;
#ifdef LOG_OUTPUT_ARDUINO
                std::cout << "event->" << value << std::endl;
#endif
            }
        }

        void ArduinoDriver::send(char *c)
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

        void ArduinoDriver::shutdown()
        {
            if (!closed_)
            {
                std::cout << "shutting down LeonardoDriver." << std::endl;
                serial::put(port_, &arduino::CLEAR);
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

        const std::string LeonardoDriver::_identifier("leonardo");

        const std::string& LeonardoDriver::identifier()
        {
            return _identifier;
        };

        LeonardoDriver::LeonardoDriver(const serial_t& port):
            ArduinoDriver(port)
        {
            std::cout << "initializing LeonardoDriver." << std::endl;
            clear();
        }

        const std::string UnoDriver::_identifier("uno");

        const std::string& UnoDriver::identifier()
        {
            return _identifier;
        };

        UnoDriver::UnoDriver(const serial_t& port):
            ArduinoDriver(port)
        {
            std::cout << "initializing UnoDriver." << std::endl;
            sleep_seconds(3);
            waitForLine();
        }
    }
}
