#ifndef HOMESCREEN_H
#define HOMESCREEN_H

#include <QDialog>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QShowEvent>
#include <QListWidgetItem>

#include "reminderGestor.h"
#include "taskGestor.h"

class notes;
class schedule;

namespace Ui {
class homeScreen;
}

class homeScreen : public QDialog
{
    Q_OBJECT

public:
    explicit homeScreen(QWidget *parent = nullptr);
    ~homeScreen();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void addTask();
    void addReminder();
    void handleTaskItemAction(QListWidgetItem *item);
    void handleReminderItemAction(QListWidgetItem *item);
    void openSettings();
    void openNotes();
    void openSchedule();
    void openCompletedItems();

private:
    void updateLogoLabel();
    void loadStoredData();
    void appendTaskItem(const Task& task);
    void appendReminderItem(const Reminder& reminder);
    void refreshTaskItem(QListWidgetItem* item, const Task& task);
    void refreshReminderItem(QListWidgetItem* item, const Reminder& reminder);
    QString buildTaskText() const;
    QString buildReminderText() const;
    void clearTaskInputs();
    void clearReminderInputs();

    Ui::homeScreen *ui;
    TaskData taskData_;
    ReminderData reminderData_;
    notes *notesWindow_;
    schedule *scheduleWindow_;
};

#endif // HOMESCREEN_H
