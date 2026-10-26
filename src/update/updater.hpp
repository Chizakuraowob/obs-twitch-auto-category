#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QSaveFile>
#include <QCryptographicHash>
#include <QTimer>
#include <memory>
namespace tca {
struct Release { QString version, notes, filename, digest; QUrl page, download; qint64 size = 0; };
bool validUpdateRepository(const QString &repository);
bool parseRelease(const QByteArray &data, const QString &repository, const QString &current, Release *release, QString *error);
bool updateDownloadUrl(const QUrl &url);
class Updater : public QObject {
    Q_OBJECT
public:
    Updater(QString repository, QString current, QObject *parent = nullptr, QNetworkAccessManager *transport = nullptr);
    void check();
    void download(const QString &directory);
    void cancel();
    bool launchInstaller(QString *error);
    bool configured() const { return validUpdateRepository(repository); }
    bool busy() const { return !reply.isNull(); }
    Release latest;
    QString status, downloaded;
signals:
    void changed();
    void progress(qint64 received, qint64 total);
    void downloadVerified();
private:
    void fetch(const QUrl &url, int redirects);
    void consume();
    void finishDownload(const QString &error);
    QString repository, current, target;
    QNetworkAccessManager *network;
    QPointer<QNetworkReply> reply;
    std::unique_ptr<QSaveFile> output;
    QCryptographicHash hash{QCryptographicHash::Sha256};
    qint64 received = 0;
    QTimer deadline;
};
}
