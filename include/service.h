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
*   service.h -- service functionality
*
*   we have Service, DriverThread and ResponseThread that take care of the client requests.
*
*   1. Service receives requests and push it into the buffer shared with DriverThread.
*   2. DriverThread reads requests from the buffer shared with Service.
*   3. Based on the request, DriverThread transacts with the output driver.
*   4. After transaction, DriverThread push the same command/state into the buffer
*      shared with ResponseThread.
*   5. ResponseThread sends the response (the same command) back to the client.
*
*   If Service receives 'SHUTDOWN' command, it pushes the 'EOF' into the buffer,
*   which then sequentially shuts down the other downstream threads.
*
*   On the other hand, for the time being there is no way for Service to know 
*   whether or not DriverThread and/or ResponseThread is running properly.
*   This may or may not cause some problems in the future, but I cannot predict it.
*   I leave this spec as it is for the moment.
*/

#ifndef __FE_SERVICE_H__
#define __FE_SERVICE_H__

#ifdef _WIN32
#include <winsock2.h>
#else
  #include <sys/socket.h>
  #include <sys/types.h>
  #include <netinet/in.h>
#endif

#include <stdint.h>

#include "ks/utils.h"
#include "ks/thread.h"
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
        const int       MSG_SIZE     = 2;
        const uint8_t   INDEX_BYTE   = 0;
        const uint8_t   STATUS_BYTE  = 1;
    }

    /**
     * the thread-safe wrapper for a socket.
     */
    class Socket
    {
    public:
        explicit Socket(const socket_t& sock);
        ~Socket();

        int recv(char *buf, const int& len, const int& offset,
                    struct sockaddr_in* sender);
        int send(const char *buf, const int& len, const int& offset,
                    struct sockaddr_in* client);

        void close();
    private:
        socket_t    socket_;
        ks::Mutex   lock_;
    };

    /**
     * the buffer structure for communication with threads
     */
    class IOBuffer
    {
    public:
        IOBuffer();
        ~IOBuffer();

        /**
         * wait for the update, read into buffer
         *
         * returns false if the buffer is at EOF (i.e. the read process failed).
         * returns true otherwise.
         */
        bool read(struct sockaddr_in* client, char *buffer);

        /**
         * write into buffer, flag update
         */
        void write(const struct sockaddr_in* client, 
                   const char *buffer,
                   const bool& is_eof=false);

        void write_eof() { write(0, 0, true); }

    private:
        /**
         * the client socket info
         */
        struct sockaddr_in  client_;
        /**
         * the container for the command packet
         */
        char                packet_[protocol::MSG_SIZE]; 
        /**
         * whether or not the buffer is at the eof
         */
        bool                is_eof_;

        /**
         * the event flag object that monitors packet update
         * including its associated mutex object
         */
        ks::Flag            update_;
    };

    /**
     * a thread class that handles communication with the driver
     */
    class DriverThread: public ks::Thread
    {
    public:
        DriverThread(OutputDriver* driver):
            ks::Thread(), driver_(driver), input_(), output_() { }

        ~DriverThread() { }

        /**
         * returns its input-side IO buffer
         */
        IOBuffer *getInputBufferRef();

        /**
         * returns its output-side IO buffer
         */
        IOBuffer *getOutputBufferRef();

        void run();

    private:
        /**
         * shuts down the driver
         */
        void shutdown();

        /**
        *   the output generator driver to be used
        */
        OutputDriver *driver_;

        /**
         * IO buffers for communication between I/O threads
         */
        IOBuffer      input_;
        IOBuffer      output_;

        struct sockaddr_in  client_;
        char                buffer_[protocol::MSG_SIZE];
    };

    /**
     * a thread class for managing output back to client
     */
    class ResponseThread: public ks::Thread
    {
    public:
        ResponseThread(Socket *socket, IOBuffer *input):
            ks::Thread(), socket_(socket), input_(input) { }
        ~ResponseThread() { }

        void run();

    private:
        Socket             *socket_;
        IOBuffer           *input_;
        char                buffer_[protocol::MSG_SIZE];
        struct sockaddr_in  client_;
    };

    /**
    *   a class that handles the actual FastEventServer service
    */
    class Service {
    public:
        static const size_t MAX_MSG_SIZE = 32;
        enum Status { Acqknowledge, HandlingError, CloseRequest, ShutdownRequest };

        /**
        *   attempts to build a Service instance.
        *   @param      cfg     the configuration options for the service
        *   @returns    result  if successful(), get() will yield a Service object
        */
        static ks::Result<Service *> configure(Config& cfg, const bool& verbose=true);

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
        static ks::Result<socket_t> bind(uint16_t port, const bool& verbose=true);

        /**
        *   the routine for handling a request from a client.
        *   `driver` is explicitly passed from `loop()` in case we plug an
        *   external handler in the future.
        *
        *   @returns    status  a Service::Status value to represent the resulting response
        */
        Status  handle();

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
        socket_t       socket_desc_;
        Socket         socket_;

        /**
        *   `fdread_` and `fdwatch_` are used as arguments of `select()` call
        */
        fd_set         fdread_;
        int            fdwatch_;

        /**
         * the other threads
         */
        DriverThread   *driver_;
        ResponseThread *response_;

        /**
         * the I/O buffer for communication between the other threads
         */
        IOBuffer      *output_;
    };
}

#endif
