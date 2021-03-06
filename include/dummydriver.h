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
*   dummydriver -- the trigger output driver that outputs no trigger
*/

#ifndef __FE_DUMMYDRIVER_H__
#define __FE_DUMMYDRIVER_H__

#include "ks/utils.h"
#include "driver.h"

namespace fastevent {
    namespace driver {

        class DummyDriver: public OutputDriver
        {
        private:
            static const std::string _identifier;
        public:
            static const std::string& identifier();
            static ks::Result<OutputDriver *> setup(Config& cfg);

            DummyDriver(Config& cfg);
            ~DummyDriver();
            void update(const char& out);
            void shutdown();
        };

        class VerboseDummyDriver: public OutputDriver
        {
        private:
            static const std::string _identifier;
        public:
            static const std::string& identifier();
            static ks::Result<OutputDriver *> setup(Config& cfg);

            VerboseDummyDriver(Config& cfg);
            ~VerboseDummyDriver();
            void update(const char& out);
            void shutdown();
        };
    }
}

#endif
