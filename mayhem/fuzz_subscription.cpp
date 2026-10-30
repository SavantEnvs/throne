// Subscription / share-link / config-file import: every format ParseDocument auto-detects
// (base64 lists, vmess/vless/ss/trojan/... links, clash YAML, sing-box and xray JSON,
// WireGuard/OpenVPN/OpenConnect files, AmneziaWG vpn:// containers).
#include <QByteArray>
#include <QCoreApplication>
#include <QString>
#include <QTemporaryDir>

#include <cstddef>
#include <cstdint>
#include <memory>

#include "include/configs/common/Outbound.h"
#include "include/global/Configs.hpp"
#include "include/configs/sub/SubscriptionParser.hpp"
#include "include/database/entities/Profile.h"

extern "C" int LLVMFuzzerInitialize(int *, char ***) {
    static int argc = 1;
    static char name[] = "fuzz_subscription";
    static char *argv[] = {name, nullptr};
    static QCoreApplication app(argc, argv);
    // The importers read settings through the global DatabaseManager, so give them a real one in a scratch dir.
    static QTemporaryDir dir;
    if (!dir.isValid()) return 1;
    Configs::initDB((dir.path() + "/throne.db").toStdString());
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size == 0) return -1;

    Subscription::ParseSink sink;
    sink.profile = [](std::shared_ptr<Configs::Profile> profile) {
        if (!profile || !profile->outbound) return;
        (void) profile->outbound->DisplayTypeAndName();
        (void) profile->outbound->ExportToLink();
        (void) profile->outbound->ExportToJson();
        (void) profile->outbound->ExportJsonLink();
    };
    sink.report = [](const Subscription::ParseReport &report) { (void) report.issues.size(); };

    Subscription::ParseDocument(QByteArray(reinterpret_cast<const char *>(data), static_cast<qsizetype>(size)), sink);
    return 0;
}
