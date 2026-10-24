#include "twitch/client.hpp"
#include "release-config.hpp"
#include <QCoreApplication>
#include <QSslSocket>
#include <cstdio>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const auto args = app.arguments();
    // Do not accidentally find a developer SDK TLS plugin: reproduce OBS's deployment.
    QCoreApplication::setLibraryPaths({});
    const bool missing = args.contains("--expect-unavailable");
    if (!missing && args.size() > 1) QCoreApplication::addLibraryPath(args[1]);
    const bool available = QSslSocket::supportsSsl();
    std::printf("Qt %s; TLS available=%s; backend=%s\n", qVersion(), available ? "yes" : "no", QSslSocket::activeBackend().toUtf8().constData());
    if (missing && available) return 1;
    if (!missing && (!available || QSslSocket::activeBackend() != "schannel")) return 2;
    if (!missing && !args.contains("--device")) return 0;
    if (!missing && !*tca::PublisherClientId) return 3;
    // Explicit live smoke check: request a device URL only, never open a browser, poll, or store tokens.
    tca::Client client(nullptr, nullptr, false);
    bool missingTlsReported = false;
    QObject::connect(&client, &tca::Client::status, &app, [&](const QString &s) {
        missingTlsReported = s.contains("TLS");
        std::fprintf(stderr, "%s\n", s.toUtf8().constData());
    });
    QObject::connect(&client, &tca::Client::authorization, &app, [&](const QUrl &url, const QString &, int) {
        std::printf("PASS Twitch device authorization URL: %s%s (code/query omitted)\n", url.host().toUtf8().constData(), url.path().toUtf8().constData());
        QTimer::singleShot(0, &app, [&] { client.shutdown(); app.exit(0); });
    });
    QObject::connect(&client, &tca::Client::authorizationFinished, &app, [&] { app.exit(missing && missingTlsReported ? 0 : 4); });
    QTimer::singleShot(20000, &app, [&] { client.shutdown(); app.exit(5); });
    QTimer::singleShot(0, &app, [&] { client.login(missing ? QStringLiteral("testclient") : QString::fromLatin1(tca::PublisherClientId)); });
    return app.exec();
}
