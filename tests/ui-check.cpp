#include "ui/dock.hpp"
#include <QApplication>
#include <QDialog>
#include <QDir>
#include <QPushButton>
#include <QTemporaryDir>
#include <QWizard>
#include <QCheckBox>
#include <QSpinBox>
#include <QKeyEvent>
#include <QLineEdit>
#include <QLabel>
#include <QMouseEvent>
#include <QDialogButtonBox>
#include <QEventLoop>
#include <QComboBox>
#include <QAbstractItemView>
#include <QFile>
#include <QListWidget>
#include <QSignalBlocker>
#include <QFileInfo>
#include <QMessageBox>
#include <QTableWidget>
#include <QScrollArea>
#include <QScrollBar>
#include <cstdio>
#include <QDateTime>
#include <QTextBrowser>
#include <QTabWidget>
#include <QPromise>
#include <QJsonDocument>
#include <QJsonArray>
#include <QElapsedTimer>
#include <QNetworkReply>
#include <QVBoxLayout>
#include <QStyle>
#include <QThread>
#include <cstring>

class UiReply : public QNetworkReply {
public:
    UiReply(const QNetworkRequest &request, QByteArray bytes, int status, int delay, QObject *parent)
        : QNetworkReply(parent), bytes(std::move(bytes)) {
        setRequest(request); setUrl(request.url()); setAttribute(QNetworkRequest::HttpStatusCodeAttribute, status);
        if (status >= 400) setError(ContentAccessDenied, "fixture error");
        open(QIODevice::ReadOnly | QIODevice::Unbuffered);
        QTimer::singleShot(delay, this, [this] { if (!isFinished()) { setFinished(true); emit readyRead(); emit finished(); } });
    }
    void abort() override { if (!isFinished()) { setFinished(true); setError(OperationCanceledError, "cancelled"); emit finished(); } }
    qint64 bytesAvailable() const override { return bytes.size() - offset + QIODevice::bytesAvailable(); }
protected:
    qint64 readData(char *buffer, qint64 maximum) override {
        const auto count = qMin(maximum, static_cast<qint64>(bytes.size()) - offset);
        if (!count) return -1;
        std::memcpy(buffer, bytes.constData() + offset, static_cast<size_t>(count)); offset += count; return count;
    }
private:
    QByteArray bytes; qint64 offset = 0;
};
class UiTransport : public QNetworkAccessManager {
public:
    QString clientId; int searches = 0, reads = 0; bool failRead = false, unexpected = false;
protected:
    QNetworkReply *createRequest(Operation, const QNetworkRequest &request, QIODevice *) override {
        const auto path = request.url().path(); QByteArray body; int status = 200, delay = 0;
        if (path == "/oauth2/device") body = R"({"device_code":"fixture","user_code":"TEST","verification_uri":"https://www.twitch.tv/activate","interval":5,"expires_in":60})";
        else if (path == "/oauth2/token") body = R"({"access_token":"fixture-access","refresh_token":"fixture-refresh","token_type":"bearer"})";
        else if (path == "/oauth2/validate") body = QJsonDocument(QJsonObject{{"client_id", clientId}, {"user_id", "123"}, {"login", "fixture_streamer"}, {"scopes", QJsonArray{"channel:manage:broadcast"}}, {"expires_in", 3600}}).toJson();
        else if (path == "/helix/channels") { ++reads; delay = 300; status = failRead ? 503 : 200; body = R"({"data":[{"broadcaster_id":"123","game_id":"42","game_name":"Minecraft"}]})"; }
        else if (path == "/helix/search/categories") { ++searches; delay = 180; body = R"({"data":[{"id":"42","name":"Minecraft","box_art_url":""}],"pagination":{}})"; }
        else if (path == "/helix/games" || path == "/oauth2/revoke") body = R"({"data":[]})";
        else { unexpected = true; status = 500; body = "{}"; }
        return new UiReply(request, body, status, delay, this);
    }
};

namespace tca {
struct DockTestAccess {
    static Client &twitch(Dock &dock) { return dock.client; }
    static QString prepareFixture(Dock &dock) {
        dock.config.clientId = "fixture123"; dock.config.termsVersion = dock.config.privacyVersion = DocumentVersion;
        dock.config.termsUtc = dock.config.privacyUtc = "2026-10-27T00:00:00Z";
        dock.config.animations = true; return dock.effectiveClientId();
    }
    static void lightTheme(Dock &dock) { dock.config.theme = 1; dock.theme(); }
    static void noAnimation(Dock &dock) { dock.config.animations = false; dock.theme(); }
    static bool refreshAnimating(Dock &dock) { return dock.refreshTimer.isActive(); }
    static bool preview(Dock &dock, const QString &path) { return dock.contentBody->grab().save(path); }
    static bool layoutOrder(Dock &dock) {
        auto *layout = dock.contentBody->layout(); int previous = -1;
        for (const auto &name : {"accountCard", "statusCard", "currentCategoryCard", "suggestionCard", "manualSearchCard"}) {
            auto *widget = dock.findChild<QWidget *>(name); if (!widget || layout->indexOf(widget) <= previous) return false;
            previous = layout->indexOf(widget);
        }
        return layout->indexOf(dock.activityToggle) > previous;
    }
    static bool pausedCopy(Dock &dock) { dock.scan(); return dock.unknownLabel->text() == "自動偵測已暫停"; }
    static bool documents(Dock &dock) { return dock.confirmUpdatedDocuments(); }
    static bool attemptSave(Dock &dock) { return dock.save(); }
    static void developerScan(Dock &dock) {
        // A real foreground window can change while these UI assertions run.
        dock.scanTimer.stop(); dock.watcher.disconnect(&dock); dock.watcher.waitForFinished();
        Processes snapshot; snapshot.available = true;
        snapshot.executables = {QFileInfo(QCoreApplication::applicationFilePath()).fileName().toLower()};
        snapshot.foreground = snapshot.executables.first();
        QPromise<Processes> promise; promise.start(); promise.addResult(snapshot); promise.finish();
        dock.watcher.setFuture(promise.future());
        dock.processScan(); dock.stableSince = QDateTime::currentSecsSinceEpoch() - 10; dock.processScan();
    }
    static void loginSuccess(Dock &dock) { dock.showLoginSuccess(); }
    static void searchResult(Dock &dock) { emit dock.client.results({{"555", "搜尋到的遊戲", {}}}, {}); }
    static bool games(Dock &dock) { dock.suggestionSession.played = true; dock.recognitionDialog(); return dock.suggestionSession.played; }
    static bool decisionRemembered(Dock &dock, bool accept) {
        dock.config.developerMode = true;
        Candidate candidate{"fixture.exe", accept ? "123" : "124", "Decision fixture", 0};
        dock.detected = {candidate}; dock.notifySuggestion(candidate);
        auto *prompt = dock.suggestionPrompt.data();
        for (auto *button : prompt->buttons()) if (prompt->buttonRole(button) == (accept ? QMessageBox::AcceptRole : QMessageBox::RejectRole)) { button->click(); break; }
        return dock.suggestionSession.handled.contains(suggestionKey(candidate));
    }
    static bool idleDecisionRemembered(Dock &dock, int action, const QString &screenshot) {
        dock.scanTimer.stop(); dock.watcher.disconnect(&dock); dock.watcher.waitForFinished();
        dock.config.developerMode = true; dock.config.notifications = true; dock.config.chattingSuggestion = true;
        dock.config.autoDetect = true; dock.config.paused = false;
        dock.suggestionSession = {};
        const auto now = QDateTime::currentSecsSinceEpoch();
        Processes process; process.available = true; process.executables = {"finished-game.exe"};
        dock.suggestionSession.update(process, {{"finished-game.exe", "42", "Game", 0}}, now - 1000);
        process.executables.clear();
        dock.suggestionSession.update(process, {}, now - dock.config.chattingDelay - 1);
        auto scan = [&] {
            QPromise<Processes> promise; promise.start(); promise.addResult(process); promise.finish();
            dock.watcher.setFuture(promise.future()); dock.processScan();
        };
        scan();
        auto *prompt = dock.suggestionPrompt.data();
        if (!prompt || !prompt->isVisible() || !prompt->property("idleSuggestion").toBool() || prompt->buttons().size() != 2) return false;
        QAbstractButton *button = nullptr;
        for (auto *candidate : prompt->buttons()) {
            if (prompt->buttonRole(candidate) == (action == 2 ? QMessageBox::AcceptRole : QMessageBox::RejectRole)) button = candidate;
        }
        if (!button || (action != 2 && button->text() != "這次不再提醒")) return false;
        if (action == 0 && !prompt->grab().save(screenshot)) return false;
        if (action == 1) prompt->reject(); else button->click();
        // Repeated scans and a restarted countdown must preserve the user's decision.
        scan(); dock.suggestionSession.pause();
        dock.suggestionSession.idleSince = now - dock.config.chattingDelay - 1;
        scan(); scan();
        if (!dock.suggestionSession.idleNotified || (dock.suggestionPrompt && dock.suggestionPrompt->isVisible())) return false;
        process.executables = {"next-game.exe"};
        dock.suggestionSession.update(process, {{"next-game.exe", "43", "Next game", 0}}, now);
        if (dock.suggestionSession.idleNotified) return false;
        process.executables.clear(); dock.suggestionSession.idleSince = now - dock.config.chattingDelay - 1;
        scan();
        prompt = dock.suggestionPrompt.data();
        const bool nextGameReminds = prompt && prompt->isVisible() && prompt->property("idleSuggestion").toBool();
        if (prompt) prompt->reject();
        return nextGameReminds;
    }
    static bool promptOnce(Dock &dock, const WindowEvidence &window) {
        dock.config.developerMode = true;
        Processes snapshot; snapshot.windows = {window};
        dock.updateUnknown(snapshot);
        dock.updateUnknown(snapshot);
        if (dock.pendingWindow.executable != window.executable) return false;
        return !dock.findChild<QMessageBox *>("unknownGamePrompt");
    }
    static void review(Dock &dock, const WindowEvidence &window) {
        dock.config.developerMode = true;
        dock.pendingWindow = window; dock.observedWindows = {window};
        dock.reviewUnknown();
    }
};
}

static void typeSeconds(QSpinBox *spin, const QString &text)
{
    spin->setFocus(); spin->selectAll();
    auto *edit = spin->findChild<QLineEdit *>();
    for (auto c : text) {
        QKeyEvent press(QEvent::KeyPress, c.unicode(), Qt::NoModifier, QString(c));
        QKeyEvent release(QEvent::KeyRelease, c.unicode(), Qt::NoModifier, QString(c));
        QApplication::sendEvent(edit, &press); QApplication::sendEvent(edit, &release);
    }
}
static bool hasDelayHint(QWidget *widget)
{
    for (auto *text : widget->findChildren<QLabel *>())
        if (text->text().contains("15–600 秒") && text->text().contains("10 分鐘")) return true;
    return false;
}
static bool connectedUi(const QString &output)
{
    QTemporaryDir temp; UiTransport network; QString error;
    tca::Settings settings; settings.theme = 2; settings.wizardDone = true;
    settings.favorites = {{"42", "Minecraft"}};
    if (!settings.save(temp.filePath("settings.json"), &error)) return false;
    tca::Dock dock(temp.path(), nullptr, &network); dock.resize(420, 1000); dock.show();
    auto wait = [](const std::function<bool()> &done, int timeout = 2000) {
        QElapsedTimer timer; timer.start();
        while (!done() && timer.elapsed() < timeout) { QApplication::processEvents(QEventLoop::AllEvents, 20); QThread::msleep(1); }
        return done();
    };
    auto settle = [](int duration) { QEventLoop loop; QTimer::singleShot(duration, &loop, &QEventLoop::quit); loop.exec(); };
#define UI_CHECK(condition) do { if (!(condition)) { std::fprintf(stderr, "Connected UI failure at line %d: %s\n", __LINE__, #condition); return false; } } while (false)
    auto &client = tca::DockTestAccess::twitch(dock);
    auto *accountLight = dock.findChild<QLabel *>("accountIndicator"); auto *statusLight = dock.findChild<QLabel *>("statusIndicator");
    UI_CHECK(tca::DockTestAccess::layoutOrder(dock));
    UI_CHECK(accountLight && accountLight->property("state") == "error");
    network.clientId = tca::DockTestAccess::prepareFixture(dock); client.login(network.clientId);
    UI_CHECK(accountLight->property("state") == "notice");
    UI_CHECK(wait([&] { return client.connected() && !client.readingCurrent(); }, 8000));
    UI_CHECK(accountLight->property("state") == "success" && statusLight->property("state") == "success");
    UI_CHECK(dock.findChild<QLabel *>("accountName")->text().contains("fixture_streamer"));
    UI_CHECK(dock.findChild<QPushButton *>("connectAccount")->isHidden());
    UI_CHECK(tca::DockTestAccess::pausedCopy(dock));
    auto *input = dock.findChild<QLineEdit *>("categorySearchInput"); auto *search = dock.findChild<QPushButton *>("categorySearchButton");
    UI_CHECK(input && search && search->isEnabled() && !search->property("primary").toBool());
    input->setText("   "); search->click();
    UI_CHECK(network.searches == 0 && dock.findChild<QLabel *>("operationStatus")->text().contains("請先輸入"));
    input->setText("Minecraft"); settle(550);
    UI_CHECK(network.searches == 0 && search->property("primary").toBool());
    search->click(); search->click();
    UI_CHECK(network.searches == 1 && !search->isEnabled());
    UI_CHECK(wait([&] { return search->isEnabled(); }));
    QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier); QApplication::sendEvent(input, &enter);
    UI_CHECK(network.searches == 2); UI_CHECK(wait([&] { return search->isEnabled(); }));
    auto *refresh = dock.findChild<QPushButton *>("refreshCurrent"); UI_CHECK(refresh && refresh->isEnabled());
    const auto reads = network.reads; refresh->click(); refresh->click();
    UI_CHECK(network.reads == reads + 1 && refresh->property("refreshing").toBool() && !refresh->isEnabled());
    UI_CHECK(statusLight->property("state") == "notice");
    const bool animations = QApplication::style()->styleHint(QStyle::SH_Widget_Animate);
    UI_CHECK(tca::DockTestAccess::refreshAnimating(dock) == animations);
    const auto iconFrame = refresh->icon().cacheKey(); settle(120);
    UI_CHECK(!animations || refresh->icon().cacheKey() != iconFrame);
    UI_CHECK(wait([&] { return !client.readingCurrent(); }));
    UI_CHECK(!tca::DockTestAccess::refreshAnimating(dock) && statusLight->property("state") == "success");
    QPushButton *settingsButton = nullptr; for (auto *candidate : dock.findChildren<QPushButton *>()) if (candidate->text() == "設定") settingsButton = candidate;
    UI_CHECK(settingsButton);
    bool accountEntry = false;
    QTimer::singleShot(50, &dock, [&] {
        auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        auto *summary = dialog ? dialog->findChild<QLabel *>("settingsAccountName") : nullptr;
        accountEntry = dialog && dialog->findChild<QPushButton *>("changeTwitchAccount") && summary && summary->text().contains("fixture_streamer");
        if (dialog && !dialog->grab().save(output + "/account-settings.png")) accountEntry = false;
        if (dialog) dialog->reject();
    });
    settingsButton->click(); UI_CHECK(accountEntry);
    settle(160); UI_CHECK(tca::DockTestAccess::preview(dock, output + "/connected-dark.png"));
    tca::DockTestAccess::lightTheme(dock); settle(160); UI_CHECK(tca::DockTestAccess::preview(dock, output + "/connected-light.png"));
    network.failRead = true; refresh->click(); UI_CHECK(wait([&] { return !client.readingCurrent(); }));
    UI_CHECK(statusLight->property("state") == "error" && !tca::DockTestAccess::refreshAnimating(dock));
    tca::DockTestAccess::noAnimation(dock); network.failRead = false; refresh->click();
    UI_CHECK(client.readingCurrent() && !tca::DockTestAccess::refreshAnimating(dock));
    dock.stop(); UI_CHECK(!client.readingCurrent() && !tca::DockTestAccess::refreshAnimating(dock));
    UI_CHECK(!network.unexpected);
#undef UI_CHECK
    return true;
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    // Reproduce host-theme inheritance instead of testing only an unstyled Qt application.
    app.setStyleSheet("QWidget {color:white;background:#202126;} QListWidget::item {color:white;} QListWidget::item:selected {background:#393d49;} QComboBox QAbstractItemView {color:white;background:#202126;} QComboBox::down-arrow {image:url(:/missing-host-arrow.svg);} QScrollArea > QWidget > QWidget {background:transparent;}");
    if (!QFile::exists(":/tca/down.svg") || !QFile::exists(":/tca/up.svg")) return 21;
    auto settle = [&] { QEventLoop loop; QTimer::singleShot(180, &loop, &QEventLoop::quit); loop.exec(); };
    QTemporaryDir temp;
    if (!temp.isValid()) return 1;
    tca::Settings settings;
    settings.theme = 2; settings.wizardDone = true;
    settings.favorites = {{"509658", "Just Chatting"}, {"42", "測試遊戲"}};
    settings.rules = {{"legacy-fixture.exe", "77", "既有遊戲", 0}}; settings.categoryCache.insert("steam:1", {"78", "遊戲庫遊戲"});
    // No consent: never restore the real user's Credential Manager session in UI tests.
    QString error;
    if (!settings.save(temp.filePath("settings.json"), &error)) return 2;
    tca::Dock dock(temp.path()); dock.resize(380, 900); dock.show();
    const auto output = QDir::current().filePath("ui-check-" + QGuiApplication::platformName() + "-" + qEnvironmentVariable("QT_SCALE_FACTOR", "1")); QDir().mkpath(output);
    int result = 0;
    QTimer::singleShot(100, &app, [&] {
        if (!dock.grab().save(output + "/dock-dark.png")) result = 3;
        dock.resize(280, 700); app.processEvents();
        if (!dock.grab().save(output + "/dock-narrow.png")) result = 4;
        QPushButton *settingsButton = nullptr;
        for (auto *button : dock.findChildren<QPushButton *>()) {
            if (button->text() == "設定") settingsButton = button;
        }
        if (!settingsButton || dock.findChild<QPushButton *>("openSetupWizard")) { app.exit(5); return; }
        QTimer::singleShot(100, &app, [&] {
            auto *dialog = qobject_cast<QDialog *>(app.activeModalWidget());
            if (!dialog || !dialog->grab().save(output + "/settings-dark.png")) result = 6;
            if (dialog) {
                auto *about = dialog->findChild<QPushButton *>("openAbout");
                if (!about) result = 52;
                else {
                    QTimer::singleShot(50, &app, [&] {
                        auto *page = qobject_cast<QDialog *>(app.activeModalWidget());
                        auto *tabs = page ? page->findChild<QTabWidget *>() : nullptr;
                        if (!tabs || tabs->count() != 5) result = 53;
                        if (tabs) {
                            for (int i = 0; i < tabs->count(); ++i) {
                                auto *text = qobject_cast<QTextBrowser *>(tabs->widget(i));
                                if (!text || text->toPlainText().size() < 100 || text->toPlainText().contains("文件無法讀取")) result = 54;
                            }
                            if (!page->grab().save(output + "/about.png")) result = 55;
                        }
                        if (page) page->reject();
                    });
                    about->click();
                }
                for (const auto &name : {"openTerms", "openPrivacy"}) {
                    auto *open = dialog->findChild<QPushButton *>(name);
                    if (!open) { result = 56; continue; }
                    QTimer::singleShot(50, &app, [&] {
                        auto *page = qobject_cast<QDialog *>(app.activeModalWidget());
                        auto *text = page ? page->findChild<QTextBrowser *>() : nullptr;
                        if (!text || !text->toPlainText().contains(tca::DocumentVersion)) result = 57;
                        if (page) page->reject();
                    });
                    open->click();
                }
                auto *size = dialog->findChild<QSpinBox *>("fontSize");
                if (!size) result = 13;
                else size->setValue(18);
                auto *delay = dialog->findChild<QSpinBox *>("chattingDelay");
                if (!delay || delay->minimum() != 15 || delay->maximum() != 600) result = 41;
                else {
                    if (!hasDelayHint(dialog)) result = 48;
                    for (const auto &input : {QString("15"), QString("600"), QString("120")}) {
                        typeSeconds(delay, input);
                        if (delay->value() != input.toInt()) result = 49;
                    }
                    // Exercise Save with an uncommitted edit, without Enter or focus loss.
                    delay->setKeyboardTracking(false);
                    typeSeconds(delay, "45");
                }
                auto *save = dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save);
                const QPointF pos(save->rect().center()), global(save->mapToGlobal(save->rect().center()));
                QMouseEvent press(QEvent::MouseButtonPress, pos, global, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                QMouseEvent release(QEvent::MouseButtonRelease, pos, global, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
                QApplication::sendEvent(save, &press); QApplication::sendEvent(save, &release);
                if (tca::Settings::load(temp.filePath("settings.json"), &error).fontSize != 18) result = 14;
                if (tca::Settings::load(temp.filePath("settings.json"), &error).chattingDelay != 45) result = 42;
            }
        });
        settingsButton->click();
        tca::DockTestAccess::loginSuccess(dock); app.processEvents();
        bool successMessage = false;
        for (auto *text : dock.findChildren<QLabel *>()) if (text->text().contains("授權網頁可以關閉")) successMessage = true;
        if (!successMessage) result = 58;
        QTimer::singleShot(200, &app, [&] {
            auto *wizard = qobject_cast<QWizard *>(app.activeModalWidget());
            if (!wizard) { result = 7; return; }
            auto *delay = wizard->findChild<QSpinBox *>("chattingDelay"); if (!delay || delay->value() != 45) result = 43;
            if (!hasDelayHint(wizard)) result = 50;
            if (delay) { typeSeconds(delay, "120"); if (delay->value() != 120) result = 51; }
            wizard->next(); wizard->next(); app.processEvents();
            if (wizard->button(QWizard::NextButton)->isEnabled()) result = 8;
            settle();
            if (!wizard->grab().save(output + "/consent-dark.png")) result = 9;
            for (auto *box : wizard->currentPage()->findChildren<QCheckBox *>()) box->setChecked(true);
            wizard->next(); app.processEvents();
            if (wizard->currentId() != 3 || wizard->button(QWizard::NextButton)->isEnabled()) result = 15;
            wizard->next(); // Even a programmatic Next must not bypass incomplete OAuth.
            if (wizard->currentId() != 3) result = 16;
            settle();
            if (!wizard->grab().save(output + "/login-required.png")) result = 17;
            wizard->reject();
        });
        QTimer::singleShot(80, &app, [&] {
            auto *dialog = qobject_cast<QDialog *>(app.activeModalWidget());
            auto *setup = dialog ? dialog->findChild<QPushButton *>("openSetupWizard") : nullptr;
            if (!setup) { result = 33; if (dialog) dialog->reject(); return; }
            setup->click();
        });
        settingsButton->click();
        dock.resize(312, 442); app.processEvents();
        if (!dock.grab().save(output + "/dock-compact-large-font.png")) result = 18;
        dock.stop(); dock.hide();
        if (app.arguments().contains("--delay-only")) { app.exit(result); return; }
        QTemporaryDir lightTemp; settings.theme = 1;
        if (!settings.save(lightTemp.filePath("settings.json"), &error)) result = 11;
        tca::Dock light(lightTemp.path()); light.resize(560, 900); light.show(); app.processEvents();
        if (!light.grab().save(output + "/dock-light.png")) result = 12;
        auto *favorites = light.findChild<QComboBox *>("favoriteChoice");
        if (!favorites || favorites->count() != 2 || light.findChild<QPushButton *>("favoriteApply")->isEnabled()) result = 25;
        light.findChild<QPushButton *>("favoriteRemove")->click();
        if (tca::Settings::load(lightTemp.filePath("settings.json"), &error).favorites.size() != 1) result = 26;
        auto *results = light.findChild<QListWidget *>("searchResults");
        { const QSignalBlocker blocker(results); results->addItems({"Just Cause", "Just Cause 2", "Just Dance"}); results->setCurrentRow(0); }
        results->show(); results->setFixedHeight(180);
        app.processEvents();
        if (results->palette().color(QPalette::Text).lightness() > 100 || !results->grab().save(output + "/search-results-light.png")) result = 27;
        QTimer::singleShot(100, &app, [&] {
            auto *dialog = qobject_cast<QDialog *>(app.activeModalWidget());
            auto *combo = dialog ? dialog->findChild<QComboBox *>() : nullptr;
            if (!combo) { result = 22; if (dialog) dialog->reject(); return; }
            combo->showPopup(); settle();
            if (combo->view()->palette().color(QPalette::Text).lightness() > 100) result = 23;
            if (!dialog->grab().save(output + "/settings-light.png") ||
                !combo->view()->window()->grab().save(output + "/theme-popup-light.png")) result = 24;
            combo->hidePopup();
            auto *scroll = dialog->findChild<QScrollArea *>();
            if (scroll) { scroll->verticalScrollBar()->setValue(dialog->findChild<QWidget *>("checkUpdates")->mapTo(scroll->widget(), QPoint{}).y() - 180); settle(); if (!dialog->grab().save(output + "/settings-updates.png")) result = 48; }
            dialog->reject();
        });
        for (auto *button : light.findChildren<QPushButton *>()) if (button->text() == "設定") { button->click(); break; }
        const tca::WindowEvidence fixture{"unlisted.exe", "GameWindow", "Independent Game", {}, "launcher.exe", {}, true};
        if (!tca::DockTestAccess::promptOnce(light, fixture)) result = 39;
        QTimer::singleShot(100, &app, [&] {
            auto *dialog = qobject_cast<QDialog *>(app.activeModalWidget());
            auto *category = dialog ? dialog->findChild<QComboBox *>("recognitionCategory") : nullptr;
            if (!category || category->count() < 2) { result = 34; if (dialog) dialog->reject(); return; }
            auto *buttons = dialog->findChild<QDialogButtonBox *>();
            if (buttons->button(QDialogButtonBox::Save)->isEnabled()) result = 35;
            if (dialog->findChild<QCheckBox *>("recognitionUsePath")->isEnabled()) result = 36;
            tca::DockTestAccess::searchResult(light);
            const int found = category->findData("555"); if (found < 1) result = 49;
            category->setCurrentIndex(found); settle();
            if (!dialog->grab().save(output + "/recognition-confirm.png")) result = 37;
            buttons->button(QDialogButtonBox::Save)->click();
        });
        tca::DockTestAccess::review(light, fixture);
        const auto learnedSettings = tca::Settings::load(lightTemp.filePath("settings.json"), &error);
        if (learnedSettings.recognitionRules.size() != 1 || tca::matchRecognition(fixture, learnedSettings.recognitionRules) != 0) result = 38;
        QTimer::singleShot(100, &app, [&] {
            auto *dialog = qobject_cast<QDialog *>(app.activeModalWidget()); auto *table = dialog ? dialog->findChild<QTableWidget *>("myGamesTable") : nullptr;
            if (!table || table->rowCount() != 3) { result = 44; if (dialog) dialog->reject(); return; }
            table->setCurrentCell(1, 0);
            for (auto *button : dialog->findChildren<QPushButton *>()) if (button->text() == "不是遊戲") { button->click(); break; }
            if (!tca::Settings::load(lightTemp.filePath("settings.json"), &error).rules[0].ignored) result = 45;
            if (!dialog->grab().save(output + "/my-games.png")) result = 46; dialog->reject();
        });
        if (!tca::DockTestAccess::games(light)) result = 47;
        if (!tca::DockTestAccess::decisionRemembered(light, true) || !tca::DockTestAccess::decisionRemembered(light, false)) result = 40;
        for (int action = 0; action < 3; ++action)
            if (!tca::DockTestAccess::idleDecisionRemembered(light, action, output + "/chatting-suggestion.png")) result = 74 + action;
        light.stop(); light.hide();
        QTemporaryDir upgradeTemp;
        auto legacy = settings.json(); legacy["wizardDone"] = true; legacy.remove("wizardRevision");
        legacy["savedByVersion"] = "0.5.2"; legacy["termsVersion"] = "old-policy"; legacy["privacyVersion"] = "old-policy";
        legacy["termsUtc"] = legacy["privacyUtc"] = "2026-10-24T00:00:00Z";
        const auto upgradePath = upgradeTemp.filePath("settings.json");
        const auto original = QJsonDocument(legacy).toJson();
        { QFile file(upgradePath); if (!file.open(QIODevice::WriteOnly) || file.write(original) != original.size()) result = 59; }
        tca::Dock upgraded(upgradeTemp.path()); upgraded.show();
        if (upgraded.needsSetup()) result = 60;
        QEventLoop updatePrompt;
        QTimer::singleShot(300, &app, [&] {
            auto *dialog = qobject_cast<QDialog *>(app.activeModalWidget());
            if (!dialog || dialog->objectName() != "updatedDocuments" || qobject_cast<QWizard *>(dialog)) result = 61;
            if (dialog) { dialog->grab().save(output + "/updated-documents.png"); dialog->reject(); }
            updatePrompt.quit();
        });
        upgraded.beginFirstRun(); updatePrompt.exec();
        { QFile file(upgradePath); if (!file.open(QIODevice::ReadOnly) || file.readAll() != original) result = 62; }
        QTimer::singleShot(50, &app, [&] {
            auto *dialog = qobject_cast<QDialog *>(app.activeModalWidget());
            auto *buttons = dialog ? dialog->findChild<QDialogButtonBox *>() : nullptr;
            if (!buttons) { result = 63; if (dialog) dialog->reject(); return; }
            auto *ok = buttons->button(QDialogButtonBox::Ok);
            if (ok->isEnabled()) result = 64;
            const auto boxes = dialog->findChildren<QCheckBox *>();
            if (boxes.size() != 2) { result = 65; dialog->reject(); return; }
            boxes[0]->setChecked(true); if (ok->isEnabled()) result = 66;
            boxes[1]->setChecked(true); ok->click();
        });
        // Directly exercise consent persistence without restoring real Credential Manager tokens.
        if (!tca::DockTestAccess::documents(upgraded)) result = 67;
        const auto savedUpgrade = tca::Settings::load(upgradePath, &error);
        if (!savedUpgrade.consented() || !savedUpgrade.wizardDone || upgraded.needsSetup()) result = 68;
        auto expected = tca::Settings::fromJson(legacy).json(); auto actual = savedUpgrade.json();
        for (const auto &key : {"termsVersion", "privacyVersion", "termsUtc", "privacyUtc"}) { expected.remove(key); actual.remove(key); }
        if (actual != expected || QDir(upgradeTemp.path()).entryList({"settings.json.backup-*"}).size() != 1) result = 69;
        upgraded.stop(); upgraded.hide();
        QTemporaryDir corruptTemp; const auto corruptPath = corruptTemp.filePath("settings.json");
        { QFile file(corruptPath); if (!file.open(QIODevice::WriteOnly) || file.write("{broken") != 7) result = 73; }
        tca::Dock corrupt(corruptTemp.path()); corrupt.show();
        if (corrupt.needsSetup() || tca::DockTestAccess::attemptSave(corrupt)) result = 70;
        QEventLoop recoveryPrompt;
        QTimer::singleShot(300, &app, [&] {
            auto *dialog = qobject_cast<QDialog *>(app.activeModalWidget());
            if (!dialog || dialog->objectName() != "settingsRecovery") result = 71;
            if (dialog) { dialog->grab().save(output + "/settings-recovery.png"); dialog->reject(); }
            recoveryPrompt.quit();
        });
        corrupt.beginFirstRun(); recoveryPrompt.exec();
        { QFile file(corruptPath); if (!file.open(QIODevice::ReadOnly) || file.readAll() != "{broken") result = 72; }
        corrupt.stop(); corrupt.hide();
        QTemporaryDir firstTemp;
        tca::Dock first(firstTemp.path()); first.resize(380, 700); first.show();
        QEventLoop firstRun;
        QTimer::singleShot(350, &app, [&] {
            auto *wizard = qobject_cast<QWizard *>(app.activeModalWidget());
            if (!wizard || wizard->currentId() != 0) result = 19;
            if (wizard) { wizard->grab().save(output + "/first-run.png"); wizard->reject(); }
            firstRun.quit();
        });
        first.beginFirstRun(); firstRun.exec();
        if (!first.needsSetup()) result = 20; // Cancel must not mark first use as complete.
        first.stop(); first.hide();
        QTemporaryDir devTemp; tca::Settings devSettings;
        devSettings.developerMode = true; devSettings.automatic = true; devSettings.multiGame = 2;
        devSettings.rules.append({QFileInfo(QCoreApplication::applicationFilePath()).fileName().toLower(), "42", "本機程序預演測試", 1});
        if (!devSettings.save(devTemp.filePath("settings.json"), &error)) result = 28;
        tca::Dock developer(devTemp.path()); developer.resize(380, 900); developer.show();
        tca::DockTestAccess::developerScan(developer);
        bool previewLogged = false;
        for (auto *list : developer.findChildren<QListWidget *>())
            for (int i = 0; i < list->count(); ++i)
                if (list->item(i)->text().contains("開發者預演：依規則自動套用")) previewLogged = true;
        if (!previewLogged) result = 29;
        auto *prompt = developer.findChild<QMessageBox *>("categorySuggestionPrompt");
        if (!prompt || !prompt->isVisible() || !prompt->text().contains("本機程序預演測試")) result = 30;
        if (prompt && !prompt->grab().save(output + "/category-suggestion.png")) result = 32;
        if (prompt) prompt->reject();
        tca::DockTestAccess::developerScan(developer); app.processEvents();
        if (auto *repeated = developer.findChild<QMessageBox *>("categorySuggestionPrompt"); repeated && repeated->isVisible()) result = 31;
        developer.grab().save(output + "/developer-preview.png");
        developer.stop(); developer.hide();
        if (!connectedUi(output)) result = 80;
        app.exit(result);
    });
    QTimer::singleShot(25000, &app, [&] { app.exit(10); });
    const auto code = app.exec();
    std::printf("UI smoke result: %d; renders: %s\n", code, output.toUtf8().constData());
    return code;
}
