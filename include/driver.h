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
*   driver.h -- trigger output driver interface
*/

#ifndef __FE_DRIVER_H__
#define __FE_DRIVER_H__

#include <map>
#include <string>
#include "ks/utils.h"
#include "config.h"

#include <iostream> // for debug

#define MASK_EVENT    ((char)0x20)
#define MASK_SYNC     ((char)0x10)
#define MASK_QUIT     ((char)0x03)
#define MASK_COMMANDS ((char)0x3f)

namespace fastevent {
    const inline bool has_event(const char& out) {
        return ((out & MASK_EVENT) != 0);
    }
    const inline bool has_sync(const char& out) {
        return ((out & MASK_SYNC) != 0);
    }
    const inline bool has_shutdown(const char& out) {
        return ((out & MASK_QUIT) != 0);
    }

    /**
    *   OutputDriver class is the base interface for output generator driver.
    *
    *   in addition to sync(), event() and update() methods, the subclasses
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
        typedef std::map<std::string, ks::Result<OutputDriver *>(*)(Config&)> Registry;
    private:
        static Registry _registry;
    public:

        virtual ~OutputDriver() {}

        /**
         * updates the output of the driver
         */
        virtual void update(const char& out)=0;

        /**
         * shuts down the driver
         */
        virtual void shutdown()=0;

        template <typename T>
        static void register_output_driver()
        {
            _registry[T::identifier()] = &T::setup;
        }

        static ks::Result<OutputDriver *> setup(const std::string &name, Config& cfg, const bool& verbose=true);
    };
}

#define registerOutputDriver(CLS) (fastevent::OutputDriver::register_output_driver<CLS>())

#endif
