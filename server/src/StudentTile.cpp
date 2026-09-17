// =============================================================================
// ClassroomMonitor — Student Tile Implementation (Minimalist Grid Item)
// =============================================================================

#include "server/StudentTile.h"
#include "server/ThemeManager.h"

#include <QVBoxLayout>
#include <QMouseEvent>

namespace cm {
namespace server {

StudentTile::StudentTile(uint32_t clientId, QWidget* parent)
    : QWidget(parent)
    , m_clientId(clientId)
    , m_studentName(QString("PC-%1").arg(clientId, 2, 10, QChar('0')))
{
    setupUi();
    setCursor(Qt::PointingHandCursor);
}

void StudentTile::setupUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_screenLabel = new QLabel(this);
    m_screenLabel->setAlignment(Qt::AlignCenter);
    m_screenLabel->setStyleSheet("background-color: #292725; color: #a89984; font-size: 11px;");
    m_screenLabel->setText(QString("Экран %1").arg(m_studentName));
    layout->addWidget(m_screenLabel);

    // Плавающий бейдж имени в левом нижнем углу
    m_nameLabel = new QLabel(m_studentName, this);
    m_nameLabel->setStyleSheet(
        "background-color: rgba(40, 40, 40, 0.9);"
        "border: 1px solid #504945;"
        "border-radius: 4px;"
        "padding: 4px 7px;"
        "font-size: 11px;"
        "color: #ebdbb2;"
    );
    m_nameLabel->move(7, height() - 30);

    setSelected(false);
}

void StudentTile::resizeEvent(QResizeEvent* /*event*/) {
    if (m_nameLabel) {
        m_nameLabel->adjustSize();
        m_nameLabel->move(7, height() - m_nameLabel->height() - 7);
    }
}

void StudentTile::updateFrame(const uint8_t* frameData, size_t size,
                               uint16_t width, uint16_t height) {
    if (!frameData || size == 0) return;

    QImage img;
    if (img.loadFromData(frameData, static_cast<int>(size), "JPEG") ||
        img.loadFromData(frameData, static_cast<int>(size))) {
        QPixmap pixmap = QPixmap::fromImage(std::move(img)).scaled(
            m_screenLabel->size(),
            Qt::KeepAspectRatio,
            Qt::FastTransformation
        );
        m_screenLabel->setPixmap(pixmap);
        m_screenLabel->setText("");
    } else {
        m_screenLabel->setText(QString("📺 %1x%2").arg(width).arg(height));
    }
}

void StudentTile::setStudentName(const QString& name) {
    m_studentName = name;
    m_nameLabel->setText(name);
    m_nameLabel->adjustSize();
    m_nameLabel->move(7, height() - m_nameLabel->height() - 7);
}

void StudentTile::setOnline(bool online) {
    m_online = online;
}

void StudentTile::setSelected(bool selected) {
    const auto& theme = ThemeManager::instance().currentTheme();
    QString borderCol = selected ? theme.yellow : theme.border;
    setStyleSheet(QString(
        "StudentTile {"
        "  background-color: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 5px;"
        "}"
    ).arg(theme.surface).arg(borderCol));
}

void StudentTile::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        emit clicked(m_clientId);
    }
    QWidget::mousePressEvent(event);
}

void StudentTile::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        emit doubleClicked(m_clientId);
    }
    QWidget::mouseDoubleClickEvent(event);
}

} // namespace server
} // namespace cm
