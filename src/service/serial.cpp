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
*   serial.cpp -- see serial.h for description
*/

#ifndef _WIN32
#include <sys/types.h>
#include <sys/uio.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#endif

#include <iostream>
#include <sstream>
#include "serial.h"

namespace fastevent
{
    namespace serial
    {
#ifdef _WIN32
        Result<serial_t> open(const std::string& path, const uint32_t& baud)
        {
            serial_t    desc;
            DCB         params = {0};

            params.BaudRate = baud;
            params.ByteSize = 8;
            params.StopBits = ONESTOPBIT;
            params.Parity   = NOPARITY;

            if ((desc = CreateFile(path.c_str(),
                             GENERIC_READ | GENERIC_WRITE,
                             0,
                             0,
                             OPEN_EXISTING,
                             FILE_ATTRIBUTE_NORMAL,
                             0)) == INVALID_HANDLE_VALUE)
            {
                std::stringstream ss;
                ss << "failed to open serial port at " << path << ": " << error_message();
                return Result<serial_t>::failure(ss.str());
            }

            if (!SetCommState(desc, &params))
            {
                std::stringstream ss;
                ss << "failed to configure the serial port: " << error_message();
                CloseHandle(desc);
                return Result<serial_t>::failure(ss.str());
            }

            return Result<serial_t>::success(desc);
        }

        // TODO: change to WIN32 API
        Status get(serial_t port, char* c)
        {
            DWORD count = 0;

            // TODO:
            // the whole while loop may not be needed, since
            // MSDN says it blocks (in sync mode) until the completion
            // of the procedure.
            //
            // Nevertheless, I would rather overprocess here to avoid
            // any weird errors.
            // If the latency becomes a concern, I consider removing
            // the while loop.
            while (count == 0)
            {
                if (ReadFile(port, c, 1, &count, NULL) == 0)
                {
                    // this block may not be needed...
                    switch(GetLastError())
                    {
                    // waiting for the data to come
                    // (probably only for async communication)
                    case ERROR_IO_PENDING:
                        continue;
                    // one byte read but more in the read buffer
                    // (probably only for communication through pipes)
                    case ERROR_MORE_DATA:
                        if (count == 1) {
                            return Success;
                        }
                        break;
                    // in other cases, this means a 'proper' error
                    default:
                        return Error;
                    }
                }
            }
            return Success;
        }

        // TODO: change to WIN32 API
        Status put(serial_t port, char* c)
        {
            DWORD count = 0;

            // TODO:
            // the whole while loop may not be needed, since
            // MSDN says it blocks (in sync mode) until the completion
            // of the procedure.
            //
            // Nevertheless, I would rather overprocess here to avoid
            // any weird errors.
            // If the latency becomes a concern, I consider removing
            // the while loop.
            while (count == 0)
            {
                if (WriteFile(port, c, 1, &count, NULL) == 0)
                {
                    // this block may not be needed...
                    switch(GetLastError())
                    {
                    // waiting for the data to come
                    // (probably only for async communication)
                    case ERROR_IO_PENDING:
                        continue;
                    // in other cases, this means a 'proper' error
                    default:
                        return Error;
                    }
                }
            }
            return Success;
        }

        void close(serial_t port)
        {
            CloseHandle(port);
        }

#else
        Result<serial_t> open(const std::string& path, const uint32_t& baud)
        {
            serial_t desc;
            struct termios tio;

            memset(&tio,0,sizeof(tio));
            tio.c_iflag=0;
            tio.c_oflag=0;
            tio.c_cflag=CS8|CREAD|CLOCAL;           // 8n1, see termios.h for more information
            tio.c_lflag=0;
            tio.c_cc[VMIN]=1;
            tio.c_cc[VTIME]=5;

            if ((desc = ::open(path.c_str(), O_RDWR | O_NONBLOCK)) < 1)
            {
                std::stringstream ss;
                ss << "failed to open serial port at " << path << ": " << error_message();
                return Result<serial_t>::failure(ss.str());
            }
            if ( (cfsetospeed(&tio,baud) < 0) || (cfsetispeed(&tio,baud) < 0) )
            {
                ::close(desc); // close `desc` no matter
                std::stringstream ss;
                ss << "failed to configure the baud rate at " << baud << ": " << error_message();
                return Result<serial_t>::failure(ss.str());
            }

            if (tcsetattr(desc,TCSANOW,&tio) < 0)
            {
                ::close(desc); // close `desc` no matter
                std::stringstream ss;
                ss << "failed to configure the serial port at " << path << ": " << error_message();
                return Result<serial_t>::failure(ss.str());
            }

            return Result<serial_t>::success(desc);
        }

        Status get(serial_t port, char* c)
        {
            int resp;
            while ((resp = ::read(port, c, 1)) != 1)
            {
                switch (resp)
                {
                case 0:
                    return Closed;
                case -1:
                default:
                    break;
                }
            }
            return Success;
        }

        Status put(serial_t port, char* c)
        {
            int resp;
            bool full = false;
            while ((resp = ::write(port, c, 1)) != 1)
            {
                if (resp < 0) {
                    return Error;
                } else if (!full && (resp == 0)) {
                    std::cerr << "***write buffer is full on the serial port" << std::endl;
                    full = true;
                }
            }
            return Success;
        }

        void close(serial_t port)
        {
            ::close(port);
        }
#endif
    }
}
