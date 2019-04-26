/*
 * Copyright (C) 2018-2019 Keisuke Sehara
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
#include "ks/utils.h"

const uint64_t MAX_LATENCY = 10000000000000000000ULL;
#endif

// #define LOG_OUTPUT_ARDUINO

namespace fastevent {
    namespace driver {
        namespace arduino {
            // placeholders for commands to be sent
            const char CLEAR     = 'H';
            const char EVENT     = 'L';
            const char SYNC      = 'A';
            const char LINE_END  = '\n';
        }

        ArduinoDriver::ArduinoDriver(const serial_t& port):
            port_(port), closed_(false), prev_(arduino::CLEAR)
#ifdef __FE_PROFILE_IO__
            , latency(MAX_LATENCY), minimum(MAX_LATENCY), maximum(0)
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
            update(arduino::CLEAR);
        }

        void ArduinoDriver::waitForLine()
        {
            std::cerr << "waiting...";
            char buf;
            while (true) {
                switch (serial::get(port_, &buf))
                {
                case serial::Success:
                    std::cerr << buf;
                    break;
                case serial::Error:
                    std::cerr << "***error receiving the response: " << ks::error_message() << std::endl;
                    // fallthrough
                case serial::Closed:
                default:
                    shutdown();
                    return;
                }

                if (buf == arduino::LINE_END)
                {
                    std::cerr << std::endl;
                    break;
                }
            }
            std::cerr << "--- Arduino is ready." << std::endl;
        }

        void ArduinoDriver::update(const char& cmd) {
            if (closed_) {
                std::cerr << "***port already closed" << std::endl;
            }

            if (cmd == prev_) {
                if (cmd != arduino::CLEAR) {
                    return;
                }
            }

#ifdef __FE_PROFILE_IO__
            uint64_t start, stop;
            clock_.get(&start);
#endif
            char out = arduino::CLEAR;
            if (has_event(cmd)) {
                out |= arduino::EVENT;
            }
            if (has_sync(cmd)) {
                out |= arduino::SYNC;
            }
            switch (serial::put(port_, &out))
            {
            case serial::Success:
                break;
            case serial::Error:
            default:
                std::cerr << "***error sending serial command: "
                          << ks::error_message() << std::endl;
                shutdown();
                return;
            }

            char buf;
            switch (serial::get(port_, &buf))
            {
            case serial::Success:
                break;
            case serial::Error:
                std::cerr << "***error receiving the response: "
                          << ks::error_message() << std::endl;
                // fallthrough
            case serial::Closed:
            default:
                shutdown();
                return;
            }

            // TODO: assert buf == out??
            // for the moment, let us not do it since it seems worthless
            prev_ = out;

#ifdef __FE_PROFILE_IO__
            clock_.get(&stop);
            uint64_t lat = stop - start;
            latency.add(lat);
            if (minimum > lat) minimum = lat;
            if (maximum < lat) maximum = lat;
#endif
        }

        void ArduinoDriver::shutdown()
        {
            if (!closed_)
            {
                std::cerr << "shutting down ArduinoDriver." << std::endl;
                serial::put(port_, &arduino::CLEAR);
                serial::close(port_);
                closed_ = true;

#ifdef __FE_PROFILE_IO__
                double lat = latency.get();
                std::cerr << "------------------------------------------------" << std::endl;
                std::cerr << "minimal response latency: " << ((double)minimum)/1000 << "usec" << std::endl;
                std::cerr << "maximal response latency: " << ((double)maximum)/1000 << "usec" << std::endl;
                std::cerr << "------------------------------------------------" << std::endl;
                std::cerr << "average response latency: " << lat/1000 << " usec/transaction" << std::endl;
                std::cerr << "(negative latency means there was no transaction)" << std::endl;
                std::cerr << "------------------------------------------------" << std::endl;
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
            std::cerr << "initializing LeonardoDriver." << std::endl;
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
            std::cerr << "initializing UnoDriver." << std::endl;
            waitForLine();
        }
    }
}
