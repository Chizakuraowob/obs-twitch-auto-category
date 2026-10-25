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
        "QPushButton[link=true] {color:%1;text-align:left;padding:7px 10px;}"
        "QListWidget,QTableWidget,QTextBrowser {background:%3;border:1px solid %4;border-radius:8px;}"
        "QListWidget {color:%1;selection-color:white;selection-background-color:#6554CB;}"
        "QListWidget::item {padding:8px;color:%1;background:%3;}"
        "QListWidget::item:hover {color:%1;background:%2;}"
        "QListWidget::item:selected,QListWidget::item:selected:!active {background:#6554CB;color:white;}"
        "QListWidget::item:disabled {color:%8;background:%3;}"
        "QCheckBox {background:transparent;spacing:8px;padding:5px;} QToolTip {background:%3;color:%1;border:1px solid %4;}"
        "QScrollBar:vertical {background:transparent;width:8px;margin:0;} QScrollBar::handle:vertical {background:%4;border-radius:4px;min-height:24px;}"
        "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical {height:0;} QScrollBar::add-page:vertical,QScrollBar::sub-page:vertical {background:transparent;}")
        .arg(text, bg, panel, edge).arg(config.fontSize).arg(family).arg(config.fontSize + 3).arg(dark ? "#A9AFBB" : "#606978");
}
Dock::Dock(QString settingsDirectory, QWidget *parent)
    : QWidget(parent), directory(std::move(settingsDirectory)), settingsPath(QDir(directory).filePath("settings.json")), client(this), images(this)
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
    favoriteRemove = button("移除收藏"); favoriteRemove->setObjectName("favoriteRemove"); favorites->addWidget(favoriteRemove);
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
    connect(favoriteAdd, &QPushButton::clicked, this, [this] {
        const int row = searchList->currentRow(); if (row < 0 || row >= searchCategories.size()) return;
        const auto c = searchCategories[row];
        for (const auto &f : config.favorites) if (f.id == c.id) return;
        if (config.favorites.size() >= 100) { setStatus("我的最愛已達 100 筆，請先移除不需要的收藏"); return; }
        config.favorites.append({c.id, c.name}); save(); refreshFavorites(); favoriteChoice->setCurrentIndex(config.favorites.size() - 1);
        setStatus("已收藏：" + c.name);
    });
    moreButton = button("載入更多"); moreButton->hide(); search->addWidget(moreButton);
    mappingButton = button("＋ 建立這款遊戲的規則"); mappingButton->setProperty("link", true); search->addWidget(mappingButton);
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
    connect(mappingButton, &QPushButton::clicked, this, [this] {
        const int row = searchList->currentRow(); if (row < 0 || row >= searchCategories.size()) return;
        bool ok = false; const auto exe = QInputDialog::getText(this, "新增遊戲規則", "遊戲程序檔名（例如 game.exe）", QLineEdit::Normal, {}, &ok).trimmed().toLower();
        if (!ok) return;
        if (!validExecutable(exe)) { QMessageBox::warning(this, "規則", "請輸入 .exe 檔名，不含路徑"); return; }
        for (const auto &rule : config.rules) if (rule.executable == exe) { QMessageBox::warning(this, "規則", "此程序已有規則，請至設定編輯"); return; }
        const auto c = searchCategories[row]; config.rules.append({exe, c.id, c.name, 0}); save(); addActivity("已新增映射：" + exe + " → " + c.name);
    });
    auto *suggestion = card(layout, "智慧分類建議"); suggestionLabel = label("尚無候選；請先搜尋分類並新增遊戲規則"); suggestion->addWidget(suggestionLabel);
    suggestionChoice = new QComboBox; suggestion->addWidget(suggestionChoice);
    suggestionApply = button("套用建議"); suggestion->addWidget(suggestionApply); connect(suggestionApply, &QPushButton::clicked, this, &Dock::applySuggestion);
    ignoreButton = button("略過這次建議"); ignoreButton->setProperty("link", true); suggestion->addWidget(ignoreButton);
    connect(ignoreButton, &QPushButton::clicked, this, [this] {
        const auto id = suggestionChoice->currentData().toString(); if (!id.isEmpty()) ignored.insert(id); processScan();
    });
    resumeButton = button("恢復自動分類"); resumeButton->hide(); suggestion->addWidget(resumeButton);
    connect(resumeButton, &QPushButton::clicked, this, [this] { manualOverride = false; autoAttemptKey.clear(); stableKey.clear(); resumeButton->hide(); });
    auto *statusCard = card(layout, "狀態"); statusLabel = label("連接 Twitch，開始管理分類"); statusLabel->setAccessibleName("分類更新狀態"); statusCard->addWidget(statusLabel);
    activityToggle = button("最近活動 · 0  ▸"); activityToggle->setProperty("link", true); activityToggle->setCheckable(true); layout->addWidget(activityToggle);
    activityList = new QListWidget; activityList->setMaximumHeight(140); activityList->setWordWrap(true); activityList->hide(); layout->addWidget(activityList);
    connect(activityToggle, &QPushButton::toggled, this, [this](bool expanded) {
        activityList->setVisible(expanded);
        activityToggle->setText(QString("最近活動 · %1  %2").arg(activityList->count()).arg(expanded ? "▾" : "▸"));
        if (expanded) animate(activityList);
    });
    auto *setup = button("設定精靈"); setup->setProperty("link", true); layout->addWidget(setup); connect(setup, &QPushButton::clicked, this, &Dock::wizard); layout->addStretch();
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
    stopped = true; scanTimer.stop(); searchTimer.stop(); client.shutdown();
    if (suggestionPrompt) suggestionPrompt->close();
    watcher.disconnect(this); watcher.waitForFinished();
}
void Dock::setStreaming(bool active) { streaming = active; stableKey.clear(); scan(); }
void Dock::theme() { setStyleSheet(themeSheet(config)); }
void Dock::save()
{
    if (settingsReadError && QFile::exists(settingsPath)) {
        const auto backup = settingsPath + ".backup-" + QDateTime::currentDateTimeUtc().toString("yyyyMMddHHmmsszzz");
        if (!QFile::copy(settingsPath, backup)) { addActivity("無法備份原設定，已取消儲存"); return; }
        settingsReadError = false;
    }
    QString error; if (!config.save(settingsPath, &error)) addActivity(error);
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
    if (config.developerMode) { addActivity("開發者預演：" + suggestionChoice->currentText() + "（未更新 Twitch）"); return; }
    if (!config.consented()) return;
    for (const auto &c : detected) if (c.id == id) { addActivity("使用者套用建議：" + c.name); client.apply({c.id, c.name, {}}); return; }
}
void Dock::notifySuggestion(const Candidate &candidate)
{
    if (suggestionPrompt) suggestionPrompt->close();
    auto *prompt = new QMessageBox(QMessageBox::Information, "偵測到遊戲 · 分類更新建議",
        QString("偵測到：%1\n目前 Twitch 分類：%2\n%3")
            .arg(candidate.name, client.current().name.isEmpty() ? "尚未取得" : client.current().name,
                 config.developerMode ? "開發者模式：只預演，不修改 Twitch。" : "要更新直播分類嗎？"), QMessageBox::NoButton, this);
    suggestionPrompt = prompt; prompt->setObjectName("categorySuggestionPrompt");
    prompt->setAttribute(Qt::WA_ShowWithoutActivating);
    prompt->setAttribute(Qt::WA_DeleteOnClose); prompt->setWindowModality(Qt::NonModal); prompt->setStyleSheet(themeSheet(config));
    auto *apply = prompt->addButton(config.developerMode ? "預演套用" : "更新分類", QMessageBox::AcceptRole);
    auto *dismiss = prompt->addButton("稍後再說", QMessageBox::RejectRole); prompt->setDefaultButton(dismiss);
    apply->setEnabled(config.developerMode || (config.consented() && client.connected() && !client.busy()));
    const bool preview = config.developerMode;
    connect(prompt, &QMessageBox::buttonClicked, this, [this, apply, candidate, preview](QAbstractButton *clicked) {
        if (clicked != apply || stopped || preview != config.developerMode || !config.suggestionsEnabled(streaming)) return;
        bool stillDetected = false; for (const auto &c : detected) if (c.id == candidate.id && c.executable == candidate.executable) stillDetected = true;
        if (!stillDetected) return;
        if (preview) addActivity("開發者預演：" + candidate.name + "（未更新 Twitch）");
        else if (config.consented() && client.connected() && !client.busy()) { addActivity("使用者確認建議：" + candidate.name); client.apply({candidate.id, candidate.name, {}}); }
    });
    prompt->show();
}
void Dock::scan()
{
    if (stopped || (!config.consented() && !config.developerMode)) return;
    if (!config.suggestionsEnabled(streaming)) {
        if (suggestionPrompt) suggestionPrompt->close(); notificationKey.clear();
        suggestionChoice->clear(); suggestionApply->setEnabled(false); stableKey.clear();
        suggestionLabel->setText(config.paused ? "已暫停分類建議" : "尚未開始 OBS 串流；手動搜尋仍可使用"); updateAccount(); return;
    }
    if (watcher.isRunning()) return;
    watcher.setFuture(QtConcurrent::run(scanProcesses));
}
void Dock::processScan()
{
    if (stopped || !watcher.isFinished() || !watcher.future().isValid()) return;
    if (!config.suggestionsEnabled(streaming)) { scan(); return; }
    const auto processes = watcher.result(); detected = candidates(processes, config.rules);
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
    suggestionLabel->setText(!processes.available ? "無法讀取程序" : eligible.isEmpty() ? "未辨識到支援的遊戲；其他遊戲請搜尋分類並建立程序規則" : "來源：本機遊戲辨識／自訂映射規則");
    if (config.developerMode) suggestionLabel->setText("開發者模式 · 本機預演，不更新 Twitch\n" + suggestionLabel->text());
    if (chosen < 0 || !config.suggestionsEnabled(streaming)) { stableKey.clear(); notificationKey.clear(); if (suggestionPrompt) suggestionPrompt->close(); updateAccount(); return; }
    const auto &c = eligible[chosen]; const QString key = c.executable + "/" + c.id;
    const auto now = QDateTime::currentSecsSinceEpoch();
    if (stableKey != key) {
        if (suggestionPrompt) suggestionPrompt->close(); notificationKey.clear();
        stableKey = key; stableSince = now;
        if (config.notifications) addActivity("分類建議：" + c.name + "（" + c.executable + "）");
    }
    if (c.id == client.current().id) { if (suggestionPrompt) suggestionPrompt->close(); notificationKey.clear(); }
    const auto notice = key + "/" + client.current().id;
    if (config.notifications && (config.developerMode || client.connected()) && c.id != client.current().id &&
        now - stableSince >= 3 && notificationKey != notice) {
        notificationKey = notice; notifySuggestion(c);
    }
    // Require a stable candidate and one automatic attempt per candidate transition.
    if (config.developerMode) {
        if (config.automatic && now - stableSince >= 9 && now - lastAuto >= 60 && autoAttemptKey != key) {
            autoAttemptKey = key; lastAuto = now; addActivity("開發者預演：依規則自動套用 " + c.name + "（未更新 Twitch）");
        }
    } else if (config.automatic && !manualOverride && config.consented() && client.connected() && !client.busy() &&
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
        "\n\n本外掛會讀取本機程序檔名與前景程序，以比對遊戲映射；Minecraft 辨識另外在本機比對 Java／Minecraft 程序的視窗標題，不儲存或上傳標題。不讀取遊戲記憶體、不擷取畫面。\n程序清單只在本機處理，不上傳至 Twitch。搜尋文字、分類 ID 與 OAuth 必要資料送至 Twitch；分類圖片由 Twitch CDN 下載。\nToken 存於 Windows 認證管理員；偏好、映射、文件版本與 UTC 同意時間存於 OBS 外掛設定目錄。\n活動紀錄只留於本次工作階段，沒有遙測或雲端後端。\n你可登出、撤銷授權或清除本機資料。正式資料控制者及聯絡資訊尚待提供，因此本文件不是正式政策。");
    layout->addWidget(text); auto *close = new QDialogButtonBox(QDialogButtonBox::Close); layout->addWidget(close); connect(close, &QDialogButtonBox::rejected, &dialog, &QDialog::reject); dialog.exec();
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
    QDialog dialog(this); dialog.setWindowTitle("設定"); dialog.setStyleSheet(themeSheet(config)); dialog.resize(500, 600);
    auto *outer = new QVBoxLayout(&dialog); auto *scroll = new QScrollArea; scroll->setWidgetResizable(true); outer->addWidget(scroll);
    auto *body = new QWidget; auto *layout = new QVBoxLayout(body); scroll->setWidget(body);
    auto *appearance = card(layout, "外觀"); auto *themeBox = new QComboBox; themeBox->addItems({"跟隨系統", "淺色", "深色"}); themeBox->setCurrentIndex(config.theme); appearance->addWidget(themeBox);
    auto *fontBox = new QFontComboBox; fontBox->setCurrentFont(QFont(config.fontFamily)); fontBox->setAccessibleName("介面字型"); appearance->addWidget(label("字型")); appearance->addWidget(fontBox);
    auto *fontSize = new QSpinBox; fontSize->setRange(12, 22); fontSize->setSuffix(" px"); fontSize->setValue(config.fontSize); fontSize->setAccessibleName("字體大小"); appearance->addWidget(label("字體大小")); appearance->addWidget(fontSize);
    auto *animationBox = new QCheckBox("輕量淡入動畫"); animationBox->setChecked(config.animations); appearance->addWidget(animationBox);
    auto preview = [&] { auto values = config; values.theme = themeBox->currentIndex(); values.fontFamily = fontBox->currentFont().family(); values.fontSize = fontSize->value(); dialog.setStyleSheet(themeSheet(values)); };
    connect(themeBox, &QComboBox::currentIndexChanged, &dialog, preview); connect(fontBox, &QFontComboBox::currentFontChanged, &dialog, preview); connect(fontSize, &QSpinBox::valueChanged, &dialog, preview);
    auto *policy = card(layout, "分類更新策略"); auto *update = new QComboBox; update->addItems({"每次詢問（建議）", "符合規則時自動更新"}); update->setCurrentIndex(config.automatic ? 1 : 0); policy->addWidget(update);
    policy->addWidget(label("自動模式只處理明確映射、穩定候選；手動套用後暫停自動覆寫。"));
    auto *multi = new QComboBox; multi->addItems({"前景遊戲優先", "每次由我選擇", "自訂優先順序（數字較大優先）"}); multi->setCurrentIndex(config.multiGame); policy->addWidget(multi);
    auto *rules = button("管理遊戲映射與優先順序"); policy->addWidget(rules); connect(rules, &QPushButton::clicked, this, &Dock::rulesDialog);
    auto *notify = card(layout, "通知"); auto *notifications = new QCheckBox("OBS Dock 內通知"); notifications->setChecked(config.notifications); notify->addWidget(notifications);
    auto *windows = new QCheckBox("Windows 通知（此版本不支援）"); windows->setEnabled(false); notify->addWidget(windows);
    auto *live = new QCheckBox("僅在本機 OBS 串流期間顯示自動建議"); live->setChecked(config.liveOnly); notify->addWidget(live);
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
    auto *clear = button("清除本機資料…"); advanced->addWidget(clear);
    connect(clear, &QPushButton::clicked, &dialog, [&] {
        if (QMessageBox::question(&dialog, "清除本機資料", "刪除偏好、遊戲映射、同意紀錄、本次活動與目前帳號憑證，並嘗試撤銷 Twitch 授權？", QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
        client.logout(); config = {}; ignored.clear(); detected.clear(); manualOverride = false; stableKey.clear(); autoAttemptKey.clear();
        activityList->clear(); artCache.clear(); save(); theme(); refreshFavorites(); updateAccount(); dialog.reject();
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
        stableKey.clear(); notificationKey.clear(); if (suggestionPrompt) suggestionPrompt->close();
        autoAttemptKey.clear(); lastAuto = 0; save(); theme(); updateAccount(); scan(); dialog.accept();
    });
    dialog.exec();
}
void Dock::rulesDialog()
{
    QDialog dialog(this); dialog.setWindowTitle("遊戲映射規則"); dialog.setStyleSheet(themeSheet(config)); dialog.resize(650, 420);
    auto *layout = new QVBoxLayout(&dialog); layout->addWidget(label("在主畫面搜尋分類，使用「建立這款遊戲的規則」新增。此處可編輯或刪除；優先值越大越優先。"));
    auto *table = new QTableWidget(static_cast<int>(config.rules.size()), 4); table->setHorizontalHeaderLabels({"程序 .exe", "分類 ID", "分類名稱", "優先值"}); layout->addWidget(table);
    for (int row = 0; row < config.rules.size(); ++row) {
        const auto &r = config.rules[row]; const QStringList values{r.executable, r.categoryId, r.categoryName, QString::number(r.priority)};
        for (int col = 0; col < 4; ++col) { auto *item = new QTableWidgetItem(values[col]); if (col == 1 || col == 2) item->setFlags(item->flags() & ~Qt::ItemIsEditable); table->setItem(row, col, item); }
    }
    auto *remove = button("刪除選取規則"); layout->addWidget(remove); connect(remove, &QPushButton::clicked, &dialog, [table] { if (table->currentRow() >= 0) table->removeRow(table->currentRow()); });
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel); layout->addWidget(buttons); connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    buttons->button(QDialogButtonBox::Save)->setText("儲存"); buttons->button(QDialogButtonBox::Cancel)->setText("取消");
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        QVector<Rule> rules; QSet<QString> names;
        for (int row = 0; row < table->rowCount(); ++row) {
            const auto exe = table->item(row, 0)->text().trimmed().toLower(); bool ok = false; const int priority = table->item(row, 3)->text().toInt(&ok);
            if (!validExecutable(exe) || names.contains(exe) || !ok) { QMessageBox::warning(&dialog, "規則錯誤", "程序必須是唯一的 .exe 檔名，優先值必須是整數"); return; }
            names.insert(exe); rules.append({exe, table->item(row, 1)->text(), table->item(row, 2)->text(), priority});
        }
        config.rules = rules; stableKey.clear(); autoAttemptKey.clear(); save(); dialog.accept();
    }); dialog.exec();
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
    page("功能簡介", "搜尋並手動更新分類、讀回確認結果、依本機程序映射提出建議。\n預設每次詢問；手動操作不需要先開始直播。");
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
    auto *notice = page("通知偏好", "通知顯示於 OBS Dock。Windows 系統通知在此版本不支援。");
    auto *notify = new QCheckBox("OBS Dock 內通知"); notify->setChecked(config.notifications); notice->layout()->addWidget(notify);
    auto *live = new QCheckBox("僅在 OBS 串流時提出自動建議"); live->setChecked(config.liveOnly); notice->layout()->addWidget(live);
    page("完成設定", "完成後可搜尋分類，並建立程序映射。你可以隨時在設定修改偏好或重新開啟精靈。");
    connect(&wizard, &QWizard::currentIdChanged, &wizard, [this, &wizard] { if (wizard.currentPage()) animate(wizard.currentPage()); });
    if (wizard.exec() == QDialog::Accepted) {
        if (!agreeTerms->isChecked() || !agreePrivacy->isChecked() || !client.connected()) { setStatus("請先同意文件並完成 Twitch 登入"); return; }
        config.termsVersion = config.privacyVersion = DocumentVersion;
        config.termsUtc = config.privacyUtc = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        config.automatic = autoUpdate->isChecked(); config.notifications = notify->isChecked(); config.liveOnly = live->isChecked(); config.wizardDone = true; save(); updateAccount();
    }
}
}
