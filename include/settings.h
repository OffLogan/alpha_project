//Settings.h
//Header file for settings class

#ifndef SETTINGS_H
#define SETTINGS_H

#include <QApplication>
#include <QDialog>

namespace Ui {
class settings;
}

struct AppSettings {
    bool notificationsEnabled = true;
    bool darkModeEnabled = false;
};

class settings : public QDialog
{
    Q_OBJECT

public:
    explicit settings(QWidget *parent = nullptr);
    ~settings();

    static AppSettings loadAppSettings();
    static void applyAppSettings(QApplication& app);

public slots:
    void accept() override;
    void reject() override;

private:
    void loadUiFromSettings();
    void updateInfoLabels();
    void applyPreviewSettings() const;

    Ui::settings *ui;
    AppSettings initialSettings_;
};
#endif // SETTINGS_H
