/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2026 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef GDIAL_CPP_TEST_STUBS_HPP_
#define GDIAL_CPP_TEST_STUBS_HPP_

#include <cstdint>
#include <map>
#include <string>

#ifndef _T
#define _T(x) x
#endif

namespace WPEFramework {

class JsonObject {
public:
    std::map<std::string, std::string> values;

    std::string &operator[](const std::string &key)
    {
        return values[key];
    }
};

namespace Core {

static const uint32_t ERROR_NONE = 0;

class SystemInfo {
public:
    static void SetEnvironment(const std::string &, const std::string &) {}
};

namespace JSON {

class IElement {};

class String {
public:
    String() = default;
    explicit String(const std::string &v) : value_(v) {}

    const std::string &Data() const { return value_; }

    String &operator=(const std::string &v)
    {
        value_ = v;
        return *this;
    }

private:
    std::string value_;
};

template <typename T>
class ArrayType {
public:
    class Iterator {
    public:
        explicit Iterator(const ArrayType &) {}
        bool Next() { return false; }
        const T &Current() const
        {
            static T t;
            return t;
        }
    };

    const ArrayType &Elements() const { return *this; }
};

} // namespace JSON
} // namespace Core

namespace PluginHost {
namespace MetaData {

struct Service {
    Core::JSON::String JSONState;
};

} // namespace MetaData
} // namespace PluginHost

namespace JSONRPC {

template <typename T>
class LinkType {
public:
    LinkType(const std::string &, bool, const std::string &) {}

    template <typename R>
    uint32_t Get(uint32_t, const std::string &, R &)
    {
        return Core::ERROR_NONE;
    }

    uint32_t Invoke(const std::string &, const JsonObject &, JsonObject &)
    {
        return Core::ERROR_NONE;
    }
};

} // namespace JSONRPC
} // namespace WPEFramework

inline int GetSecurityToken(int, unsigned char *)
{
    return 0;
}

#endif
