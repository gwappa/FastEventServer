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
*   test client program for profiling of FastEventServer
*/

#include <iostream>
#include <fstream>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>
#endif

#include "picojson.h"
#include "json.h"
#include "utils.h"

#ifdef _WIN32
typedef SOCKET            socket_t;
typedef SOCKADDR          socketaddr_t;
#define close_socket_func ::closesocket
#else
typedef int               socket_t;
typedef struct sockaddr   socketaddr_t;
#define close_socket_func ::close

const   int INVALID_SOCKET  = -1;
const   int SOCKET_ERROR    = -1;
#endif

void run(const uint16_t& port, const int& rate, const int& num);
int  get_client(const uint16_t& port);
int  transact(socket_t sock, const char& cmd);
char random_command();

const int32_t   NANOS_IN_SEC = 1000000000;
const uint64_t  MAX_TIME_SPENT   = 10000000000000000000ULL;
const char      COMMANDS[]   = { '1','2','A','B' };
const int       NO_SOCKET    = 0;
const int       MSG_WIDTH    = 3;

char sendbuf[] = { 'A', '\r', '\n' };
char recvbuf[] = { '\0', '\0', '\0' };

using namespace fastevent;

void run(const uint16_t& port, const int& rate, const int& num)
{
    std::cout << "port=" << port << ", rate=" << rate << ", num=" << num << std::endl;

    nanostamp stamp;
    nanotimer timer;
    averager<uint64_t, uint64_t> elapsed(MAX_TIME_SPENT);
    averager<uint64_t, uint64_t> slept(MAX_TIME_SPENT);

    // calculate default intervals
    uint32_t interval  = NSEC_IN_SEC / rate;
    std::cout << "interval= " << interval << "ns" << std::endl;
    timer.set_interval(interval);

    int dotby = num/10;

    // generate random commands
    std::cout << "preparing..." << std::endl;
    char* commands = new char[num];
    for (int i=0; i<num; i++)
    {
        commands[i] = random_command();
    }

    // connect to server
    socket_t sock = get_client(port);
    uint64_t start, stop, prevstop;
    uint64_t lat;
    stamp.get(&stop);
    if (sock != NO_SOCKET) {
        try {
            std::cout << "generating" << std::flush;
            // transact with intervals
            for(int i=0; i<num; i++)
            {
                prevstop = stop;
                stamp.get(&start);
                timer.sleep();
                stamp.get(&stop);

                slept.add(stop - start);
                elapsed.add(stop - prevstop);

                if (transact(sock, commands[i]))
                {
                    break;
                }

                if (i%dotby == (dotby-1)) {
                    std::cout << "." << std::flush;
                }
            }
            std::cout << "done!" << std::endl;
            // std::cout << "  slept=" << slept.sum() << "/" << slept.num() << std::endl;
            // std::cout << "elapsed=" << elapsed.sum() << "/" << elapsed.num() << std::endl;
            std::cout << "   average sleep: " << slept.get()/1000 << " us" << std::endl;
            std::cout << "average interval: " << elapsed.get()/1000 << " us" << std::endl;
            std::cout << "   response rate: " << elapsed.get_inv(NSEC_IN_SEC) << " Hz" << std::endl;
        } catch (std::exception) {
            std::cerr << "***error during execution!" << std::endl;
        }
        // close socket
        close_socket_func(sock);
    } else {
        std::cout << "nothing to do. there is no socket open." << std::endl;
    }
    delete [] commands;
}

char random_command()
{
    return COMMANDS[rand()%4];
}

int  get_client(const uint16_t& port)
{
    socket_t sock;
    struct sockaddr_in service;

    if ((sock = ::socket(PF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
    {
        std::cerr << "***failed to create a client socket: " << error_message() << std::endl;
        return NO_SOCKET;
    }

    memset(&service, 0, sizeof(service));
    service.sin_family = PF_INET;
    service.sin_addr.s_addr = inet_addr("127.0.0.1");
    service.sin_port = htons(port);

    if (::connect(sock, (socketaddr_t *)&service, sizeof(service)) == SOCKET_ERROR) {
        std::cerr << "***failed to connect to the server: " << error_message() << std::endl;
        close_socket_func(sock);
        return NO_SOCKET;
    }

    std::cout << "connected to port " << port << "..." << std::endl;
    return sock;
}

int  transact(socket_t sock, const char& cmd)
{
    sendbuf[0] = cmd;
    int count = 0;

    // sending a command
    for(int resp=::send(sock, sendbuf, MSG_WIDTH, 0);
        count < MSG_WIDTH;
        resp = ::send(sock, sendbuf+count, MSG_WIDTH-count, 0))
    {
        switch (resp)
        {
        case SOCKET_ERROR:
            // unexpected error
            std::cerr << "***unexpected I/O error on writing: " << error_message() << std::endl;
            return 1;
        default:
            // keep writing until count == MSG_WIDTH
            count += resp;
            break;
        }
    }

    count = 0;

    // receiving a response
    for (int resp = ::recv(sock, recvbuf, MSG_WIDTH, 0);
         count < MSG_WIDTH;
         resp = ::recv(sock, recvbuf+count, MSG_WIDTH-count, 0))
    {
        switch (resp)
        {
        case 0:
            // socket closed
            std::cerr << "***socket seems to have been closed" << std::endl;
            return 1;
        case SOCKET_ERROR:
            // unexpected error
            std::cerr << "***unexpected I/O error on reading: " << error_message() << std::endl;
            return 1;
        default:
            // keep reading until count == MSG_WIDTH
            count += resp;
            break;
        }
    }

    return 0;
}

#ifdef _WIN32
#include <sstream>
#include <stdexcept>
class network
{
public:
    explicit network(){
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2,0), &wsaData);
        int err = WSAStartup(MAKEWORD(2,0), &wsaData);
        if( err != 0 ){
            std::stringstream ss;
            ss << "could not start network: " << error_message();
            throw std::runtime_error(ss.str());
        }
    }

    ~network(){
        WSACleanup();
    }
};
#endif

int main()
{
#ifdef _WIN32
    network net;
#endif
    std::ifstream cfgfile("testclient.cfg");
    json::container cfgroot;
    cfgfile >> cfgroot;

    if( cfgfile.rdstate() & std::ios_base::failbit ){
        std::cerr << "***JSON parse failed: " <<
                  picojson::get_last_error() << std::endl;

        return 1;
    }

    try {
        json::dict dict = cfgroot.get<json::dict>();

        uint16_t port = json::get<uint16_t>(dict, "port");
        int      rate = json::get<int>(dict, "rate");
        int      num  = json::get<int>(dict, "num");

        run(port, rate, num);
        return 0;

    } catch (const std::runtime_error& e) {
        std::cerr << "***parse error: " << e.what() << std::endl;
        return 1;
    }

}
