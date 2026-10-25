#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QNetworkReply>
#include <QQueue>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QDir>
#include <QThread>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include "settings/settings.hpp"
#include "detection/detector.hpp"
#include "twitch/client.hpp"

using namespace tca;
#define CHECK(expr) do { if (!(expr)) throw std::runtime_error("Line " + std::to_string(__LINE__) + ": " #expr); } while (false)
static void waitFor(const std::function<bool()> &condition, int timeout = 2000)
{
    QElapsedTimer timer; timer.start();
    while (!condition() && timer.elapsed() < timeout) { QCoreApplication::processEvents(QEventLoop::AllEvents, 10); QThread::msleep(1); }
    CHECK(condition());
}
struct Fixture { int status; QByteArray body; int delay = 0; };
class Reply : public QNetworkReply {
public:
    Reply(const QNetworkRequest &request, Fixture fixture, QObject *parent) : QNetworkReply(parent), data(fixture.body)
    {
        setRequest(request); setUrl(request.url()); setAttribute(QNetworkRequest::HttpStatusCodeAttribute, fixture.status);
        if (fixture.status >= 400) setError(QNetworkReply::ContentAccessDenied, "test HTTP error");
        open(QIODevice::ReadOnly | QIODevice::Unbuffered);
        QTimer::singleShot(fixture.delay, this, [this] { if (!isFinished()) { setFinished(true); emit readyRead(); emit finished(); } });
    }
    void abort() override { if (!isFinished()) { setFinished(true); setError(OperationCanceledError, "cancelled"); emit finished(); } }
    qint64 bytesAvailable() const override { return data.size() - offset + QIODevice::bytesAvailable(); }
protected:
    qint64 readData(char *buffer, qint64 maximum) override {
        const qint64 count = qMin(maximum, static_cast<qint64>(data.size()) - offset);
        if (!count) return -1;
        std::memcpy(buffer, data.constData() + offset, static_cast<size_t>(count)); offset += count; return count;
    }
private:
    QByteArray data; qint64 offset = 0;
};
class Transport : public QNetworkAccessManager {
public:
    QQueue<Fixture> fixtures;
    QList<QUrl> urls;
    QList<QByteArray> bodies;
    bool unexpected = false;
protected:
    QNetworkReply *createRequest(Operation, const QNetworkRequest &request, QIODevice *body) override {
        urls.append(request.url()); bodies.append(body ? body->readAll() : QByteArray{});
        if (fixtures.isEmpty()) { unexpected = true; return new Reply(request, {500, "{}"}, this); }
        return new Reply(request, fixtures.dequeue(), this);
    }
};
static QByteArray channelJson(const char *id = "42") {
    return QByteArray("{\"data\":[{\"broadcaster_id\":\"123\",\"game_id\":\"") + id + "\",\"game_name\":\"Example\"}]}";
}
static void authFixtures(Transport &net) {
    net.fixtures.enqueue({200, R"({"device_code":"test-device","user_code":"TEST","verification_uri":"https://www.twitch.tv/activate?test=1","interval":5,"expires_in":60})"});
    net.fixtures.enqueue({200, R"({"access_token":"test-access","refresh_token":"test-refresh","token_type":"bearer"})"});
    net.fixtures.enqueue({200, R"({"client_id":"client123","user_id":"123","login":"tester","scopes":["channel:manage:broadcast"],"expires_in":3600})"});
    net.fixtures.enqueue({200, channelJson()});
    net.fixtures.enqueue({200, R"({"data":[]})"});
}
static void authenticate(Client &client, Transport &net) {
    client.login("client123"); waitFor([&] { return client.connected() && client.current().id == "42" && net.fixtures.isEmpty(); }, 8000);
    QCoreApplication::processEvents();
}
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    if (app.arguments().contains("--detect-games")) {
        const auto processes = scanProcesses(); const auto found = candidates(processes, {});
        std::printf("Local library records: %d; running library games: %lld\n", processes.libraryCount, static_cast<long long>(processes.localGames.size()));
        for (const auto &game : processes.localGames) std::printf("Local game: %s -> %s [%s]\n", game.executable.toUtf8().constData(), game.name.toUtf8().constData(), game.key.toUtf8().constData());
        for (const auto &game : found) std::printf("Detected: %s -> %s (%s)\n", game.executable.toUtf8().constData(), game.name.toUtf8().constData(), game.id.toUtf8().constData());
        return found.isEmpty() && processes.localGames.isEmpty() ? 1 : 0;
    }
    int passed = 0, failed = 0;
    auto run = [&](const char *name, const std::function<void()> &test) {
        try { test(); ++passed; std::printf("PASS %s\n", name); }
        catch (const std::exception &error) { ++failed; std::fprintf(stderr, "FAIL %s: %s\n", name, error.what()); }
    };
    run("settings round trip, consent versions and defaults", [] {
        QTemporaryDir temp; CHECK(temp.isValid()); Settings s;
        s.clientId = "client123"; s.rules.append({"game.exe", "42", "遊戲", 3});
        s.termsVersion = s.privacyVersion = DocumentVersion; s.termsUtc = s.privacyUtc = "2026-10-24T00:00:00Z";
        QString error; const auto path = temp.filePath("settings.json"); CHECK(s.save(path, &error));
        const auto loaded = Settings::load(path, &error); CHECK(error.isEmpty()); CHECK(loaded.consented());
        CHECK(loaded.rules.size() == 1); CHECK(loaded.rules[0].categoryName == "遊戲");
        CHECK(Settings{}.multiGame == 1); CHECK(!Settings{}.automatic);
        auto changed = loaded; changed.privacyVersion = "old"; CHECK(!changed.consented());
    });
    run("favorites persistence, validation and developer scan policy", [] {
        Settings s; s.favorites = {{"42", "Just Chatting"}, {"43", "遊戲"}}; s.developerMode = true;
        QTemporaryDir temp; QString error; CHECK(s.save(temp.filePath("settings.json"), &error));
        auto loaded = Settings::load(temp.filePath("settings.json"), &error);
        CHECK(loaded.favorites.size() == 2); CHECK(loaded.favorites[0].name == "Just Chatting"); CHECK(loaded.developerMode);
        CHECK(loaded.suggestionsEnabled(false)); loaded.paused = true; CHECK(!loaded.suggestionsEnabled(false));
        CHECK(!Settings{}.suggestionsEnabled(false)); CHECK(Settings{}.suggestionsEnabled(true));
        auto json = s.json(); QJsonArray entries;
        entries.append(QJsonObject{{"id", "42"}, {"name", "First"}});
        entries.append(QJsonObject{{"id", "42"}, {"name", "Duplicate"}});
        entries.append(QJsonObject{{"id", "bad"}, {"name", "Invalid"}});
        entries.append(QJsonObject{{"id", "43"}, {"name", " "}});
        json["favorites"] = entries; CHECK(Settings::fromJson(json).favorites.size() == 1);
        entries = {}; for (int i = 1; i <= 105; ++i) entries.append(QJsonObject{{"id", QString::number(i)}, {"name", "Game"}});
        json["favorites"] = entries; CHECK(Settings::fromJson(json).favorites.size() == 100);
        CHECK(Settings::fromJson({}).favorites.isEmpty()); CHECK(!Settings::fromJson({}).developerMode);
    });
    run("font preferences and legacy wizard migration", [] {
        Settings s; s.fontFamily = "Segoe UI"; s.fontSize = 20; s.animations = false; s.wizardDone = true;
        auto loaded = Settings::fromJson(s.json());
        CHECK(loaded.fontFamily == s.fontFamily); CHECK(loaded.fontSize == 20); CHECK(!loaded.animations); CHECK(loaded.wizardDone);
        auto old = s.json(); old.remove("wizardRevision"); old["fontSize"] = 999;
        loaded = Settings::fromJson(old); CHECK(!loaded.wizardDone); CHECK(loaded.fontSize == 22);
        old["fontSize"] = -1; CHECK(Settings::fromJson(old).fontSize == 12);
    });
    run("local Steam and Epic manifests, path boundaries and library cache", [] {
        QTemporaryDir temp; CHECK(temp.isValid()); const auto root = temp.path();
        auto write = [](const QString &path, const QByteArray &data) { QDir().mkpath(QFileInfo(path).absolutePath()); QFile f(path); CHECK(f.open(QIODevice::WriteOnly)); CHECK(f.write(data) == data.size()); };
        write(root + "/steamapps/common/Game/bin/game.exe", "test");
        write(root + "/steamapps/common/Game2/game.exe", "test");
        write(root + "/steamapps/common/Game/crashreporter.exe", "test");
        write(root + "/steamapps/appmanifest_42.acf", "\"AppState\" { \"appid\" \"42\" \"name\" \"Test Game\" \"installdir\" \"Game\" }");
        write(root + "/steamapps/appmanifest_43.acf", "\"appid\" \"43\" \"name\" \"Escape\" \"installdir\" \"../escape\"");
        auto games = readSteamLibrary(root); CHECK(games.size() == 1); CHECK(games[0].key == "steam:42");
        CHECK(matchInstalledGame(root + "/steamapps/common/Game/bin/game.exe", games).key == "steam:42");
        CHECK(matchInstalledGame(root + "/steamapps/common/Game2/game.exe", games).key.isEmpty());
        CHECK(matchInstalledGame(root + "/steamapps/common/Game/crashreporter.exe", games).key.isEmpty());
        QJsonObject epic{{"AppName", "abc123"}, {"DisplayName", "Test Game"}, {"InstallLocation", root + "/steamapps/common/Game"}};
        write(root + "/epic/test.item", QJsonDocument(epic).toJson()); write(root + "/epic/bad.item", "{broken");
        CHECK(readEpicLibrary(root + "/epic").size() == 1);
        Settings s; s.categoryCache.insert("steam:42", {"27471", "Minecraft"}); s.allowCategoryLookup = true;
        const auto loaded = Settings::fromJson(s.json()); CHECK(loaded.categoryCache.value("steam:42").id == "27471"); CHECK(loaded.allowCategoryLookup);
        CHECK(!Settings{}.allowCategoryLookup); s.autoDetect = false; s.developerMode = true; CHECK(!s.suggestionsEnabled(true));
        CHECK(!validLibraryKey("steam:../../private")); CHECK(!validLibraryKey("file:C:/private"));
        Processes p{{"game.exe"}, "game.exe", true, {{"game.exe", "1", "A", 0}, {"game.exe", "2", "B", 0}}}; CHECK(candidates(p, {}).isEmpty());
    });
    run("name lookup is isolated, exact and does not update the channel", [] {
        Transport net; authFixtures(net); Client client(nullptr, &net, false); authenticate(client, net);
        Category found; int completed = 0; QObject::connect(&client, &Client::categoryResolved, &client, [&](const QString &, const Category &c) { found = c; ++completed; });
        net.fixtures.enqueue({200, R"({"data":[{"id":"42","name":"Test Game"}]})"});
        const auto count = net.urls.size(); client.resolveCategory("steam:42", "Test Game"); waitFor([&] { return completed == 1; });
        CHECK(found.id == "42"); CHECK(net.urls.size() == count + 1); CHECK(net.urls.last().path() == "/helix/games"); CHECK(net.bodies.last().isEmpty());
        net.fixtures.enqueue({200, R"({"data":[{"id":"43","name":"Different"}]})"});
        client.resolveCategory("steam:42", "Test Game"); waitFor([&] { return completed == 2; }); CHECK(found.id.isEmpty());
        net.fixtures.enqueue({200, R"({"data":[{"id":"42","name":"Test Game"},{"id":"43","name":"Test Game"}]})"});
        client.resolveCategory("steam:42", "Test Game"); waitFor([&] { return completed == 3; }); CHECK(found.id.isEmpty()); CHECK(!net.unexpected);
    });
    run("corrupt settings preserve original file", [] {
        QTemporaryDir temp; const auto path = temp.filePath("settings.json"); QFile file(path); CHECK(file.open(QIODevice::WriteOnly)); file.write("{broken"); file.close();
        QString error; Settings::load(path, &error); CHECK(!error.isEmpty()); CHECK(file.open(QIODevice::ReadOnly)); CHECK(file.readAll() == "{broken");
    });
    run("Minecraft recognition without manual rules and Java false positives", [] {
        const auto minecraft = recognizeGame("javaw.exe", "Minecraft* 1.21.11");
        CHECK(minecraft.categoryId == "27471");
        CHECK(recognizeGame("java.exe", "Minecraft 1.20.1 - Forge").categoryId == "27471");
        CHECK(recognizeGame("Minecraft.Windows.exe", "Minecraft").categoryId == "27471");
        CHECK(recognizeGame("javaw.exe", "Minecraft Launcher").categoryId.isEmpty());
        CHECK(recognizeGame("javaw.exe", "Java application").categoryId.isEmpty());
        CHECK(recognizeGame("chrome.exe", "Minecraft 1.21.11").categoryId.isEmpty());
        Processes p{{"javaw.exe"}, "javaw.exe", true, {minecraft}};
        CHECK(candidates(p, {}).size() == 1); CHECK(candidates(p, {})[0].id == "27471");
        CHECK(candidates(p, {{"javaw.exe", "42", "Override", 1}})[0].id == "42");
        p.recognizedGames.clear(); CHECK(candidates(p, {}).isEmpty());
    });
    run("NTE detection without a process path requires matching executable and window", [] {
        const auto game = recognizeStandaloneGame("HTGame.exe", "NTE  ");
        CHECK(game.key == "builtin:nte"); CHECK(game.name == "NTE: Neverness to Everness"); CHECK(validLibraryKey(game.key));
        CHECK(recognizeStandaloneGame("HTGame.exe", "異環").key == game.key);
        CHECK(recognizeStandaloneGame("HTGame.exe", "Other Game").key.isEmpty());
        CHECK(recognizeStandaloneGame("NTETWLauncher.exe", "NTE").key.isEmpty());
        CHECK(recognizeStandaloneGame("chrome.exe", "NTE").key.isEmpty()); CHECK(!validLibraryKey("builtin:unknown"));
        Settings s; s.categoryCache.insert(game.key, {"42", game.name}); CHECK(Settings::fromJson(s.json()).categoryCache.contains(game.key));
    });
    run("candidate policies and conflicting mappings", [] {
        Processes p{{"game.exe", "other.exe"}, "OTHER.EXE", true};
        QVector<Rule> rules{{"game.exe", "42", "First", 1}, {"other.exe", "43", "Second", 2}};
        const auto list = candidates(p, rules); CHECK(list.size() == 2);
        CHECK(chooseCandidate(list, p.foreground, 0) == 1); CHECK(chooseCandidate(list, p.foreground, 1) == -1);
        CHECK(chooseCandidate(list, p.foreground, 2) == 1);
        rules[0].priority = 2; CHECK(chooseCandidate(candidates(p, rules), {}, 2) == -1);
        rules.append({"game.exe", "99", "Conflict", 0}); CHECK(candidates(p, rules).size() == 1);
        p.available = false; CHECK(candidates(p, rules).isEmpty());
    });
    run("category data and executable validation", [] {
        Category c; CHECK(!Client::parseCategory({{"id", "evil"}, {"name", "A"}}, c));
        CHECK(!Client::parseCategory({{"id", "42"}}, c));
        CHECK(Client::parseCategory({{"id", "42"}, {"name", "遊戲"}}, c));
        CHECK(!validExecutable("C:/game.exe")); CHECK(validExecutable("GAME.EXE"));
    });
    run("cancelled authorization ignores stale response", [] {
        Transport net; net.fixtures.enqueue({200, R"({"device_code":"test","user_code":"TEST","verification_uri":"https://www.twitch.tv/activate","expires_in":30})", 50});
        Client client(nullptr, &net, false); int count = 0;
        QObject::connect(&client, &Client::authorization, &client, [&] { ++count; });
        client.login("client123"); client.cancelLogin();
        QElapsedTimer timer; timer.start(); waitFor([&] { return timer.elapsed() >= 100; });
        CHECK(count == 0); CHECK(!client.connected());
    });
    run("untrusted authorization URL rejected", [] {
        Transport net; net.fixtures.enqueue({200, R"({"device_code":"test","user_code":"TEST","verification_uri":"https://example.com/activate","expires_in":30})"});
        Client client(nullptr, &net, false); int done = 0, auth = 0;
        QObject::connect(&client, &Client::authorizationFinished, &client, [&] { ++done; });
        QObject::connect(&client, &Client::authorization, &client, [&] { ++auth; });
        client.login("client123"); waitFor([&] { return done == 1; }); CHECK(auth == 0);
    });
    run("PATCH 204 is not success before matching readback", [] {
        Transport net; authFixtures(net); Client client(nullptr, &net, false); authenticate(client, net);
        net.fixtures.enqueue({204, {}}); net.fixtures.enqueue({200, channelJson("43"), 100}); net.fixtures.enqueue({200, R"({"data":[]})"});
        QStringList statuses, activities;
        QObject::connect(&client, &Client::status, &client, [&](const QString &s) { statuses.append(s); });
        QObject::connect(&client, &Client::activity, &client, [&](const QString &s) { activities.append(s); });
        client.apply({"43", "Second", {}}); waitFor([&] { return statuses.size() >= 2; }); CHECK(client.busy()); CHECK(activities.isEmpty());
        waitFor([&] { return !client.busy(); }); CHECK(client.current().id == "43"); CHECK(activities.size() == 1);
        CHECK(activities.first().startsWith("更新成功")); CHECK(!net.unexpected);
        CHECK(net.bodies[net.bodies.size() - 3].contains("\"game_id\":\"43\""));
    });
    run("accepted PATCH plus rate-limited confirmation is unknown", [] {
        Transport net; authFixtures(net); Client client(nullptr, &net, false); authenticate(client, net);
        net.fixtures.enqueue({204, {}}); net.fixtures.enqueue({429, "{}"}); QStringList activities;
        QObject::connect(&client, &Client::activity, &client, [&](const QString &s) { activities.append(s); });
        client.apply({"43", "Second", {}}); waitFor([&] { return activities.size() == 1; });
        CHECK(activities.first().startsWith("暫時無法確認")); CHECK(client.current().id == "42"); CHECK(!net.unexpected);
        const auto count = net.urls.size(); client.search("anything"); CHECK(net.urls.size() == count);
    });
    run("insufficient scope never connects", [] {
        Transport net; authFixtures(net); net.fixtures[2].body = R"({"client_id":"client123","user_id":"123","scopes":[],"expires_in":3600})";
        net.fixtures.removeLast(); net.fixtures.removeLast(); Client client(nullptr, &net, false); QStringList statuses;
        QObject::connect(&client, &Client::status, &client, [&](const QString &s) { statuses.append(s); });
        client.login("client123"); waitFor([&] { return statuses.size() >= 2; }, 8000);
        CHECK(!client.connected()); CHECK(statuses.last().contains("驗證失敗")); CHECK(!net.unexpected);
    });
    run("401 refresh is serialized and excludes client secret; 403 and malformed responses fail", [] {
        Transport net; authFixtures(net); net.fixtures[1].body = R"({"access_token":"test-access","refresh_token":"a+b&c","token_type":"bearer"})";
        Client client(nullptr, &net, false); authenticate(client, net);
        net.fixtures.enqueue({401, "{}"});
        net.fixtures.enqueue({200, R"({"access_token":"renewed","refresh_token":"rotated","token_type":"bearer"})"});
        net.fixtures.enqueue({200, R"({"client_id":"client123","user_id":"123","login":"tester","scopes":["channel:manage:broadcast"],"expires_in":3600})"});
        net.fixtures.enqueue({200, channelJson()}); net.fixtures.enqueue({200, R"({"data":[]})"});
        client.search("test"); waitFor([&] { return net.fixtures.isEmpty() && client.connected(); });
        bool foundRefresh = false;
        for (const auto &body : net.bodies) {
            CHECK(!body.contains("client_secret"));
            if (body.contains("grant_type=refresh_token")) { CHECK(body.contains("a%2Bb%26c")); foundRefresh = true; }
        }
        CHECK(foundRefresh);
        QStringList activities, statuses;
        QObject::connect(&client, &Client::activity, &client, [&](const QString &s) { activities.append(s); });
        QObject::connect(&client, &Client::status, &client, [&](const QString &s) { statuses.append(s); });
        net.fixtures.enqueue({403, "{}"}); client.apply({"43", "Second", {}});
        waitFor([&] { return !activities.isEmpty(); }); CHECK(activities.last().startsWith("更新失敗")); CHECK(!client.busy());
        net.fixtures.enqueue({200, "not json"}); const auto before = statuses.size(); client.search("test");
        waitFor([&] { return statuses.size() > before; }); CHECK(statuses.last().contains("異常")); CHECK(!net.unexpected);
    });
    run("mismatching readback has bounded retries and never reports success", [] {
        Transport net; authFixtures(net); Client client(nullptr, &net, false); authenticate(client, net);
        net.fixtures.enqueue({204, {}});
        for (int i = 0; i < 4; ++i) net.fixtures.enqueue({200, channelJson()});
        QStringList activities; QObject::connect(&client, &Client::activity, &client, [&](const QString &s) { activities.append(s); });
        client.apply({"43", "Second", {}}); waitFor([&] { return !client.busy(); }, 10000);
        CHECK(activities.size() == 1); CHECK(activities.first().startsWith("暫時無法確認")); CHECK(net.fixtures.isEmpty()); CHECK(!net.unexpected);
    });
    run("new search ignores stale results and server error verifies before retrying write", [] {
        Transport net; authFixtures(net); Client client(nullptr, &net, false); authenticate(client, net);
        net.fixtures.enqueue({200, R"({"data":[{"id":"1","name":"Old"}]})", 100});
        net.fixtures.enqueue({200, R"({"data":[{"id":"2","name":"New"}]})"});
        QStringList names; QObject::connect(&client, &Client::results, &client, [&](const QVector<Category> &list, const QString &) { for (const auto &c : list) names.append(c.name); });
        client.search("old"); client.search("new"); QElapsedTimer timer; timer.start(); waitFor([&] { return timer.elapsed() >= 150; });
        CHECK(names == QStringList{"New"});
        net.fixtures.enqueue({503, "{}"}); net.fixtures.enqueue({200, channelJson("43")}); net.fixtures.enqueue({200, R"({"data":[]})"});
        QStringList activities; QObject::connect(&client, &Client::activity, &client, [&](const QString &s) { activities.append(s); });
        const auto count = net.urls.size(); client.apply({"43", "Second", {}}); waitFor([&] { return !client.busy(); });
        CHECK(activities.size() == 1); CHECK(activities.first().startsWith("更新成功")); CHECK(net.urls.size() == count + 3); CHECK(!net.unexpected);
    });
    run("real Windows process snapshot is readable", [] { const auto processes = scanProcesses(); CHECK(processes.available); CHECK(!processes.executables.isEmpty()); });
    std::printf("%d passed; %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
