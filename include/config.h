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
*   config.h -- retrieves configurations for server initialization
*/

#ifndef __FE_CONFIG_H__
#define __FE_CONFIG_H__

#include "json.h"
#include "ks/utils.h"

namespace fastevent {
    typedef json::dict Config;

    namespace config {
        ks::Result<Config> load(const std::string& filename);
    }
}

#endif
