#pragma once
// =============================================================================
// ClassroomMonitor — Client Sidebar
//
// Боковая панель списка учеников по HTML-макету.
// Содержит: кнопки "Все"/"Снять", список клиентов с чекбоксами, именем, IP и точкой статуса.
// =============================================================================

#include <QWidget>
#include <QFrame>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QMap>
#include <cstdint>

class QCheckBox;
class QLabel;

namespace cm {
namespace server {

class ClientItemWidget : public QFrame {
    Q_OBJECT

public:
    ClientItemWidget(uint32_t clientId, const QString& name, const QString& ip, QWidget* parent = nullptr);

    uint32_t clientId() const { return m_clientId; }
    bool isChecked() const;
    void setChecked(bool checked);

    void setName(const QString& name);
    void setIp(const QString& ip);
    void setOnline(bool online);
    void setActive(bool active);

signals:
    void clicked(uint32_t clientId);
    void doubleClicked(uint32_t clientId);
    void checkChanged(uint32_t clientId, bool checked);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    uint32_t   m_clientId;
    QCheckBox* m_checkbox = nullptr;
    QLabel*    m_nameLabel = nullptr;
    QLabel*    m_metaLabel = nullptr;
    QLabel*    m_statusDot = nullptr;
};

class ClientSidebar : public QWidget {
    Q_OBJECT

public:
    explicit ClientSidebar(QWidget* parent = nullptr);
    ~ClientSidebar() override = default;

    void addClient(uint32_t clientId, const QString& name, const QString& ip);
    void removeClient(uint32_t clientId);
    void updateClient(uint32_t clientId, const QString& name, const QString& ip, bool online);
    void setActiveClient(uint32_t clientId);

    QList<uint32_t> checkedClientIds() const;
    uint32_t activeClientId() const { return m_activeClientId; }

    int clientCount() const { return m_items.size(); }

signals:
    void clientSelected(uint32_t clientId);
    void clientDoubleClicked(uint32_t clientId);
    void selectionChanged();

public slots:
    void selectAll();
    void clearSelection();

private:
    void setupUi();

    QVBoxLayout*                     m_listLayout = nullptr;
    QMap<uint32_t, ClientItemWidget*> m_items;
    uint32_t                         m_activeClientId = 0;
};

} // namespace server
} // namespace cm
