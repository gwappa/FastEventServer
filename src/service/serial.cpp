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

#include "serial.h"

#ifndef _WIN32
#include <sys/types.h>
#include <sys/uio.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#endif

#include <iostream>
#include <sstream>

namespace fastevent
{
    namespace serial
    {
#ifdef _WIN32
        /**
         * open COM port as 'unbuffered', in a WINAPI way.
         * it looks like, that jSSC opens the port this way by default.
         */
        Result<serial_t> open(const std::string& path, const uint32_t& baud)
        {
            serial_t    desc;

            // open it OVERLAPPED (i.e async)
            if ((desc = CreateFile(path.c_str(),
                             GENERIC_READ | GENERIC_WRITE,
                             0,
                             0,
                             OPEN_EXISTING,
                             FILE_FLAG_OVERLAPPED,
                             0)) == INVALID_HANDLE_VALUE)
            {
                std::stringstream ss;
                ss << "failed to open serial port at " << path << ": " << error_message();
                return Result<serial_t>::failure(ss.str());
            }

            // set I/O buffer to zero (although they say that it is
            // rarely taken into account by the device driver)
            SetupComm(desc,0,0);

            DCB         params = {0};
            GetCommState(desc, &params);

            // Set the parameter to "8N1" without any flow control, with the specified baud rate
            params.BaudRate     = baud;
            params.ByteSize     = 8;
            params.StopBits     = ONESTOPBIT;
            params.Parity       = NOPARITY;
            params.fOutxCtsFlow = FALSE;
            params.fDtrControl  = DTR_CONTROL_DISABLE;
            params.fRtsControl  = RTS_CONTROL_DISABLE;

            if (!SetCommState(desc, &params))
            {
                std::stringstream ss;
                ss << "failed to configure the serial port: " << error_message();
                CloseHandle(desc);
                return Result<serial_t>::failure(ss.str());
            }

            return Result<serial_t>::success(desc);
        }

        Status get(serial_t port, char* c)
        {
            DWORD count = 0;

            // since we opened in OVERLAPPED mode,
            // we use ReadFile/GetOverlappedResults-based ASYNC functions.
            // therefore we need an OVERLAPPED structure.
            OVERLAPPED event;
            SecureZeroMemory(&event,sizeof(event));

            // async ReadFile call (lpNumberOfBytes must be set NULL, lpOverlapped must be set non-NULL)
            if (ReadFile(port, c, 1, 0, &event) == 0)
            // if the ReadFile operation is not complete immediately:
            {
                switch(GetLastError())
                {
                // if the read operation has not completed yet
                case ERROR_IO_PENDING:
                    // wait for the overlapped operation to complete
                    if (!GetOverlappedResult(port, &event, &count, TRUE) || (count == 0))
                    // on failure
                    {
                        return Error;
                    }
                    break;
                // in the other cases, this means a 'proper' error
                default:
                    return Error;
                }
            }

            // by this point the read operation must have been successful
            return Success;
        }


        Status put(serial_t port, char* c)
        {
            DWORD count = 0;

            // TODO: make it using WriteFile/GetOverlappedResults-based ASYNC functions.
            // since we opened in OVERLAPPED mode,
            // we use ReadFile/GetOverlappedResults-based ASYNC functions.
            // therefore we need an OVERLAPPED structure.
            OVERLAPPED event;
            SecureZeroMemory(&event,sizeof(event));

            // async WriteFile call (lpNumberOfBytesWritten should be set NULL, lpOverlapped must be set non-NULL)
            if (WriteFile(port, c, 1, 0, &event) == 0)
            // if the WriteFile operation did not complete immediately:
            {
                switch (GetLastError())
                {
                // if the write operation has not completed yet
                case ERROR_IO_PENDING:
                    // wait for the overlapped operation to complete
                    if (!GetOverlappedResult(port, &event, &count, TRUE) || (count == 0))
                    // on failure
                    {
                        return Error;
                    }
                    break;
                // in the other cases, this means a 'proper' error
                default:
                    return Error;
                }
            }

            // by this point the write operation must have been successful
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
