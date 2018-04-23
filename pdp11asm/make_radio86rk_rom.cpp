#include "make_radio86rk_rom.h"
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#ifndef WIN32
#ifdef __APPLE__


#include <libkern/OSByteOrder.h>

#define htobe16(x) OSSwapHostToBigInt16(x)
#define htole16(x) OSSwapHostToLittleInt16(x)
#define be16toh(x) OSSwapBigToHostInt16(x)
#define le16toh(x) OSSwapLittleToHostInt16(x)

#define htobe32(x) OSSwapHostToBigInt32(x)
#define htole32(x) OSSwapHostToLittleInt32(x)
#define be32toh(x) OSSwapBigToHostInt32(x)
#define le32toh(x) OSSwapLittleToHostInt32(x)

#define htobe64(x) OSSwapHostToBigInt64(x)
#define htole64(x) OSSwapHostToLittleInt64(x)
#define be64toh(x) OSSwapBigToHostInt64(x)
#define le64toh(x) OSSwapLittleToHostInt64(x)


#else
#include <endian.h>
#endif
#else
#include <winsock2.h>
#define htobe16(x) __builtin_bswap16(x)
#define htole16(x) (x)
#define be16toh(x) __builtin_bswap16(x)
#define le16toh(x) (x)
#define htobe32(x) __builtin_bswap32(x)
#define htole32(x) (x)
#define be32toh(x) __builtin_bswap32(x)
#define le32toh(x) (x)
#define htobe64(x) __builtin_bswap64(x)
#define htole64(x) (x)
#define be64toh(x) __builtin_bswap64(x)
#define le64toh(x) (x)
#endif

#pragma pack(push, 1)

typedef struct
{
    uint16_t first_address_be;
    uint16_t last_address_be;
} header_t;

typedef struct
{
    uint32_t magic_E6000000;
    uint16_t check_sum_be;
} footer_t;

typedef struct
{
    header_t header;
    uint8_t  body[65536];
    footer_t reserved;
} programm_t;

#pragma pack(pop)

uint16_t calcSpecialistCheckSum(uint8_t* data, uint8_t* end)
{
    uint16_t s = 0;
    if(data == end) return s;
    end--;
    while(data != end)
        s += *data++ * 257;
    return (s & 0xFF00) + ((s + *data) & 0xFF);
}

bool make_radio86rk_rom(const char* fileName, unsigned start, const char* buf, size_t body_size, char* error_buf, size_t error_buf_size)
{
    size_t write_size;
    int d;
    programm_t programm;

    if(body_size >= sizeof(programm.body))
    {
        snprintf(error_buf, error_buf_size, "too big input file\n");
        return false;
    }

    d = creat(fileName, 0666);
    if(d == -1)
    {
        snprintf(error_buf, error_buf_size, "cannot create output file (%i)\n", errno);
        return false;
    }

    memcpy(programm.body, buf, body_size);

    programm.header.first_address_be = htobe16(start);
    programm.header.last_address_be = htobe16(start + body_size - 1);

    footer_t* tail = (footer_t*)(programm.body + body_size);
    tail->magic_E6000000 = 0xE6000000;
    tail->check_sum_be = htobe16(calcSpecialistCheckSum(programm.body, programm.body + body_size));

    write_size = sizeof(header_t) + body_size + sizeof(footer_t);
    if(write(d, &programm, write_size) != (ssize_t)write_size)
    {
        snprintf(error_buf, error_buf_size, "cannot write output file (%i)\n", errno);
        close(d);
        return false;
    }

    close(d);
    return true;
}
