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
*   dummydriver -- see dummydriver.h for description
*/
#include <iostream>
#include "dummydriver.h"

namespace fastevent {
    namespace driver {
        const std::string DummyDriver::_identifier("dummy");
        const std::string VerboseDummyDriver::_identifier("verbose-dummy");

        const std::string& DummyDriver::identifier()
        {
            return _identifier;
        };

        ks::Result<OutputDriver *> DummyDriver::setup(Config& cfg)
        {
            std::cout << "setting up DummyDriver" << std::endl;
            return ks::Result<OutputDriver *>::success(new DummyDriver(cfg));
        }

        DummyDriver::DummyDriver(Config& cfg)
        {
            std::cout << "initializing DummyDriver." << std::endl;
        }

        DummyDriver::~DummyDriver()
        {
            // do nothing
        }

        void DummyDriver::update(const char& out)
        {
            // do nothing
        }

        void DummyDriver::shutdown()
        {
            std::cout << "shutting down DummyDriver." << std::endl;
        }

        const std::string& VerboseDummyDriver::identifier()
        {
            return _identifier;
        };

        ks::Result<OutputDriver *> VerboseDummyDriver::setup(Config& cfg)
        {
            std::cout << "setting up VerboseDummyDriver" << std::endl;
            return ks::Result<OutputDriver *>::success(new VerboseDummyDriver(cfg));
        }

        VerboseDummyDriver::VerboseDummyDriver(Config& cfg)
        {
            std::cout << "initializing VerboseDummyDriver." << std::endl;
        }

        VerboseDummyDriver::~VerboseDummyDriver()
        {
            // do nothing
        }

        void VerboseDummyDriver::update(const char& out)
        {
            // report
            const bool event    = has_event(out);
            const bool sync     = has_sync(out);
            const bool shutdown = has_shutdown(out);
            std::cout << ">>> status: E=" << event
                      << ", S=" << sync
                      << ", X=" << shutdown << std::endl;
        }

        void VerboseDummyDriver::shutdown()
        {
            std::cout << "shutting down VerboseDummyDriver." << std::endl;
        }
    }
}
