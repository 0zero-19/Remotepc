#pragma once
// =============================================================================
// ClassroomMonitor — Student Tile (Grid Item)
//
// Виджет плитки экрана студента для сетки (соответствует .grid-client в HTML).
// Превью экрана со скруглением и плавающим бейджем имени внизу слева.
// =============================================================================

#include <QWidget>
#include <QLabel>
#include <QPixmap>
#include <cstdint>

namespace cm {
namespace server {

class StudentTile : public QWidget {
    Q_OBJECT

public:
    explicit StudentTile(uint32_t clientId, QWidget* parent = nullptr);
    ~StudentTile() override = default;

    void updateFrame(const uint8_t* frameData, size_t size, uint16_t width, uint16_t height);
    void setStudentName(const QString& name);
    QString studentName() const { return m_studentName; }

    void setOnline(bool online);
    void setSelected(bool selected);

    uint32_t clientId() const { return m_clientId; }
    const QPixmap& currentPixmap() const { return m_currentFrame; }

signals:
    void clicked(uint32_t clientId);
    void doubleClicked(uint32_t clientId);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void setupUi();

    uint32_t m_clientId;
    QString  m_studentName;
    QLabel*  m_screenLabel = nullptr;
    QLabel*  m_nameLabel   = nullptr;
    QPixmap  m_currentFrame;
    bool     m_online = true;
};

} // namespace server
} // namespace cm
