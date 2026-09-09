#pragma once
// =============================================================================
// ClassroomMonitor — Student Tile
//
// QWidget виджет для отображения экрана одного студента в сетке.
// Показывает: превью экрана, имя ПК, статус (online/offline).
// =============================================================================

#include <QWidget>
#include <QLabel>
#include <QImage>
#include <QPixmap>
#include <cstdint>

namespace cm {
namespace server {

class StudentTile : public QWidget {
    Q_OBJECT

public:
    explicit StudentTile(uint32_t clientId, QWidget* parent = nullptr);
    ~StudentTile() override;

    /// Обновить отображаемый кадр
    /// @param frameData — закодированные H.264 данные (или декодированное изображение)
    /// @param size — размер данных
    /// @param width — ширина кадра
    /// @param height — высота кадра
    void updateFrame(const uint8_t* frameData, size_t size,
                     uint16_t width, uint16_t height);

    /// Установить имя студента/ПК
    void setStudentName(const QString& name);

    /// Установить статус online/offline
    void setOnline(bool online);

    uint32_t clientId() const { return m_clientId; }

signals:
    /// Пользователь кликнул по тайлу
    void clicked(uint32_t clientId);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    void setupUi();

    uint32_t m_clientId;
    QLabel*  m_screenLabel  = nullptr;
    QLabel*  m_nameLabel    = nullptr;
    QLabel*  m_statusLabel  = nullptr;
    QPixmap  m_currentFrame;
    bool     m_online = true;
};

} // namespace server
} // namespace cm
