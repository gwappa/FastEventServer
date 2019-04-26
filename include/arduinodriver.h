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
*   leonardo.h -- the trigger output driver that uses Leonardo-type Arduinos.
*/

#ifndef __FE_ARDUINODRIVER_H__
#define __FE_ARDUINODRIVER_H__
#include <sstream>

#include "ks/utils.h"
#include "ks/timing.h"
#include "driver.h"
#include "serial.h"

// ASSERT if you want latency profile at the end of the session
// COMMENT-OUT if you don't need latency profile
#define __FE_PROFILE_IO__

#ifdef __FE_PROFILE_IO__
#include <stdint.h>
#endif

namespace fastevent {
    namespace driver {

        namespace arduino {
            template <typename T>
            ks::Result<OutputDriver *> setup(Config& cfg)
            {
                std::cerr << "setting up Arduino" << std::endl;

                try {
                    std::string path = json::get<std::string>(cfg, "port");
                    std::cerr << "port=" << path << std::endl;
                    ks::Result<serial_t> portsetup = serial::open(path);
                    if (portsetup.failed()) {
                        std::stringstream ss;
                        ss << "error setting up serial port: " << portsetup.what();
                        return ks::Result<OutputDriver *>::failure(ss.str());
                    }
                    return ks::Result<OutputDriver *>::success(new T(portsetup.get()));

                } catch (const std::runtime_error& e) {
                    std::stringstream ss;
                    ss << "parse error in 'options/port': " << e.what() << ".";
                    ss << " (set the path to your Arduino in 'options/port' key of 'service.cfg')";
                    return ks::Result<OutputDriver *>::failure(ss.str());
                }
            }
        }

        class ArduinoDriver: public OutputDriver
        {
        public:
            ArduinoDriver(const serial_t& port);
            ~ArduinoDriver();
            void update(const char& out);
            void shutdown();

        protected:
            void waitForLine();
            void clear();

        private:
            serial_t    port_;
            bool        closed_;
            char        prev_;

#ifdef __FE_PROFILE_IO__
            ks::nanostamp  clock_;
            // placeholder for IO profiling info
            ks::averager<uint64_t, uint64_t> latency;
            uint64_t minimum, maximum;
#endif
        };

        class LeonardoDriver: public ArduinoDriver
        {
        private:
            static const std::string _identifier;
        public:
            static const std::string& identifier();
            static ks::Result<OutputDriver *> setup(Config& cfg) { return arduino::setup<LeonardoDriver>(cfg); }

            LeonardoDriver(const serial_t& port);
        };

        class UnoDriver: public ArduinoDriver
        {
        private:
            static const std::string _identifier;
        public:
            static const std::string& identifier();
            static ks::Result<OutputDriver *> setup(Config& cfg) { return arduino::setup<UnoDriver>(cfg); }

            UnoDriver(const serial_t& port);
        };
    }
}

#endif
