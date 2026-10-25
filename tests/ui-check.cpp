#include "ui/dock.hpp"
#include <QApplication>
#include <QDialog>
#include <QDir>
#include <QPushButton>
#include <QTemporaryDir>
#include <QWizard>
#include <QCheckBox>
#include <QSpinBox>
#include <QDialogButtonBox>
#include <QEventLoop>
#include <QComboBox>
#include <QAbstractItemView>
#include <QFile>
#include <QListWidget>
#include <QSignalBlocker>
#include <QFileInfo>
#include <QMessageBox>
#include <cstdio>

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
                auto *size = dialog->findChild<QSpinBox *>();
                if (!size) result = 13;
                else size->setValue(18);
                dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
                if (tca::Settings::load(temp.filePath("settings.json"), &error).fontSize != 18) result = 14;
            }
        });
        settingsButton->click();
        QTimer::singleShot(200, &app, [&] {
            auto *wizard = qobject_cast<QWizard *>(app.activeModalWidget());
            if (!wizard) { result = 7; return; }
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
            combo->hidePopup(); dialog->reject();
        });
        for (auto *button : light.findChildren<QPushButton *>()) if (button->text() == "設定") { button->click(); break; }
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
