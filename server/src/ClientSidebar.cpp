// =============================================================================
// ClassroomMonitor — Client Sidebar Implementation
// =============================================================================

#include "server/ClientSidebar.h"
#include "server/ThemeManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QScrollArea>
#include <QMouseEvent>
#include <QStyle>

namespace cm {
namespace server {

// =============================================================================
// ClientItemWidget
// =============================================================================

ClientItemWidget::ClientItemWidget(uint32_t clientId, const QString& name, const QString& ip, QWidget* parent)
    : QFrame(parent)
    , m_clientId(clientId)
{
    setObjectName("clientItem");
    setProperty("class", "clientItem");
    setCursor(Qt::PointingHandCursor);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(8);

    m_checkbox = new QCheckBox(this);
    m_checkbox->setChecked(true);
    connect(m_checkbox, &QCheckBox::toggled, this, [this](bool checked) {
        emit checkChanged(m_clientId, checked);
    });
    layout->addWidget(m_checkbox);

    auto* infoCol = new QVBoxLayout();
    infoCol->setSpacing(2);

    m_nameLabel = new QLabel(name, this);
    m_nameLabel->setStyleSheet("font-size: 13px; font-weight: 500;");
    infoCol->addWidget(m_nameLabel);

    m_metaLabel = new QLabel(ip.isEmpty() ? "127.0.0.1" : ip, this);
    m_metaLabel->setStyleSheet("font-size: 10px; color: " + ThemeManager::instance().currentTheme().textMuted + ";");
    infoCol->addWidget(m_metaLabel);

    layout->addLayout(infoCol, 1);

    m_statusDot = new QLabel(this);
    m_statusDot->setFixedSize(8, 8);
    setOnline(true);
    layout->addWidget(m_statusDot);
}

bool ClientItemWidget::isChecked() const {
    return m_checkbox->isChecked();
}

void ClientItemWidget::setChecked(bool checked) {
    m_checkbox->setChecked(checked);
}

void ClientItemWidget::setName(const QString& name) {
    m_nameLabel->setText(name);
}

void ClientItemWidget::setIp(const QString& ip) {
    m_metaLabel->setText(ip);
}

void ClientItemWidget::setOnline(bool online) {
    QString col = online ? ThemeManager::instance().currentTheme().greenHover
                         : ThemeManager::instance().currentTheme().redHover;
    m_statusDot->setStyleSheet(QString("background-color: %1; border-radius: 4px;").arg(col));
}

void ClientItemWidget::setActive(bool active) {
    setProperty("active", active);
    style()->unpolish(this);
    style()->polish(this);
}

void ClientItemWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        emit clicked(m_clientId);
    }
    QFrame::mousePressEvent(event);
}

void ClientItemWidget::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        emit doubleClicked(m_clientId);
    }
    QFrame::mouseDoubleClickEvent(event);
}

// =============================================================================
// ClientSidebar
// =============================================================================

ClientSidebar::ClientSidebar(QWidget* parent)
    : QWidget(parent)
{
    setFixedWidth(245);
    setupUi();
}

void ClientSidebar::setupUi() {
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    auto* panel = new QFrame(this);
    panel->setObjectName("clientsPanel");
    auto* panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(0, 0, 0, 0);
    panelLayout->setSpacing(0);

    // Верхняя часть: кнопки "Все" / "Снять"
    auto* topWidget = new QWidget(panel);
    topWidget->setObjectName("clientsTop");
    auto* topLayout = new QHBoxLayout(topWidget);
    topLayout->setContentsMargins(10, 10, 10, 10);
    topLayout->setSpacing(8);

    auto* selectAllBtn = new QPushButton("Все", topWidget);
    selectAllBtn->setProperty("class", "smallButton");
    selectAllBtn->setCursor(Qt::PointingHandCursor);
    connect(selectAllBtn, &QPushButton::clicked, this, &ClientSidebar::selectAll);
    topLayout->addWidget(selectAllBtn);

    auto* clearBtn = new QPushButton("Снять", topWidget);
    clearBtn->setProperty("class", "smallButton");
    clearBtn->setCursor(Qt::PointingHandCursor);
    connect(clearBtn, &QPushButton::clicked, this, &ClientSidebar::clearSelection);
    topLayout->addWidget(clearBtn);

    panelLayout->addWidget(topWidget);

    // Скроллируемый список клиентов
    auto* scrollArea = new QScrollArea(panel);
    scrollArea->setObjectName("clientsScroll");
    scrollArea->setWidgetResizable(true);

    auto* container = new QWidget();
    container->setStyleSheet("background: transparent;");
    m_listLayout = new QVBoxLayout(container);
    m_listLayout->setContentsMargins(6, 6, 6, 6);
    m_listLayout->setSpacing(4);
    m_listLayout->addStretch();

    scrollArea->setWidget(container);
    panelLayout->addWidget(scrollArea, 1);

    outerLayout->addWidget(panel);
}

void ClientSidebar::addClient(uint32_t clientId, const QString& name, const QString& ip) {
    if (m_items.contains(clientId)) return;

    auto* item = new ClientItemWidget(clientId, name, ip, this);
    connect(item, &ClientItemWidget::clicked, this, [this](uint32_t id) {
        setActiveClient(id);
        emit clientSelected(id);
    });
    connect(item, &ClientItemWidget::doubleClicked, this, &ClientSidebar::clientDoubleClicked);
    connect(item, &ClientItemWidget::checkChanged, this, [this](uint32_t, bool) {
        emit selectionChanged();
    });

    m_items.insert(clientId, item);
    // Вставляем перед растяжкой (последний элемент layout)
    m_listLayout->insertWidget(m_listLayout->count() - 1, item);

    if (m_activeClientId == 0) {
        setActiveClient(clientId);
    }
}

void ClientSidebar::removeClient(uint32_t clientId) {
    if (auto* item = m_items.take(clientId)) {
        m_listLayout->removeWidget(item);
        item->deleteLater();
    }
    if (m_activeClientId == clientId) {
        m_activeClientId = m_items.isEmpty() ? 0 : m_items.begin().key();
        if (m_activeClientId != 0) {
            setActiveClient(m_activeClientId);
            emit clientSelected(m_activeClientId);
        }
    }
}

void ClientSidebar::updateClient(uint32_t clientId, const QString& name, const QString& ip, bool online) {
    if (auto* item = m_items.value(clientId, nullptr)) {
        if (!name.isEmpty()) item->setName(name);
        if (!ip.isEmpty()) item->setIp(ip);
        item->setOnline(online);
    }
}

void ClientSidebar::setActiveClient(uint32_t clientId) {
    m_activeClientId = clientId;
    for (auto it = m_items.begin(); it != m_items.end(); ++it) {
        it.value()->setActive(it.key() == clientId);
    }
}

QList<uint32_t> ClientSidebar::checkedClientIds() const {
    QList<uint32_t> res;
    for (auto it = m_items.begin(); it != m_items.end(); ++it) {
        if (it.value()->isChecked()) {
            res.append(it.key());
        }
    }
    return res;
}

void ClientSidebar::selectAll() {
    for (auto* item : m_items) {
        item->setChecked(true);
    }
    emit selectionChanged();
}

void ClientSidebar::clearSelection() {
    for (auto* item : m_items) {
        item->setChecked(false);
    }
    emit selectionChanged();
}

} // namespace server
} // namespace cm
