// =============================================================================
// ClassroomMonitor — Student Tile Implementation
// =============================================================================

#include "server/StudentTile.h"

#include <QVBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QStyleOption>

namespace cm {
namespace server {

StudentTile::StudentTile(uint32_t clientId, QWidget* parent)
    : QWidget(parent)
    , m_clientId(clientId)
{
    setupUi();
    setFixedSize(320, 220);
    setCursor(Qt::PointingHandCursor);
}

StudentTile::~StudentTile() = default;

void StudentTile::setupUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(4);
    layout->setContentsMargins(4, 4, 4, 4);

    // Превью экрана
    m_screenLabel = new QLabel(this);
    m_screenLabel->setMinimumSize(312, 175);
    m_screenLabel->setAlignment(Qt::AlignCenter);
    m_screenLabel->setStyleSheet(
        "background-color: #1a1a2e; "
        "border-radius: 6px; "
        "color: #666;"
    );
    m_screenLabel->setText("Ожидание...");
    layout->addWidget(m_screenLabel, 1);

    // Нижняя панель: имя + статус
    auto* bottomLayout = new QHBoxLayout();

    m_nameLabel = new QLabel(QString("Студент #%1").arg(m_clientId), this);
    m_nameLabel->setStyleSheet("font-weight: bold; color: #e0e0e0; font-size: 12px;");
    bottomLayout->addWidget(m_nameLabel);

    bottomLayout->addStretch();

    m_statusLabel = new QLabel("●", this);
    m_statusLabel->setStyleSheet("color: #4caf50; font-size: 16px;"); // Зелёный = online
    bottomLayout->addWidget(m_statusLabel);

    layout->addLayout(bottomLayout);

    // Стиль карточки
    setStyleSheet(
        "StudentTile {"
        "  background-color: #16213e;"
        "  border: 1px solid #0f3460;"
        "  border-radius: 8px;"
        "}"
        "StudentTile:hover {"
        "  border-color: #e94560;"
        "  background-color: #1a1a3e;"
        "}"
    );
}

void StudentTile::updateFrame(const uint8_t* frameData, size_t size,
                               uint16_t width, uint16_t height) {
    Q_UNUSED(frameData);
    m_screenLabel->setText(QString("📺 %1x%2\n[Поток активен (%3 КБ)]")
        .arg(width).arg(height).arg(size / 1024));
    m_screenLabel->setStyleSheet(
        "background-color: #0d1b2a; "
        "border-radius: 6px; "
        "color: #00e676; "
        "font-weight: bold;"
    );
}

void StudentTile::setStudentName(const QString& name) {
    m_nameLabel->setText(name);
}

void StudentTile::setOnline(bool online) {
    m_online = online;
    if (online) {
        m_statusLabel->setStyleSheet("color: #4caf50; font-size: 16px;"); // Зелёный
        m_statusLabel->setToolTip("Online");
    } else {
        m_statusLabel->setStyleSheet("color: #f44336; font-size: 16px;"); // Красный
        m_statusLabel->setToolTip("Offline");
    }
}

void StudentTile::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        emit clicked(m_clientId);
    }
    QWidget::mousePressEvent(event);
}

void StudentTile::paintEvent(QPaintEvent* event) {
    // Позволяем стилизацию через stylesheets для QWidget
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
    QWidget::paintEvent(event);
}

} // namespace server
} // namespace cm
