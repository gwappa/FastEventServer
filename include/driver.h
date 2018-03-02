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
*   driver.h -- trigger output driver interface
*/

#ifndef __FE_DRIVER_H__
#define __FE_DRIVER_H__

#include <map>
#include <string>
#include "utils.h"
#include "config.h"

#include <iostream> // for debug

namespace fastevent {
    /**
    *   OutputDriver class is the base interface for output generator driver.
    *
    *   in addition to sync() and event() methods, the subclasses
    *   must implement the following public static members:
    *
    *   + static std::string identifier()
    *   + static Result<Output*> setup(Config&)
    *
    *   when you implement a new driver, you also have to
    *   call `registerOutputDriver(SubClassSignature)` in the initialization code
    */
    class OutputDriver
    {
        typedef std::map<std::string, Result<OutputDriver *>(*)(Config&)> Registry;
    private:
        static Registry _registry;
    public:
        virtual ~OutputDriver() {}
        virtual void sync(const bool& value)=0;
        virtual void event(const bool& value)=0;
        virtual void shutdown()=0;

        template <typename T>
        static void register_output_driver()
        {
            _registry[T::identifier()] = &T::setup;
            // std::cerr << "registered: " << T::driver_name() << std::endl;
        }

        static Result<OutputDriver *> setup(const std::string &name, Config& cfg, const bool& verbose=true);
    };
}

#define registerOutputDriver(CLS) (fastevent::OutputDriver::register_output_driver<CLS>())

#endif
