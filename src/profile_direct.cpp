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
*   profile_direct.cpp -- the code for profiling direct control over the driver
*/
#include <iostream>
#include <string>
#include <stdio.h>
#include <string.h>

#include "ks/utils.h"
#include "ks/timing.h"
#include "config.h"
#include "driver.h"
#include "dummydriver.h"
#include "arduinodriver.h"

const unsigned DEFAULT_NUMIO = 10000;

int print_usage(const char *progname) {
    std::cerr << "***usage: " << progname 
            << " [-n <num_transactions, defaults to 10000>]"
            << " <config file path>" << std::endl;
    return 1;
}

int main(int argc, char* argv[])
{
    unsigned int num_io = DEFAULT_NUMIO;
    unsigned int cfgref = 1;

    if (argc < 2) {
        return print_usage(argv[0]);
    } else if (strncmp(argv[1], "-n", 2) == 0) {
        if (argc != 4) {
            return print_usage(argv[0]);

        } else if (sscanf(argv[2], "%u", &num_io) == std::char_traits<char>::eof()) {
            std::cerr << "***failed to parse number of transactions: "
                << argv[2] << std::endl;
            return print_usage(argv[0]);

        } else {
            cfgref = 3;

        }
    }
    std::cerr << "config file:       " << argv[cfgref] << std::endl;
    std::cerr << "# of transactions: " << num_io << std::endl;

    ks::Result<fastevent::Config> config = fastevent::config::load(argv[cfgref]);
    if (config.failed()) {
        std::cerr << "***failed to load config file" << std::endl;
        return 1;
    }

    registerOutputDriver(fastevent::driver::DummyDriver);
    registerOutputDriver(fastevent::driver::VerboseDummyDriver);
    registerOutputDriver(fastevent::driver::UnoDriver);
    registerOutputDriver(fastevent::driver::LeonardoDriver);

    // get information from config file
    fastevent::Config cfg = config.get();
    std::string drivername(fastevent::json::get<std::string>(cfg, "driver"));
    fastevent::json::dict options(fastevent::json::get<fastevent::json::dict>(cfg, "options"));
    std::cerr << "driver=" << drivername << std::endl;

    // initialize driver
    fastevent::OutputDriver *driver = 0;
    ks::Result<fastevent::OutputDriver *> driversetup = fastevent::OutputDriver::setup(drivername, options, true);

    if (driversetup.successful()) {
        driver =  driversetup.get();
    } else {
        std::cerr << "***failed to initialize the output driver: " << driversetup.what() << "." << std::endl;
        std::cerr << "***falling back to using a dummy output driver." << std::endl;
        driver = new fastevent::driver::DummyDriver(options);
    }

    // try IN/OUT for some time
    std::cerr << "sending test commands";
    uint64_t *sent = new uint64_t[num_io];
    uint64_t *recv = new uint64_t[num_io];

    ks::nanostamp   nanos;
    bool event = true;
    const char EVENT_ON  = 'A';
    const char EVENT_OFF = 'D';

    for(int i=0; i<num_io; i++) {
        event = !event;
        nanos.get(sent+i);
        driver->update(event? EVENT_ON:EVENT_OFF);
        nanos.get(recv+i);
        if (i % 500 == 499) {
            std::cerr << ".";
        }
    }
    std::cerr << std::endl;

    // write into std out
    std::cerr << "writing...";
    std::cout << "Sent,Received" << std::endl;
    for(int i=0; i<num_io; i++){
        std::cout << sent[i] << ',' << recv[i] << std::endl;
    }
    std::cerr << "done." << std::endl;

    delete[] sent;
    delete[] recv;
    delete driver;
    return 0;
}
