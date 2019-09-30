
//
// Created by berli on 2019/9/30.
//

#ifndef SAFE_EVPP_ENDIAN_H
#define SAFE_EVPP_ENDIAN_H
#include <endian.h>
#include <stdint.h>

namespace sockets
{

    inline uint16_t hostToNetwork16(uint16_t host16)
    {
        return htobe16(host16);
    }

    inline uint32_t hostToNetwork32(uint32_t host32)
    {
        return htobe32(host32);
    }

    inline uint16_t networkToHost16(uint16_t net16)
    {
        return be16toh(net16);
    }

    inline uint32_t networkToHost32(uint32_t net32)
    {
        return be32toh(net32);
    }
}

#endif //SAFE_EVPP_ENDIAN_H
