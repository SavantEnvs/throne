// Known-answer probe for mayhem/test.sh, built with the project's normal flags. It drives the real
// subscription importers and the QR decoder and prints one line per fact; test.sh compares the
// whole output against a golden file, so a build that does nothing (or parses wrongly) fails.
#include <QCoreApplication>
#include <QImage>
#include <QTemporaryDir>

#include <cstdio>
#include <string>

#include "3rdparty/QrDecoder.h"
#include "3rdparty/qrcodegen.hpp"
#include "include/configs/common/Outbound.h"
#include "include/configs/sub/SubscriptionParser.hpp"
#include "include/database/entities/Profile.h"
#include "include/global/Configs.hpp"

static void parseAndPrint(const char *label, const QString &text) {
    int count = 0;
    Subscription::ParseSink sink;
    sink.profile = [&](std::shared_ptr<Configs::Profile> profile) {
        const auto &o = profile->outbound;
        std::printf("%s #%d type=%s name=%s server=%s port=%d\n", label, ++count,
                    o->DisplayType().toUtf8().constData(), o->name.toUtf8().constData(),
                    o->server.toUtf8().constData(), o->server_port);
    };
    sink.report = [](const Subscription::ParseReport &) {};
    Subscription::ParseText(text, sink);
    std::printf("%s count=%d\n", label, count);
}

static void qrRoundTrip(const char *label, const char *payload, int scale) {
    const qrcodegen::QrCode qr = qrcodegen::QrCode::encodeText(payload, qrcodegen::QrCode::Ecc::MEDIUM);
    const int border = 4;
    const int side = (qr.getSize() + 2 * border) * scale;
    QImage image(side, side, QImage::Format_Grayscale8);
    image.fill(255);
    for (int y = 0; y < side; ++y)
        for (int x = 0; x < side; ++x)
            image.scanLine(y)[x] = qr.getModule(x / scale - border, y / scale - border) ? 0 : 255;
    QrDecoder decoder;
    const auto codes = decoder.decode(image);
    std::printf("%s decoded=%d first=%s\n", label, static_cast<int>(codes.size()),
                codes.isEmpty() ? "-" : codes.first().toUtf8().constData());
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir dir;
    if (!dir.isValid()) return 2;
    Configs::initDB((dir.path() + "/throne.db").toStdString());

    parseAndPrint("ss", "ss://YWVzLTEyOC1nY206cGFzc3dvcmQ@example.com:8388#my-ss");
    parseAndPrint("trojan", "trojan://secret@trojan.example.org:443?sni=front.example.org#my-trojan");
    parseAndPrint("vmess",
                  "vmess://eyJ2IjoiMiIsInBzIjoibXktdm1lc3MiLCJhZGQiOiJ2bS5leGFtcGxlLm5ldCIsInBvcnQiOiI0NDMiLCJpZCI6ImFiY2RlZjAxLTIzNDUtNjc4OS1hYmNkLWVmMDEyMzQ1Njc4OSIsImFpZCI6IjAiLCJuZXQiOiJ0Y3AifQ==");
    parseAndPrint("clash",
                  "proxies:\n"
                  "  - name: clash-ss\n    type: ss\n    server: 192.0.2.7\n    port: 8443\n    cipher: aes-128-gcm\n    password: pw\n"
                  "  - name: clash-trojan\n    type: trojan\n    server: t.example.com\n    port: 443\n    password: pw\n");
    parseAndPrint("garbage", "this is not a subscription");

    qrRoundTrip("qr-url", "https://example.com/throne", 4);
    qrRoundTrip("qr-link", "trojan://secret@trojan.example.org:443#qr", 3);
    return 0;
}
