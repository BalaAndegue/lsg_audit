#pragma once
// validator.hpp — RFC 1918 address guard
// No dependencies outside the C standard library.

#include <string>
#include <stdexcept>
#include <arpa/inet.h>

class RFC1918Validator {
public:
    // Returns true iff ip_str is in RFC1918 or loopback space.
    // Throws std::invalid_argument on malformed input.
    static bool is_private(const std::string& ip_str) {
        struct in_addr addr{};
        if (inet_pton(AF_INET, ip_str.c_str(), &addr) != 1)
            throw std::invalid_argument("Format IPv4 invalide : " + ip_str);

        const uint32_t ip = ntohl(addr.s_addr);
        return ((ip & 0xFF000000u) == 0x0A000000u)  // 10.0.0.0/8
            || ((ip & 0xFFF00000u) == 0xAC100000u)  // 172.16.0.0/12
            || ((ip & 0xFFFF0000u) == 0xC0A80000u)  // 192.168.0.0/16
            || ((ip & 0xFF000000u) == 0x7F000000u); // 127.0.0.0/8
    }

    RFC1918Validator() = delete; // utility class, not instantiable
};
