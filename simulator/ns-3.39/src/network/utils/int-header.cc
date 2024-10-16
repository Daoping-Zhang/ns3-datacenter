#include "int-header.h"

namespace ns3
{

const uint64_t IntHop::lineRateValues[8] = {25000000000LU,
                                            50000000000LU,
                                            100000000000LU,
                                            200000000000LU,
                                            400000000000LU,
                                            0,
                                            0,
                                            40000000000LU};
uint32_t IntHop::multi = 1;

IntHeader::Mode IntHeader::mode = NONE;
int IntHeader::pint_bytes = 2;

IntHeader::IntHeader()
    : nhop(0)
{
    for (uint32_t i = 0; i < maxHop; i++)
    {
        hop[i] = {{{0}}};
    }
}

uint32_t
IntHeader::GetStaticSize()
{
    switch (mode)
    {
    case NORMAL:
        return sizeof(hop) + sizeof(nhop);
    case TS:
        return sizeof(ts);
    case PINT:
        return sizeof(pint);
    case SWIFT:
        return sizeof(swift);
    default:
        return 0;
    }
}

void
IntHeader::PushHop(uint64_t time, uint64_t bytes, uint32_t qlen, uint64_t rate)
{
    // only do this in INT mode
    if (mode == NORMAL)
    {
        uint32_t idx = nhop % maxHop;
        hop[idx].Set(time, bytes, qlen, rate);
        nhop++;
    }
}

void
IntHeader::Serialize(Buffer::Iterator start) const
{
    Buffer::Iterator i = start;
    switch (mode)
    {
    case NORMAL:
        for (uint32_t j = 0; j < maxHop; j++)
        {
            i.WriteU32(hop[j].buf[0]);
            i.WriteU32(hop[j].buf[1]);
        }
        i.WriteU16(nhop);
        break;
    case TS:
        i.WriteU64(ts);
        break;
    case PINT:
        if (pint_bytes == 1)
        {
            i.WriteU8(pint.power_lo8);
        }
        else if (pint_bytes == 2)
        {
            i.WriteU16(pint.power);
        }
        break;
    case SWIFT:
        i.WriteU64(swift.remote_delay);
        i.WriteU64(swift.ts);
        i.WriteU64(swift.nhop);
        break;
    default:
        break;
    }
}

uint32_t
IntHeader::Deserialize(Buffer::Iterator start)
{
    Buffer::Iterator i = start;
    switch (mode)
    {
    case NORMAL:
        for (uint32_t j = 0; j < maxHop; j++)
        {
            hop[j].buf[0] = i.ReadU32();
            hop[j].buf[1] = i.ReadU32();
        }
        nhop = i.ReadU16();
        break;
    case TS:
        ts = i.ReadU64();
        break;
    case PINT:
        if (pint_bytes == 1)
        {
            pint.power_lo8 = i.ReadU8();
        }
        else if (pint_bytes == 2)
        {
            pint.power = i.ReadU16();
        }
        break;
    case SWIFT:
        swift.remote_delay = i.ReadU64();
        swift.ts = i.ReadU64();
        swift.nhop = i.ReadU64();
        break;
    default:
        break;
    }
    return GetStaticSize();
}

uint64_t
IntHeader::GetTs(void) const
{
    if (mode == TS)
    {
        return ts;
    }
    return 0;
}

uint16_t
IntHeader::GetPower(void) const
{
    if (mode == PINT)
    {
        return pint_bytes == 1 ? pint.power_lo8 : pint.power;
    }
    return 0;
}

void
IntHeader::SetPower(uint16_t power)
{
    if (mode == PINT)
    {
        if (pint_bytes == 1)
        {
            pint.power_lo8 = power;
        }
        else
        {
            pint.power = power;
        }
    }
}

// (for Swift) increment hop count
void
IntHeader::IncrementHop()
{
    if (mode == SWIFT)
    {
        swift.nhop++;
    }
}

} // namespace ns3
