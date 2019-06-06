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
#include <string.h>

#define NO_CLIENT 0

#define RipCommands(BUF) ((BUF[protocol::STATUS_BYTE])&MASK_COMMANDS)

namespace fastevent {

#define IsShutdown(BUF) has_shutdown(BUF[protocol::STATUS_BYTE])

    namespace network {
        bool initialized = false;

        Manager::Manager(){
            if (!initialized) {
                std::cerr << "network: starting up..." << std::endl;
#ifdef _WIN32
                int err;
                WSADATA wsaData;
                err = WSAStartup(MAKEWORD(2,0), &wsaData);
                if( err != 0 ){
                    std::stringstream ss;
                    ss << "could not start network: " << ks::error_message();
                    throw std::runtime_error(ss.str());
                }
#endif
                initialized = true;
            }
        }

        Manager::~Manager(){
            std::cerr << "network: cleaning up..." << std::endl;
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

    Socket::Socket(const socket_t& sock):
        socket_(sock), lock_() { }

    Socket::~Socket() { }

    int Socket::recv(char *buf, const int& len,
                        struct sockaddr_in* sender) {
        ks::MutexLocker locker(&lock_);
        socketlen_t addr = sizeof(struct sockaddr_in);
        return ::recvfrom(socket_, buf, len, 0,
                            (struct sockaddr*)sender, &addr);
    }

    int Socket::send(const char *buf, const int& len,
                        struct sockaddr_in* client) {
        ks::MutexLocker locker(&lock_);
        return ::sendto(socket_, buf, len, 0,
                            (struct sockaddr*)client, sizeof(struct sockaddr_in));
    }

    void Socket::close() {
        ks::MutexLocker locker(&lock_);
        if( network::close_socket(socket_) ){
            std::cerr << "***an error seem to have occurred while closing the listening socket, but ignored: ";
            std::cerr << ks::error_message() << std::endl;
        }
    }

    IOBuffer::IOBuffer() {}
    IOBuffer::~IOBuffer()
    {
        update_.set();
        update_.notifyAll();
    }

    bool IOBuffer::read(struct sockaddr_in* client, char *buffer)
    {
        if (is_eof_) {
            return false;
        }

        update_.lock();
        if (!update_.isset()) {
            update_.wait();
        }
        if (is_eof_) {
            // do not reset the flag, but only release the lock
            update_.unlock();
            return false;
        }
        memcpy(client, &client_, sizeof(client_));
        memcpy(buffer, packet_, protocol::MSG_SIZE);
        update_.unset();
        update_.unlock();
        return true;
    }

    void IOBuffer::write(const struct sockaddr_in* client,
                         const char *buffer,
                         const bool& is_eof)
    {
        update_.lock();
        if (!is_eof) {
            memcpy(&client_, client, sizeof(client_));
            memcpy(packet_, buffer, protocol::MSG_SIZE);
        }
        is_eof_ = is_eof;
        update_.set();
        update_.notifyAll();
        update_.unlock();
    }


    IOBuffer *DriverThread::getInputBufferRef() { return &input_; };

    IOBuffer *DriverThread::getOutputBufferRef() { return &output_; };

    void DriverThread::run()
    {
        while(true) {
            if (!input_.read(&client_, buffer_)) {
                // shutdown
                goto FINALLY;
            }

            // send command to the driver
            switch (buffer_[protocol::STATUS_BYTE]) {

            // newline characters
            case '\r':
            case '\n':
                // do nothing
                break;

            // other characters are treated as a command
            default:
                driver_->update(RipCommands(buffer_));
                break;
            }

            output_.write(&client_, buffer_);
        }
FINALLY:
        output_.write_eof();
        shutdown();
    }

    void DriverThread::shutdown() {
        // shut down the output driver
        driver_->shutdown();
        delete driver_;
    }

    void ResponseThread::run()
    {
        while(true) {
            if (!(input_->read(&client_, buffer_))) {
                // shutdown
                goto FINALLY;
            }

            // send the command back to the client
            while (true) {
                switch (socket_->send(buffer_, protocol::MSG_SIZE, &client_))
                {

                case protocol::MSG_SIZE:
                    // success
                    goto DONE_SENDING;

                case 0:
                    // waiting
                    continue;

                case SOCKET_ERROR:
                default:
                    std::cerr << "***failed to send a packet: "
                              << ks::error_message() << std::endl;
                    goto FINALLY;
                }
            }
DONE_SENDING:
            // continue the loop
            continue;
        }
FINALLY:
        return;
    }

    Service::Service(socket_t listening, OutputDriver *driver):
        socket_desc_(listening), socket_(listening),
        fdwatch_(static_cast<int>(listening+1))
    {
        FD_ZERO(&fdread_);
        FD_SET(socket_desc_, &fdread_);

        driver_     = new DriverThread(driver);
        response_   = new ResponseThread(&socket_, driver_->getOutputBufferRef());
        output_     = driver_->getInputBufferRef();
    }

    ks::Result<Service *> Service::configure(Config& cfg, const bool& verbose)
    {
        // json::dump(cfg);
        uint16_t port = json::get<uint16_t>(cfg, "port");
        json::dict d;
        std::string drivername(json::get<std::string>(cfg, "driver"));
        json::dict options(json::get<json::dict>(cfg, "options"));
        if (verbose) {
            std::cerr << "port=" << port << ", driver=" << drivername << std::endl;
        }

        // initialize driver
        OutputDriver *driver = get_driver(drivername, options, verbose);

        // initialize server
        ks::Result<socket_t> servicesetup = Service::bind(port);
        if (servicesetup.failed()) {
            driver->shutdown();
            delete driver;
            return ks::Result<Service *>::failure(servicesetup.what());
        }
        socket_t sock = servicesetup.get();

        return ks::Result<Service *>::success(new Service(sock, driver));
    }

    OutputDriver* Service::get_driver(const std::string& name, Config& options, const bool& verbose)
    {
        ks::Result<OutputDriver *> driversetup = OutputDriver::setup(name, options, verbose);

        if (driversetup.successful()) {
            std::cout << ">>> driver: " << name << std::endl;
            return driversetup.get();
        } else {
            std::cerr << "***failed to initialize the output driver: " << driversetup.what() << "; "
                      << "falling back to using a dummy output driver." << std::endl;
            std::cout << ">>> driver: " << fastevent::driver::DummyDriver::identifier() << std::endl;
            return new fastevent::driver::DummyDriver(options);
        }
    }

    ks::Result<socket_t> Service::bind(uint16_t port, const bool& verbose)
    {
        // create a socket to listen to
        socket_t listening = socket(AF_INET, SOCK_DGRAM, 0);
        if (listening == INVALID_SOCKET) {
            return ks::Result<socket_t>::failure("network error: could not initialize the listening socket");
        }

        // configure socket option
        if( network::set_reuse_address(listening) == SOCKET_ERROR ){
            return ks::Result<socket_t>::failure("network error: configuration failed for the listening socket");
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
            ss << "network error: failed to bind to port " << port << " (" << ks::error_message() << ")";
            return ks::Result<socket_t>::failure(ss.str());
        }

        // do not start listening here,
        // as it is not a TCP socket...

        if (verbose) {
            std::cout << ">>> port: " << port << std::endl;
        }

        return ks::Result<socket_t>::success(listening);
    }

    void Service::run(const bool& verbose)
    {
        driver_->start();
        response_->start();

        // perform select(2)
        fd_set              _mask;

        while(true){
            memcpy(&_mask, &fdread_, sizeof(fdread_));
            select(fdwatch_, &_mask, NULL, NULL, NULL);

            // in case there is an input in the socket:
            if (FD_ISSET(socket_desc_, &_mask)) {
                switch(handle()) {
                case HandlingError:
                case ShutdownRequest:
                    goto FINALLY;
                default:
                    continue;
                }
            }

            // should not reach (as there must be no timeout)
            std::cerr << "***service error: select() timeout" << std::endl;
            output_->write_eof();
            goto FINALLY;
        }
FINALLY:
        shutdown();
    }

    Service::Status Service::handle()
    {
        char                buf[protocol::MSG_SIZE];
        struct sockaddr_in  sender;

        // read a UDP packet
        switch (socket_.recv(buf, protocol::MSG_SIZE, &sender)) {
        case 0:
            // do nothing
            break;
        case SOCKET_ERROR:
            std::cerr << "***failed to receive a packet: " << ks::error_message() << std::endl;
            return HandlingError;
        default:
            // message received
            if (IsShutdown(buf)) {
                output_->write_eof();
                return ShutdownRequest;
            } else {
                output_->write(&sender, buf);
            }
            break;
        }
        return Acqknowledge;
    }

    void Service::shutdown(const bool& verbose)
    {
        if (verbose) {
            std::cerr << "shutting down the server..." << std::endl;
        }

        driver_->join();
        response_->join();

        // close the listening socket
        socket_.close();

        delete driver_;
        delete response_;
    }
}
