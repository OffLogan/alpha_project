//Settings.cc
//Implementation file for settings class

#include "../include/settings.h"
#include "../include/appPaths.h"
#include "ui_settings.h"

#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPalette>
#include <QSettings>
#include <QString>

namespace {
constexpr const char* kNotificationsKey = "preferences/notifications_enabled";
constexpr const char* kDarkModeKey = "preferences/dark_mode_enabled";

QString darkThemeStyleSheet()
{
    return QString(
        "QWidget { background-color: #1f2430; color: #f1f5f9; }"
        "QLineEdit, QTextEdit, QPlainTextEdit, QTableWidget, QListWidget, QTreeWidget, QComboBox {"
        " background-color: #283244; color: #f8fafc; border: 1px solid #4b5563; border-radius: 4px; }"
        "QPushButton, QDialogButtonBox QPushButton {"
        " background-color: #334155; color: #f8fafc; border: 1px solid #64748b; border-radius: 5px; padding: 6px 10px; }"
        "QPushButton:hover, QDialogButtonBox QPushButton:hover { background-color: #475569; }"
        "QHeaderView::section { background-color: #334155; color: #f8fafc; padding: 4px; border: 1px solid #475569; }"
        "QCheckBox { spacing: 8px; }"
    );
}

void applyLightPalette(QApplication& app)
{
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(255, 255, 255));
    palette.setColor(QPalette::WindowText, QColor(15, 23, 42));
    palette.setColor(QPalette::Base, QColor(255, 255, 255));
    palette.setColor(QPalette::AlternateBase, QColor(241, 245, 249));
    palette.setColor(QPalette::ToolTipBase, QColor(255, 255, 255));
    palette.setColor(QPalette::ToolTipText, QColor(15, 23, 42));
    palette.setColor(QPalette::Text, QColor(15, 23, 42));
    palette.setColor(QPalette::Button, QColor(255, 255, 255));
    palette.setColor(QPalette::ButtonText, QColor(15, 23, 42));
    palette.setColor(QPalette::BrightText, QColor(255, 255, 255));
    palette.setColor(QPalette::Highlight, QColor(37, 99, 235));
    palette.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
    app.setPalette(palette);
    app.setStyleSheet(
        "QWidget { background-color: #ffffff; color: #0f172a; }"
        "QLineEdit, QTextEdit, QPlainTextEdit, QTableWidget, QListWidget, QTreeWidget, QComboBox {"
        " background-color: #ffffff; color: #0f172a; border: 1px solid #cbd5e1; border-radius: 4px; }"
        "QPushButton, QDialogButtonBox QPushButton {"
        " background-color: #f8fafc; color: #0f172a; border: 1px solid #cbd5e1; border-radius: 5px; padding: 6px 10px; }"
        "QPushButton:hover, QDialogButtonBox QPushButton:hover { background-color: #eef2f7; }"
        "QHeaderView::section { background-color: #f8fafc; color: #0f172a; padding: 4px; border: 1px solid #cbd5e1; }"
        "QCheckBox { spacing: 8px; }"
    );
}

void applyDarkPalette(QApplication& app)
{
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(31, 36, 48));
    palette.setColor(QPalette::WindowText, QColor(241, 245, 249));
    palette.setColor(QPalette::Base, QColor(40, 50, 68));
    palette.setColor(QPalette::AlternateBase, QColor(51, 65, 85));
    palette.setColor(QPalette::ToolTipBase, QColor(241, 245, 249));
    palette.setColor(QPalette::ToolTipText, QColor(15, 23, 42));
    palette.setColor(QPalette::Text, QColor(248, 250, 252));
    palette.setColor(QPalette::Button, QColor(51, 65, 85));
    palette.setColor(QPalette::ButtonText, QColor(248, 250, 252));
    palette.setColor(QPalette::BrightText, QColor(255, 255, 255));
    palette.setColor(QPalette::Highlight, QColor(59, 130, 246));
    palette.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
    app.setPalette(palette);
    app.setStyleSheet(darkThemeStyleSheet());
}
}

settings::settings(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::settings)
    , initialSettings_(loadAppSettings())
{
    ui->setupUi(this);
    setWindowTitle("Settings");

    loadUiFromSettings();
    updateInfoLabels();

    connect(ui->darkModeCheckBox, &QCheckBox::toggled, this, [this](bool) {
        applyPreviewSettings();
    });
}

settings::~settings()
{
    delete ui;
}

AppSettings settings::loadAppSettings()
{
    QSettings preferences;
    AppSettings appSettings;
    appSettings.notificationsEnabled = preferences.value(kNotificationsKey, true).toBool();
    appSettings.darkModeEnabled = preferences.value(kDarkModeKey, false).toBool();
    return appSettings;
}

void settings::applyAppSettings(QApplication& app)
{
    const AppSettings appSettings = loadAppSettings();

    if (appSettings.darkModeEnabled) {
        applyDarkPalette(app);
        return;
    }

    applyLightPalette(app);
}

void settings::accept()
{
    QSettings preferences;
    preferences.setValue(kNotificationsKey, ui->notificationsCheckBox->isChecked());
    preferences.setValue(kDarkModeKey, ui->darkModeCheckBox->isChecked());
    preferences.sync();

    if (QApplication::instance() != nullptr) {
        applyAppSettings(*qobject_cast<QApplication*>(QApplication::instance()));
    }

    QDialog::accept();
}

void settings::reject()
{
    QSettings preferences;
    preferences.setValue(kNotificationsKey, initialSettings_.notificationsEnabled);
    preferences.setValue(kDarkModeKey, initialSettings_.darkModeEnabled);
    preferences.sync();

    if (QApplication::instance() != nullptr) {
        applyAppSettings(*qobject_cast<QApplication*>(QApplication::instance()));
    }

    QDialog::reject();
}

void settings::loadUiFromSettings()
{
    ui->notificationsCheckBox->setChecked(initialSettings_.notificationsEnabled);
    ui->darkModeCheckBox->setChecked(initialSettings_.darkModeEnabled);
}

void settings::updateInfoLabels()
{
    ui->versionValueLabel->setText(QCoreApplication::applicationVersion().trimmed().isEmpty()
                                       ? QString("0.0.3")
                                       : QCoreApplication::applicationVersion());
    ui->dataPathValueLabel->setText(appDataDirectoryPath());
}

void settings::applyPreviewSettings() const
{
    QSettings preferences;
    preferences.setValue(kNotificationsKey, ui->notificationsCheckBox->isChecked());
    preferences.setValue(kDarkModeKey, ui->darkModeCheckBox->isChecked());
    preferences.sync();

    if (QApplication::instance() != nullptr) {
        applyAppSettings(*qobject_cast<QApplication*>(QApplication::instance()));
    }
}
