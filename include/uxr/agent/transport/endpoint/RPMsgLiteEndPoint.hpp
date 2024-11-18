#ifndef _UXR_AGENT_TRANSPORT_RPMSG_LITE_ENDPOINT_HPP_
#define _UXR_AGENT_TRANSPORT_RPMSG_LITE_ENDPOINT_HPP_

#include <stdint.h>

namespace eprosima {
namespace uxr {

class RPMsgLiteEndPoint
{
public:
    RPMsgLiteEndPoint() = default;

    RPMsgLiteEndPoint(
            uint8_t addr)
        : addr_{addr}
    {}

    ~RPMsgLiteEndPoint() {}

    bool operator<(const RPMsgLiteEndPoint& other) const
    {
        return (addr_ < other.addr_);
    }

    friend std::ostream& operator<<(std::ostream& os, const RPMsgLiteEndPoint& endpoint)
    {
        os << static_cast<int>(endpoint.addr_);
        return os;
    }

    uint8_t get_addr() const { return addr_; }

private:
    uint8_t addr_;
};

} // namespace uxr
} // namespace eprosima

#endif //_UXR_AGENT_TRANSPORT_RPMSG_LITE_ENDPOINT_HPP_
