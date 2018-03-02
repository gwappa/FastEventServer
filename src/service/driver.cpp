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
*   driver.cpp -- see driver.h for description
*/

#include "driver.h"
#include <sstream>

namespace fastevent {
    OutputDriver::Registry OutputDriver::_registry;

    Result<OutputDriver *> OutputDriver::setup(const std::string& name, Config& cfg, const bool& verbose)
    {
        if (verbose) {
            std::cerr << "<-- registered output drivers" << std::endl;
            for(Registry::iterator it=_registry.begin();
                it!=_registry.end(); it++)
            {
                std::cerr << it->first << std::endl;
            }
            std::cerr << "-->" << std::endl;
        }

        Registry::iterator iter = _registry.find(name);

        if ( iter != _registry.end() ) {
            if (verbose) {
                std::cout << "found output driver: " << name << std::endl;
            }
            return (iter->second)(cfg);
        } else {
            std::stringstream ss;
            ss << "could not find the output driver with name '" << name << "'";
            return Result<OutputDriver *>::failure(ss.str());
        }
    }
}
