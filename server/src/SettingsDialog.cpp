// =============================================================================
// ClassroomMonitor — Settings Dialog Implementation
// =============================================================================

#include "server/SettingsDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFrame>

namespace cm {
namespace server {

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, false);
    setFixedWidth(360);
    setupUi();
}

void SettingsDialog::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(14, 14, 14, 14);
    mainLayout->setSpacing(12);

    // Заголовок
    auto* header = new QHBoxLayout();
    auto* title = new QLabel("Настройки", this);
    title->setStyleSheet("font-size: 15px; font-weight: bold;");
    header->addWidget(title);

    header->addStretch();

    auto* closeBtn = new QPushButton("✕", this);
    closeBtn->setFixedSize(28, 28);
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setStyleSheet("border: none; font-size: 14px;");
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    header->addWidget(closeBtn);

    mainLayout->addLayout(header);

    auto* sub = new QLabel("Тема интерфейса", this);
    sub->setStyleSheet("font-size: 12px; margin-bottom: 4px;");
    mainLayout->addWidget(sub);

    // Список опций тем
    auto themes = ThemeManager::instance().allThemes();
    for (const auto& t : themes) {
        auto* item = new QFrame(this);
        item->setCursor(Qt::PointingHandCursor);
        item->setStyleSheet(QString(
            "QFrame {"
            "  background-color: %1;"
            "  border: 1px solid %2;"
            "  border-radius: 5px;"
            "  padding: 6px;"
            "}"
            "QFrame:hover {"
            "  background-color: %3;"
            "}"
        ).arg(ThemeManager::instance().currentTheme().surface2)
         .arg(ThemeManager::instance().currentTheme().border)
         .arg(ThemeManager::instance().currentTheme().surface3));

        auto* itemLayout = new QHBoxLayout(item);
        itemLayout->setContentsMargins(8, 6, 8, 6);
        itemLayout->setSpacing(10);

        // Квадрат превью цвета
        auto* preview = new QLabel(item);
        preview->setFixedSize(28, 28);
        preview->setStyleSheet(QString("background-color: %1; border-radius: 4px; border: 1px solid rgba(255,255,255,0.15);").arg(t.bg));
        itemLayout->addWidget(preview);

        // Инфо
        auto* infoCol = new QVBoxLayout();
        infoCol->setSpacing(2);
        auto* nameLbl = new QLabel(t.displayName, item);
        nameLbl->setStyleSheet("font-size: 13px; font-weight: 500;");
        auto* descLbl = new QLabel(t.description, item);
        descLbl->setStyleSheet("font-size: 10px; color: " + ThemeManager::instance().currentTheme().textMuted + ";");
        infoCol->addWidget(nameLbl);
        infoCol->addWidget(descLbl);
        itemLayout->addLayout(infoCol, 1);

        // Клик по теме
        auto* clickableBtn = new QPushButton(item);
        clickableBtn->setStyleSheet("background: transparent; border: none;");
        clickableBtn->setGeometry(item->rect());
        clickableBtn->raise();
        QString tName = t.name;
        connect(clickableBtn, &QPushButton::clicked, this, [this, tName]() {
            ThemeManager::instance().setTheme(tName);
            emit themeChanged(tName);
            accept();
        });

        mainLayout->addWidget(item);
    }
}

} // namespace server
} // namespace cm
