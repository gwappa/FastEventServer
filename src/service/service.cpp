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

    network::Manager Service::_network;

    Service::Service(socket_t listening, OutputDriver *driver):
        socket_(listening),
        driver_(driver),
        fdwatch_(static_cast<int>(listening+1))
    {
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
        socket_t listening = socket(AF_INET, SOCK_DGRAM, 0);
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

        // do not start listening here,
        // as it is not a TCP socket...

        if (verbose) {
            std::cout << "prepared port " << port << "..." << std::endl;
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
        fd_set              _mask;

        memcpy(&_mask, &fdread_, sizeof(fdread_));
        select(fdwatch_, &_mask, NULL, NULL, NULL);

        // placeholder for `handle()` return value
        Status res;

        // in case there is an input in the socket:
        if (FD_ISSET(socket_, &_mask)) {
            res = handle(driver_);
            if (res == HandlingError) {
                return false;
            } else if (res == ShutdownRequest) {
                return false;
            }
            return true;
        }

        // should not reach (as there must be no timeout)
        std::cerr << "***service error: select() timeout" << std::endl;
        return false;
    }

    Service::Status Service::handle(OutputDriver* driver)
    {
        char                buf[MAX_MSG_SIZE];
        struct sockaddr_in  sender;
        socketlen_t         address_len = sizeof(sender);

        // read a UDP packet
        switch (recvfrom(socket_, buf, MAX_MSG_SIZE - 1, 0,
                    (struct sockaddr*)&sender, &address_len)) {
        case 0:
            // do nothing
            break;
        case SOCKET_ERROR:
            std::cerr << "***failed to receive a packet: " << error_message() << std::endl;
            return HandlingError;
        default:
            // message received
            switch (buf[0]) {
            // in case of single command
            case protocol::SYNC_ON:
                driver->sync(true);
                return acqknowledge(sender);
            case protocol::SYNC_OFF:
                driver->sync(false);
                return acqknowledge(sender);
            case protocol::EVENT_ON:
                driver->event(true);
                return acqknowledge(sender);
            case protocol::EVENT_OFF:
                driver->event(false);
                return acqknowledge(sender);
            // multiplexed command
            case (protocol::SYNC_ON) | (protocol::EVENT_ON):
                driver->update(true, true);
                return acqknowledge(sender);
            case (protocol::SYNC_ON) | (protocol::EVENT_OFF):
                driver->update(true, false);
                return acqknowledge(sender);
            case (protocol::SYNC_OFF) | (protocol::EVENT_ON):
                driver->update(false, true);
                return acqknowledge(sender);
            case (protocol::SYNC_OFF) | (protocol::EVENT_OFF):
                driver->update(false, false);
                return acqknowledge(sender);
            // shutdown command
            case protocol::SHUTDOWN:
                acqknowledge(sender);
                return ShutdownRequest;
            // newline characters
            case '\r':
            case '\n':
                // do nothing
                break;
            // others
            default:
                std::cerr << "unknown command: '" << buf[0] << "'" << std::endl;
                return HandlingError;
            }
            break;
        }
        return Acqknowledge;
    }

    Service::Status Service::acqknowledge(struct sockaddr_in& sender) {
        char msg[] = { protocol::ACQKNOWLEDGE };
        while (true) {
            switch (sendto(socket_, msg, 1, 0, (struct sockaddr*)&sender, sizeof(struct sockaddr_in))) {
            case 1:
                // success
                return Acqknowledge;
            case 0:
                // waiting
                continue;
            case SOCKET_ERROR:
            default:
                std::cerr << "***failed to send a packet: " << error_message() << std::endl;
                return HandlingError;
            }
        }
    }

    void Service::shutdown(const bool& verbose)
    {
        if (verbose) {
            std::cout << "shutting down the server..." << std::endl;
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
