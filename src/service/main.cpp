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
*   main.cpp -- the main routine for FastEventServer
*/
#include <iostream>
#include "config.h"
#include "dummydriver.h"
#include "leonardo.h"
#include "service.h"

int main()
{
    fastevent::Result<fastevent::Config> config = fastevent::config::load("service.cfg");
    if (config.failed()) {
        std::cerr << "***failed to load config file" << std::endl;
        return 1;
    }

    registerOutputDriver(fastevent::driver::DummyDriver);
    registerOutputDriver(fastevent::driver::LeonardoDriver);

    fastevent::Result<fastevent::Service *> result = fastevent::Service::configure(config.get());
    if (result.failed()) {
        std::cerr << "***failed to set up the service: " << result.what() << "." << std::endl;
        return 1;
    }

    fastevent::Service* service = result.get();
    service->run();
    delete service;
    return 0;
}
