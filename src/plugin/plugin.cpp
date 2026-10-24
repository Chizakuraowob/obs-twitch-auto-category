#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/platform.h>
#include <QMetaObject>
#include <QPointer>
#include <QMainWindow>
#include <QDockWidget>
#include <QTimer>
#include "ui/dock.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("twitch-category-assistant", "zh-TW")
MODULE_EXPORT const char *obs_module_description(void) { return "Twitch Category Assistant — native category management dock"; }
static QPointer<tca::Dock> dock;
static bool exiting = false;
static void eventCallback(enum obs_frontend_event event, void *)
{
    if (!dock) return;
    if (event == OBS_FRONTEND_EVENT_EXIT) { exiting = true; dock->stop(); return; }
    if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING && dock->needsSetup()) {
        QTimer::singleShot(0, dock, [] {
            if (!dock || exiting) return;
            auto *main = static_cast<QMainWindow *>(obs_frontend_get_main_window());
            if (auto *container = main->findChild<QDockWidget *>("twitch-category-assistant")) {
                container->setFloating(false);
                main->addDockWidget(Qt::RightDockWidgetArea, container);
                container->show(); container->raise();
            }
            dock->beginFirstRun();
        });
    }
    if (event == OBS_FRONTEND_EVENT_STREAMING_STARTED || event == OBS_FRONTEND_EVENT_STREAMING_STOPPED) {
        const bool active = event == OBS_FRONTEND_EVENT_STREAMING_STARTED;
        QMetaObject::invokeMethod(dock, [active] { if (dock) dock->setStreaming(active); }, Qt::QueuedConnection);
    }
}
bool obs_module_load(void) { return true; }
void obs_module_post_load(void)
{
    auto *main = static_cast<QWidget *>(obs_frontend_get_main_window());
    if (!main) return;
    char *config = obs_module_config_path("");
    if (!config) return;
    const auto path = QString::fromUtf8(config); bfree(config);
    dock = new tca::Dock(path, main);
    if (!obs_frontend_add_dock_by_id("twitch-category-assistant", "Twitch 分類助手", dock)) { delete dock; return; }
    dock->setStreaming(obs_frontend_streaming_active());
    obs_frontend_add_event_callback(eventCallback, nullptr);
    blog(LOG_INFO, "[Twitch Category Assistant] native dock initialized");
}
void obs_module_unload(void)
{
    if (!exiting) {
        obs_frontend_remove_event_callback(eventCallback, nullptr);
        if (dock) { dock->stop(); obs_frontend_remove_dock("twitch-category-assistant"); }
    }
}
