// PowerShow — 悬浮蓝牙电量卡片（Qt Widgets 单进程自绘版）
#include <QApplication>
#include <QWidget>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <QMenu>
#include <QSystemTrayIcon>
#include <QProcess>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCursor>
#include <QMouseEvent>
#include <QSlider>
#include <QIcon>
#include <QDate>
#include <cmath>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

// ---------------------------------------------------------------- 数据采集

struct Device {
    QString name;
    int level = 0;
    QString kind = "device";
};

static QString classify(const QString &name)
{
    const QString n = name.toLower();
    if (n.contains("keyboard") || name.contains(QString::fromUtf8("键盘"))) return "keyboard";
    if (n.contains("mouse") || name.contains(QString::fromUtf8("鼠标")) || n.contains("trackpad")) return "mouse";
    if (n.contains("headset") || n.contains("headphone") || n.contains("earbud")
        || n.contains("freebuds") || name.contains(QString::fromUtf8("耳机"))) return "headset";
    return "device";
}

#ifdef Q_OS_LINUX
static QList<Device> collectDevices(QString *error)
{
    QProcess p;
    p.start("upower", {"--dump"});
    if (!p.waitForFinished(5000)) { *error = "upower 执行超时"; return {}; }
    const QString dump = QString::fromUtf8(p.readAllStandardOutput());

    QList<Device> list;
    Device cur;
    bool inDevice = false, hasLevel = false;

    const auto flush = [&]() {
        if (inDevice && hasLevel) {
            if (cur.name.isEmpty()) cur.name = QString::fromUtf8("蓝牙设备");
            list.append(cur);
        }
        inDevice = false;
        hasLevel = false;
        cur = Device();
    };

    for (const QString &raw : dump.split('\n')) {
        const QString line = raw.trimmed();
        const int idx = line.indexOf(':');
        if (idx < 0) continue;
        const QString key = line.left(idx).trimmed();
        const QString val = line.mid(idx + 1).trimmed();

        if (key == "Device") { flush(); continue; }
        if (key == "native-path") {
            if (val.contains("/org/bluez/")) {
                inDevice = true;
                cur.kind = "device";
            }
            continue;
        }
        if (!inDevice) continue;
        if (key == "model") { cur.name = val; cur.kind = classify(val); }
        else if (key == "percentage") {
            bool ok = false;
            const double v = QString(val).remove('%').trimmed().toDouble(&ok);
            if (ok) { cur.level = int(std::lround(v)); hasLevel = true; }
        }
    }
    flush();
    if (list.isEmpty()) *error = QString::fromUtf8("未发现上报电量的蓝牙外设");
    return list;
}
#else
static QList<Device> collectDevices(QString *error)
{
    QProcess p;
    p.start("powershell.exe", {"-NoProfile", "-NonInteractive", "-Command",
        "Get-CimInstance Win32_Battery | Select-Object Name, EstimatedChargeRemaining | ConvertTo-Json -Compress"});
    if (!p.waitForFinished(15000)) { *error = "PowerShell 执行超时"; return {}; }
    const QByteArray out = p.readAllStandardOutput().trimmed();
    QList<Device> list;
    if (out.isEmpty()) { *error = QString::fromUtf8("未发现上报电量的蓝牙外设"); return list; }

    QJsonDocument doc = QJsonDocument::fromJson(out);
    QJsonArray arr;
    if (doc.isArray()) arr = doc.array();
    else if (doc.isObject()) arr.append(doc.object());
    for (const QJsonValue &v : arr) {
        const QJsonObject o = v.toObject();
        if (!o.contains("EstimatedChargeRemaining")) continue;
        Device d;
        d.name = o["Name"].toString("蓝牙设备");
        d.level = o["EstimatedChargeRemaining"].toInt();
        d.kind = classify(d.name);
        list.append(d);
    }
    if (list.isEmpty()) *error = QString::fromUtf8("未发现上报电量的蓝牙外设");
    return list;
}
#endif

// ---------------------------------------------------------------- 主题

struct Theme {
    QColor bg;          // 背景底色（alpha 由设置控制）
    QColor border;
    QColor text;
    QColor textDim;
    QColor shell;       // 电池外壳描边
    QColor shellFill;
    QColor green, yellow, red;
};

static Theme currentTheme()
{
    const bool dark = QGuiApplication::palette().color(QPalette::Window).lightness() < 128;
    Theme t;
    if (dark) {
        t.bg = QColor(28, 28, 32);
        t.border = QColor(255, 255, 255, 26);
        t.text = QColor(242, 242, 244);
        t.textDim = QColor(242, 242, 244, 107);
        t.shell = QColor(242, 242, 244, 102);
        t.shellFill = QColor(242, 242, 244, 20);
        t.green = QColor(0x30, 0xd1, 0x58);
        t.yellow = QColor(0xff, 0xd6, 0x0a);
        t.red = QColor(0xff, 0x45, 0x3a);
    } else {
        t.bg = QColor(246, 246, 248);
        t.border = QColor(255, 255, 255, 140);
        t.text = QColor(29, 29, 31);
        t.textDim = QColor(29, 29, 31, 107);
        t.shell = QColor(29, 29, 31, 72);
        t.shellFill = QColor(29, 29, 31, 15);
        t.green = QColor(0x00, 0xc0, 0x30);
        t.yellow = QColor(0xf5, 0xb8, 0x00);
        t.red = QColor(0xff, 0x3b, 0x30);
    }
    return t;
}

// ---------------------------------------------------------------- 卡片窗口

class BatteryCard : public QWidget {
    Q_OBJECT
public:
    explicit BatteryCard(QWidget *parent = nullptr);

public slots:
    void refresh();

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void leaveEvent(QEvent *) override;

private:
    enum class Hit { None, Refresh, Gear, Settings };

    QList<Device> m_devices;
    QString m_error;
    double m_bgAlpha = 0.78;
    bool m_settingsOpen = false;
    int m_hover = -1;             // 0 refresh, 1 gear
    double m_spinAngle = 0;
    bool m_spinning = false;
    QPoint m_dragOffset;
    bool m_dragging = false;
    QSlider *m_slider = nullptr;

    QRect refreshRect() const;
    QRect gearRect() const;
    QRect settingsRect() const;
    QRect rowRect(int i) const;
    QRect cardRect() const;

    void loadSettings();
    void saveSettings();
    void toggleSettings();
    void layoutForDevices();
    void drawCapsule(QPainter &p, const QRect &r, int level, const Theme &t) const;
    void drawGear(QPainter &p, const QRect &r, const Theme &t, bool hover) const;
    void drawRefresh(QPainter &p, const QRect &r, const Theme &t, bool hover) const;
};

static const int PAD_X = 20, PAD_TOP = 16, PAD_BOTTOM = 18;
static const int HEADER_H = 22, HEADER_GAP = 12, ROW_H = 38;
static const int CARD_W = 300;

BatteryCard::BatteryCard(QWidget *parent) : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setMouseTracking(true);
    setWindowTitle("PowerShow");

    m_slider = new QSlider(Qt::Horizontal, this);
    m_slider->setRange(30, 100);
    m_slider->setSingleStep(5);
    m_slider->setValue(int(m_bgAlpha * 100));
    m_slider->hide();
    connect(m_slider, &QSlider::valueChanged, this, [this](int v) {
        m_bgAlpha = v / 100.0;
        saveSettings();
        update();
    });

    loadSettings();
    layoutForDevices();
    refresh();

    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &BatteryCard::refresh);
    timer->start(60 * 1000);
}

void BatteryCard::loadSettings()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QFile f(dir + "/state.json");
    if (f.open(QIODevice::ReadOnly)) {
        const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
        if (o.contains("bgAlpha")) m_bgAlpha = o["bgAlpha"].toDouble();
    }
    if (m_slider) m_slider->setValue(int(m_bgAlpha * 100));
}

void BatteryCard::saveSettings()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(dir);
    QFile f(dir + "/state.json");
    if (f.open(QIODevice::WriteOnly)) {
        QJsonObject o{{"bgAlpha", m_bgAlpha}};
        f.write(QJsonDocument(o).toJson());
    }
}

void BatteryCard::layoutForDevices()
{
    const int rows = int(m_devices.size());
    int h = PAD_TOP + HEADER_H + HEADER_GAP + rows * ROW_H + PAD_BOTTOM;
    h = qMax(h, 96);
    resize(CARD_W, h);
    if (m_slider) {
        const QRect sr = settingsRect();
        m_slider->setGeometry(sr.left() + 78, sr.top() + 10, sr.width() - 78 - 16, 20);
    }
}

void BatteryCard::refresh()
{
    QString err;
    const QList<Device> list = collectDevices(&err);
    if (!err.isEmpty() && list.isEmpty()) {
        m_error = err;
        m_devices.clear();
    } else {
        m_error.clear();
        m_devices = list;
    }
    layoutForDevices();
    update();
}

// --- 布局辅助

QRect BatteryCard::cardRect() const { return QRect(0, 0, width(), height()); }

QRect BatteryCard::refreshRect() const
{
    return QRect(width() - PAD_X - 15 - 6 - 15, PAD_TOP + 3, 15, 15);
}

QRect BatteryCard::gearRect() const
{
    return QRect(width() - PAD_X - 15, PAD_TOP + 3, 15, 15);
}

QRect BatteryCard::settingsRect() const
{
    return QRect(PAD_X - 6, PAD_TOP + HEADER_H + 4, width() - 2 * (PAD_X - 6), 40);
}

QRect BatteryCard::rowRect(int i) const
{
    const int y = PAD_TOP + HEADER_H + HEADER_GAP + i * ROW_H;
    return QRect(PAD_X, y, width() - 2 * PAD_X, ROW_H);
}

// --- 绘制

void BatteryCard::drawCapsule(QPainter &p, const QRect &r, int level, const Theme &t) const
{
    QColor fill = level < 15 ? t.red : level < 35 ? t.yellow : t.green;
    const QRect body(r.left(), r.top() + 1, 40, 20);
    const QRect cap(r.left() + 41, r.top() + 8, 3, 8);

    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(t.shell, 1.8));
    p.setBrush(t.shellFill);
    p.drawRoundedRect(body.adjusted(1, 1, -1, -1), 6, 6);
    p.fillRect(cap, t.shell);

    const int innerW = int((40 - 6) * level / 100.0);
    if (innerW > 0) {
        p.setPen(Qt::NoPen);
        p.setBrush(fill);
        p.drawRoundedRect(QRect(body.left() + 3, body.top() + 3, innerW, 14), 3.5, 3.5);
    }
}

void BatteryCard::drawGear(QPainter &p, const QRect &r, const Theme &t, bool hover) const
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    QColor c = hover ? t.text : t.textDim;
    p.setPen(QPen(c, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));

    const QPointF ctr = r.center();
    const double R = r.width() / 2.0;
    const double rOuter = R * 0.94, rInner = R * 0.62;
    const int teeth = 6;
    QPainterPath path;
    for (int i = 0; i < teeth * 2; ++i) {
        const double a0 = M_PI * i / teeth - M_PI / 2;
        const double a1 = M_PI * (i + 1) / teeth - M_PI / 2;
        const double rr0 = (i % 2 == 0) ? rOuter : rInner;
        const double rr1 = (i % 2 == 0) ? rInner : rOuter;
        const QPointF p0(ctr.x() + rr0 * std::cos(a0), ctr.y() + rr0 * std::sin(a0));
        const QPointF p1(ctr.x() + rr1 * std::cos(a0), ctr.y() + rr1 * std::sin(a0));
        const QPointF p2(ctr.x() + rr1 * std::cos(a1), ctr.y() + rr1 * std::sin(a1));
        if (i == 0) path.moveTo(p0);
        path.lineTo(p1);
        path.lineTo(p2);
    }
    path.closeSubpath();
    p.drawPath(path);
    p.drawEllipse(ctr, R * 0.22, R * 0.22);
    p.restore();
}

void BatteryCard::drawRefresh(QPainter &p, const QRect &r, const Theme &t, bool hover) const
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    QColor c = hover ? t.text : t.textDim;
    p.setPen(QPen(c, 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));

    const QPointF ctr = r.center();
    const double R = r.width() * 0.38;
    if (m_spinning) {
        p.translate(ctr);
        p.rotate(m_spinAngle);
        p.translate(-ctr);
    }
    const QRectF arcR(ctr.x() - R, ctr.y() - R, 2 * R, 2 * R);
    p.drawArc(arcR, 60 * 16, 280 * 16);
    // 箭头
    const double endA = 60 * M_PI / 180.0;
    const QPointF tip(ctr.x() + R * std::cos(endA), ctr.y() - R * std::sin(endA));
    QPainterPath arrow;
    arrow.moveTo(tip.x() + 4, tip.y() - 1);
    arrow.lineTo(tip.x() - 1, tip.y() - 4);
    arrow.lineTo(tip.x() - 1, tip.y() + 3);
    arrow.closeSubpath();
    p.fillPath(arrow, c);
    p.restore();
}

void BatteryCard::paintEvent(QPaintEvent *)
{
    const Theme t = currentTheme();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // 卡片背景
    QColor bg = t.bg;
    bg.setAlphaF(m_bgAlpha);
    p.setPen(QPen(t.border, 1));
    p.setBrush(bg);
    p.drawRoundedRect(cardRect().adjusted(1, 1, -1, -1), 20, 20);

    // 标题
    QFont f = font();
    f.setPixelSize(12);
    f.setWeight(QFont::DemiBold);
    p.setFont(f);
    p.setPen(t.text);
    p.drawText(QRect(PAD_X, PAD_TOP, 100, HEADER_H), Qt::AlignLeft | Qt::AlignVCenter,
               QString::fromUtf8("电量"));

    // 按钮
    const bool hoverRefresh = (m_hover == 0);
    const bool hoverGear = (m_hover == 1);
    drawRefresh(p, refreshRect(), t, hoverRefresh);
    drawGear(p, gearRect(), t, hoverGear);

    // 设置面板
    if (m_settingsOpen) {
        const QRect sr = settingsRect();
        QColor panelBg = t.bg;
        panelBg.setAlphaF(qMin(1.0, m_bgAlpha + 0.12));
        p.setPen(QPen(t.border, 1));
        p.setBrush(panelBg);
        p.drawRoundedRect(sr, 12, 12);

        f.setPixelSize(11);
        f.setWeight(QFont::Normal);
        p.setFont(f);
        p.setPen(t.textDim);
        p.drawText(QRect(sr.left() + 12, sr.top(), 60, sr.height()),
                   Qt::AlignLeft | Qt::AlignVCenter, QString::fromUtf8("透明度"));
        p.setPen(t.text);
        p.drawText(QRect(sr.right() - 50, sr.top(), 38, sr.height()),
                   Qt::AlignRight | Qt::AlignVCenter, QString("%1%").arg(int(m_bgAlpha * 100 + 0.5)));
    }

    // 设备行
    if (m_error.size() && m_devices.isEmpty()) {
        f.setPixelSize(12);
        p.setFont(f);
        p.setPen(t.textDim);
        p.drawText(QRect(PAD_X, PAD_TOP + HEADER_H + HEADER_GAP, width() - 2 * PAD_X, 40),
                   Qt::AlignLeft | Qt::AlignVCenter, m_error);
        return;
    }

    for (int i = 0; i < m_devices.size(); ++i) {
        const Device &d = m_devices.at(i);
        const QRect r = rowRect(i);

        f.setPixelSize(13);
        f.setWeight(QFont::Medium);
        p.setFont(f);
        p.setPen(t.text);
        const int textW = r.width() - 44 - 8 - 40 - 10;
        p.drawText(QRect(r.left(), r.top(), textW, r.height()),
                   Qt::AlignLeft | Qt::AlignVCenter, d.name);

        const QRect cap(r.right() - 40 - 10 - 44, r.center().y() - 11, 44, 22);
        drawCapsule(p, cap, d.level, t);

        f.setPixelSize(14);
        f.setWeight(QFont::DemiBold);
        p.setFont(f);
        p.setPen(d.level < 15 ? t.red : t.text);
        p.drawText(QRect(r.right() - 40, r.top(), 40, r.height()),
                   Qt::AlignRight | Qt::AlignVCenter, QString("%1%").arg(d.level));
    }
}

// --- 交互

void BatteryCard::mousePressEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton) return;
    const QPoint pos = e->pos();

    if (refreshRect().adjusted(-4, -4, 4, 4).contains(pos)) {
        m_spinning = true;
        m_spinAngle = 0;
        auto *spin = new QTimer(this);
        connect(spin, &QTimer::timeout, this, [this, spin]() {
            m_spinAngle += 30;
            if (m_spinAngle >= 450) { m_spinning = false; spin->stop(); spin->deleteLater(); }
            update();
        });
        spin->start(16);
        refresh();
        return;
    }
    if (gearRect().adjusted(-4, -4, 4, 4).contains(pos)) {
        toggleSettings();
        return;
    }
    if (m_settingsOpen && settingsRect().contains(pos)) return;

    m_dragging = true;
    m_dragOffset = pos;
}

void BatteryCard::mouseMoveEvent(QMouseEvent *e)
{
    const QPoint pos = e->pos();
    int hover = -1;
    if (refreshRect().adjusted(-4, -4, 4, 4).contains(pos)) hover = 0;
    else if (gearRect().adjusted(-4, -4, 4, 4).contains(pos)) hover = 1;
    if (hover != m_hover) { m_hover = hover; update(); }

    if (m_dragging) {
        move(e->globalPosition().toPoint() - m_dragOffset);
    }
}

void BatteryCard::mouseReleaseEvent(QMouseEvent *)
{
    m_dragging = false;
    // 点击面板外关闭设置
    if (m_settingsOpen && !settingsRect().contains(mapFromGlobal(QCursor::pos()))) {
        toggleSettings();
    }
}

void BatteryCard::leaveEvent(QEvent *)
{
    if (m_hover != -1) { m_hover = -1; update(); }
}

void BatteryCard::toggleSettings()
{
    m_settingsOpen = !m_settingsOpen;
    if (m_slider) {
        m_slider->setVisible(m_settingsOpen);
        if (m_settingsOpen) m_slider->raise();
    }
    update();
}

// ---------------------------------------------------------------- 入口

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("PowerShow");
    QApplication::setOrganizationName("lanshan812");

    // 调试：--dump 打印采集结果后退出
    if (app.arguments().contains("--dump")) {
        QString err;
        const QList<Device> list = collectDevices(&err);
        for (const Device &d : list)
            qInfo().noquote() << d.name << d.level << d.kind;
        if (!err.isEmpty()) qWarning().noquote() << err;
        return list.isEmpty() ? 1 : 0;
    }

    BatteryCard card;
    card.show();

    // 托盘
    QIcon icon = QIcon::fromTheme("battery-good");
#ifdef Q_OS_LINUX
    if (icon.isNull()) icon = QIcon::fromTheme("battery-full-symbolic");
#endif
    auto *tray = new QSystemTrayIcon(icon, &app);
    tray->setToolTip("PowerShow");

    QMenu menu;
    QAction *actRefresh = menu.addAction(QString::fromUtf8("立即刷新"));
    QAction *actToggle = menu.addAction(QString::fromUtf8("显示/隐藏"));
    menu.addSeparator();
    QAction *actQuit = menu.addAction(QString::fromUtf8("退出"));
    tray->setContextMenu(&menu);

    QObject::connect(actRefresh, &QAction::triggered, &card, &BatteryCard::refresh);
    QObject::connect(tray, &QSystemTrayIcon::activated, &app, [&card](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger) {
            card.isVisible() ? card.hide() : card.show();
        }
    });
    QObject::connect(actToggle, &QAction::triggered, &card, [&card]() {
        card.isVisible() ? card.hide() : card.show();
    });
    QObject::connect(actQuit, &QAction::triggered, &app, &QApplication::quit);

    tray->show();

    return app.exec();
}

#include "main.moc"