#include "L2Trader/StrategyProtocol/ProtocolVersion.h"

#include <cassert>

namespace L2Trader::StrategyProtocol
{
    void populateProtocolVersion(l2trader::strategy::v1::ProtocolVersion* p_version)
    {
        assert(p_version != nullptr);
        if (p_version == nullptr)
        {
            return;
        }

        p_version->set_major(kProtocolMajor);
        p_version->set_minor(kProtocolMinor);
        p_version->set_patch(kProtocolPatch);
        p_version->set_prerelease(std::string(kProtocolPrerelease));
    }

    bool isCompatibleProtocol(const l2trader::strategy::v1::ProtocolVersion& p_version)
    {
        return p_version.major() == kProtocolMajor && p_version.minor() == kProtocolMinor;
    }
} // namespace L2Trader::StrategyProtocol
