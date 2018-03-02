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
*   json.h -- working with PICOJSON objects
*/

#ifndef _JSON_H_
#define _JSON_H_
#include <string>
#include <typeinfo>
#include <stdint.h>
#include <stdexcept>
#include "picojson.h"

namespace fastevent {
  namespace json {

    typedef picojson::object          dict;
    typedef picojson::array           array;
    typedef picojson::array::iterator iterator;
    typedef picojson::value           container;

    template <typename T> inline
    void dump(T &val, std::ostream &os=std::cerr){
      os << "***cannot dump type: " << typeid(val).name() << std::endl;
    }

    template <> inline
    void dump(picojson::object &val, std::ostream &os)
    {
      for( picojson::object::iterator it=val.begin(); it!=val.end(); ++it ){
        os << it->first << "=>" << (it->second).to_str() << std::endl;
      }
    }

    template <> inline
    void dump(picojson::array &val, std::ostream &os)
    {
      for( picojson::array::iterator it=val.begin(); it!=val.end(); ++it ){
        os << *it << std::endl;
      }
    }

    template <typename ParsedType, typename JSONType> inline
    ParsedType get_converted(picojson::object &dict, const std::string &key)
    {
      picojson::value val = dict[key];
      if( val.is<JSONType>() ){
        return static_cast<ParsedType>(val.get<JSONType>());
      } else {
        throw std::runtime_error("malformed '"+key+"' attribute");
      }
    }

    template <typename T> inline
    T get(picojson::object &dict, const std::string &key)
    {
      picojson::value val = dict[key];
      if( val.is<T>() ){
        return val.get<T>();
      } else {
        throw std::runtime_error("malformed '"+key+"' attribute");
      }
    }

    template <> inline
    int get(picojson::object &dict, const std::string &key)
    {
      return get_converted<int, double>(dict, key);
    }

    template <> inline
    uint16_t get(picojson::object &dict, const std::string &key)
    {
      return get_converted<uint16_t, double>(dict, key);
    }

    template <> inline
    float get(picojson::object &dict, const std::string &key)
    {
      return get_converted<float, double>(dict, key);
    }

  }
}

#endif
