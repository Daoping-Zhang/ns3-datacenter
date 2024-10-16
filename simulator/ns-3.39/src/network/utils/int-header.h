#ifndef INT_HEADER_H
#define INT_HEADER_H

#include "ns3/buffer.h"

#include <cstdio>
#include <stdint.h>

namespace ns3
{

class IntHop
{
  public:
    static const uint32_t timeWidth = 24;
    static const uint32_t bytesWidth = 20;
    static const uint32_t qlenWidth = 17;
    static const uint64_t lineRateValues[8];

    union {
        struct
        {
            uint64_t lineRate : 64 - timeWidth - bytesWidth - qlenWidth,
                time : timeWidth, bytes : bytesWidth, qlen : qlenWidth;
        };

        uint32_t buf[2];
    };

    static const uint32_t byteUnit = 128;
    static const uint32_t qlenUnit = 80;
    static uint32_t multi;

    uint64_t GetLineRate() const
    {
        return lineRateValues[lineRate];
    }

    uint64_t GetBytes() const
    {
        return (uint64_t)bytes * byteUnit * multi;
    }

    uint32_t GetQlen() const
    {
        return (uint32_t)qlen * qlenUnit * multi;
    }

    uint64_t GetTime() const
    {
        return time;
    }

    void Set(uint64_t _time, uint64_t _bytes, uint32_t _qlen, uint64_t _rate)
    {
        time = _time;
        bytes = _bytes / (byteUnit * multi);
        qlen = _qlen / (qlenUnit * multi);
        switch (_rate)
        {
        case 25000000000LU:
            lineRate = 0;
            break;
        case 50000000000LU:
            lineRate = 1;
            break;
        case 100000000000LU:
            lineRate = 2;
            break;
        case 200000000000LU:
            lineRate = 3;
            break;
        case 400000000000LU:
            lineRate = 4;
            break;
        case 0LU:
            lineRate = 5;
            break;
        case 100LU:
            lineRate = 6;
            break;
        case 40000000000LU:
            lineRate = 7;
            break;
        default:
            printf("Error: IntHeader unknown rate: %lu\n", _rate);
            break;
        }
    }

    uint64_t GetBytesDelta(IntHop& b) const
    {
        if (bytes >= b.bytes)
        {
            return (bytes - b.bytes) * byteUnit * multi;
        }
        else
        {
            return (bytes + (1 << bytesWidth) - b.bytes) * byteUnit * multi;
        }
    }

    uint64_t GetTimeDelta(IntHop& b) const
    {
        if (time >= b.time)
        {
            return time - b.time;
        }
        else
        {
            return time + (1 << timeWidth) - b.time;
        }
    }
};

// INT header goes with the packet, and therefore can record e.g. RTT and queue status along the
// way.
class IntHeader
{
  public:
    static const uint32_t maxHop = 5;

    enum Mode
    {
        NORMAL = 0,
        TS = 1,
        PINT = 2,
        SWIFT = 3,
        NONE
    };

    static Mode mode;
    static int pint_bytes;

    // Note: the structure of IntHeader must have no internal padding, because we will directly
    // transform the part of packet buffer to IntHeader*
    union {
        struct
        {
            IntHop hop[maxHop];
            uint16_t nhop;
        };

        uint64_t ts;

        union {
            uint16_t power;

            struct
            {
                uint8_t power_lo8, power_hi8;
            };
        } pint;

        struct
        {
            // remote queueing delay
            uint64_t remote_delay;
            // sent timestamp
            uint64_t ts;
            // hop count
            // required no internal padding, so outside padding?
            uint64_t nhop;
        } swift; // WARNING: Swift fields are serialized and de- in REVERSE ORDER.
    };

    IntHeader();
    static uint32_t GetStaticSize();
    void PushHop(uint64_t time, uint64_t bytes, uint32_t qlen, uint64_t rate);
    void Serialize(Buffer::Iterator start) const;
    uint32_t Deserialize(Buffer::Iterator start);
    uint64_t GetTs(void) const;
    uint16_t GetPower(void) const;
    void SetPower(uint16_t);
    void IncrementHop();
};

} // namespace ns3

#endif /* INT_HEADER_H */
