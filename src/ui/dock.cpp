#include "dock.hpp"
#include "release-config.hpp"
#include <QApplication>
#include <QBuffer>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QFuture>
#include <QGroupBox>
#include <QFontComboBox>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QStyle>
#include <QScopedValueRollback>
#include <QSignalBlocker>
#include <QFileDialog>
#include <QSaveFile>
#include <QJsonDocument>
#include <QImageReader>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QNetworkReply>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSpinBox>
#include <QStyleHints>
#include <QTableWidget>
#include <QHeaderView>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QWizard>
#include <QWizardPage>
#include <QtConcurrent/QtConcurrentRun>
#include <algorithm>

namespace tca {
class ConsentPage : public QWizardPage {
public:
    void require(const QString &name, QCheckBox *box) { registerField(name + "*", box); }
};
class LoginPage : public QWizardPage {
public:
    explicit LoginPage(Client *client) : client(client) {
        connect(client, &Client::accountChanged, this, [this] { emit completeChanged(); });
    }
    bool isComplete() const override { return client->connected(); }
    bool validatePage() override { return isComplete(); }
private:
    Client *client;
};
static QLabel *label(const QString &text, QWidget *parent = nullptr)
{
    auto *value = new QLabel(text, parent); value->setWordWrap(true); value->setTextFormat(Qt::PlainText); return value;
}
static QPushButton *button(const QString &text) { auto *b = new QPushButton(text); b->setMinimumHeight(36); return b; }
static QVBoxLayout *card(QVBoxLayout *parent, const QString &title)
{
    auto *box = new QFrame; box->setProperty("card", true);
    auto *layout = new QVBoxLayout(box); layout->setSpacing(8); layout->setContentsMargins(14, 12, 14, 12);
    if (!title.isEmpty()) { auto *heading = label(title); heading->setProperty("role", "heading"); layout->addWidget(heading); }
    parent->addWidget(box); return layout;
}
static QString themeSheet(const Settings &config)
{
    const int mode = config.theme;
    const bool dark = mode == 2 || (mode == 0 && QGuiApplication::styleHints()->colorScheme() != Qt::ColorScheme::Light);
    const QString bg = dark ? "#141518" : "#F7F8FA", panel = dark ? "#1D1F23" : "#FFFFFF";
    const QString edge = dark ? "#34373D" : "#E3E5E9", text = dark ? "#F3F4F6" : "#202329";
    QString family = config.fontFamily; family.remove('\\'); family.remove('\''); family.remove('\n'); family.remove('\r');
    return QString("QWidget {color:%1;background:%2; font-family:'%6';font-size:%5px;}"
        "QWidget#tcaRoot,QDialog {background:%2;} QWidget#tcaRoot {border:0;border-radius:0;}"
        "QScrollArea,QScrollArea > QWidget {border:0;background:%2;border-radius:0;}"
        "QFrame[card=true] {background:%3;border:1px solid %4;border-radius:12px;}"
        "QLabel {background:transparent;} QLabel[role=heading] {font-weight:600;} QLabel[role=muted] {color:%8;}"
        "QLabel[role=title] {font-weight:700;font-size:%7px;} QLabel[badge=true] {color:#8B7CF6;}"
        "QPushButton,QComboBox,QLineEdit,QSpinBox {background:%3;border:1px solid %4;border-radius:8px;padding:7px;}"
        "QPushButton:hover {border-color:#8B7CF6;} QPushButton:focus,QLineEdit:focus,QComboBox:focus {border-color:#8B7CF6;}"
        "QComboBox,QSpinBox {padding-right:30px;}"
        "QComboBox QAbstractItemView {color:%1;background:%3;selection-color:white;selection-background-color:#6554CB;border:1px solid %4;padding:4px;outline:0;}"
        "QComboBox QAbstractItemView::item {color:%1;background:%3;padding:7px;}"
        "QComboBox QAbstractItemView::item:selected,QComboBox QAbstractItemView::item:hover {color:white;background:#6554CB;}"
        "QComboBox::drop-down {subcontrol-origin:padding;subcontrol-position:top right;width:28px;border:0;}"
        "QComboBox::down-arrow {image:url(:/tca/down.svg);width:12px;height:12px;}"
        "QSpinBox::up-button,QSpinBox::down-button {subcontrol-origin:border;width:26px;border:0;background:transparent;}"
        "QSpinBox::up-button {subcontrol-position:top right;} QSpinBox::down-button {subcontrol-position:bottom right;}"
        "QSpinBox::up-arrow {image:url(:/tca/up.svg);width:10px;height:10px;} QSpinBox::down-arrow {image:url(:/tca/down.svg);width:10px;height:10px;}"
        "QPushButton:disabled {color:#81858d;} QPushButton[primary=true] {background:#7565DF;color:white;}"
        "QPushButton[primary=true]:disabled {background:%3;color:#81858d;}"
        "QPushButton[secondary=true] {background:#EAE5FF;color:#443287;border-color:#B8A9EE;}"
        "QPushButton[danger=true] {background:%3;color:#C33D50;border-color:#C87580;}"
        "QPushButton[secondary=true]:disabled,QPushButton[danger=true]:disabled {background:%3;color:%8;border-color:%4;}"
        "QPushButton[link=true] {color:%1;text-align:left;padding:7px 10px;}"
        "QListWidget,QTableWidget,QTextBrowser {background:%3;border:1px solid %4;border-radius:8px;}"
        "QListWidget {color:%1;selection-color:white;selection-background-color:#6554CB;}"
        "QListWidget::item {padding:8px;color:%1;background:%3;}"
        "QListWidget::item:hover {color:%1;background:%2;}"
        "QListWidget::item:selected,QListWidget::item:selected:!active {background:#6554CB;color:white;}"
        "QListWidget::item:disabled {color:%8;background:%3;}"
        "QCheckBox {background:transparent;spacing:8px;padding:5px;} QToolTip {background:%3;color:%1;border:1px solid %4;}"
        "QCheckBox::indicator {width:16px;height:16px;border:1px solid #81858D;border-radius:4px;background:%3;image:none;}"
        "QCheckBox::indicator:checked {background:#7565DF;border-color:#7565DF;image:url(:/tca/check.svg);}"
        "QCheckBox::indicator:disabled {border-color:%4;background:%2;}"
        "QScrollBar:vertical {background:transparent;width:8px;margin:0;} QScrollBar::handle:vertical {background:%4;border-radius:4px;min-height:24px;}"
        "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical {height:0;} QScrollBar::add-page:vertical,QScrollBar::sub-page:vertical {background:transparent;}")
        .arg(text, bg, panel, edge).arg(config.fontSize).arg(family).arg(config.fontSize + 3).arg(dark ? "#A9AFBB" : "#606978");
}
Dock::Dock(QString settingsDirectory, QWidget *parent)
    : QWidget(parent), directory(std::move(settingsDirectory)), settingsPath(QDir(directory).filePath("settings.json")), client(this), updater(UpdateRepository, PluginVersion, this), images(this)
{
    setObjectName("tcaRoot"); setAttribute(Qt::WA_StyledBackground); setMinimumWidth(300);
    QDir().mkpath(directory);
    QString error; config = Settings::load(settingsPath, &error); settingsReadError = !error.isEmpty();
    auto *outer = new QVBoxLayout(this); outer->setContentsMargins(0, 0, 0, 0); outer->setSpacing(0);
    auto *scroll = new QScrollArea; scroll->setWidgetResizable(true); outer->addWidget(scroll);
    auto *body = new QWidget; body->setObjectName("tcaRoot"); scroll->setWidget(body);
    auto *layout = new QVBoxLayout(body); layout->setSpacing(10); layout->setContentsMargins(10, 10, 10, 10);
    auto *head = new QHBoxLayout;
    auto *title = label("直播分類管理"); title->setProperty("role", "heading"); head->addWidget(title, 1);
    auto *settings = button("設定"); head->addWidget(settings); layout->addLayout(head);
    auto *subtitle = label("讓直播分類跟上你的遊戲"); subtitle->setProperty("role", "muted"); layout->addWidget(subtitle);
    connect(settings, &QPushButton::clicked, this, &Dock::settingsDialog);
    auto *account = card(layout, "Twitch 帳號"); accountLabel = label("尚未連線"); account->addWidget(accountLabel);
    connectButton = button("連接帳號"); account->addWidget(connectButton); connect(connectButton, &QPushButton::clicked, this, [this] { if (needsSetup()) wizard(); else login(); });
    connectButton->setProperty("primary", true);
    auto *current = card(layout, "目前直播分類");
    auto *currentRow = new QHBoxLayout; currentArt = label("—"); currentArt->setFixedSize(42, 56);
    currentLabel = label("尚未取得"); currentRow->addWidget(currentArt); currentRow->addWidget(currentLabel, 1); current->addLayout(currentRow);
    syncLabel = label("尚未同步"); current->addWidget(syncLabel);
    auto *reload = button("重新整理"); reload->setProperty("link", true); current->addWidget(reload); connect(reload, &QPushButton::clicked, &client, &Client::readCurrent);
    auto *favorites = card(layout, "我的最愛");
    favoriteChoice = new QComboBox; favoriteChoice->setObjectName("favoriteChoice"); favoriteChoice->setAccessibleName("收藏的 Twitch 分類"); favorites->addWidget(favoriteChoice);
    favoriteChoice->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon); favoriteChoice->setMinimumContentsLength(12);
    favoriteApply = button("套用收藏分類"); favoriteApply->setObjectName("favoriteApply"); favorites->addWidget(favoriteApply);
    favoriteApply->setProperty("primary", true);
    favoriteRemove = button("移除收藏"); favoriteRemove->setObjectName("favoriteRemove"); favorites->addWidget(favoriteRemove);
    favoriteRemove->setProperty("danger", true);
    connect(favoriteChoice, &QComboBox::currentIndexChanged, this, [this] { updateAccount(); });
    connect(favoriteApply, &QPushButton::clicked, this, [this] {
        const int row = favoriteChoice->currentIndex();
        if (row < 0 || row >= config.favorites.size() || !client.connected() || client.busy() || !config.consented()) return;
        const auto f = config.favorites[row]; manualOverride = true; resumeButton->show();
        addActivity("手動套用收藏：" + f.name); client.apply({f.id, f.name, {}});
    });
    connect(favoriteRemove, &QPushButton::clicked, this, [this] {
        const int row = favoriteChoice->currentIndex(); if (row < 0 || row >= config.favorites.size()) return;
        config.favorites.removeAt(row); save(); refreshFavorites();
    });
    auto *search = card(layout, "搜尋 Twitch 分類"); searchInput = new QLineEdit; searchInput->setPlaceholderText("輸入遊戲或分類名稱…"); searchInput->setMaxLength(200);
    searchInput->setAccessibleName("搜尋遊戲或分類"); search->addWidget(searchInput);
    searchButton = button("搜尋"); search->addWidget(searchButton);
    searchHint = label("連接 Twitch 後即可搜尋與更新分類"); searchHint->setProperty("role", "muted"); search->addWidget(searchHint);
    searchList = new QListWidget; searchList->setObjectName("searchResults"); searchList->setWordWrap(true); searchList->setFixedHeight(90); searchList->setIconSize(QSize(42, 56)); search->addWidget(searchList);
    applyButton = button("套用選取分類"); applyButton->setProperty("primary", true); search->addWidget(applyButton);
    favoriteAdd = button("加入我的最愛"); favoriteAdd->setObjectName("favoriteAdd"); search->addWidget(favoriteAdd);
    favoriteAdd->setProperty("secondary", true);
    connect(favoriteAdd, &QPushButton::clicked, this, [this] {
        const int row = searchList->currentRow(); if (row < 0 || row >= searchCategories.size()) return;
        const auto c = searchCategories[row];
        for (const auto &f : config.favorites) if (f.id == c.id) return;
        if (config.favorites.size() >= 100) { setStatus("我的最愛已達 100 筆，請先移除不需要的收藏"); return; }
        config.favorites.append({c.id, c.name}); save(); refreshFavorites(); favoriteChoice->setCurrentIndex(config.favorites.size() - 1);
        setStatus("已收藏：" + c.name);
    });
    moreButton = button("載入更多"); moreButton->hide(); search->addWidget(moreButton);
    mappingButton = button("＋ 記住正在玩的遊戲"); mappingButton->setProperty("link", true); search->addWidget(mappingButton);
    localGameChoice = new QComboBox; localGameChoice->setAccessibleName("偵測到的本機遊戲"); localGameChoice->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon); localGameChoice->setMinimumContentsLength(12); search->addWidget(localGameChoice);
    linkLocalGame = button("將選取分類連結到這款本機遊戲"); linkLocalGame->setProperty("secondary", true); search->addWidget(linkLocalGame);
    connect(linkLocalGame, &QPushButton::clicked, this, [this] {
        const int row = searchList->currentRow(); const auto key = localGameChoice->currentData().toString();
        if (row < 0 || row >= searchCategories.size() || !validLibraryKey(key)) return;
        if (!config.categoryCache.contains(key) && config.categoryCache.size() >= 1000) { setStatus("分類對應快取已滿，請先在設定清除快取"); return; }
        const auto category = searchCategories[row]; config.categoryCache.insert(key, {category.id, category.name}); save();
        stableKey.clear(); autoAttemptKey.clear(); setStatus("已在本機連結：" + localGameChoice->currentText() + " → " + category.name); scan();
    });
    searchTimer.setSingleShot(true); searchTimer.setInterval(400);
    auto doSearch = [this] {
        searchQuery = searchInput->text().trimmed(); searchCategories.clear(); searchList->clear(); cursor.clear(); moreButton->hide();
        searchButton->setEnabled(false); client.search(searchQuery);
    };
    connect(&searchTimer, &QTimer::timeout, this, doSearch);
    connect(searchButton, &QPushButton::clicked, this, doSearch);
    connect(searchInput, &QLineEdit::returnPressed, this, doSearch);
    connect(searchInput, &QLineEdit::textChanged, this, [this] { searchTimer.start(); });
    connect(moreButton, &QPushButton::clicked, this, [this] { moreButton->setEnabled(false); client.search(searchQuery, cursor); });
    connect(applyButton, &QPushButton::clicked, this, &Dock::applySelected);
    connect(searchList, &QListWidget::currentRowChanged, this, [this] { updateAccount(); });
    connect(mappingButton, &QPushButton::clicked, this, &Dock::pickGame);
    auto *suggestion = card(layout, "智慧分類建議"); suggestionLabel = label("尚無遊戲候選；可按「我的遊戲」新增並記住"); suggestion->addWidget(suggestionLabel);
    unknownLabel = label("未辨識的視窗只作參考，不會被當成遊戲。可按下方按鈕選取遊戲。"); unknownLabel->setObjectName("unknownRecognitionStatus"); suggestion->addWidget(unknownLabel);
    reviewUnknownButton = button("選擇正在玩的遊戲"); reviewUnknownButton->setObjectName("reviewUnknownGame"); reviewUnknownButton->setProperty("secondary", true); reviewUnknownButton->setEnabled(true); suggestion->addWidget(reviewUnknownButton);
    connect(reviewUnknownButton, &QPushButton::clicked, this, &Dock::pickGame);
    suggestionChoice = new QComboBox; suggestion->addWidget(suggestionChoice);
    suggestionApply = button("套用建議"); suggestion->addWidget(suggestionApply); connect(suggestionApply, &QPushButton::clicked, this, &Dock::applySuggestion);
    suggestionApply->setProperty("primary", true);
    ignoreButton = button("略過這次建議"); ignoreButton->setProperty("link", true); suggestion->addWidget(ignoreButton);
    connect(ignoreButton, &QPushButton::clicked, this, [this] {
        const auto id = suggestionChoice->currentData().toString(); if (!id.isEmpty()) ignored.insert(id);
        for (const auto &candidate : detected) if (candidate.id == id) suggestionSession.handled.insert(suggestionKey(candidate));
        if (suggestionPrompt) suggestionPrompt->close(); processScan();
    });
    resumeButton = button("恢復自動分類"); resumeButton->hide(); suggestion->addWidget(resumeButton);
    connect(resumeButton, &QPushButton::clicked, this, [this] { manualOverride = false; suggestionSession.handled.clear(); autoAttemptKey.clear(); stableKey.clear(); resumeButton->hide(); });
    auto *statusCard = card(layout, "狀態"); statusLabel = label("連接 Twitch，開始管理分類"); statusLabel->setAccessibleName("分類更新狀態"); statusCard->addWidget(statusLabel);
    activityToggle = button("最近活動 · 0  ▸"); activityToggle->setProperty("link", true); activityToggle->setCheckable(true); layout->addWidget(activityToggle);
    activityList = new QListWidget; activityList->setMaximumHeight(140); activityList->setWordWrap(true); activityList->hide(); layout->addWidget(activityList);
    connect(activityToggle, &QPushButton::toggled, this, [this](bool expanded) {
        activityList->setVisible(expanded);
        activityToggle->setText(QString("最近活動 · %1  %2").arg(activityList->count()).arg(expanded ? "▾" : "▸"));
        if (expanded) animate(activityList);
    });
    layout->addStretch();
    connect(&client, &Client::categoryResolved, this, [this](const QString &key, const Category &category) {
        lookupPending = false;
        if (!config.allowCategoryLookup || !config.autoDetect || !config.detectLibraries || !lookupAttempted.contains(key) || stopped) return;
        if (category.id.isEmpty()) { addActivity("找不到唯一同名 Twitch 分類；請搜尋後連結本機遊戲"); return; }
        if (!config.categoryCache.contains(key) && config.categoryCache.size() < 1000) { config.categoryCache.insert(key, {category.id, category.name}); save(); }
        scan();
    });
    connect(&client, &Client::accountChanged, this, [this] { lookupPending = false; lookupAttempted.clear(); });
    connect(&client, &Client::accountChanged, this, &Dock::updateAccount);
    connect(&client, &Client::status, this, [this](const QString &text) { setStatus(text); updateAccount(); });
    connect(&client, &Client::activity, this, &Dock::addActivity);
    connect(&client, &Client::searchFinished, this, [this] { searchButton->setEnabled(client.connected()); moreButton->setEnabled(true); });
    connect(&client, &Client::results, this, [this](const QVector<Category> &list, const QString &next) {
        for (const auto &c : list) {
            bool exists = false; for (const auto &old : searchCategories) if (old.id == c.id) exists = true;
            if (exists) continue;
            searchCategories.append(c); auto *item = new QListWidgetItem(c.name, searchList); item->setData(Qt::UserRole, c.id); fetchArt(c);
        }
        cursor = next; moreButton->setVisible(!cursor.isEmpty());
        searchList->setFixedHeight(std::clamp(static_cast<int>(searchCategories.size()) * 72 + 8, 90, 240));
        searchHint->setText(searchCategories.isEmpty() ? "沒有符合的分類，試試其他名稱" : QString("找到 %1 個分類").arg(searchCategories.size()));
        updateAccount();
    });
    connect(&client, &Client::currentChanged, this, [this] {
        const auto c = client.current(); currentLabel->setText(c.name.isEmpty() ? (client.connected() ? "未設定分類" : "尚未取得") : c.name);
        currentArt->clear(); currentArt->setText("—"); currentArt->setVisible(client.connected());
        syncLabel->setText(client.connected() ? "最後確認：" + QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss") : "未同步"); fetchArt(c);
    });
    connect(&watcher, &QFutureWatcher<Processes>::finished, this, &Dock::processScan);
    connect(&scanTimer, &QTimer::timeout, this, &Dock::scan); scanTimer.start(3000);
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, [this] { if (config.theme == 0) theme(); });
    theme(); refreshFavorites(); updateAccount();
    if (!error.isEmpty()) addActivity(error);
    connect(&updater, &Updater::changed, this, [this] { if (!stopped && !updater.busy() && !updater.latest.version.isEmpty() && updater.latest.version != config.skippedUpdate) setStatus("有新版本 " + updater.latest.version + "，請至設定 → 版本與更新。"); });
    QTimer::singleShot(10000, this, [this] { if (!stopped && config.checkUpdates && config.consented()) updater.check(); });
    if (config.consented()) client.restore(effectiveClientId());
}
QString Dock::effectiveClientId() const { return *PublisherClientId ? QString::fromLatin1(PublisherClientId) : config.clientId; }
void Dock::beginFirstRun() { if (!stopped && needsSetup()) QTimer::singleShot(150, this, &Dock::wizard); }
void Dock::setStatus(const QString &message) { if (statusLabel->text() != message) { statusLabel->setText(message); animate(statusLabel); } }
void Dock::animate(QWidget *widget)
{
    if (!config.animations || !QApplication::style()->styleHint(QStyle::SH_Widget_Animate)) return;
    if (widget->graphicsEffect()) return;
    auto *effect = new QGraphicsOpacityEffect(widget); widget->setGraphicsEffect(effect);
    auto *animation = new QPropertyAnimation(effect, "opacity", effect); animation->setDuration(140);
    animation->setStartValue(0.35); animation->setEndValue(1.0); animation->setEasingCurve(QEasingCurve::OutCubic);
    connect(animation, &QPropertyAnimation::finished, widget, [widget] { widget->setGraphicsEffect(nullptr); }); animation->start();
}
Dock::~Dock() { stop(); }
void Dock::stop()
{
    if (stopped) return;
    stopped = true; scanTimer.stop(); searchTimer.stop(); client.shutdown(); updater.cancel();
    if (suggestionPrompt) suggestionPrompt->close();

    watcher.disconnect(this); watcher.waitForFinished();
}
void Dock::setStreaming(bool active) { streaming = active; stableKey.clear(); scan(); }
void Dock::theme() { setStyleSheet(themeSheet(config)); }
bool Dock::save()
{
    if (settingsReadError && QFile::exists(settingsPath)) {
        const auto backup = settingsPath + ".backup-" + QDateTime::currentDateTimeUtc().toString("yyyyMMddHHmmsszzz");
        if (!QFile::copy(settingsPath, backup)) { addActivity("無法備份原設定，已取消儲存"); return false; }
        settingsReadError = false;
    }
    QString error; if (!config.save(settingsPath, &error)) { addActivity(error); return false; } return true;
}
void Dock::updateAccount()
{
    accountLabel->setText(client.connected() ? "● 已連線 · " + client.userName() : "○ 尚未連線");
    connectButton->setText(client.connected() ? "切換帳號" : "連接 Twitch");
    const bool allowed = client.connected() && config.consented() && !client.busy();
    applyButton->setEnabled(allowed && searchList->currentRow() >= 0);
    suggestionApply->setEnabled((allowed || config.developerMode) && suggestionChoice->currentIndex() >= 0);
    suggestionApply->setText(config.developerMode ? "預演套用建議（不更新 Twitch）" : "套用建議");
    favoriteApply->setEnabled(allowed && !config.favorites.isEmpty());
    favoriteRemove->setEnabled(!config.favorites.isEmpty());
    const int selected = searchList->currentRow();
    bool saved = false;
    if (selected >= 0 && selected < searchCategories.size())
        for (const auto &f : config.favorites) if (f.id == searchCategories[selected].id) saved = true;
    favoriteAdd->setVisible(selected >= 0 && selected < searchCategories.size());
    favoriteAdd->setEnabled(!saved);
    favoriteAdd->setText(saved ? "已加入我的最愛" : "加入我的最愛");
    searchButton->setEnabled(client.connected());
    searchInput->setEnabled(client.connected());
    currentArt->setVisible(client.connected());
    searchList->setVisible(client.connected() && !searchCategories.isEmpty());
    applyButton->setVisible(!searchCategories.isEmpty());
    mappingButton->setVisible(searchList->currentRow() >= 0);
    localGameChoice->setVisible(!localGames.isEmpty());
    linkLocalGame->setVisible(!localGames.isEmpty());
    linkLocalGame->setEnabled(selected >= 0 && selected < searchCategories.size() && localGameChoice->currentIndex() >= 0);
    const bool hasSuggestion = suggestionChoice->count() > 0;
    suggestionChoice->setVisible(hasSuggestion); suggestionApply->setVisible(hasSuggestion); ignoreButton->setVisible(hasSuggestion);
    if (!client.connected()) { searchCategories.clear(); searchList->clear(); searchList->setFixedHeight(90); moreButton->hide(); applyButton->hide(); mappingButton->hide(); }
}
void Dock::refreshFavorites()
{
    const QSignalBlocker blocker(favoriteChoice);
    const auto previous = favoriteChoice->currentData(); favoriteChoice->clear();
    for (const auto &f : config.favorites) favoriteChoice->addItem(f.name, f.id);
    if (config.favorites.isEmpty()) favoriteChoice->addItem("尚無收藏：搜尋分類後加入我的最愛");
    const int index = favoriteChoice->findData(previous); if (index >= 0) favoriteChoice->setCurrentIndex(index);
    favoriteChoice->setEnabled(!config.favorites.isEmpty()); updateAccount();
}
void Dock::addActivity(const QString &text)
{
    activityList->insertItem(0, QDateTime::currentDateTime().toString("HH:mm:ss") + "  " + text);
    while (activityList->count() > 100) delete activityList->takeItem(activityList->count() - 1);
    activityToggle->setText(QString("最近活動 · %1  %2").arg(activityList->count()).arg(activityList->isVisible() ? "▾" : "▸"));
}
void Dock::applySelected()
{
    const int row = searchList->currentRow(); if (row < 0 || row >= searchCategories.size() || !config.consented()) return;
    manualOverride = true; resumeButton->show(); addActivity("手動套用：" + searchCategories[row].name); client.apply(searchCategories[row]);
}
void Dock::applySuggestion()
{
    const auto id = suggestionChoice->currentData().toString(); if (id.isEmpty()) return;
    for (const auto &candidate : detected) if (candidate.id == id) suggestionSession.handled.insert(suggestionKey(candidate));
    if (suggestionPrompt) suggestionPrompt->close();
    if (config.developerMode) { addActivity("開發者預演：" + suggestionChoice->currentText() + "（未更新 Twitch）"); return; }
    if (!config.consented()) return;
    for (const auto &c : detected) if (c.id == id) { addActivity("使用者套用建議：" + c.name); client.apply({c.id, c.name, {}}); return; }
}
void Dock::notifySuggestion(const Candidate &candidate)
{
    const bool idle = candidate.executable == "@idle";
    if (suggestionPrompt) suggestionPrompt->close();
    auto *prompt = new QMessageBox(QMessageBox::Information, "偵測到遊戲 · 分類更新建議",
        QString("偵測到：%1\n目前 Twitch 分類：%2\n%3")
            .arg(candidate.name, client.current().name.isEmpty() ? "尚未取得" : client.current().name,
                 config.developerMode ? "開發者模式：只預演，不修改 Twitch。" : "要更新直播分類嗎？"), QMessageBox::NoButton, this);
    suggestionPrompt = prompt; prompt->setObjectName("categorySuggestionPrompt");
    prompt->setProperty("idleSuggestion", idle);
    if (idle) { prompt->setWindowTitle("遊戲已結束 · 分類更新建議"); prompt->setText(QString("遊戲結束已等待 %1 秒，是否改為 Just Chatting？\n如果仍在換遊戲，可選擇延後提醒。\n").arg(config.chattingDelay) + QString(config.developerMode ? "開發者模式：只預演，不修改 Twitch。" : "只在你確認後更新，不會自動切換。")); }
    prompt->setAttribute(Qt::WA_ShowWithoutActivating);
    prompt->setAttribute(Qt::WA_DeleteOnClose); prompt->setWindowModality(Qt::NonModal); prompt->setStyleSheet(themeSheet(config));
    auto *apply = prompt->addButton(config.developerMode ? "預演套用" : "更新分類", QMessageBox::AcceptRole);
    auto *dismiss = prompt->addButton(idle ? "維持目前分類" : "本次遊戲不再提醒", QMessageBox::RejectRole); prompt->setDefaultButton(dismiss);
    apply->setEnabled(config.developerMode || (config.consented() && client.connected() && !client.busy()));
    auto *snooze = idle ? prompt->addButton("正在換遊戲，延後提醒", QMessageBox::ActionRole) : nullptr;
    const bool preview = config.developerMode;
    connect(prompt, &QMessageBox::buttonClicked, this, [this, apply, candidate, preview, idle, snooze](QAbstractButton *clicked) {
        if (stopped || preview != config.developerMode || !config.suggestionsEnabled(streaming)) return;
        if (snooze && clicked == snooze) { suggestionSession.pause(); suggestionSession.idleNotified = false; return; }
        if (!idle) suggestionSession.handled.insert(suggestionKey(candidate));
        if (clicked != apply) return;
        bool stillDetected = false; for (const auto &c : detected) if (c.id == candidate.id && c.executable == candidate.executable) stillDetected = true;
        if (idle ? !suggestionSession.idleReady : !stillDetected) return;
        if (preview) addActivity("開發者預演：" + candidate.name + "（未更新 Twitch）");
        else if (config.consented() && client.connected() && !client.busy()) { addActivity("使用者確認建議：" + candidate.name); client.apply({candidate.id, candidate.name, {}}); }
    });
    prompt->show();
}
void Dock::updateUnknown(const Processes &processes)
{
    observedWindows = processes.windows;
    if (!config.learnUnknown) { pendingWindow = {}; reviewUnknownButton->setEnabled(true); unknownLabel->setText("未確認視窗提示已關閉；仍可手動選擇遊戲。"); return; }
    bool present = false;
    for (const auto &w : observedWindows) if (windowKey(w) == windowKey(pendingWindow)) present = true;
    if (!present) pendingWindow = {};
    for (const auto &w : observedWindows) if (w.foreground && canLearnWindow(w.executable) && matchRecognition(w, config.recognitionRules) < 0) {
        const bool known = std::any_of(detected.begin(), detected.end(), [&](const Candidate &c) { return c.executable == w.executable; });
        if (!known) pendingWindow = w;
    }
    reviewUnknownButton->setEnabled(true);
    unknownLabel->setText(pendingWindow.executable.isEmpty() ? "開啟遊戲後按「選擇正在玩的遊戲」，確認一次即可記住。" : "未確認視窗（不影響聊天倒數）：" + pendingWindow.title.left(120));
}
bool Dock::rememberRecognition(const RecognitionRule &rule)
{
    if (!validRecognitionRule(rule)) { setStatus("辨識條件不完整；未儲存"); return false; }
    const auto previous = config.recognitionRules; bool replaced = false;
    for (auto &old : config.recognitionRules) if (recognitionKey(old) == recognitionKey(rule)) { old = rule; replaced = true; break; }
    if (!replaced) {
        if (config.recognitionRules.size() >= 500) { setStatus("辨識規則已達 500 筆，請先在設定刪除不需要的規則"); return false; }
        config.recognitionRules.append(rule);
    }
    if (!save()) { config.recognitionRules = previous; setStatus("儲存失敗，尚未記住此遊戲"); return false; }
    pendingWindow = {};   reviewUnknownButton->setEnabled(true);
    stableKey.clear(); autoAttemptKey.clear();
    setStatus(rule.ignored ? "已記住：此視窗不是遊戲" : "已記住辨識條件 → " + rule.categoryName + "；下次掃描開始比對"); scan(); return true;
}
void Dock::reviewUnknown()
{
    if (stopped || reviewingUnknown || pendingWindow.executable.isEmpty()) return;
    QScopedValueRollback<bool> guard(reviewingUnknown, true); const auto window = pendingWindow;
    QDialog dialog(this); dialog.setObjectName("recognitionConfirmDialog"); dialog.setWindowTitle("確認遊戲並記住"); dialog.setStyleSheet(themeSheet(config)); dialog.resize(560, 420);
    auto *outer = new QVBoxLayout(&dialog); auto *scroll = new QScrollArea; scroll->setWidgetResizable(true); outer->addWidget(scroll);
    auto *body = new QWidget; auto *layout = new QVBoxLayout(body); scroll->setWidget(body); layout->setAlignment(Qt::AlignTop);
    layout->addWidget(label("遊戲視窗：" + window.title));
    layout->addWidget(label("請選擇這個遊戲的 Twitch 分類。只記住你確認的條件，不會將視窗標題或路徑自動送到網路。"));
    auto *category = new QComboBox; category->setObjectName("recognitionCategory"); category->addItem("請選擇分類（搜尋結果／收藏／目前分類）");
    QSet<QString> ids; const auto addCategory = [&](const QString &id, const QString &name) { if (validCategoryId(id) && !ids.contains(id)) { ids.insert(id); category->addItem(name, id); } };
    for (const auto &c : searchCategories) addCategory(c.id, c.name);
    for (const auto &c : config.favorites) addCategory(c.id, c.name);
    addCategory(client.current().id, client.current().name); layout->addWidget(category);
    auto *gameName = new QLineEdit(window.suggestedName); gameName->setMaxLength(200); gameName->setPlaceholderText("沒有分類？自行輸入遊戲名稱後搜尋"); layout->addWidget(gameName);
    auto *search = button("先搜尋 Twitch 分類（會聯網）"); layout->addWidget(search);
    auto *lookupStatus = label("可直接搜尋分類，結果會顯示在上方選單。"); layout->addWidget(lookupStatus);
    connect(&client, &Client::results, &dialog, [&](const QVector<Category> &results, const QString &) {
        category->clear(); ids.clear(); category->addItem("請選擇分類（搜尋結果／收藏／目前分類）");
        for (const auto &c : results) addCategory(c.id, c.name);
        for (const auto &c : config.favorites) addCategory(c.id, c.name);
        addCategory(client.current().id, client.current().name);
        lookupStatus->setText(QString("找到 %1 筆結果，請從上方選單選擇正確分類。").arg(results.size()));
    });
    connect(&client, &Client::searchFinished, &dialog, [=] { search->setEnabled(true); });
    connect(&client, &Client::status, lookupStatus, &QLabel::setText);
    connect(search, &QPushButton::clicked, &dialog, [&] {
        if (!client.connected() || !config.consented()) { QMessageBox::information(&dialog, "搜尋", "請先完成同意並連接 Twitch；既有收藏可離線確認。"); return; }
        if (gameName->text().trimmed().isEmpty()) { gameName->setFocus(); return; }
        searchQuery = gameName->text().trimmed(); searchInput->setText(searchQuery); searchTimer.stop(); search->setEnabled(false); lookupStatus->setText("正在向 Twitch 搜尋…"); client.search(searchQuery);
    });
    auto *advancedToggle = button("進階辨識資訊 ▸"); advancedToggle->setCheckable(true); layout->addWidget(advancedToggle);
    auto *advanced = new QWidget; auto *scope = new QVBoxLayout(advanced); layout->addWidget(advanced); advanced->hide(); connect(advancedToggle, &QPushButton::toggled, advanced, &QWidget::setVisible);
    scope->addWidget(label("程序：" + window.executable + "\n視窗類別：" + window.windowClass + "\n啟動來源：" + window.parentExecutable));
    auto *usePath = new QCheckBox("使用本機路徑指紋＋程序＋視窗類別（不受標題變化影響）"); usePath->setObjectName("recognitionUsePath");
    const bool shared = QRegularExpression("^(javaw?|pythonw?|dotnet|rundll32)\\.exe$", QRegularExpression::CaseInsensitiveOption).match(window.executable).hasMatch();
    usePath->setEnabled(!window.pathHash.isEmpty() && !shared); usePath->setChecked(usePath->isEnabled()); scope->addWidget(usePath);
    auto *title = new QLineEdit(window.title); title->setObjectName("recognitionTitle"); title->setMaxLength(256); scope->addWidget(title);
    auto *prefix = new QCheckBox("只比對以上標題前綴（至少 4 字；請勿使用過於寬泛的名稱）"); scope->addWidget(prefix);
    auto refreshScope = [&] { title->setEnabled(!usePath->isChecked()); prefix->setEnabled(!usePath->isChecked()); }; refreshScope(); connect(usePath, &QCheckBox::toggled, &dialog, refreshScope);
    scope->addWidget(label(window.pathHash.isEmpty() ? "路徑不可讀：使用程序＋視窗類別＋標題，無須提高權限。" : "指紋只用於本機比對；更換安裝位置後可能需要重新確認。"));
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel); outer->addWidget(buttons);
    buttons->button(QDialogButtonBox::Cancel)->setText("取消"); buttons->button(QDialogButtonBox::Save)->setText("確認並記住"); buttons->button(QDialogButtonBox::Save)->setEnabled(false);
    connect(category, &QComboBox::currentIndexChanged, &dialog, [=](int index) { buttons->button(QDialogButtonBox::Save)->setEnabled(index > 0); });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        if (stopped) return;
        RecognitionRule rule{window.executable, window.windowClass, usePath->isChecked() ? QString{} : title->text().simplified(), usePath->isChecked() ? window.pathHash : QString{}, category->currentData().toString(), category->currentText(), !usePath->isChecked() && prefix->isChecked(), false};
        if (!validRecognitionRule(rule) || matchRecognition(window, {rule}) != 0) { QMessageBox::warning(&dialog, "條件不足", "請選擇分類並填寫符合目前視窗的標題；前綴至少 4 字。"); return; }
        if (rememberRecognition(rule)) dialog.accept();
    });
    dialog.exec();
}
void Dock::pickGame()
{
    if (stopped) return;
    QDialog dialog(this); dialog.setWindowTitle("選擇正在玩的遊戲"); dialog.resize(560, 420); dialog.setStyleSheet(themeSheet(config));
    auto *layout = new QVBoxLayout(&dialog); layout->addWidget(label("先開啟遊戲，再從視窗清單選取。只有你確認後才會記住，不需填寫程序名稱。"));
    auto *list = new QListWidget; list->setObjectName("gameWindows"); layout->addWidget(list);
    auto *status = label("正在讀取本機可見視窗…"); layout->addWidget(status);
    auto *refresh = button("重新讀取視窗"); layout->addWidget(refresh);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel); layout->addWidget(buttons);
    buttons->button(QDialogButtonBox::Ok)->setText("選擇這個遊戲"); buttons->button(QDialogButtonBox::Cancel)->setText("取消"); buttons->button(QDialogButtonBox::Ok)->setEnabled(false);
    QVector<WindowEvidence> windows; QFutureWatcher<Processes> reader;
    connect(&reader, &QFutureWatcher<Processes>::finished, &dialog, [&] {
        windows = reader.result().windows; list->clear(); for (const auto &w : windows) list->addItem(w.title + " · " + w.executable);
        status->setText(windows.isEmpty() ? "沒有可選視窗，請開啟遊戲後重新讀取。" : "選取遊戲視窗後，下一步選擇 Twitch 分類。"); refresh->setEnabled(true);
    });
    auto read = [&] { if (reader.isRunning()) return; refresh->setEnabled(false); buttons->button(QDialogButtonBox::Ok)->setEnabled(false); reader.setFuture(QtConcurrent::run([] { return scanProcesses(false, {}, false, true); })); };
    connect(refresh, &QPushButton::clicked, &dialog, read);
    connect(list, &QListWidget::currentRowChanged, &dialog, [=](int row) { buttons->button(QDialogButtonBox::Ok)->setEnabled(row >= 0); });
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept); connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    read(); const bool accepted = dialog.exec() == QDialog::Accepted; reader.waitForFinished();
    if (accepted && list->currentRow() >= 0 && list->currentRow() < windows.size()) { pendingWindow = windows[list->currentRow()]; observedWindows = windows; reviewUnknown(); }
}
void Dock::recognitionDialog()
{
    QDialog dialog(this); dialog.setWindowTitle("我的遊戲"); dialog.setObjectName("myGamesDialog"); dialog.setStyleSheet(themeSheet(config)); dialog.resize(650, 500);
    auto *outer = new QVBoxLayout(&dialog); auto *scroll = new QScrollArea; scroll->setWidgetResizable(true); outer->addWidget(scroll); auto *body = new QWidget; auto *layout = new QVBoxLayout(body); scroll->setWidget(body); auto *empty = label("尚未記住遊戲。先開啟遊戲，再按「新增遊戲」選擇視窗與 Twitch 分類。"); layout->addWidget(empty);
    auto *add = button("＋ 新增遊戲"); add->setProperty("primary", true); layout->addWidget(add);
    auto *table = new QTableWidget(0, 3); table->setObjectName("myGamesTable"); table->setMinimumHeight(180); table->setHorizontalHeaderLabels({"遊戲", "Twitch 分類", "辨識狀態"}); table->setSelectionBehavior(QAbstractItemView::SelectRows); table->setSelectionMode(QAbstractItemView::SingleSelection); table->setEditTriggers(QAbstractItemView::NoEditTriggers); table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch); layout->addWidget(table);
    auto populate = [&] {
        table->setRowCount(0);
        auto row = [&](const QString &name, const QString &category, const QString &state, int type, const QVariant &key) {
            const int i = table->rowCount(); table->insertRow(i); auto *item = new QTableWidgetItem(name); item->setData(Qt::UserRole, type); item->setData(Qt::UserRole + 1, key); table->setItem(i, 0, item); table->setItem(i, 1, new QTableWidgetItem(category)); table->setItem(i, 2, new QTableWidgetItem(state));
        };
        for (int i = 0; i < config.recognitionRules.size(); ++i) { const auto &r = config.recognitionRules[i]; row(r.title.isEmpty() ? (r.ignored ? r.executable : r.categoryName) : r.title, r.ignored ? "—" : r.categoryName, r.ignored ? "不是遊戲" : "已確認視窗", 0, i); }
        for (int i = 0; i < config.rules.size(); ++i) { const auto &r = config.rules[i]; row(r.categoryName, r.ignored ? "—" : r.categoryName, r.ignored ? "不是遊戲" : "既有程序規則", 1, i); }
        for (auto it = config.categoryCache.begin(); it != config.categoryCache.end(); ++it) row(it->name, config.ignoredLibraries.contains(it.key()) ? "—" : it->name, config.ignoredLibraries.contains(it.key()) ? "已忽略遊戲庫項目" : "遊戲庫已連結", 2, it.key());
        empty->setVisible(table->rowCount() == 0);
    }; populate();
    auto finishEdit = [&](const Settings &old) { if (!save()) config = old; detected.clear(); stableKey.clear(); const bool played = suggestionSession.played; suggestionSession = {}; suggestionSession.played = played; if (suggestionPrompt) suggestionPrompt->close(); scan(); populate(); };
    connect(add, &QPushButton::clicked, &dialog, [&] { pickGame(); populate(); });
    auto *change = button("更改分類"); change->setProperty("secondary", true); layout->addWidget(change);
    connect(change, &QPushButton::clicked, &dialog, [&] {
        const int row = table->currentRow(); if (row < 0) return;
        QDialog choose(&dialog); choose.setWindowTitle("選擇 Twitch 分類"); auto *v = new QVBoxLayout(&choose); v->addWidget(label("選擇搜尋結果或我的最愛；沒有想要的分類時，先回主畫面搜尋。")); auto *box = new QComboBox; v->addWidget(box); QSet<QString> ids;
        auto append = [&](const QString &id, const QString &name) { if (validCategoryId(id) && !ids.contains(id)) { ids.insert(id); box->addItem(name, id); } };
        for (const auto &c : searchCategories) append(c.id, c.name); for (const auto &c : config.favorites) append(c.id, c.name); append(client.current().id, client.current().name);
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel); v->addWidget(buttons); buttons->button(QDialogButtonBox::Save)->setText("儲存"); buttons->button(QDialogButtonBox::Cancel)->setText("取消"); buttons->button(QDialogButtonBox::Save)->setEnabled(box->count() > 0); connect(buttons, &QDialogButtonBox::accepted, &choose, &QDialog::accept); connect(buttons, &QDialogButtonBox::rejected, &choose, &QDialog::reject);
        if (choose.exec() != QDialog::Accepted) return;
        const auto old = config; auto *item = table->item(row, 0); const auto key = item->data(Qt::UserRole + 1); const auto id = box->currentData().toString(), name = box->currentText();
        if (item->data(Qt::UserRole).toInt() == 0) { auto &r = config.recognitionRules[key.toInt()]; r.categoryId = id; r.categoryName = name; r.ignored = false; }
        else if (item->data(Qt::UserRole).toInt() == 1) { auto &r = config.rules[key.toInt()]; r.categoryId = id; r.categoryName = name; r.ignored = false; }
        else { config.categoryCache[key.toString()] = {id, name}; config.ignoredLibraries.removeAll(key.toString()); }
        finishEdit(old);
    });
    auto *ignore = button("不是遊戲"); layout->addWidget(ignore);
    auto *remove = button("忘記這款遊戲"); remove->setProperty("danger", true); layout->addWidget(remove);
    auto edit = [&](bool forget) {
        const int row = table->currentRow(); if (row < 0) return; const auto old = config; auto *item = table->item(row, 0); const auto key = item->data(Qt::UserRole + 1);
        if (item->data(Qt::UserRole).toInt() == 0) { if (forget) config.recognitionRules.removeAt(key.toInt()); else { auto &r = config.recognitionRules[key.toInt()]; r.ignored = true; r.categoryId.clear(); r.categoryName.clear(); } }
        else if (item->data(Qt::UserRole).toInt() == 1) { if (forget) config.rules.removeAt(key.toInt()); else config.rules[key.toInt()].ignored = true; }
        else if (forget) { config.categoryCache.remove(key.toString()); config.ignoredLibraries.removeAll(key.toString()); } else if (!config.ignoredLibraries.contains(key.toString())) config.ignoredLibraries.append(key.toString());
        finishEdit(old);
    };
    connect(remove, &QPushButton::clicked, &dialog, [&] { edit(true); }); connect(ignore, &QPushButton::clicked, &dialog, [&] { edit(false); });
    auto *details = button("進階辨識資訊"); layout->addWidget(details);
    connect(details, &QPushButton::clicked, &dialog, [&] { const int row = table->currentRow(); if (row < 0) return; const auto *item = table->item(row, 0); const auto key = item->data(Qt::UserRole + 1); QString text;
        if (item->data(Qt::UserRole).toInt() == 0) { const auto &r = config.recognitionRules[key.toInt()]; text = "程序：" + r.executable + "\n視窗類別：" + r.windowClass + "\n標題：" + r.title + "\n路徑指紋：" + r.pathHash; }
        else if (item->data(Qt::UserRole).toInt() == 1) text = "程序：" + config.rules[key.toInt()].executable; else text = "遊戲庫識別：" + key.toString();
        QMessageBox info(QMessageBox::Information, "進階辨識資訊", {}, QMessageBox::Ok, &dialog); info.setTextFormat(Qt::PlainText); info.setText(text); info.exec();
    });
    layout->addWidget(label("匯入／匯出只包含已確認視窗規則，不含舊程序規則與遊戲庫快取。分享前請檢查私人標題。"));
    auto *exportButton = button("匯出辨識規則…"); layout->addWidget(exportButton);
    connect(exportButton, &QPushButton::clicked, &dialog, [&] { const auto path = QFileDialog::getSaveFileName(&dialog, "匯出本機規則（可能包含私人視窗標題）", "tca-recognition.json", "JSON (*.json)"); if (path.isEmpty()) return; QSaveFile file(path); const auto bytes = QJsonDocument(QJsonObject{{"format", "tca-recognition"}, {"version", 1}, {"rules", recognitionJson(config.recognitionRules)}}).toJson(); if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) QMessageBox::warning(&dialog, "匯出失敗", "無法寫入規則檔。"); });
    auto *importButton = button("匯入辨識規則…"); layout->addWidget(importButton);
    connect(importButton, &QPushButton::clicked, &dialog, [&] {
        const auto path = QFileDialog::getOpenFileName(&dialog, "匯入可信任的辨識規則", {}, "JSON (*.json)"); if (path.isEmpty()) return;
        QFile file(path); if (!file.open(QIODevice::ReadOnly) || file.size() > 1024 * 1024) { QMessageBox::warning(&dialog, "匯入失敗", "檔案無法讀取或超過 1 MB。"); return; }
        QVector<RecognitionRule> merged; QString error;
        if (!importRecognitionRules(file.readAll(), config.recognitionRules, &merged, &error)) { QMessageBox::warning(&dialog, "匯入失敗", error); return; }
        if (QMessageBox::question(&dialog, "確認匯入", QString("新增 %1 筆辨識規則？符合規則時會啟用分類建議；若已開啟自動更新，可能更新 Twitch 分類。只匯入可信任來源。").arg(merged.size() - config.recognitionRules.size()), QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
        const auto old = config.recognitionRules; config.recognitionRules = merged; if (!save()) config.recognitionRules = old; stableKey.clear();  detected.clear(); if (suggestionPrompt) suggestionPrompt->close(); scan(); populate();
    });
    auto *close = new QDialogButtonBox(QDialogButtonBox::Close); close->button(QDialogButtonBox::Close)->setText("關閉"); outer->addWidget(close); connect(close, &QDialogButtonBox::rejected, &dialog, &QDialog::reject); dialog.exec();
}
void Dock::scan()
{
    if (stopped || (!config.consented() && !config.developerMode)) return;
    if (!config.suggestionsEnabled(streaming)) {
        suggestionSession.pause();
         pendingWindow = {}; observedWindows.clear(); reviewUnknownButton->setEnabled(true);  unknownLabel->setText("辨識學習隨自動偵測暫停");
        if (suggestionPrompt) suggestionPrompt->close(); notificationKey.clear();
        suggestionChoice->clear(); suggestionApply->setEnabled(false); stableKey.clear();
        localGames.clear(); localGameChoice->clear();
        suggestionLabel->setText(!config.autoDetect ? "自動偵測已關閉" : config.paused ? "已暫停分類建議" : "尚未開始 OBS 串流；手動搜尋仍可使用"); updateAccount(); return;
    }
    if (watcher.isRunning()) return;
    QStringList watched; for (const auto &rule : config.recognitionRules) watched.append(rule.executable);
    if (!pendingWindow.executable.isEmpty()) watched.append(pendingWindow.executable);
    watched.removeDuplicates();
    watcher.setFuture(QtConcurrent::run([libraries = config.detectLibraries, watched, learn = config.learnUnknown] { return scanProcesses(libraries, watched, learn); }));
}
void Dock::processScan()
{
    if (stopped || (!config.consented() && !config.developerMode) || !watcher.isFinished() || !watcher.future().isValid()) return;
    if (!config.suggestionsEnabled(streaming)) { scan(); return; }
    auto processes = watcher.result(); libraryCount = config.detectLibraries ? processes.libraryCount : 0; localGames = config.detectLibraries ? processes.localGames : QVector<RunningGame>{};
    localGames.erase(std::remove_if(localGames.begin(), localGames.end(), [&](const RunningGame &g) { return config.ignoredLibraries.contains(g.key); }), localGames.end());
    processes.localGames = localGames;
    { const QSignalBlocker blocker(localGameChoice); const auto previousGame = localGameChoice->currentData(); localGameChoice->clear();
      for (const auto &game : localGames) localGameChoice->addItem(game.name, game.key);
      const int row = localGameChoice->findData(previousGame); if (row >= 0) localGameChoice->setCurrentIndex(row); }
    for (const auto &game : localGames) {
        if (config.categoryCache.contains(game.key)) {
            const auto category = config.categoryCache.value(game.key); processes.recognizedGames.append({game.executable, category.id, category.name, 0});
        } else if (config.allowCategoryLookup && config.consented() && client.connected() && !lookupPending && !lookupAttempted.contains(game.key) && config.categoryCache.size() < 1000) {
            lookupPending = true; lookupAttempted.insert(game.key); client.resolveCategory(game.key, game.name);
        }
    }
    auto manualRules = config.rules;
    for (const auto &learned : config.recognitionRules) {
        const auto sameExe = [&](const Rule &r) { return r.executable.compare(learned.executable, Qt::CaseInsensitive) == 0; };
        processes.recognizedGames.erase(std::remove_if(processes.recognizedGames.begin(), processes.recognizedGames.end(), sameExe), processes.recognizedGames.end());
        manualRules.erase(std::remove_if(manualRules.begin(), manualRules.end(), sameExe), manualRules.end());
    }
    processes.recognizedGames += learnedCandidates(processes.windows, config.recognitionRules);
    detected = candidates(processes, manualRules);
    updateUnknown(processes);
    const auto now = QDateTime::currentSecsSinceEpoch();
    suggestionSession.update(processes, detected, now, config.chattingDelay);
    if (!config.chattingSuggestion) suggestionSession.pause();
    if (suggestionPrompt && suggestionPrompt->property("idleSuggestion").toBool() && (!suggestionSession.idleReady || client.current().id == "509658")) suggestionPrompt->close();
    if (suggestionSession.idleReady && !suggestionSession.idleNotified && config.notifications &&
        (config.developerMode || client.connected()) && client.current().id != "509658") {
        suggestionSession.idleNotified = true; notifySuggestion({"@idle", "509658", "Just Chatting", 0});
    }
    QSet<QString> running;
    for (const auto &c : detected) running.insert(c.id);
    ignored.intersect(running);
    if (detected.isEmpty()) autoAttemptKey.clear();
    QVector<Candidate> eligible;
    for (const auto &c : detected) if (!ignored.contains(c.id)) eligible.append(c);
    const auto previous = suggestionChoice->currentData().toString(); suggestionChoice->clear();
    for (const auto &c : eligible) suggestionChoice->addItem(c.name + " · " + c.executable, c.id);
    const int chosen = chooseCandidate(eligible, processes.foreground, config.multiGame);
    if (chosen >= 0) suggestionChoice->setCurrentIndex(chosen);
    else { suggestionChoice->setCurrentIndex(suggestionChoice->findData(previous)); }
    QStringList gameNames; for (const auto &game : localGames) if (!gameNames.contains(game.name)) gameNames.append(game.name);
    suggestionLabel->setText(!processes.available ? "無法讀取程序" : eligible.isEmpty() ? (localGames.isEmpty() ? "尚未偵測到遊戲視窗；可搜尋分類建立程序規則" : "已偵測到：" + gameNames.join("、") + (lookupPending ? "\n正在向 Twitch 查詢分類…" : "\n尚未對應 Twitch 分類。搜尋後連結，或在設定允許名稱查詢。")) : "來源：本機遊戲庫／內建辨識／自訂映射規則");
    if (config.developerMode) suggestionLabel->setText("開發者模式 · 本機預演，不更新 Twitch\n" + suggestionLabel->text());
    bool uncertainForeground = false;
    for (const auto &window : processes.windows) if (window.foreground) {
        const int match = matchRecognition(window, config.recognitionRules);
        const bool known = std::any_of(detected.begin(), detected.end(), [&](const Candidate &c) { return c.executable.compare(window.executable, Qt::CaseInsensitive) == 0; });
        if (match == -2 || (match == -1 && !known)) uncertainForeground = true;
    }
    if (uncertainForeground) suggestionLabel->setText("前景視窗尚未確認；暫停背景遊戲的自動分類，請先確認遊戲。");
    if (uncertainForeground || chosen < 0 || !config.suggestionsEnabled(streaming)) { stableKey.clear(); notificationKey.clear(); if (suggestionPrompt && !suggestionPrompt->property("idleSuggestion").toBool()) suggestionPrompt->close(); updateAccount(); return; }
    const auto &c = eligible[chosen]; const QString key = c.executable + "/" + c.id;
    if (stableKey != key) {
        if (suggestionPrompt) suggestionPrompt->close(); notificationKey.clear();
        stableKey = key; stableSince = now;
        if (config.notifications) addActivity("分類建議：" + c.name + "（" + c.executable + "）");
    }
    if (c.id == client.current().id) { if (suggestionPrompt) suggestionPrompt->close(); notificationKey.clear(); }
    const auto notice = key + "/" + client.current().id;
    if (config.notifications && (!config.automatic || config.developerMode || manualOverride) && (config.developerMode || client.connected()) && c.id != client.current().id &&
        now - stableSince >= 3 && notificationKey != notice && !suggestionSession.handled.contains(suggestionKey(c))) {
        notificationKey = notice; notifySuggestion(c);
    }
    // Require a stable candidate and one automatic attempt per candidate transition.
    if (config.developerMode) {
        if (config.automatic && !suggestionSession.handled.contains(suggestionKey(c)) && now - stableSince >= 9 && now - lastAuto >= 60 && autoAttemptKey != key) {
            autoAttemptKey = key; lastAuto = now; addActivity("開發者預演：依規則自動套用 " + c.name + "（未更新 Twitch）");
        }
    } else if (config.automatic && !manualOverride && !suggestionSession.handled.contains(suggestionKey(c)) && config.consented() && client.connected() && !client.busy() &&
        now - stableSince >= 9 && now - lastAuto >= 60 && c.id != client.current().id && autoAttemptKey != key) {
        autoAttemptKey = key; lastAuto = now; addActivity("依規則自動套用：" + c.name); client.apply({c.id, c.name, {}});
    }
    updateAccount();
}
void Dock::fetchArt(const Category &category)
{
    if (category.id.isEmpty()) return;
    auto show = [this, id = category.id](const QPixmap &pixmap) {
        if (client.current().id == id) currentArt->setPixmap(pixmap.scaled(42, 56, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        for (int row = 0; row < searchList->count(); ++row)
            if (searchList->item(row)->data(Qt::UserRole).toString() == id) searchList->item(row)->setIcon(QIcon(pixmap));
    };
    if (artCache.contains(category.id)) { show(artCache.value(category.id)); return; }
    if (pendingArt.contains(category.id)) return;
    const QUrl url(category.art); if (url.scheme() != "https" || url.host() != "static-cdn.jtvnw.net" || !url.userInfo().isEmpty()) return;
    QNetworkRequest req(url); req.setTransferTimeout(10000); req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    auto *reply = images.get(req); pendingArt.insert(category.id);
    connect(reply, &QNetworkReply::readyRead, this, [reply] { if (reply->bytesAvailable() > 1024 * 1024) reply->abort(); });
    connect(reply, &QNetworkReply::finished, this, [this, reply, id = category.id, show] {
        pendingArt.remove(id); reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError || reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) return;
        auto data = reply->readAll(); if (data.size() > 1024 * 1024) return;
        QBuffer buffer(&data); buffer.open(QIODevice::ReadOnly); QImageReader reader(&buffer);
        const auto size = reader.size(); if (!size.isValid() || size.width() > 2048 || size.height() > 2048) return;
        reader.setScaledSize(QSize(90, 120)); const auto pixmap = QPixmap::fromImage(reader.read()); if (pixmap.isNull()) return;
        if (artCache.size() >= 100) artCache.clear(); artCache.insert(id, pixmap); show(pixmap);
    });
}
void Dock::document(const QString &kind)
{
    QDialog dialog(this); dialog.setWindowTitle(kind + " — 待發布測試草稿"); dialog.setStyleSheet(themeSheet(config)); dialog.resize(520, 440);
    auto *layout = new QVBoxLayout(&dialog); auto *text = new QTextBrowser;
    if (kind == "服務條款") text->setPlainText("服務條款：待發布測試草稿\n版本：" + DocumentVersion +
        "\n\n本軟體目前為本機測試版本，正式服務條款尚未發布。\n功能用於讀取與修改你授權的 Twitch 頻道分類。自動更新必須由你在設定中啟用。\n分類更新依賴 Twitch API 與網路，請以讀回確認的狀態為準。\n本草稿僅供測試流程辨識，不表示正式法律文件已完成。");
    else text->setPlainText("隱私權政策：待發布測試草稿\n版本：" + DocumentVersion +
        "\n\n本外掛會讀取本機程序檔名與前景程序，以比對遊戲映射；通用辨識讀取符合條件的前景視窗，以及已記住／待確認程序的視窗標題、類別、可讀取的路徑與父程序名稱；父程序只作線索。確認後將程序、類別、路徑 SHA-256 指紋或確認標題、分類／忽略選擇存於本機。未確認標題只留記憶體。Minecraft／異環另有本機標題比對。不讀取遊戲記憶體、不擷取畫面。\n本機遊戲庫辨識會讀取 Steam／Epic 安裝紀錄、GOG 登錄資訊與可見視窗所屬程序路徑，只在本機比對。程序清單、路徑、視窗標題及完整遊戲庫不會上傳。\n自動遊戲名稱查詢預設關閉；啟用後將偵測到的遊戲名稱送到 Twitch 官方 API。手動搜尋文字、分類 ID 與 OAuth 必要資料亦送至 Twitch；分類圖片由 Twitch CDN 下載。Twitch 可接收連線 IP 與請求時間。\nToken 存於 Windows 認證管理員；偏好、收藏、平台遊戲 ID 對 Twitch 分類的快取、映射、文件版本與 UTC 同意時間存於 OBS 外掛設定目錄。快取可在設定清除；關閉名稱查詢會停止新增查詢，但不會撤回已送出的請求。\n未知視窗標題不會自動作為聯網查詢；手動搜尋會送出你輸入的名稱。辨識規則可刪除或匯入／匯出；匯出可能含私人標題，分享前請檢查。路徑指紋不是匿名化保證。手動選擇遊戲時會在本機列出可見視窗，只儲存確認的規則。版本檢查及更新下載會連線 GitHub API、github.com 與 GitHub 下載 CDN；預設不自動檢查，不傳送 Twitch Token 或遊戲資訊。GitHub 可取得 IP 與請求資訊。下載會核對 API 提供的 SHA-256，Windows 簽章檢查僅用本機快取，未自動核對作者身分；不會自動執行安裝。更新檔保存在設定目錄 updates 子目錄，需自行刪除。活動紀錄只留於本次工作階段，沒有遙測或雲端後端。\n你可登出、撤銷授權或清除本機資料。正式資料控制者及聯絡資訊尚待提供，因此本文件不是正式政策。");
    layout->addWidget(text); auto *close = new QDialogButtonBox(QDialogButtonBox::Close); close->button(QDialogButtonBox::Close)->setText("關閉"); layout->addWidget(close); connect(close, &QDialogButtonBox::rejected, &dialog, &QDialog::reject); dialog.exec();
}
void Dock::login()
{
    if (!config.consented()) { wizard(); return; }
    if (effectiveClientId().isEmpty()) { setStatus("此測試版本尚未配置發行者登入服務，請使用已配置的發行版"); return; }
    QDialog dialog(this); dialog.setStyleSheet(themeSheet(config)); dialog.setWindowTitle("連接 Twitch 帳號"); dialog.resize(460, 280);
    auto *layout = new QVBoxLayout(&dialog); auto *instructions = label("正在連接 Twitch…"); layout->addWidget(instructions);
    auto *progress = label("授權只在 Twitch 官方網站進行，密碼不會交給外掛。"); progress->setProperty("role", "muted"); layout->addWidget(progress);
    auto *open = button("開啟 Twitch 官方授權頁"); open->setEnabled(false); layout->addWidget(open);
    auto *cancel = button("取消／關閉"); layout->addWidget(cancel);
    QUrl authorizeUrl;
    connect(open, &QPushButton::clicked, &dialog, [&] { if (!authorizeUrl.isEmpty()) QDesktopServices::openUrl(authorizeUrl); });
    connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
    const auto auth = connect(&client, &Client::authorization, &dialog, [&](const QUrl &url, const QString &code, int seconds) {
        authorizeUrl = url; open->setEnabled(true); instructions->setText("已開啟 Twitch，請在瀏覽器按下授權。\n完成後會自動返回設定流程。");
        progress->setText("若頁面要求驗證碼：" + code + QString(" · 約 %1 分鐘內有效").arg(seconds / 60));
        if (!QDesktopServices::openUrl(url)) instructions->setText("無法自動開啟瀏覽器，請按下方按鈕重試。");
    });
    const auto account = connect(&client, &Client::accountChanged, &dialog, [&] { if (client.connected()) dialog.accept(); });
    const auto status = connect(&client, &Client::status, &dialog, [&](const QString &s) { instructions->setText(s); });
    const auto finished = connect(&client, &Client::authorizationFinished, &dialog, [&] { open->setEnabled(false); });
    client.login(effectiveClientId()); const int result = dialog.exec();
    disconnect(auth); disconnect(account); disconnect(status); disconnect(finished);
    if (result != QDialog::Accepted && !client.connected()) client.cancelLogin();
}
void Dock::settingsDialog()
{
    bool openWizard = false;
    QDialog dialog(this); dialog.setWindowTitle("設定"); dialog.setStyleSheet(themeSheet(config)); dialog.resize(500, 600);
    auto *outer = new QVBoxLayout(&dialog); auto *scroll = new QScrollArea; scroll->setWidgetResizable(true); outer->addWidget(scroll);
    auto *body = new QWidget; auto *layout = new QVBoxLayout(body); scroll->setWidget(body);
    auto *setupCard = card(layout, "開始與設定精靈");
    setupCard->addWidget(label("重新查看功能說明、登入 Twitch 與設定更新偏好。"));
    auto *setup = button("開啟設定精靈"); setup->setObjectName("openSetupWizard"); setup->setProperty("secondary", true); setupCard->addWidget(setup);
    connect(setup, &QPushButton::clicked, &dialog, [&] { openWizard = true; dialog.reject(); });
    auto *detection = card(layout, "自動偵測與分類更新詢問");
    auto *autoDetect = new QCheckBox("啟用遊戲自動偵測"); autoDetect->setChecked(config.autoDetect); detection->addWidget(autoDetect);
    auto *libraries = new QCheckBox("辨識本機遊戲庫與支援的獨立遊戲"); libraries->setChecked(config.detectLibraries); detection->addWidget(libraries);
    detection->addWidget(label("支援 Steam／Epic／GOG 安裝紀錄；異環另以程序與遊戲視窗辨識，不要求讀取程序路徑。"));
    detection->addWidget(label(QString("上次掃描：%1 筆安裝紀錄，%2 筆分類對應。遊戲庫約每分鐘重新讀取，程序約每 3 秒偵測。").arg(libraryCount).arg(config.categoryCache.size())));
    auto *learn = new QCheckBox("在面板顯示未確認視窗（不彈窗）"); learn->setChecked(config.learnUnknown); detection->addWidget(learn);
    detection->addWidget(label("本機比對程序、視窗類別與路徑指紋；路徑不可讀時使用你確認的標題。啟動器資訊只作線索，未確認不會自動更新。"));
    auto *learned = button("我的遊戲…"); detection->addWidget(learned); connect(learned, &QPushButton::clicked, this, &Dock::recognitionDialog);
    auto *lookup = new QCheckBox("向 Twitch 查詢遊戲名稱（聯網）"); lookup->setChecked(config.allowCategoryLookup); detection->addWidget(lookup);
    detection->addWidget(label("預設不傳送偵測資料。啟用後只向 Twitch 傳送正在執行的遊戲名稱與 OAuth 必要資訊；不傳送程序／路徑／完整遊戲庫。僅採用唯一同名分類，結果保存在本機。"));
    auto *clearCache = button("清除分類對應快取"); clearCache->setProperty("danger", true); detection->addWidget(clearCache);
    connect(clearCache, &QPushButton::clicked, &dialog, [this] { config.categoryCache.clear(); lookupAttempted.clear(); save(); setStatus("已清除本機分類對應；保留收藏與自訂程序規則"); });
    auto *chatting = new QCheckBox("遊戲結束後建議切換 Just Chatting"); chatting->setObjectName("chattingSuggestion"); chatting->setChecked(config.chattingSuggestion); detection->addWidget(chatting);
    auto *chattingDelay = new QSpinBox; chattingDelay->setObjectName("chattingDelay"); chattingDelay->setRange(15, 600); chattingDelay->setSuffix(" 秒"); chattingDelay->setValue(config.chattingDelay); chattingDelay->setEnabled(chatting->isChecked()); detection->addWidget(chattingDelay);
    connect(chatting, &QCheckBox::toggled, chattingDelay, &QWidget::setEnabled);
    detection->addWidget(label("從最後一款已確認遊戲的程序退出後計時。一般視窗不阻止倒數；只詢問，不自動切換。修改後重新計時。"));
    auto *appearance = card(layout, "外觀"); auto *themeBox = new QComboBox; themeBox->addItems({"跟隨系統", "淺色", "深色"}); themeBox->setCurrentIndex(config.theme); appearance->addWidget(themeBox);
    auto *fontBox = new QFontComboBox; fontBox->setCurrentFont(QFont(config.fontFamily)); fontBox->setAccessibleName("介面字型"); appearance->addWidget(label("字型")); appearance->addWidget(fontBox);
    auto *fontSize = new QSpinBox; fontSize->setObjectName("fontSize"); fontSize->setRange(12, 22); fontSize->setSuffix(" px"); fontSize->setValue(config.fontSize); fontSize->setAccessibleName("字體大小"); appearance->addWidget(label("字體大小")); appearance->addWidget(fontSize);
    auto *animationBox = new QCheckBox("輕量淡入動畫"); animationBox->setChecked(config.animations); appearance->addWidget(animationBox);
    auto preview = [&] { auto values = config; values.theme = themeBox->currentIndex(); values.fontFamily = fontBox->currentFont().family(); values.fontSize = fontSize->value(); dialog.setStyleSheet(themeSheet(values)); };
    connect(themeBox, &QComboBox::currentIndexChanged, &dialog, preview); connect(fontBox, &QFontComboBox::currentFontChanged, &dialog, preview); connect(fontSize, &QSpinBox::valueChanged, &dialog, preview);
    auto *policy = detection; auto *update = new QComboBox; update->addItems({"每次詢問是否更新（建議）", "符合規則時自動更新"}); update->setCurrentIndex(config.automatic ? 1 : 0); policy->addWidget(update);
    policy->addWidget(label("自動模式只處理明確映射、穩定候選；手動套用後暫停自動覆寫。"));
    auto *multi = new QComboBox; multi->addItems({"前景遊戲優先", "每次由我選擇", "自訂優先順序（數字較大優先）"}); multi->setCurrentIndex(config.multiGame); policy->addWidget(multi);

    auto *notify = detection; auto *notifications = new QCheckBox("分類不同時顯示更新詢問／建議視窗"); notifications->setChecked(config.notifications); notify->addWidget(notifications);
    auto *windows = new QCheckBox("Windows 通知（此版本不支援）"); windows->setEnabled(false); notify->addWidget(windows);
    auto *live = new QCheckBox("僅在本機 OBS 串流期間顯示自動建議"); live->setChecked(config.liveOnly); notify->addWidget(live);
    auto *updates = card(layout, "版本與更新"); updates->addWidget(label("目前版本：" + QString::fromLatin1(PluginVersion) + "\nGitHub：" + QString::fromLatin1(UpdateRepository)));
    auto *updateStatus = label(updater.status); updateStatus->setObjectName("updateStatus"); updates->addWidget(updateStatus);
    auto *autoCheck = new QCheckBox("啟動後自動檢查 GitHub 更新（預設關閉）"); autoCheck->setChecked(config.checkUpdates); autoCheck->setEnabled(updater.configured()); updates->addWidget(autoCheck);
    updates->addWidget(label("檢查與下載會連線 GitHub，對方可取得 IP 與請求資訊；不傳送 Twitch Token 或遊戲資料。更新須先關閉 OBS，不支援執行中替換外掛。"));
    auto *check = button("立即檢查更新"); check->setObjectName("checkUpdates"); updates->addWidget(check);
    auto *notes = new QTextBrowser; notes->setMaximumHeight(160); notes->setOpenExternalLinks(false); updates->addWidget(notes);
    auto *download = button("下載並核對 SHA-256"); download->setProperty("secondary", true); updates->addWidget(download);
    auto *cancelDownload = button("取消下載／檢查"); updates->addWidget(cancelDownload);
    auto *skip = button("略過此版本"); updates->addWidget(skip);
    auto *folder = button("開啟已下載更新資料夾"); updates->addWidget(folder);
    auto refreshUpdate = [=] { updateStatus->setText(updater.status + (updater.latest.version == config.skippedUpdate && !config.skippedUpdate.isEmpty() ? "（已略過自動提醒）" : "")); notes->setPlainText(updater.latest.notes); notes->setVisible(!updater.latest.notes.isEmpty()); check->setEnabled(updater.configured() && !updater.busy()); download->setEnabled(!updater.latest.version.isEmpty() && !updater.busy()); cancelDownload->setEnabled(updater.busy()); skip->setEnabled(!updater.latest.version.isEmpty() && !updater.busy()); folder->setEnabled(!updater.downloaded.isEmpty()); };
    connect(&updater, &Updater::changed, &dialog, refreshUpdate); refreshUpdate();
    connect(&updater, &Updater::progress, &dialog, [=](qint64 received, qint64 total) { updateStatus->setText(QString("下載中：%1 / %2 KB").arg(received / 1024).arg(total / 1024)); });
    connect(check, &QPushButton::clicked, &updater, &Updater::check);
    connect(download, &QPushButton::clicked, &dialog, [this] { updater.download(QDir(directory).filePath("updates")); });
    connect(cancelDownload, &QPushButton::clicked, &updater, &Updater::cancel);
    connect(skip, &QPushButton::clicked, &dialog, [&, this] { const auto old = config.skippedUpdate; config.skippedUpdate = updater.latest.version; if (!save()) config.skippedUpdate = old; refreshUpdate(); });
    connect(folder, &QPushButton::clicked, &dialog, [this] { if (!updater.downloaded.isEmpty()) QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(updater.downloaded).absolutePath())); });
    auto *advanced = card(layout, "進階與帳號"); auto *pause = new QCheckBox("暫停自動分類建議"); pause->setChecked(config.paused); advanced->addWidget(pause);
    auto *developerMode = new QCheckBox("開發者模式：不開台測試分類建議"); developerMode->setObjectName("developerMode"); developerMode->setChecked(config.developerMode); advanced->addWidget(developerMode);
    advanced->addWidget(label("以實際本機程序與映射規則預演，不需登入或開台，不會自動修改 Twitch。啟用自動更新策略可測試穩定候選的套用紀錄；手動搜尋及收藏套用仍會更新 Twitch。"));
    QLineEdit *id = nullptr;
    if (!*PublisherClientId) {
        auto *developerToggle = button("開發者選項 ▸"); developerToggle->setCheckable(true); developerToggle->setProperty("link", true); advanced->addWidget(developerToggle);
        auto *developer = new QWidget; auto *devLayout = new QVBoxLayout(developer);
        devLayout->addWidget(label("僅供作者測試：Public Client ID。發行版由作者內建，一般使用者不需填寫。"));
        id = new QLineEdit(config.clientId); id->setMaxLength(128); devLayout->addWidget(id); developer->hide(); advanced->addWidget(developer);
        connect(developerToggle, &QPushButton::toggled, developer, &QWidget::setVisible);
    }
    auto *logout = button("登出並嘗試撤銷授權"); advanced->addWidget(logout); connect(logout, &QPushButton::clicked, &client, &Client::logout);
    auto *clear = button("清除本機資料…"); clear->setProperty("danger", true); advanced->addWidget(clear);
    connect(clear, &QPushButton::clicked, &dialog, [&] {
        if (QMessageBox::question(&dialog, "清除本機資料", "刪除偏好、遊戲映射、同意紀錄、本次活動與目前帳號憑證，並嘗試撤銷 Twitch 授權？", QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
        client.logout(); config = {}; suggestionSession = {}; ignored.clear(); detected.clear(); manualOverride = false; stableKey.clear(); autoAttemptKey.clear();
        pendingWindow = {}; observedWindows.clear();   reviewUnknownButton->setEnabled(true); activityList->clear(); artCache.clear(); save(); theme(); refreshFavorites(); updateAccount(); dialog.reject();
    });
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel); outer->addWidget(buttons);
    buttons->button(QDialogButtonBox::Save)->setText("儲存"); buttons->button(QDialogButtonBox::Cancel)->setText("取消");
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        const auto newId = id ? id->text().trimmed() : config.clientId;
        if (!QRegularExpression("^[a-zA-Z0-9]{0,128}$").match(newId).hasMatch()) { QMessageBox::warning(&dialog, "Client ID", "Client ID 只可包含英數字"); return; }
        if (newId != config.clientId) client.logout();
        config.clientId = newId; config.theme = themeBox->currentIndex(); config.automatic = update->currentIndex() == 1;
        config.fontFamily = fontBox->currentFont().family(); config.fontSize = fontSize->value(); config.animations = animationBox->isChecked();
        config.multiGame = multi->currentIndex(); config.notifications = notifications->isChecked(); config.liveOnly = live->isChecked(); config.paused = pause->isChecked();
        config.developerMode = developerMode->isChecked();
        config.checkUpdates = autoCheck->isChecked(); config.chattingSuggestion = chatting->isChecked(); config.chattingDelay = chattingDelay->value(); config.learnUnknown = learn->isChecked(); config.autoDetect = autoDetect->isChecked(); config.detectLibraries = libraries->isChecked(); config.allowCategoryLookup = lookup->isChecked(); lookupAttempted.clear();
        stableKey.clear(); notificationKey.clear(); if (suggestionPrompt) suggestionPrompt->close();
        suggestionSession.pause(); autoAttemptKey.clear(); lastAuto = 0; save(); theme(); updateAccount(); scan(); dialog.accept();
    });
    dialog.exec();
    if (openWizard) wizard();
}
void Dock::wizard()
{
    if (wizardOpen || stopped) return;
    QScopedValueRollback<bool> guard(wizardOpen, true);
    QWizard wizard(this); wizard.setWizardStyle(QWizard::ModernStyle); wizard.setWindowTitle("Twitch 分類助手 · 首次設定"); wizard.setStyleSheet(themeSheet(config)); wizard.resize(540, 470);
    wizard.setButtonText(QWizard::NextButton, "下一步"); wizard.setButtonText(QWizard::BackButton, "上一步"); wizard.setButtonText(QWizard::FinishButton, "完成設定"); wizard.setButtonText(QWizard::CancelButton, "稍後設定");
    auto page = [&](const QString &title, const QString &text) {
        auto *p = new QWizardPage; p->setTitle(title); auto *layout = new QVBoxLayout(p); layout->addWidget(label(text)); wizard.addPage(p); return p;
    };
    page("歡迎", "在 OBS 原生 Dock 管理 Twitch 分類。此版本為測試版，正式條款與隱私政策待發布。");
    page("功能簡介", "搜尋並手動更新分類、讀回確認結果、依已記住的遊戲提出建議。\n預設每次詢問；手動操作不需要先開始直播。");
    auto *consent = new ConsentPage; consent->setTitle("閱讀文件並同意");
    auto *consentLayout = new QVBoxLayout(consent); consentLayout->addWidget(label("以下是待發布測試草稿。正式版本發布後需要重新同意。")); wizard.addPage(consent);
    auto *terms = button("閱讀服務條款（測試草稿）"); consent->layout()->addWidget(terms); connect(terms, &QPushButton::clicked, this, [this] { document("服務條款"); });
    auto *privacy = button("閱讀隱私權政策（測試草稿）"); consent->layout()->addWidget(privacy); connect(privacy, &QPushButton::clicked, this, [this] { document("隱私權政策"); });
    auto *agreeTerms = new QCheckBox("我已閱讀並同意服務條款測試草稿"); auto *agreePrivacy = new QCheckBox("我已閱讀並同意隱私權政策測試草稿");
    consent->layout()->addWidget(agreeTerms); consent->layout()->addWidget(agreePrivacy);
    consent->require("terms", agreeTerms); consent->require("privacy", agreePrivacy);
    auto *authorize = new LoginPage(&client); authorize->setTitle("連接 Twitch 帳號"); wizard.addPage(authorize);
    auto *authLayout = new QVBoxLayout(authorize); authLayout->setAlignment(Qt::AlignTop);
    authLayout->addWidget(label("按下連接，在 Twitch 官方網頁完成授權。外掛只管理你的直播分類，不會取得 Twitch 密碼。"));
    auto *authStatus = label(client.connected() ? "已連線：" + client.userName() : "尚未連線 · 完成登入後才能繼續"); authStatus->setObjectName("wizardAuthStatus"); authLayout->addWidget(authStatus);
    auto *signIn = button("連接 Twitch"); signIn->setProperty("primary", true); authLayout->addWidget(signIn); signIn->setEnabled(false);
    if (effectiveClientId().isEmpty()) authLayout->addWidget(label("此開發測試版尚未配置作者的登入服務。正式發行版會自動提供，一般使用者不需要註冊開發者應用程式。"));
    auto enableConsent = [this, signIn, agreeTerms, agreePrivacy] { signIn->setEnabled(agreeTerms->isChecked() && agreePrivacy->isChecked() && !effectiveClientId().isEmpty()); };
    connect(agreeTerms, &QCheckBox::toggled, &wizard, enableConsent); connect(agreePrivacy, &QCheckBox::toggled, &wizard, enableConsent);
    connect(&client, &Client::accountChanged, &wizard, [&] { authStatus->setText(client.connected() ? "✓ 已連線：" + client.userName() : "尚未連線 · 完成登入後才能繼續"); });
    connect(signIn, &QPushButton::clicked, &wizard, [&] {
        if (!agreeTerms->isChecked() || !agreePrivacy->isChecked()) return;
        config.termsVersion = config.privacyVersion = DocumentVersion;
        config.termsUtc = config.privacyUtc = QDateTime::currentDateTimeUtc().toString(Qt::ISODate); save(); login();
    });
    auto *policy = page("分類更新策略", "預設每次詢問。自動模式只套用明確且穩定的映射，手動修改會暫停自動覆寫。");
    auto *autoUpdate = new QCheckBox("符合規則時自動更新"); autoUpdate->setChecked(config.automatic); policy->layout()->addWidget(autoUpdate);
    auto *chatting = new QCheckBox("遊戲結束後建議切換 Just Chatting"); chatting->setObjectName("chattingSuggestion"); chatting->setChecked(config.chattingSuggestion); policy->layout()->addWidget(chatting);
    policy->layout()->addWidget(label("最後一款已確認遊戲退出後的等待時間；只提出建議，不自動切換。"));
    auto *chattingDelay = new QSpinBox; chattingDelay->setObjectName("chattingDelay"); chattingDelay->setRange(15, 600); chattingDelay->setSuffix(" 秒"); chattingDelay->setValue(config.chattingDelay); chattingDelay->setEnabled(chatting->isChecked()); policy->layout()->addWidget(chattingDelay);
    connect(chatting, &QCheckBox::toggled, chattingDelay, &QWidget::setEnabled);
    auto *notice = page("通知偏好", "通知顯示於 OBS Dock。Windows 系統通知在此版本不支援。");
    auto *notify = new QCheckBox("OBS Dock 內通知"); notify->setChecked(config.notifications); notice->layout()->addWidget(notify);
    auto *live = new QCheckBox("僅在 OBS 串流時提出自動建議"); live->setChecked(config.liveOnly); notice->layout()->addWidget(live);
    page("完成設定", "完成後可搜尋分類，並在「我的遊戲」選取視窗與分類。你可以隨時在設定修改偏好或重新開啟精靈。");
    connect(&wizard, &QWizard::currentIdChanged, &wizard, [this, &wizard] { if (wizard.currentPage()) animate(wizard.currentPage()); });
    if (wizard.exec() == QDialog::Accepted) {
        if (!agreeTerms->isChecked() || !agreePrivacy->isChecked() || !client.connected()) { setStatus("請先同意文件並完成 Twitch 登入"); return; }
        config.termsVersion = config.privacyVersion = DocumentVersion;
        config.termsUtc = config.privacyUtc = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        config.chattingSuggestion = chatting->isChecked(); config.chattingDelay = chattingDelay->value(); suggestionSession.pause(); config.automatic = autoUpdate->isChecked(); config.notifications = notify->isChecked(); config.liveOnly = live->isChecked(); config.wizardDone = true; save(); updateAccount();
    }
}
}
