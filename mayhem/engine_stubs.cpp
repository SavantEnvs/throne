// The app's engine lifecycle, event bus and core-RPC client are process-level services that need a
// running core and UI. The import/config code under test only reaches them from persistence paths
// that the harnesses never take, so they are reduced to no-ops here.
#include <QList>
#include <QString>

#include "include/api/RPC.h"
#include "include/engine/EngineEvents.h"
#include "include/engine/Lifecycle.h"
#include "include/scanner/DefaultIpLists.h"
#include "include/sys/UrlScheme.hpp"

namespace API {
    bool Client::IsConnected() const { return false; }

    PrivilegeInfo Client::Privileges(bool *rpcOK, int) const {
        if (rpcOK != nullptr) *rpcOK = false;
        return {};
    }

    QString Client::CheckConfig(bool *rpcOK, const QString &, bool) const {
        if (rpcOK != nullptr) *rpcOK = false;
        return {};
    }
} // namespace API

namespace Engine {
    void EngineEvents::GroupOrderChanged(const QList<int> &) {}

    namespace Lifecycle {
        void StopRunningProfile(bool, bool, bool, bool) {}
    } // namespace Lifecycle
} // namespace Engine

namespace Scanner::DefaultIpLists {
    int EnsureAll() { return 0; }
} // namespace Scanner::DefaultIpLists

bool UrlScheme_AutoRegisterByDefault() { return false; }
