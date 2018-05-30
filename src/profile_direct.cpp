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
*   profile_direct.cpp -- the code for profiling direct control over the driver
*/
#include <iostream>
#include "ks/utils.h"
#include "ks/timing.h"
#include "config.h"
#include "driver.h"
#include "dummydriver.h"
#include "arduinodriver.h"

const unsigned NUMIO = 10000;

int main()
{
    ks::Result<fastevent::Config> config = fastevent::config::load("service.cfg");
    if (config.failed()) {
        std::cerr << "***failed to load config file" << std::endl;
        return 1;
    }

    registerOutputDriver(fastevent::driver::DummyDriver);
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
    uint64_t *sent = new uint64_t[NUMIO];
    uint64_t *recv = new uint64_t[NUMIO];

    ks::nanostamp   nanos;
    bool event = true;

    for(int i=0; i<NUMIO; i++) {
        event = !event;
        nanos.get(sent+i);
        driver->event(event);
        nanos.get(recv+i);
        if (i % 500 == 499) {
            std::cerr << ".";
        }
    }
    std::cerr << std::endl;

    // write into std out
    std::cerr << "writing...";
    std::cout << "Sent,Received" << std::endl;
    for(int i=0; i<NUMIO; i++){
        std::cout << sent[i] << ',' << recv[i] << std::endl;
    }
    std::cerr << "done." << std::endl;

    delete[] sent;
    delete[] recv;
    delete driver;
    return 0;
}
