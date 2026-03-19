#include "../include/homescreen.h"
#include "../include/notes.h"
#include "../include/settings.h"
#include "../include/schedule.h"
#include "ui_homescreen.h"

#include <QMessageBox>
#include <QListWidget>
#include <QDate>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QPixmap>
#include <QString>
#include <QStringList>
#include <Qt>

namespace {
QString normalizedField(const QString &value, const QString &defaultLabel)
{
    const QString trimmedValue = value.trimmed();
    return trimmedValue.compare(defaultLabel, Qt::CaseInsensitive) == 0 ? QString() : trimmedValue;
}

QDate parseDueDate(const QString& rawValue)
{
    const QString trimmedValue = rawValue.trimmed();
    const QStringList formats = {"dd/MM/yy", "dd/MM/yyyy"};

    for (const QString& format : formats) {
        const QDate parsedDate = QDate::fromString(trimmedValue, format);
        if (parsedDate.isValid()) {
            return parsedDate;
        }
    }

    return QDate();
}

QString taskTextFromTask(const Task& task)
{
    if (task.GetDescription().empty()) {
        return QString::fromStdString(task.GetName());
    }

    return QString("%1 | %2")
        .arg(QString::fromStdString(task.GetName()),
             QString::fromStdString(task.GetDescription()));
}

QString reminderTextFromReminder(const Reminder& reminder)
{
    QStringList parts;
    parts << QString::fromStdString(reminder.GetName());

    if (!reminder.GetDescription().empty()) {
        parts << QString::fromStdString(reminder.GetDescription());
    }

    parts << QString("Due: %1").arg(reminder.GetDue());
    return parts.join(" | ");
}

QPixmap loadLogoPixmap()
{
    const QPixmap resourcePixmap(":/images/kmn_logo.PNG");
    if (!resourcePixmap.isNull()) {
        return resourcePixmap;
    }

    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QDir::current().filePath("resources/kmn_logo.PNG"),
        QDir(appDir).filePath("resources/kmn_logo.PNG"),
        QDir(appDir).filePath("../resources/kmn_logo.PNG"),
        QDir(appDir).filePath("../../resources/kmn_logo.PNG"),
        QDir(appDir).filePath("../../../resources/kmn_logo.PNG")
    };

    for (const QString& path : candidates) {
        if (QFileInfo::exists(path)) {
            const QPixmap filePixmap(path);
            if (!filePixmap.isNull()) {
                return filePixmap;
            }
        }
    }

    return QPixmap();
}

QPixmap buildHomeLogoPixmap(const QSize& targetSize)
{
    const QPixmap logoPixmap = loadLogoPixmap();
    if (logoPixmap.isNull() || !targetSize.isValid()) {
        return QPixmap();
    }

    const int squareSide = qMin(logoPixmap.width(), logoPixmap.height());
    const QRect cropRect((logoPixmap.width() - squareSide) / 2,
                         (logoPixmap.height() - squareSide) / 2,
                         squareSide,
                         squareSide);
    const QPixmap squareLogo = logoPixmap.copy(cropRect);

    const QSize fittedSize(qMax(1, static_cast<int>(targetSize.width() * 0.98)),
                           qMax(1, static_cast<int>(targetSize.height() * 0.98)));

    return squareLogo.scaled(fittedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}
}

homeScreen::homeScreen(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::homeScreen)
    , taskData_()
    , reminderData_()
    , notesWindow_(nullptr)
    , scheduleWindow_(nullptr)
{
    ui->setupUi(this);

    QFont inputFont = ui->lineEdit->font();
    inputFont.setPointSize(14);
    ui->lineEdit->setFont(inputFont);
    ui->lineEdit_3->setFont(inputFont);
    ui->lineEdit_5->setFont(inputFont);
    ui->textEdit->setFont(inputFont);
    ui->textEdit_2->setFont(inputFont);

    ui->textEdit->document()->setDocumentMargin(6);
    ui->textEdit_2->document()->setDocumentMargin(6);
    ui->textEdit->setStyleSheet("QTextEdit { color: white; padding-top: 2px; padding-bottom: 2px; }");
    ui->textEdit_2->setStyleSheet("QTextEdit { color: white; padding-top: 2px; padding-bottom: 2px; }");

    taskData_.Load();
    reminderData_.Load();
    loadStoredData();

    connect(ui->pushButton, &QPushButton::clicked, this, &homeScreen::addTask);
    connect(ui->pushButton_3, &QPushButton::clicked, this, &homeScreen::addReminder);
    connect(ui->pushButton_2, &QPushButton::clicked, this, &homeScreen::openSettings);
    connect(ui->pushButton_4, &QPushButton::clicked, this, &homeScreen::openNotes);
    connect(ui->pushButton_5, &QPushButton::clicked, this, &homeScreen::openSchedule);
    connect(ui->listWidget, &QListWidget::itemDoubleClicked, this, &homeScreen::removeTaskItem);
    connect(ui->listWidget_2, &QListWidget::itemDoubleClicked, this, &homeScreen::removeReminderItem);
    ui->lineEdit->setPlaceholderText("Name");
    ui->textEdit->setPlaceholderText("Description");
    ui->lineEdit_3->setPlaceholderText("Name");
    ui->textEdit_2->setPlaceholderText("Description");
    ui->lineEdit_5->setPlaceholderText("Day/Month/Year");
    ui->lineEdit->setStyleSheet("QLineEdit { color: white; padding-top: 2px; padding-bottom: 2px; }");
    ui->lineEdit_3->setStyleSheet("QLineEdit { color: white; padding-top: 2px; padding-bottom: 2px; }");
    ui->lineEdit_5->setStyleSheet("QLineEdit { color: white; padding-top: 2px; padding-bottom: 2px; }");

    const QPixmap logoPixmap = loadLogoPixmap();
    if (!logoPixmap.isNull()) {
        ui->label->setPixmap(logoPixmap.scaled(ui->label->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        ui->label->setAlignment(Qt::AlignCenter);
    } else {
        ui->label->setText("Logo not found");
        ui->label->setAlignment(Qt::AlignCenter);
    }
}

homeScreen::~homeScreen()
{
    delete ui;
}

void homeScreen::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);

    taskData_.Load();
    reminderData_.Load();
    loadStoredData();
    updateLogoLabel();
}

void homeScreen::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
    updateLogoLabel();
}

void homeScreen::updateLogoLabel()
{
    const QPixmap logoPixmap = buildHomeLogoPixmap(ui->label->size());
    if (!logoPixmap.isNull()) {
        ui->label->setPixmap(logoPixmap);
        ui->label->setAlignment(Qt::AlignCenter);
        return;
    }

    ui->label->setText("Logo not found");
    ui->label->setAlignment(Qt::AlignCenter);
}

void homeScreen::loadStoredData()
{
    ui->listWidget->clear();
    ui->listWidget_2->clear();

    for (const Task& task : taskData_.Data()) {
        appendTaskItem(task);
    }

    for (const Reminder& reminder : reminderData_.Data()) {
        appendReminderItem(reminder);
    }
}

void homeScreen::appendTaskItem(const Task& task)
{
    QListWidgetItem* item = new QListWidgetItem(taskTextFromTask(task), ui->listWidget);
    item->setData(Qt::UserRole, task.GetTaskId());
}

void homeScreen::appendReminderItem(const Reminder& reminder)
{
    QListWidgetItem* item = new QListWidgetItem(reminderTextFromReminder(reminder), ui->listWidget_2);
    item->setData(Qt::UserRole, reminder.GetTaskId());
}

QString homeScreen::buildTaskText() const
{
    const QString name = normalizedField(ui->lineEdit->text(), "Name");
    const QString description = normalizedField(ui->textEdit->toPlainText(), "Description");

    if (description.isEmpty()) {
        return name;
    }

    return QString("%1 | %2").arg(name, description);
}

QString homeScreen::buildReminderText() const
{
    const QString name = normalizedField(ui->lineEdit_3->text(), "Name");
    const QString description = normalizedField(ui->textEdit_2->toPlainText(), "Description");
    const QString due = normalizedField(ui->lineEdit_5->text(), "Due");

    QStringList parts;
    parts << name;

    if (!description.isEmpty()) {
        parts << description;
    }

    if (!due.isEmpty()) {
        parts << QString("Due: %1").arg(due);
    }

    return parts.join(" | ");
}

void homeScreen::clearTaskInputs()
{
    ui->lineEdit->clear();
    ui->textEdit->clear();
}

void homeScreen::clearReminderInputs()
{
    ui->lineEdit_3->clear();
    ui->textEdit_2->clear();
    ui->lineEdit_5->clear();
}

void homeScreen::addTask()
{
    const QString name = normalizedField(ui->lineEdit->text(), "Name");
    const QString description = normalizedField(ui->textEdit->toPlainText(), "Description");
    const QString taskText = buildTaskText();

    if (taskText.isEmpty() || name.isEmpty()) {
        QMessageBox::warning(this, "Task", "Write at least a name for the task.");
        return;
    }

    Task task(name.toStdString(), description.toStdString());
    if (!task.SetId(taskData_.NextId())) {
        QMessageBox::warning(this, "Task", "Could not generate a valid id for the task.");
        return;
    }

    if (!taskData_.AddTask(task)) {
        QMessageBox::warning(this, "Task", "Could not save the task in tasks.json.");
        return;
    }

    appendTaskItem(task);
    clearTaskInputs();
}

void homeScreen::addReminder()
{
    const QString name = normalizedField(ui->lineEdit_3->text(), "Name");
    const QString description = normalizedField(ui->textEdit_2->toPlainText(), "Description");
    const QString dueText = normalizedField(ui->lineEdit_5->text(), "Due").trimmed();
    const QString reminderText = buildReminderText();

    if (reminderText.isEmpty() || name.isEmpty() || dueText.isEmpty()) {
        QMessageBox::warning(this, "Reminder", "Write a name and a due date for the reminder.");
        return;
    }

    const QDate dueDate = parseDueDate(dueText);
    if (!dueDate.isValid()) {
        QMessageBox::warning(this, "Reminder", "The due date must use the format dd/MM/yy or dd/MM/yyyy.");
        return;
    }

    Reminder reminder(name.toStdString(), description.toStdString(), dueDate.toString("dd/MM/yy").toStdString());
    if (!reminder.SetId(reminderData_.NextId())) {
        QMessageBox::warning(this, "Reminder", "Could not generate a valid id for the reminder.");
        return;
    }

    if (!reminderData_.AddReminder(reminder)) {
        QMessageBox::warning(this, "Reminder", "Could not save the reminder in reminders.json.");
        return;
    }

    appendReminderItem(reminder);
    clearReminderInputs();
}

void homeScreen::removeTaskItem(QListWidgetItem *item)
{
    const int id = item->data(Qt::UserRole).toInt();
    if (!taskData_.RemoveTask(id)) {
        QMessageBox::warning(this, "Task", "Could not delete the task from tasks.json.");
        return;
    }

    delete ui->listWidget->takeItem(ui->listWidget->row(item));
}

void homeScreen::removeReminderItem(QListWidgetItem *item)
{
    const int id = item->data(Qt::UserRole).toInt();
    if (!reminderData_.RemoveReminder(id)) {
        QMessageBox::warning(this, "Reminder", "Could not delete the reminder from reminders.json.");
        return;
    }

    delete ui->listWidget_2->takeItem(ui->listWidget_2->row(item));
}

void homeScreen::openSettings()
{
    settings settingsDialog(this);
    settingsDialog.exec();
}

void homeScreen::openNotes()
{
    if (notesWindow_ == nullptr) {
        notesWindow_ = new notes(this);
    }

    hide();
    notesWindow_->show();
}

void homeScreen::openSchedule()
{
    if (scheduleWindow_ == nullptr) {
        scheduleWindow_ = new schedule(this);
    }

    scheduleWindow_->load();
    hide();
    scheduleWindow_->show();
}
