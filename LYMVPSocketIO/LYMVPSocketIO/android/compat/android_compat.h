#ifndef ANDROID_COMPAT_H
#define ANDROID_COMPAT_H

#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <byteswap.h>

static __inline uint16_t __bswap16_macro(uint16_t __x) {
    return (__x >> 8) | (__x << 8);
}

#define htobe16(x) __bswap16_macro(x)
#define htole16(x) (x)
#define be16toh(x) __bswap16_macro(x)
#define le16toh(x) (x)

#define htobe32(x) bswap_32(x)
#define htole32(x) (x)
#define be32toh(x) bswap_32(x)
#define le32toh(x) (x)

#define htobe64(x) bswap_64(x)
#define htole64(x) (x)
#define be64toh(x) bswap_64(x)
#define le64toh(x) (x)

#ifdef __cplusplus
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cwchar>
#include <cstdint>
#include <string>
#include <memory>
#include <sstream>
#include <type_traits>
#include <utility>

namespace std {
    using ::snprintf;
    using ::vsnprintf;
    using ::round;
    using ::lround;
    using ::llround;
    using ::trunc;
    using ::nearbyint;
    using ::nextafter;
    using ::exp2;
    using ::expm1;
    using ::log1p;
    using ::cbrt;
    using ::hypot;
    using ::erf;
    using ::erfc;
    using ::lgamma;
    using ::tgamma;
    using ::strtoll;
    using ::strtoull;
    using ::llabs;
    using ::strtof;
    using ::strtod;
    using ::strtol;
    using ::strtoul;

    inline long double strtold(const char* str, char** endptr) {
        return static_cast<long double>(::strtod(str, endptr));
    }

    template <typename T>
    string to_string(const T& value) {
        ostringstream oss;
        oss << value;
        return oss.str();
    }

    template <typename T, typename... Args>
    unique_ptr<T> make_unique(Args&&... args) {
        return unique_ptr<T>(new T(std::forward<Args>(args)...));
    }

    template <typename T>
    unique_ptr<T> make_unique(size_t n) {
        return unique_ptr<T>(new typename std::remove_extent<T>::type[n]());
    }

    inline int stoi(const string& str, size_t* idx = 0, int base = 10) {
        const char* cstr = str.c_str();
        char* end = nullptr;
        long val = ::strtol(cstr, &end, base);
        if (idx) *idx = end - cstr;
        return static_cast<int>(val);
    }

    inline long stol(const string& str, size_t* idx = 0, int base = 10) {
        const char* cstr = str.c_str();
        char* end = nullptr;
        long val = ::strtol(cstr, &end, base);
        if (idx) *idx = end - cstr;
        return val;
    }

    inline unsigned long stoul(const string& str, size_t* idx = 0, int base = 10) {
        const char* cstr = str.c_str();
        char* end = nullptr;
        unsigned long val = ::strtoul(cstr, &end, base);
        if (idx) *idx = end - cstr;
        return val;
    }

    inline long long stoll(const string& str, size_t* idx = 0, int base = 10) {
        const char* cstr = str.c_str();
        char* end = nullptr;
        long long val = ::strtoll(cstr, &end, base);
        if (idx) *idx = end - cstr;
        return val;
    }

    inline unsigned long long stoull(const string& str, size_t* idx = 0, int base = 10) {
        const char* cstr = str.c_str();
        char* end = nullptr;
        unsigned long long val = ::strtoull(cstr, &end, base);
        if (idx) *idx = end - cstr;
        return val;
    }

    inline float stof(const string& str, size_t* idx = 0) {
        const char* cstr = str.c_str();
        char* end = nullptr;
        float val = ::strtof(cstr, &end);
        if (idx) *idx = end - cstr;
        return val;
    }

    inline double stod(const string& str, size_t* idx = 0) {
        const char* cstr = str.c_str();
        char* end = nullptr;
        double val = ::strtod(cstr, &end);
        if (idx) *idx = end - cstr;
        return val;
    }
}

#include <locale.h>

namespace std {
    using ::lconv;
    using ::localeconv;
    using ::setlocale;
}

#endif

#endif
