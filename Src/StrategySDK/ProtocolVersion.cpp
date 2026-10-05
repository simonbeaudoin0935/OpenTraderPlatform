#include "OpenTraderPlatform/StrategyProtocol/ProtocolVersion.h"

#include <cassert>

namespace OpenTraderPlatform::StrategyProtocol
{
    void populateProtocolVersion(opentraderplatform::strategy::v1::ProtocolVersion* p_version)
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

    bool isCompatibleProtocol(const opentraderplatform::strategy::v1::ProtocolVersion& p_version)
    {
        return p_version.major() == kProtocolMajor && p_version.minor() == kProtocolMinor;
    }
} // namespace OpenTraderPlatform::StrategyProtocol
