#include "ProfileNotification.h"

#include "AppIcon.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QScreen>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

#include <dwmapi.h>

namespace {

// Same duration as the messages of the tray icon
constexpr int CloseDelay = 10000;
// Distance to the screen edges and the taskbar
constexpr int Margin = 12;

} // namespace

ProfileNotification::ProfileNotification(const QString &title, const QString &text, const QStringList &profiles,
                                         const QString &current, QWidget *parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(title);

    auto *icon = new QLabel;
    const int iconSize = style()->pixelMetric(QStyle::PM_LargeIconSize);
    icon->setPixmap(appIcon().pixmap(iconSize, iconSize));
    icon->setAlignment(Qt::AlignTop);

    auto *titleLabel = new QLabel(title);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);

    auto *closeButton = new QToolButton;
    closeButton->setAutoRaise(true);
    closeButton->setText(QStringLiteral("✕"));
    closeButton->setToolTip(tr("Close"));
    connect(closeButton, &QToolButton::clicked, this, &QWidget::close);

    auto *textLabel = new QLabel(text);
    textLabel->setWordWrap(true);

    // Nothing selected at first, so that choosing the first profile also counts
    m_profiles = new QComboBox;
    m_profiles->addItems(profiles);
    m_profiles->setPlaceholderText(tr("Choose a profile…"));
    m_profiles->setCurrentIndex(-1);
    m_profiles->setMinimumContentsLength(20);
    const int active = profiles.indexOf(current);
    if (active >= 0)
        m_profiles->setItemData(active, tr("%1 (active)").arg(current), Qt::DisplayRole);
    connect(m_profiles, &QComboBox::activated, this, [this, profiles](int index) {
        if (index < 0 || index >= profiles.size())
            return;
        emit profileSelected(profiles.at(index));
        close();
    });

    auto *header = new QHBoxLayout;
    header->addWidget(titleLabel, 1);
    header->addWidget(closeButton);

    auto *content = new QVBoxLayout;
    content->addLayout(header);
    content->addWidget(textLabel);
    content->addWidget(m_profiles);

    auto *layout = new QHBoxLayout(this);
    layout->addWidget(icon);
    layout->addLayout(content, 1);

    m_closeTimer.setSingleShot(true);
    m_closeTimer.setInterval(CloseDelay);
    connect(&m_closeTimer, &QTimer::timeout, this, [this] {
        // Not while the list of the combo box is open
        if (m_profiles->view()->isVisible())
            m_closeTimer.start();
        else
            close();
    });
}

void ProfileNotification::popup()
{
    adjustSize();
    setFixedWidth(qMax(width(), 360));
    adjustSize();

    // Rounded corners and the frame of Windows 11 notifications
    const HWND hwnd = reinterpret_cast<HWND>(winId());
    const DWORD corners = 2; // DWMWCP_ROUND
    DwmSetWindowAttribute(hwnd, 33 /* DWMWA_WINDOW_CORNER_PREFERENCE */, &corners, sizeof(corners));

    const QRect available = QGuiApplication::primaryScreen()->availableGeometry();
    move(available.right() - width() - Margin + 1, available.bottom() - height() - Margin + 1);
    show();
    raise();
    m_closeTimer.start();
}

void ProfileNotification::enterEvent(QEnterEvent *event)
{
    m_closeTimer.stop();
    QWidget::enterEvent(event);
}

void ProfileNotification::leaveEvent(QEvent *event)
{
    m_closeTimer.start();
    QWidget::leaveEvent(event);
}
