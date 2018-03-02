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
*   service.h -- TCP-service functionality
*/

#ifndef __FE_SERVICE_H__
#define __FE_SERVICE_H__

#ifdef _WIN32
#include <winsock2.h>
#else
  #include <sys/socket.h>
  #include <sys/types.h>
#endif

#include <stdint.h>

#include "config.h"
#include "driver.h"

namespace fastevent {

    /**
    *   let `fastevent::socket_t` be the correct type for native sockets
    */
#ifdef _WIN32
    typedef SOCKET        socket_t;
#else
    typedef int           socket_t;
#endif

    /**
    *   maximal number of acceptable clients
    */
    const int CONN_MAX  = 8;

    /**
    *   utility functions/classes related to network management
    */
    namespace network {
        /**
        *   a support class for managing network initialization and cleanup
        */
        class Manager
        {
        public:
            Manager();
            ~Manager();
        };
    }

    namespace protocol {

        enum Status { Success, Error, Closed };

        const char SYNC_ON      = '1';
        const char SYNC_OFF     = '2';
        const char EVENT_ON     = 'A';
        const char EVENT_OFF    = 'B';
        const char SHUTDOWN     = 'Q';

        Status read(socket_t socket, char* cmdbuf);
        Status acq(socket_t socket);
    }

    /**
    *   a class that handles the actual FastEventServer service
    */
    class Service {
    public:
        enum Status { Acknowledge, HandlingError, CloseRequest, ShutdownRequest };

        /**
        *   attempts to build a Service instance.
        *   @param      cfg     the configuration options for the service
        *   @returns    result  if successful(), get() will yield a Service object
        */
        static Result<Service *> configure(Config& cfg, const bool& verbose=true);

        /**
        *   runs the loop, and automatically shuts down itself afterwards.
        */
        void   run(const bool& verbose=true);

    private:
        /**
        *   just to make sure proper startup/cleanup in Windows.
        */
        static network::Manager _network;

        /**
        *   a private subroutine to configure output
        *   @param      name    the identifier of the output driver
        *   @param      options configuration options for the output driver
        *   @returns    driver  the driver with the specified identifier, or the dummy output if not found
        */
        static OutputDriver*    get_driver(const std::string& name, Config& options, const bool& verbose=true);

        /**
        *   a private routine for attempting to bind to the specified port.
        *   the bound listening socket will be returned when Result::successful().
        */
        static Result<socket_t> bind(uint16_t port, const bool& verbose=true);

        /**
        *   the private routine inside the while(true) loop.
        *   @returns next   true if no errors are detected, and no shutdown command has been received
        */
        bool    loop(const bool& verbose=true);

        /**
        *   a private routine for accepting an incoming client.
        *   searches for an empty space in `client_` and add to it.
        *   also handles updating `nclient_`, `fdread_` and `fdwatch_`.
        */
        void    acceptClient();

        /**
        *   a private routine for closing a client socket.
        *   removes the client socket from the `client_` list.
        *   also handles updating `nclient_`, `fdread_` and `fdwatch_`.
        */
        void    closeClient(socket_t client);

        /**
        *   a private routine for updating `fdwatch_`.
        *   normally called from `acceptClient()` or `closeClient()`.
        */
        void    compute_fdwatch();

        /**
        *   the routine for handling a request from a client.
        *   `driver` is explicitly passed from `loop()` in case we plug an
        *   external handler in the future.
        *
        *   @param      client  the client socket from which a request is read
        *   @param      driver  the output generator driver to be used
        *   @returns    status  a Service::Status value to represent the resulting response
        */
        Status  handle(socket_t client, OutputDriver* driver);

        /**
        *   a private routine for shutting down the service.
        *   called internally from `run()`.
        */
        void    shutdown(const bool& verbose=true);

        /**
        *   the private constructor.
        *   use `configure()` instead to build a Service.
        */
        Service(socket_t listening, OutputDriver* driver);

        /**
        *   the listening socket object
        */
        socket_t        socket_;

        /**
        *   the output generator driver to be used
        */
        OutputDriver*   driver_;

        /**
        *   list of client sockets
        */
        socket_t        client_[CONN_MAX];

        /**
        *   the number of clients that are currently connecting to the service
        */
        size_t          nclient_;

        /**
        *   `fdread_` and `fdwatch_` are used as arguments of `select()` call
        */
        fd_set         fdread_;
        int            fdwatch_;
    };
}

#endif
