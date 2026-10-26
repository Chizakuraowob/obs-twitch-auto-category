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

namespace tca {
struct DockTestAccess {
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
        light.stop(); light.hide();
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
        QEventLoop preview;
        QTimer::singleShot(14500, &preview, &QEventLoop::quit); preview.exec();
        bool previewLogged = false;
        for (auto *list : developer.findChildren<QListWidget *>())
            for (int i = 0; i < list->count(); ++i)
                if (list->item(i)->text().contains("開發者預演：依規則自動套用")) previewLogged = true;
        if (!previewLogged) result = 29;
        auto *prompt = developer.findChild<QMessageBox *>("categorySuggestionPrompt");
        if (!prompt || !prompt->isVisible() || !prompt->text().contains("本機程序預演測試")) result = 30;
        if (prompt && !prompt->grab().save(output + "/category-suggestion.png")) result = 32;
        if (prompt) prompt->reject();
        QEventLoop noRepeat; QTimer::singleShot(3500, &noRepeat, &QEventLoop::quit); noRepeat.exec();
        if (auto *repeated = developer.findChild<QMessageBox *>("categorySuggestionPrompt"); repeated && repeated->isVisible()) result = 31;
        developer.grab().save(output + "/developer-preview.png");
        developer.stop(); app.exit(result);
    });
    QTimer::singleShot(25000, &app, [&] { app.exit(10); });
    const auto code = app.exec();
    std::printf("UI smoke result: %d; renders: %s\n", code, output.toUtf8().constData());
    return code;
}
