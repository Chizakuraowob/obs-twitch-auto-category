#include "dock.hpp"
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

namespace tca {
class ConsentPage : public QWizardPage {
public:
    void require(const QString &name, QCheckBox *box) { registerField(name + "*", box); }
};
static QLabel *label(const QString &text, QWidget *parent = nullptr)
{
    auto *value = new QLabel(text, parent); value->setWordWrap(true); value->setTextFormat(Qt::PlainText); return value;
}
static QPushButton *button(const QString &text) { auto *b = new QPushButton(text); b->setMinimumHeight(36); return b; }
static QVBoxLayout *card(QVBoxLayout *parent, const QString &title)
{
    auto *box = new QGroupBox(title); auto *layout = new QVBoxLayout(box); layout->setSpacing(8); layout->setContentsMargins(12, 20, 12, 12); parent->addWidget(box); return layout;
}
static QString themeSheet(int mode)
{
    const bool dark = mode == 2 || (mode == 0 && QGuiApplication::styleHints()->colorScheme() != Qt::ColorScheme::Light);
    const QString bg = dark ? "#141518" : "#F7F8FA", panel = dark ? "#1D1F23" : "#FFFFFF";
    const QString edge = dark ? "#34373D" : "#E3E5E9", text = dark ? "#F3F4F6" : "#202329";
    return QString("QWidget {color:%1;background:%2; font-family:'Microsoft JhengHei UI';font-size:13px;}"
        "QWidget#tcaRoot,QDialog {background:%2;} QScrollArea {border:0;background:transparent;}"
        "QGroupBox {background:%3;border:1px solid %4;border-radius:12px;margin-top:10px;font-weight:bold;}"
        "QGroupBox::title {subcontrol-origin:margin;left:12px;} QLabel {background:transparent;}"
        "QPushButton,QComboBox,QLineEdit,QSpinBox {background:%3;border:1px solid %4;border-radius:8px;padding:7px;}"
        "QPushButton:hover {border-color:#8B7CF6;} QPushButton:focus,QLineEdit:focus,QComboBox:focus {border:2px solid #8B7CF6;}"
        "QPushButton:disabled {color:#81858d;} QPushButton[primary=true] {background:#7565DF;color:white;}"
        "QPushButton[primary=true]:disabled {background:%3;color:#81858d;}"
        "QListWidget,QTableWidget,QTextBrowser {background:%3;border:1px solid %4;border-radius:8px;}"
        "QListWidget::item {padding:8px;} QListWidget::item:selected {background:#54478D;color:white;}"
        "QCheckBox {spacing:8px;padding:5px;} QToolTip {background:%3;color:%1;border:1px solid %4;}")
        .arg(text, bg, panel, edge);
}
Dock::Dock(QString settingsDirectory, QWidget *parent)
    : QWidget(parent), directory(std::move(settingsDirectory)), settingsPath(QDir(directory).filePath("settings.json")), client(this), images(this)
{
    setObjectName("tcaRoot"); setMinimumWidth(280);
    QDir().mkpath(directory);
    QString error; config = Settings::load(settingsPath, &error); settingsReadError = !error.isEmpty();
    auto *outer = new QVBoxLayout(this); outer->setContentsMargins(0, 0, 0, 0);
    auto *scroll = new QScrollArea; scroll->setWidgetResizable(true); outer->addWidget(scroll);
    auto *body = new QWidget; body->setObjectName("tcaRoot"); scroll->setWidget(body);
    auto *layout = new QVBoxLayout(body); layout->setSpacing(12); layout->setContentsMargins(12, 12, 12, 12);
    auto *head = new QHBoxLayout;
    auto *title = label("Twitch 分類助手\n智慧直播分類管理"); head->addWidget(title, 1);
    auto *settings = button("設定"); head->addWidget(settings); layout->addLayout(head);
    connect(settings, &QPushButton::clicked, this, &Dock::settingsDialog);
    auto *account = card(layout, "Twitch 帳號"); accountLabel = label("尚未連線"); account->addWidget(accountLabel);
    connectButton = button("連接帳號"); account->addWidget(connectButton); connect(connectButton, &QPushButton::clicked, this, &Dock::login);
    auto *current = card(layout, "目前直播分類");
    auto *currentRow = new QHBoxLayout; currentArt = label("封面"); currentArt->setFixedSize(60, 80);
    currentLabel = label("尚未取得"); currentRow->addWidget(currentArt); currentRow->addWidget(currentLabel, 1); current->addLayout(currentRow);
    syncLabel = label("尚未同步"); current->addWidget(syncLabel);
    auto *reload = button("重新整理"); current->addWidget(reload); connect(reload, &QPushButton::clicked, &client, &Client::readCurrent);
    auto *search = card(layout, "搜尋 Twitch 分類"); searchInput = new QLineEdit; searchInput->setPlaceholderText("輸入遊戲或分類名稱…"); searchInput->setMaxLength(200);
    searchInput->setAccessibleName("搜尋遊戲或分類"); search->addWidget(searchInput);
    searchButton = button("搜尋"); search->addWidget(searchButton);
    searchList = new QListWidget; searchList->setMinimumHeight(150); searchList->setMaximumHeight(300); searchList->setIconSize(QSize(42, 56)); search->addWidget(searchList);
    applyButton = button("套用選取分類"); applyButton->setProperty("primary", true); search->addWidget(applyButton);
    moreButton = button("載入更多"); moreButton->hide(); search->addWidget(moreButton);
    auto *mapping = button("將選取分類加入遊戲規則"); search->addWidget(mapping);
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
    connect(mapping, &QPushButton::clicked, this, [this] {
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
    auto *ignore = button("忽略此建議（直到該遊戲結束）"); suggestion->addWidget(ignore);
    connect(ignore, &QPushButton::clicked, this, [this] {
        const auto id = suggestionChoice->currentData().toString(); if (!id.isEmpty()) ignored.insert(id); processScan();
    });
    resumeButton = button("恢復自動分類"); resumeButton->hide(); suggestion->addWidget(resumeButton);
    connect(resumeButton, &QPushButton::clicked, this, [this] { manualOverride = false; autoAttemptKey.clear(); stableKey.clear(); resumeButton->hide(); });
    auto *statusCard = card(outer, "更新狀態"); statusLabel = label("尚未操作；更新後會重新讀取 Twitch 分類確認"); statusCard->addWidget(statusLabel);
    auto *activity = card(layout, "最近活動（本次 OBS 工作階段）"); activityList = new QListWidget; activityList->setMaximumHeight(160); activityList->setWordWrap(true); activity->addWidget(activityList);
    auto *setup = button("設定精靈"); layout->addWidget(setup); connect(setup, &QPushButton::clicked, this, &Dock::wizard); layout->addStretch();
    connect(&client, &Client::accountChanged, this, &Dock::updateAccount);
    connect(&client, &Client::status, this, [this](const QString &text) { statusLabel->setText(text); updateAccount(); });
    connect(&client, &Client::activity, this, &Dock::addActivity);
    connect(&client, &Client::searchFinished, this, [this] { searchButton->setEnabled(client.connected()); moreButton->setEnabled(true); });
    connect(&client, &Client::results, this, [this](const QVector<Category> &list, const QString &next) {
        for (const auto &c : list) {
            bool exists = false; for (const auto &old : searchCategories) if (old.id == c.id) exists = true;
            if (exists) continue;
            searchCategories.append(c); auto *item = new QListWidgetItem(c.name, searchList); item->setData(Qt::UserRole, c.id); fetchArt(c);
        }
        cursor = next; moreButton->setVisible(!cursor.isEmpty());
        if (searchCategories.isEmpty()) statusLabel->setText("找不到符合的分類");
    });
    connect(&client, &Client::currentChanged, this, [this] {
        const auto c = client.current(); currentLabel->setText(c.name.isEmpty() ? (client.connected() ? "未設定分類" : "尚未取得") : c.name);
        currentArt->clear(); currentArt->setText("無封面");
        syncLabel->setText(client.connected() ? "最後確認：" + QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss") : "未同步"); fetchArt(c);
    });
    connect(&watcher, &QFutureWatcher<Processes>::finished, this, &Dock::processScan);
    connect(&scanTimer, &QTimer::timeout, this, &Dock::scan); scanTimer.start(3000);
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, [this] { if (config.theme == 0) theme(); });
    theme(); updateAccount();
    if (!error.isEmpty()) addActivity(error);
    if (config.consented()) client.restore(config.clientId);
    if (!config.wizardDone || !config.consented()) QTimer::singleShot(1000, this, [this] { if (!stopped) wizard(); });
}
Dock::~Dock() { stop(); }
void Dock::stop()
{
    if (stopped) return;
    stopped = true; scanTimer.stop(); searchTimer.stop(); client.shutdown();
    watcher.disconnect(this); watcher.waitForFinished();
}
void Dock::setStreaming(bool active) { streaming = active; stableKey.clear(); scan(); }
void Dock::theme() { setStyleSheet(themeSheet(config.theme)); }
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
    accountLabel->setText(client.connected() ? "已連線：" + client.userName() : "尚未連線／需要重新授權");
    connectButton->setText(client.connected() ? "重新連線" : "連接帳號");
    const bool allowed = client.connected() && config.consented() && !client.busy();
    applyButton->setEnabled(allowed && searchList->currentRow() >= 0);
    suggestionApply->setEnabled(allowed && suggestionChoice->currentIndex() >= 0);
    searchButton->setEnabled(client.connected());
    if (!client.connected()) { searchCategories.clear(); searchList->clear(); moreButton->hide(); }
}
void Dock::addActivity(const QString &text)
{
    activityList->insertItem(0, QDateTime::currentDateTime().toString("HH:mm:ss") + "  " + text);
    while (activityList->count() > 100) delete activityList->takeItem(activityList->count() - 1);
}
void Dock::applySelected()
{
    const int row = searchList->currentRow(); if (row < 0 || row >= searchCategories.size() || !config.consented()) return;
    manualOverride = true; resumeButton->show(); addActivity("手動套用：" + searchCategories[row].name); client.apply(searchCategories[row]);
}
void Dock::applySuggestion()
{
    const auto id = suggestionChoice->currentData().toString(); if (id.isEmpty() || !config.consented()) return;
    for (const auto &c : detected) if (c.id == id) { addActivity("使用者套用建議：" + c.name); client.apply({c.id, c.name, {}}); return; }
}
void Dock::scan()
{
    if (stopped || !config.consented()) return;
    if (config.paused || (config.liveOnly && !streaming)) {
        suggestionChoice->clear(); suggestionApply->setEnabled(false); stableKey.clear();
        suggestionLabel->setText(config.paused ? "已暫停分類建議" : "尚未開始 OBS 串流；手動搜尋仍可使用"); return;
    }
    if (watcher.isRunning()) return;
    watcher.setFuture(QtConcurrent::run(scanProcesses));
}
void Dock::processScan()
{
    if (stopped || !watcher.isFinished() || !watcher.future().isValid()) return;
    if (config.paused || (config.liveOnly && !streaming)) { scan(); return; }
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
    suggestionLabel->setText(!processes.available ? "無法讀取程序" : eligible.isEmpty() ? "沒有可用建議（未知遊戲不自動更新）" : "來源：本機程序＋自訂映射規則");
    if (chosen < 0 || config.paused || (config.liveOnly && !streaming)) { stableKey.clear(); updateAccount(); return; }
    const auto &c = eligible[chosen]; const QString key = c.executable + "/" + c.id;
    const auto now = QDateTime::currentSecsSinceEpoch();
    if (stableKey != key) {
        stableKey = key; stableSince = now;
        if (config.notifications) addActivity("分類建議：" + c.name + "（" + c.executable + "）");
    }
    // Require a stable candidate and one automatic attempt per candidate transition.
    if (config.automatic && !manualOverride && config.consented() && client.connected() && !client.busy() &&
        now - stableSince >= 9 && now - lastAuto >= 60 && c.id != client.current().id && autoAttemptKey != key) {
        autoAttemptKey = key; lastAuto = now; addActivity("依規則自動套用：" + c.name); client.apply({c.id, c.name, {}});
    }
    updateAccount();
}
void Dock::fetchArt(const Category &category)
{
    if (category.id.isEmpty()) return;
    auto show = [this, id = category.id](const QPixmap &pixmap) {
        if (client.current().id == id) currentArt->setPixmap(pixmap.scaled(60, 80, Qt::KeepAspectRatio, Qt::SmoothTransformation));
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
    QDialog dialog(this); dialog.setWindowTitle(kind + " — 待發布測試草稿"); dialog.setStyleSheet(themeSheet(config.theme)); dialog.resize(520, 440);
    auto *layout = new QVBoxLayout(&dialog); auto *text = new QTextBrowser;
    if (kind == "服務條款") text->setPlainText("服務條款：待發布測試草稿\n版本：" + DocumentVersion +
        "\n\n本軟體目前為本機測試版本，正式服務條款尚未發布。\n功能用於讀取與修改你授權的 Twitch 頻道分類。自動更新必須由你在設定中啟用。\n分類更新依賴 Twitch API 與網路，請以讀回確認的狀態為準。\n本草稿僅供測試流程辨識，不表示正式法律文件已完成。");
    else text->setPlainText("隱私權政策：待發布測試草稿\n版本：" + DocumentVersion +
        "\n\n本外掛會讀取本機程序檔名與前景程序，以比對你設定的遊戲映射；不讀取遊戲記憶體、不擷取畫面。\n程序清單只在本機處理，不上傳至 Twitch。搜尋文字、分類 ID 與 OAuth 必要資料送至 Twitch；分類圖片由 Twitch CDN 下載。\nToken 存於 Windows 認證管理員；偏好、映射、文件版本與 UTC 同意時間存於 OBS 外掛設定目錄。\n活動紀錄只留於本次工作階段，沒有遙測或雲端後端。\n你可登出、撤銷授權或清除本機資料。正式資料控制者及聯絡資訊尚待提供，因此本文件不是正式政策。");
    layout->addWidget(text); auto *close = new QDialogButtonBox(QDialogButtonBox::Close); layout->addWidget(close); connect(close, &QDialogButtonBox::rejected, &dialog, &QDialog::reject); dialog.exec();
}
void Dock::login()
{
    if (!config.consented()) { wizard(); return; }
    if (config.clientId.isEmpty()) { QMessageBox::information(this, "Client ID", "請先在設定輸入 Twitch Public Client ID"); settingsDialog(); return; }
    QDialog dialog(this); dialog.setStyleSheet(themeSheet(config.theme)); dialog.setWindowTitle("連接 Twitch 帳號"); dialog.resize(460, 280);
    auto *layout = new QVBoxLayout(&dialog); auto *instructions = label("正在取得裝置授權碼…"); layout->addWidget(instructions);
    auto *open = button("開啟 Twitch 官方授權頁"); open->setEnabled(false); layout->addWidget(open);
    auto *cancel = button("取消／關閉"); layout->addWidget(cancel);
    QUrl authorizeUrl;
    connect(open, &QPushButton::clicked, &dialog, [&] { if (!authorizeUrl.isEmpty()) QDesktopServices::openUrl(authorizeUrl); });
    connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
    const auto auth = connect(&client, &Client::authorization, &dialog, [&](const QUrl &url, const QString &code, int seconds) {
        authorizeUrl = url; open->setEnabled(true); instructions->setText("請在 Twitch 官方頁面完成授權\n裝置碼：" + code + QString("\n有效時間：約 %1 分鐘").arg(seconds / 60)); QDesktopServices::openUrl(url);
    });
    const auto account = connect(&client, &Client::accountChanged, &dialog, [&] { if (client.connected()) dialog.accept(); });
    const auto status = connect(&client, &Client::status, &dialog, [&](const QString &s) { if (authorizeUrl.isEmpty()) instructions->setText(s); });
    const auto finished = connect(&client, &Client::authorizationFinished, &dialog, [&] { open->setEnabled(false); instructions->setText("授權程序已結束，正在確認結果；若未連線請關閉後重試"); });
    client.login(config.clientId); const int result = dialog.exec();
    disconnect(auth); disconnect(account); disconnect(status); disconnect(finished);
    if (result != QDialog::Accepted && !client.connected()) client.cancelLogin();
}
void Dock::settingsDialog()
{
    QDialog dialog(this); dialog.setWindowTitle("設定"); dialog.setStyleSheet(themeSheet(config.theme)); dialog.resize(500, 600);
    auto *outer = new QVBoxLayout(&dialog); auto *scroll = new QScrollArea; scroll->setWidgetResizable(true); outer->addWidget(scroll);
    auto *body = new QWidget; auto *layout = new QVBoxLayout(body); scroll->setWidget(body);
    auto *appearance = card(layout, "外觀"); auto *themeBox = new QComboBox; themeBox->addItems({"跟隨系統", "淺色", "深色"}); themeBox->setCurrentIndex(config.theme); appearance->addWidget(themeBox);
    auto *policy = card(layout, "分類更新策略"); auto *update = new QComboBox; update->addItems({"每次詢問（建議）", "符合規則時自動更新"}); update->setCurrentIndex(config.automatic ? 1 : 0); policy->addWidget(update);
    policy->addWidget(label("自動模式只處理明確映射、穩定候選；手動套用後暫停自動覆寫。"));
    auto *multi = new QComboBox; multi->addItems({"前景遊戲優先", "每次由我選擇", "自訂優先順序（數字較大優先）"}); multi->setCurrentIndex(config.multiGame); policy->addWidget(multi);
    auto *rules = button("管理遊戲映射與優先順序"); policy->addWidget(rules); connect(rules, &QPushButton::clicked, this, &Dock::rulesDialog);
    auto *notify = card(layout, "通知"); auto *notifications = new QCheckBox("OBS Dock 內通知"); notifications->setChecked(config.notifications); notify->addWidget(notifications);
    auto *windows = new QCheckBox("Windows 通知（此版本不支援）"); windows->setEnabled(false); notify->addWidget(windows);
    auto *live = new QCheckBox("僅在本機 OBS 串流期間顯示自動建議"); live->setChecked(config.liveOnly); notify->addWidget(live);
    auto *advanced = card(layout, "進階與帳號"); auto *pause = new QCheckBox("暫停自動分類建議"); pause->setChecked(config.paused); advanced->addWidget(pause);
    advanced->addWidget(label("Twitch Public Client ID（不是 Client Secret）")); auto *id = new QLineEdit(config.clientId); id->setMaxLength(128); advanced->addWidget(id);
    auto *logout = button("登出並嘗試撤銷授權"); advanced->addWidget(logout); connect(logout, &QPushButton::clicked, &client, &Client::logout);
    auto *clear = button("清除本機資料…"); advanced->addWidget(clear);
    connect(clear, &QPushButton::clicked, &dialog, [&] {
        if (QMessageBox::question(&dialog, "清除本機資料", "刪除偏好、遊戲映射、同意紀錄、本次活動與目前帳號憑證，並嘗試撤銷 Twitch 授權？", QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
        client.logout(); config = {}; ignored.clear(); detected.clear(); manualOverride = false; stableKey.clear(); autoAttemptKey.clear();
        activityList->clear(); artCache.clear(); save(); theme(); updateAccount(); dialog.reject();
    });
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel); outer->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        const auto newId = id->text().trimmed();
        if (!QRegularExpression("^[a-zA-Z0-9]{0,128}$").match(newId).hasMatch()) { QMessageBox::warning(&dialog, "Client ID", "Client ID 只可包含英數字"); return; }
        if (newId != config.clientId) client.logout();
        config.clientId = newId; config.theme = themeBox->currentIndex(); config.automatic = update->currentIndex() == 1;
        config.multiGame = multi->currentIndex(); config.notifications = notifications->isChecked(); config.liveOnly = live->isChecked(); config.paused = pause->isChecked();
        stableKey.clear(); autoAttemptKey.clear(); save(); theme(); scan(); dialog.accept();
    });
    dialog.exec();
}
void Dock::rulesDialog()
{
    QDialog dialog(this); dialog.setWindowTitle("遊戲映射規則"); dialog.setStyleSheet(themeSheet(config.theme)); dialog.resize(650, 420);
    auto *layout = new QVBoxLayout(&dialog); layout->addWidget(label("在主畫面搜尋真實分類，使用「將選取分類加入遊戲規則」新增。此處可編輯或刪除；優先值越大越優先。"));
    auto *table = new QTableWidget(static_cast<int>(config.rules.size()), 4); table->setHorizontalHeaderLabels({"程序 .exe", "分類 ID", "分類名稱", "優先值"}); layout->addWidget(table);
    for (int row = 0; row < config.rules.size(); ++row) {
        const auto &r = config.rules[row]; const QStringList values{r.executable, r.categoryId, r.categoryName, QString::number(r.priority)};
        for (int col = 0; col < 4; ++col) { auto *item = new QTableWidgetItem(values[col]); if (col == 1 || col == 2) item->setFlags(item->flags() & ~Qt::ItemIsEditable); table->setItem(row, col, item); }
    }
    auto *remove = button("刪除選取規則"); layout->addWidget(remove); connect(remove, &QPushButton::clicked, &dialog, [table] { if (table->currentRow() >= 0) table->removeRow(table->currentRow()); });
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel); layout->addWidget(buttons); connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
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
    QWizard wizard(this); wizard.setWindowTitle("Twitch 分類助手 · 首次設定"); wizard.setStyleSheet(themeSheet(config.theme)); wizard.resize(540, 470);
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
    auto *authorize = page("Twitch OAuth 登入", "輸入在 Twitch Developer Console 建立的 Public Client ID。授權會在系統瀏覽器進行。可稍後登入。");
    auto *id = new QLineEdit(config.clientId); id->setPlaceholderText("Public Client ID"); authorize->layout()->addWidget(id);
    auto *signIn = button("同意並繼續使用 Twitch 登入"); authorize->layout()->addWidget(signIn); signIn->setEnabled(false);
    auto enableConsent = [=] { signIn->setEnabled(agreeTerms->isChecked() && agreePrivacy->isChecked()); };
    connect(agreeTerms, &QCheckBox::toggled, &wizard, enableConsent); connect(agreePrivacy, &QCheckBox::toggled, &wizard, enableConsent);
    connect(signIn, &QPushButton::clicked, &wizard, [&] {
        if (!agreeTerms->isChecked() || !agreePrivacy->isChecked()) return;
        const auto newId = id->text().trimmed();
        if (!QRegularExpression("^[a-zA-Z0-9]{1,128}$").match(newId).hasMatch()) { QMessageBox::warning(&wizard, "Client ID", "請輸入有效的 Public Client ID"); return; }
        if (config.clientId != newId) client.logout(); config.clientId = newId;
        config.termsVersion = config.privacyVersion = DocumentVersion;
        config.termsUtc = config.privacyUtc = QDateTime::currentDateTimeUtc().toString(Qt::ISODate); save(); login();
    });
    auto *policy = page("分類更新策略", "預設每次詢問。自動模式只套用明確且穩定的映射，手動修改會暫停自動覆寫。");
    auto *autoUpdate = new QCheckBox("符合規則時自動更新"); autoUpdate->setChecked(config.automatic); policy->layout()->addWidget(autoUpdate);
    auto *notice = page("通知偏好", "通知顯示於 OBS Dock。Windows 系統通知在此版本不支援。");
    auto *notify = new QCheckBox("OBS Dock 內通知"); notify->setChecked(config.notifications); notice->layout()->addWidget(notify);
    auto *live = new QCheckBox("僅在 OBS 串流時提出自動建議"); live->setChecked(config.liveOnly); notice->layout()->addWidget(live);
    page("完成設定", "完成後可搜尋分類，並建立程序映射。你可以隨時在設定修改偏好或重新開啟精靈。");
    if (wizard.exec() == QDialog::Accepted) {
        if (!agreeTerms->isChecked() || !agreePrivacy->isChecked()) { addActivity("尚未同意兩份測試文件，設定精靈未完成"); return; }
        config.termsVersion = config.privacyVersion = DocumentVersion;
        config.termsUtc = config.privacyUtc = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        config.automatic = autoUpdate->isChecked(); config.notifications = notify->isChecked(); config.liveOnly = live->isChecked(); config.wizardDone = true; save(); updateAccount();
    }
}
}

