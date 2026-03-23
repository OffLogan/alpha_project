#ifndef SCHEDULE_H
#define SCHEDULE_H

#include <QMainWindow>

class QTableWidgetItem;

namespace Ui {
class schedule;
}

class schedule : public QMainWindow
{
    Q_OBJECT

public:
    explicit schedule(QWidget *parent = nullptr);
    ~schedule();
    void load();

private slots:
    void goBackHome();
    void addEntry();
    void deleteSelectedEntry();
    void saveSchedule();
    void exportSchedule();
    void importSchedule();
    void clearSchedule();

private:
    bool writeSchedule(bool showSuccessMessage);
    bool writeScheduleToFile(const QString& filePath);
    bool loadScheduleFromFile(const QString& filePath, bool showErrorMessage);
    void loadSchedule();
    void appendRow(const QString& day,
                   const QString& time,
                   const QString& subject,
                   const QString& location);
    void clearInputs();
    QString scheduleFilePath() const;
    QString defaultExportSchedulePath() const;

    Ui::schedule *ui;
};

#endif // SCHEDULE_H
