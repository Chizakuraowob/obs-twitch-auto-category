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
#include <cstdio>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    auto settle = [&] { QEventLoop loop; QTimer::singleShot(180, &loop, &QEventLoop::quit); loop.exec(); };
    QTemporaryDir temp;
    if (!temp.isValid()) return 1;
    tca::Settings settings;
    settings.theme = 2; settings.wizardDone = true;
    settings.termsVersion = settings.privacyVersion = tca::DocumentVersion;
    settings.termsUtc = settings.privacyUtc = "2026-10-24T00:00:00Z";
    QString error;
    if (!settings.save(temp.filePath("settings.json"), &error)) return 2;
    tca::Dock dock(temp.path()); dock.resize(380, 900); dock.show();
    const auto output = QDir::current().filePath("ui-check-" + QGuiApplication::platformName() + "-" + qEnvironmentVariable("QT_SCALE_FACTOR", "1")); QDir().mkpath(output);
    int result = 0;
    QTimer::singleShot(100, &app, [&] {
        if (!dock.grab().save(output + "/dock-dark.png")) result = 3;
        dock.resize(280, 700); app.processEvents();
        if (!dock.grab().save(output + "/dock-narrow.png")) result = 4;
        QPushButton *settingsButton = nullptr, *wizardButton = nullptr;
        for (auto *button : dock.findChildren<QPushButton *>()) {
            if (button->text() == "設定") settingsButton = button;
            if (button->text() == "設定精靈") wizardButton = button;
        }
        if (!settingsButton || !wizardButton) { app.exit(5); return; }
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
        QTimer::singleShot(100, &app, [&] {
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
        wizardButton->click();
        dock.resize(312, 442); app.processEvents();
        if (!dock.grab().save(output + "/dock-compact-large-font.png")) result = 18;
        dock.stop(); dock.hide();
        QTemporaryDir lightTemp; settings.theme = 1;
        if (!settings.save(lightTemp.filePath("settings.json"), &error)) result = 11;
        tca::Dock light(lightTemp.path()); light.resize(560, 900); light.show(); app.processEvents();
        if (!light.grab().save(output + "/dock-light.png")) result = 12;
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
        first.stop(); app.exit(result);
    });
    QTimer::singleShot(10000, &app, [&] { app.exit(10); });
    const auto code = app.exec();
    std::printf("UI smoke result: %d; renders: %s\n", code, output.toUtf8().constData());
    return code;
}
