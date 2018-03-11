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
*   serial -- serial port-related functions for cross-platform usage
*/

#ifndef __FE_SERIAL_H__
#define __FE_SERIAL_H__

#ifdef _WIN32
#include <winsock2.h>
#endif

#include "utils.h"
#include <stdint.h>

/**
*   wrapper types and functions for serial ports.
*   note that the actual implementation is completely separate for Windows and *NIX.
*/
namespace fastevent
{
#ifdef _WIN32
    typedef HANDLE  serial_t;
#else
    typedef int     serial_t;
#endif

    const uint32_t DEFAULT_BAUDRATE = 230400;

    namespace serial
    {
        enum Status { Success, Closed, Error };

        /**
        *   8-bit, no-parity, 1-stopbit
        */
        Result<serial_t>  open(const std::string& path, const uint32_t& baud=DEFAULT_BAUDRATE);

        /**
        *   reads a byte from the serial port. returns fastevent::serial::Status.
        */
        Status get(serial_t port, char* c);

        /**
        *   writes a character. returns fastevent::serial::Status.
        */
        Status put(serial_t port, char* c);

        void   close(serial_t port);
    }
}

#endif
