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
*   dummydriver -- see dummydriver.h for description
*/
#include <iostream>
#include "dummydriver.h"

namespace fastevent {
    namespace driver {
        const std::string DummyDriver::_identifier("dummy");

        const std::string& DummyDriver::identifier()
        {
            return _identifier;
        };

        Result<OutputDriver *> DummyDriver::setup(Config& cfg)
        {
            std::cout << "setting up DummyDriver" << std::endl;
            return Result<OutputDriver *>::success(new DummyDriver(cfg));
        }

        DummyDriver::DummyDriver(Config& cfg)
        {
            std::cout << "initializing DummyDriver." << std::endl;
        }

        DummyDriver::~DummyDriver()
        {
            // do nothing
        }

        void DummyDriver::sync(const bool& value)
        {
            std::cout << "sync->" << value << std::endl;
        }

        void DummyDriver::event(const bool& value)
        {
            std::cout << "event->" << value << std::endl;
        }

        void DummyDriver::update(const bool& sync, const bool& event)
        {
            this->sync(sync);
            this->event(event);
        }

        void DummyDriver::shutdown()
        {
            std::cout << "shutting down DummyDriver." << std::endl;
        }
    }
}
