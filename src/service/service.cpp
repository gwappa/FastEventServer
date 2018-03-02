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
*   service.cpp -- see service.h for description
*/
#include "service.h"
#include "dummydriver.h"

#ifdef _WIN32
typedef int             socketlen_t;
typedef char *          optionvalue_t;

#else
#include <unistd.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
const int INVALID_SOCKET = -1;
const int SOCKET_ERROR  = -1;

typedef socklen_t       socketlen_t;
typedef void *          optionvalue_t;
#endif

#include <iostream>
#include <sstream>

#define NO_CLIENT 0

namespace fastevent {

    namespace network {
        bool initialized = false;

        Manager::Manager(){
            if (!initialized) {
                std::cout << "network: starting up..." << std::endl;
#ifdef _WIN32
                int err;
                WSADATA wsaData;
                err = WSAStartup(MAKEWORD(2,0), &wsaData);
                if( err != 0 ){
                    std::stringstream ss;
                    ss << "could not start network: " << error_message();
                    throw std::runtime_error(ss.str());
                }
#endif
                initialized = true;
            }
        }

        Manager::~Manager(){
            std::cout << "network: cleaning up..." << std::endl;
#ifdef _WIN32
            WSACleanup();
#endif
        }


        /**
        *   just don't want to write this in-place
        */
        inline int set_reuse_address(socket_t sock, int enable=1)
        {
          return setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
                            (optionvalue_t)&enable, sizeof(enable));
        }

        /**
        *   handle differences in names for closing socket
        */
#ifdef _WIN32
#define close_socket__  closesocket
#else
#define close_socket__  ::close
#endif
        inline int close_socket(socket_t sock)
        {
            return close_socket__(sock);
        }
#undef close_socket__
    }

    namespace protocol {
        const size_t WIDTH = 3; // command byte + '\r' + '\n'

        Status read(socket_t socket, char *cmdbuf)
        {
            char buf[WIDTH];
            int count = 0;

            for (int resp = ::recv(socket, buf, WIDTH, 0);
                 count < WIDTH;
                 resp = ::recv(socket, buf+count, WIDTH-count, 0))
            {
                switch (resp)
                {
                case 0:
                    // socket closed
                    return Closed;
                case -1:
                    // unexpected error
                    return Error;
                default:
                    // keep reading until count == WIDTH
                    count += resp;
                    break;
                }
            }
            *cmdbuf = buf[0];
            return Success;
        }

        Status acq(socket_t socket)
        {
            const char msg[] = "Y\r\n";
            int count = 0;

            for (int resp = ::send(socket, msg, WIDTH, 0);
                 count < WIDTH;
                 resp = ::send(socket, msg+count, WIDTH-count, 0))
            {
                switch (resp)
                {
                case -1:
                    // unexpected error
                    return Error;
                default:
                    // keep reading until count == WIDTH
                    count += resp;
                    break;
                }
            }
            return Success;
        }
    }

    network::Manager Service::_network;

    Service::Service(socket_t listening, OutputDriver *driver):
        socket_(listening),
        driver_(driver),
        nclient_(0),
        fdwatch_(listening+1)
    {
        for(int i=0; i<CONN_MAX; i++){
            client_[i] = NO_CLIENT;
        }
        FD_ZERO(&fdread_);
        FD_SET(socket_, &fdread_);
    }

    Result<Service *> Service::configure(Config& cfg, const bool& verbose)
    {
        // json::dump(cfg);
        uint16_t port = json::get<uint16_t>(cfg, "port");
        json::dict d;
        std::string drivername(json::get<std::string>(cfg, "driver"));
        json::dict options(json::get<json::dict>(cfg, "options"));
        if (verbose) {
            std::cout << "port=" << port << ", driver=" << drivername << std::endl;
        }

        // initialize driver
        OutputDriver *driver = get_driver(drivername, options, verbose);

        // initialize server
        Result<socket_t> servicesetup = Service::bind(port);
        if (servicesetup.failed()) {
            driver->shutdown();
            delete driver;
            return Result<Service *>::failure(servicesetup.what());
        }
        socket_t sock = servicesetup.get();
        return Result<Service *>::success(new Service(sock, driver));
    }

    OutputDriver* Service::get_driver(const std::string& name, Config& options, const bool& verbose)
    {
        Result<OutputDriver *> driversetup = OutputDriver::setup(name, options, verbose);

        if (driversetup.successful()) {
            return driversetup.get();
        } else {
            std::cerr << "***failed to initialize the output driver: " << driversetup.what() << "." << std::endl;
            std::cerr << "***falling back to using a dummy output driver." << std::endl;
            return new fastevent::driver::DummyDriver(options);
        }
    }

    Result<socket_t> Service::bind(uint16_t port, const bool& verbose)
    {
        // create a socket to listen to
        socket_t listening = socket(AF_INET, SOCK_STREAM, 0);
        if (listening == INVALID_SOCKET) {
            return Result<socket_t>::failure("network error: could not initialize the listening socket");
        }

        // configure socket option
        if( network::set_reuse_address(listening) == SOCKET_ERROR ){
            return Result<socket_t>::failure("network error: configuration failed for the listening socket");
        }

        // address/port settings
        struct sockaddr_in service;
        memset(&service, 0, sizeof(service));
        service.sin_family      = AF_INET;
        service.sin_port        = htons(port);
        service.sin_addr.s_addr = htonl(INADDR_ANY);

        // binding
        if( ::bind( listening, (struct sockaddr *)&service, sizeof(service) ) == SOCKET_ERROR ){
            std::stringstream ss;
            ss << "network error: failed to bind to port " << port << " (" << error_message() << ")";
            return Result<socket_t>::failure(ss.str());
        }

        // start listening
        if( ::listen(listening, CONN_MAX) == SOCKET_ERROR ){
            std::stringstream ss;
            ss << "network error: failed to start listening to port " << port
                << " (" << error_message() << ")";
            return Result<socket_t>::failure(ss.str());
        }

        if (verbose) {
            std::cout << "start listening to port " << port << "..." << std::endl;
        }

        return Result<socket_t>::success(listening);
    }

    void Service::run(const bool& verbose)
    {
        while(loop(verbose));
        shutdown();
    }

    bool Service::loop(const bool& verbose)
    {
        // perform select(2)
        fd_set _mask;
        memcpy(&_mask, &fdread_, sizeof(fdread_));
        select(fdwatch_, &_mask, NULL, NULL, NULL);

        // placeholder for `handle()` return value
        Status res;

        // check client sockets
        for (int i=0; i<CONN_MAX; i++) {
            if (client_[i] == NO_CLIENT) {
                continue;
            }
            if (FD_ISSET(client_[i], &_mask)) {
                // receiving data from an existing client
                res = handle(client_[i], driver_);

                if( res == HandlingError ){ // unexpected error during handle()
                    return false;

                } else if ( res == CloseRequest ){ // connection closed
                    closeClient(client_[i]);
                    if( nclient_ == 0 ){ // if no more client is there, just shut down
                        return false;
                    }

                } else if ( res == ShutdownRequest ){ // SHUTDOWN received
                    return false;
                }

                return true;
            } // if FD_ISSET
        }// for loop for connections

        // check the listening socket
        if (FD_ISSET(socket_, &_mask)) {
            // new client connecting to the listening socket
            acceptClient();
            return true;
        }

        // should not reach (as there must be no timeout)
        std::cerr << "***service error: select() timeout" << std::endl;
        return false;
    }

    void Service::acceptClient()
    {
        // call accept(2)
        socket_t    conn;
        struct sockaddr_in client;
        socketlen_t len = sizeof(client);

        conn = accept(socket_, (struct sockaddr *)&client, &len); // socket.h OR winsock2.h
        if( conn == INVALID_SOCKET ){
            std::cerr << "***service error: accept() failed" << std::endl;
            return;
        }

        // TODO: is this section needed?
        int nodelay = 1;
        setsockopt(conn, IPPROTO_TCP, TCP_NODELAY, (char *)&nodelay, sizeof(int));

        // add to client list: obtain the index for the new client socket
        int idx = -1;
        for( int i=0; i<CONN_MAX; i++ ){
            if( client_[i] == NO_CLIENT ){
                idx = i;
                break;
            }
        }

        // update nclient_, fdread_ and fdwatch_ according to the new client socket
        if ( idx != -1 ){
            client_[idx] = conn;
            nclient_++;
            FD_SET(conn, &fdread_);
            compute_fdwatch();
        } else {
            std::cerr << "***service error: cannot accept a client anymore (the list is full)" << std::endl;
            network::close_socket(conn);
        }
    }

    void Service::closeClient(socket_t client)
    {
        // close the socket
        network::close_socket(client);

        // remove the socket from the client list, and decrement nclient_
        int i;
        for( i=0; i<CONN_MAX; i++ ){
            if( client_[i] == client ){
                client_[i] = NO_CLIENT;
                nclient_--;
                break;
            }
        }

        // update fdread_ and fdwatch_
        FD_CLR(client, &fdread_);
        compute_fdwatch();
    }

    void Service::compute_fdwatch()
    {
        // let `watch` be the maximum file descriptor
        int watch = static_cast<int>(socket_);
        for(int i=0; i<CONN_MAX; i++)
        {
            if( watch < client_[i] ){
                watch = static_cast<int>(client_[i]);
            }
        }

        // `fdwatch_` be watch + 1 (from select(2))
        fdwatch_ = watch + 1;
    }

    Service::Status Service::handle(socket_t client, OutputDriver* driver)
    {
        char cmd;

        switch (protocol::read(client, &cmd))
        {
        case protocol::Success:
            break;
        case protocol::Closed:
            return CloseRequest;
        case protocol::Error:
        default:
            return HandlingError;
        }

        switch (cmd)
        {
        case protocol::SYNC_ON:
            driver->sync(true);
            break;
        case protocol::SYNC_OFF:
            driver->sync(false);
            break;
        case protocol::EVENT_ON:
            driver->event(true);
            break;
        case protocol::EVENT_OFF:
            driver->event(false);
            break;
        case protocol::SHUTDOWN:
            protocol::acq(client);
            return ShutdownRequest;
        default:
            std::cerr << "unknown command: '" << cmd << "'" << std::endl;
            break;
        }
        protocol::acq(client);
        return Acknowledge;
    }

    void Service::shutdown(const bool& verbose)
    {
        if (verbose) {
            std::cout << "shutting down the server..." << std::endl;
        }

        // close all the client sockets without any notification
        for(int i=0; i<CONN_MAX; i++){
            if (client_[i] != NO_CLIENT) {
                network::close_socket(client_[i]);
            }
        }

        // close the listening socket
        if( network::close_socket(socket_) ){
            std::cerr << "***an error seem to have occurred while closing the listening socket, but ignored: ";
            std::cerr << error_message() << std::endl;
        }

        // shut down the output driver
        driver_->shutdown();
        delete driver_;
    }
}
