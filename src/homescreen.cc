#include "../include/homescreen.h"
#include "../include/notes.h"
#include "../include/settings.h"
#include "../include/schedule.h"
#include "ui_homescreen.h"

#include <QMessageBox>
#include <QListWidget>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDate>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QPushButton>
#include <QSignalBlocker>
#include <QString>
#include <QStringList>
#include <QTextEdit>
#include <QVBoxLayout>
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
    const QStringList parts = trimmedValue.split('/');
    if (parts.size() != 3) {
        return QDate();
    }

    bool dayOk = false;
    bool monthOk = false;
    bool yearOk = false;
    const int day = parts[0].toInt(&dayOk);
    const int month = parts[1].toInt(&monthOk);
    int year = parts[2].toInt(&yearOk);

    if (!dayOk || !monthOk || !yearOk) {
        return QDate();
    }

    if (parts[2].size() != 2) {
        return QDate();
    }

    year += 2000;

    const QDate parsedDate(year, month, day);
    return parsedDate.isValid() ? parsedDate : QDate();
}

QString formatDateInput(const QString& rawText)
{
    QString digitsOnly;
    digitsOnly.reserve(rawText.size());

    for (const QChar character : rawText) {
        if (character.isDigit()) {
            digitsOnly.append(character);
        }
    }

    if (digitsOnly.size() > 6) {
        digitsOnly.truncate(6);
    }

    QString formatted;
    for (int i = 0; i < digitsOnly.size(); ++i) {
        if (i == 2 || i == 4) {
            formatted.append('/');
        }
        formatted.append(digitsOnly[i]);
    }

    return formatted;
}

void attachDateAutoFormatting(QLineEdit* lineEdit)
{
    if (lineEdit == nullptr) {
        return;
    }

    QObject::connect(lineEdit, &QLineEdit::textChanged, lineEdit, [lineEdit](const QString& text) {
        const QString formatted = formatDateInput(text);
        if (formatted == text) {
            return;
        }

        const QSignalBlocker blocker(lineEdit);
        lineEdit->setText(formatted);
    });
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

bool isReminderOverdue(const Reminder& reminder)
{
    const QDate dueDate = parseDueDate(QString::fromStdString(reminder.GetDue()));
    return dueDate.isValid() && dueDate < QDate::currentDate();
}

bool isReminderDueSoon(const Reminder& reminder)
{
    const QDate dueDate = parseDueDate(QString::fromStdString(reminder.GetDue()));
    if (!dueDate.isValid()) {
        return false;
    }

    const int daysUntilDue = QDate::currentDate().daysTo(dueDate);
    return daysUntilDue >= 0 && daysUntilDue <= 3;
}

void applyReminderVisualState(QListWidgetItem* item, const Reminder& reminder)
{
    if (item == nullptr) {
        return;
    }

    QFont itemFont = item->font();
    itemFont.setBold(false);
    item->setFont(itemFont);
    item->setForeground(QBrush());
    item->setBackground(QBrush());
    item->setToolTip(QString());

    if (isReminderOverdue(reminder)) {
        itemFont.setBold(true);
        item->setFont(itemFont);
        item->setForeground(QColor(185, 28, 28));
        item->setToolTip("Overdue reminder");
        return;
    }

    if (isReminderDueSoon(reminder)) {
        itemFont.setBold(true);
        item->setFont(itemFont);
        item->setForeground(QColor(180, 83, 9));
        item->setToolTip("Reminder due soon");
    }
}

bool confirmDeleteAction(QWidget* parent, const QString& title, const QString& prompt)
{
    const QMessageBox::StandardButton result = QMessageBox::question(
        parent,
        title,
        prompt,
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );

    return result == QMessageBox::Yes;
}

bool editTaskDialog(QWidget* parent, const Task& originalTask, QString* nameOut, QString* descriptionOut)
{
    if (nameOut == nullptr || descriptionOut == nullptr) {
        return false;
    }

    QDialog dialog(parent);
    dialog.setWindowTitle("Edit task");

    QFormLayout formLayout(&dialog);
    QLineEdit nameEdit(&dialog);
    QTextEdit descriptionEdit(&dialog);
    QDialogButtonBox buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);

    nameEdit.setText(QString::fromStdString(originalTask.GetName()));
    descriptionEdit.setPlainText(QString::fromStdString(originalTask.GetDescription()));
    descriptionEdit.setMinimumHeight(100);

    formLayout.addRow("Name", &nameEdit);
    formLayout.addRow("Description", &descriptionEdit);
    formLayout.addWidget(&buttonBox);

    QObject::connect(&buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(&buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    *nameOut = nameEdit.text().trimmed();
    *descriptionOut = descriptionEdit.toPlainText().trimmed();
    return true;
}

bool editReminderDialog(QWidget* parent,
                        const Reminder& originalReminder,
                        QString* nameOut,
                        QString* descriptionOut,
                        QString* dueOut)
{
    if (nameOut == nullptr || descriptionOut == nullptr || dueOut == nullptr) {
        return false;
    }

    QDialog dialog(parent);
    dialog.setWindowTitle("Edit reminder");

    QFormLayout formLayout(&dialog);
    QLineEdit nameEdit(&dialog);
    QTextEdit descriptionEdit(&dialog);
    QLineEdit dueEdit(&dialog);
    QDialogButtonBox buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);

    nameEdit.setText(QString::fromStdString(originalReminder.GetName()));
    descriptionEdit.setPlainText(QString::fromStdString(originalReminder.GetDescription()));
    descriptionEdit.setMinimumHeight(100);
    dueEdit.setText(QString::fromStdString(originalReminder.GetDue()));
    attachDateAutoFormatting(&dueEdit);

    formLayout.addRow("Name", &nameEdit);
    formLayout.addRow("Description", &descriptionEdit);
    formLayout.addRow("Due date", &dueEdit);
    formLayout.addWidget(&buttonBox);

    QObject::connect(&buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(&buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    *nameOut = nameEdit.text().trimmed();
    *descriptionOut = descriptionEdit.toPlainText().trimmed();
    *dueOut = dueEdit.text().trimmed();
    return true;
}

enum class HomeItemAction {
    Cancel,
    Edit,
    Complete,
    Delete
};

enum class CompletedItemAction {
    Cancel,
    Reopen,
    Delete
};

HomeItemAction askActiveItemAction(QWidget* parent, const QString& title, const QString& prompt)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(title);

    QVBoxLayout* rootLayout = new QVBoxLayout(&dialog);
    QLabel* promptLabel = new QLabel(prompt, &dialog);
    QHBoxLayout* buttonsLayout = new QHBoxLayout();
    QPushButton* completeButton = new QPushButton("Complete", &dialog);
    QPushButton* editButton = new QPushButton("Edit", &dialog);
    QPushButton* deleteButton = new QPushButton("Delete", &dialog);
    QPushButton* cancelButton = new QPushButton("Cancel", &dialog);

    promptLabel->setWordWrap(true);
    buttonsLayout->addWidget(completeButton);
    buttonsLayout->addWidget(editButton);
    buttonsLayout->addWidget(deleteButton);
    buttonsLayout->addStretch();
    buttonsLayout->addWidget(cancelButton);

    rootLayout->addWidget(promptLabel);
    rootLayout->addLayout(buttonsLayout);

    HomeItemAction selectedAction = HomeItemAction::Cancel;
    QObject::connect(completeButton, &QPushButton::clicked, &dialog, [&]() {
        selectedAction = HomeItemAction::Complete;
        dialog.accept();
    });
    QObject::connect(editButton, &QPushButton::clicked, &dialog, [&]() {
        selectedAction = HomeItemAction::Edit;
        dialog.accept();
    });
    QObject::connect(deleteButton, &QPushButton::clicked, &dialog, [&]() {
        selectedAction = HomeItemAction::Delete;
        dialog.accept();
    });
    QObject::connect(cancelButton, &QPushButton::clicked, &dialog, [&]() {
        selectedAction = HomeItemAction::Cancel;
        dialog.reject();
    });

    dialog.exec();
    return selectedAction;
}

CompletedItemAction askCompletedItemAction(QWidget* parent, const QString& title, const QString& prompt)
{
    QMessageBox messageBox(parent);
    messageBox.setWindowTitle(title);
    messageBox.setText(prompt);

    QPushButton* reopenButton = messageBox.addButton("Reopen", QMessageBox::AcceptRole);
    QPushButton* deleteButton = messageBox.addButton("Delete", QMessageBox::DestructiveRole);
    messageBox.addButton(QMessageBox::Cancel);
    messageBox.exec();

    if (messageBox.clickedButton() == reopenButton) {
        return CompletedItemAction::Reopen;
    }

    if (messageBox.clickedButton() == deleteButton) {
        return CompletedItemAction::Delete;
    }

    return CompletedItemAction::Cancel;
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
        QDir(appDir).filePath("../Resources/kmn_logo.PNG"),
        QDir(appDir).filePath("../resources/kmn_logo.PNG"),
        QDir(appDir).filePath("../../Resources/kmn_logo.PNG"),
        QDir(appDir).filePath("../../resources/kmn_logo.PNG"),
        QDir(appDir).filePath("../../../Resources/kmn_logo.PNG"),
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

    QPixmap displayPixmap = logoPixmap;
    const QImage logoImage = logoPixmap.toImage();
    QRect nonTransparentBounds;

    for (int y = 0; y < logoImage.height(); ++y) {
        for (int x = 0; x < logoImage.width(); ++x) {
            if (qAlpha(logoImage.pixel(x, y)) > 0) {
                if (nonTransparentBounds.isNull()) {
                    nonTransparentBounds = QRect(x, y, 1, 1);
                } else {
                    nonTransparentBounds = nonTransparentBounds.united(QRect(x, y, 1, 1));
                }
            }
        }
    }

    if (!nonTransparentBounds.isNull()) {
        displayPixmap = logoPixmap.copy(nonTransparentBounds);
    }

    const QSize fittedSize(qMax(1, static_cast<int>(targetSize.width() * 0.98)),
                           qMax(1, static_cast<int>(targetSize.height() * 0.98)));

    return displayPixmap.scaled(fittedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

void presentWindow(QWidget* window)
{
    if (window == nullptr) {
        return;
    }

    window->show();
    window->raise();
    window->activateWindow();
    window->setFocus(Qt::OtherFocusReason);
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
    ui->textEdit->setStyleSheet(
        "QTextEdit { color: palette(text); background-color: palette(base); padding-top: 2px; padding-bottom: 2px; }");
    ui->textEdit_2->setStyleSheet(
        "QTextEdit { color: palette(text); background-color: palette(base); padding-top: 2px; padding-bottom: 2px; }");
    ui->label->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    ui->label->setStyleSheet("background: transparent;");
    ui->label_8->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    taskData_.Load();
    reminderData_.Load();
    loadStoredData();

    connect(ui->pushButton, &QPushButton::clicked, this, &homeScreen::addTask);
    connect(ui->pushButton_3, &QPushButton::clicked, this, &homeScreen::addReminder);
    connect(ui->pushButton_2, &QPushButton::clicked, this, &homeScreen::openSettings);
    connect(ui->pushButton_4, &QPushButton::clicked, this, &homeScreen::openNotes);
    connect(ui->pushButton_5, &QPushButton::clicked, this, &homeScreen::openSchedule);
    connect(ui->pushButton_6, &QPushButton::clicked, this, &homeScreen::openCompletedItems);
    connect(ui->listWidget, &QListWidget::itemDoubleClicked, this, &homeScreen::handleTaskItemAction);
    connect(ui->listWidget_2, &QListWidget::itemDoubleClicked, this, &homeScreen::handleReminderItemAction);
    ui->lineEdit->setPlaceholderText("Name");
    ui->textEdit->setPlaceholderText("Description");
    ui->lineEdit_3->setPlaceholderText("Name");
    ui->textEdit_2->setPlaceholderText("Description");
    ui->lineEdit_5->setPlaceholderText("DD/MM/YY");
    attachDateAutoFormatting(ui->lineEdit_5);
    ui->lineEdit->setStyleSheet(
        "QLineEdit { color: palette(text); background-color: palette(base); padding-top: 2px; padding-bottom: 2px; }");
    ui->lineEdit_3->setStyleSheet(
        "QLineEdit { color: palette(text); background-color: palette(base); padding-top: 2px; padding-bottom: 2px; }");
    ui->lineEdit_5->setStyleSheet(
        "QLineEdit { color: palette(text); background-color: palette(base); padding-top: 2px; padding-bottom: 2px; }");
    const QPixmap logoPixmap = buildHomeLogoPixmap(ui->label->size());
    if (!logoPixmap.isNull()) {
        ui->label->setPixmap(logoPixmap);
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

void homeScreen::mousePressEvent(QMouseEvent *event)
{
    if (event != nullptr) {
        const QPoint listWidgetPosition = ui->listWidget->mapFrom(this, event->pos());
        const QPoint reminderListPosition = ui->listWidget_2->mapFrom(this, event->pos());

        const bool clickedTaskList = ui->listWidget->rect().contains(listWidgetPosition);
        const bool clickedReminderList = ui->listWidget_2->rect().contains(reminderListPosition);

        if (!clickedTaskList) {
            ui->listWidget->clearSelection();
            ui->listWidget->setCurrentItem(nullptr);
        }

        if (!clickedReminderList) {
            ui->listWidget_2->clearSelection();
            ui->listWidget_2->setCurrentItem(nullptr);
        }
    }

    QDialog::mousePressEvent(event);
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
        if (!task.GetStatus()) {
            appendTaskItem(task);
        }
    }

    for (const Reminder& reminder : reminderData_.Data()) {
        if (!reminder.GetStatus()) {
            appendReminderItem(reminder);
        }
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
    applyReminderVisualState(item, reminder);
}

void homeScreen::refreshTaskItem(QListWidgetItem* item, const Task& task)
{
    if (item == nullptr) {
        return;
    }

    item->setText(taskTextFromTask(task));
    item->setData(Qt::UserRole, task.GetTaskId());
}

void homeScreen::refreshReminderItem(QListWidgetItem* item, const Reminder& reminder)
{
    if (item == nullptr) {
        return;
    }

    item->setText(reminderTextFromReminder(reminder));
    item->setData(Qt::UserRole, reminder.GetTaskId());
    applyReminderVisualState(item, reminder);
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
        QMessageBox::warning(this, "Reminder", "The due date must use the format DD/MM/YY.");
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

void homeScreen::handleTaskItemAction(QListWidgetItem *item)
{
    if (item == nullptr) {
        return;
    }

    const int id = item->data(Qt::UserRole).toInt();
    Task* task = taskData_.FindById(id);
    if (task == nullptr) {
        QMessageBox::warning(this, "Task", "The selected task could not be found.");
        return;
    }

    const HomeItemAction action = askActiveItemAction(this, "Task", "Choose what you want to do with this task.");
    if (action == HomeItemAction::Complete) {
        Task updatedTask = *task;
        updatedTask.CompleteTask();

        if (!taskData_.UpdateTask(updatedTask)) {
            QMessageBox::warning(this, "Task", "Could not update the task in tasks.json.");
            return;
        }

        delete ui->listWidget->takeItem(ui->listWidget->row(item));
        return;
    }

    if (action == HomeItemAction::Delete) {
        if (!confirmDeleteAction(this, "Delete task", "Do you really want to delete this task?")) {
            return;
        }

        if (!taskData_.RemoveTask(id)) {
            QMessageBox::warning(this, "Task", "Could not delete the task from tasks.json.");
            return;
        }

        delete ui->listWidget->takeItem(ui->listWidget->row(item));
        return;
    }

    if (action != HomeItemAction::Edit) {
        return;
    }

    QString updatedName;
    QString updatedDescription;
    if (!editTaskDialog(this, *task, &updatedName, &updatedDescription)) {
        return;
    }

    if (updatedName.isEmpty()) {
        QMessageBox::warning(this, "Task", "The task name cannot be empty.");
        return;
    }

    Task updatedTask = *task;
    if (!updatedTask.SetName(updatedName.toStdString()) ||
        !updatedTask.SetDescription(updatedDescription.toStdString())) {
        QMessageBox::warning(this, "Task", "The task data is not valid.");
        return;
    }

    if (!taskData_.UpdateTask(updatedTask)) {
        QMessageBox::warning(this, "Task", "Could not update the task in tasks.json.");
        return;
    }

    refreshTaskItem(item, updatedTask);
}

void homeScreen::handleReminderItemAction(QListWidgetItem *item)
{
    if (item == nullptr) {
        return;
    }

    const int id = item->data(Qt::UserRole).toInt();
    Reminder* reminder = reminderData_.FindById(id);
    if (reminder == nullptr) {
        QMessageBox::warning(this, "Reminder", "The selected reminder could not be found.");
        return;
    }

    const HomeItemAction action = askActiveItemAction(this, "Reminder", "Choose what you want to do with this reminder.");
    if (action == HomeItemAction::Complete) {
        Reminder updatedReminder = *reminder;
        updatedReminder.CompleteTask();

        if (!reminderData_.UpdateReminder(updatedReminder)) {
            QMessageBox::warning(this, "Reminder", "Could not update the reminder in reminders.json.");
            return;
        }

        delete ui->listWidget_2->takeItem(ui->listWidget_2->row(item));
        return;
    }

    if (action == HomeItemAction::Delete) {
        if (!confirmDeleteAction(this, "Delete reminder", "Do you really want to delete this reminder?")) {
            return;
        }

        if (!reminderData_.RemoveReminder(id)) {
            QMessageBox::warning(this, "Reminder", "Could not delete the reminder from reminders.json.");
            return;
        }

        delete ui->listWidget_2->takeItem(ui->listWidget_2->row(item));
        return;
    }

    if (action != HomeItemAction::Edit) {
        return;
    }

    QString updatedName;
    QString updatedDescription;
    QString updatedDue;
    if (!editReminderDialog(this, *reminder, &updatedName, &updatedDescription, &updatedDue)) {
        return;
    }

    if (updatedName.isEmpty() || updatedDue.isEmpty()) {
        QMessageBox::warning(this, "Reminder", "The reminder needs a name and a due date.");
        return;
    }

    const QDate dueDate = parseDueDate(updatedDue);
    if (!dueDate.isValid()) {
        QMessageBox::warning(this, "Reminder", "The due date must use the format DD/MM/YY.");
        return;
    }

    Reminder updatedReminder = *reminder;
    if (!updatedReminder.SetName(updatedName.toStdString()) ||
        !updatedReminder.SetDescription(updatedDescription.toStdString()) ||
        !updatedReminder.SetDue(dueDate.toString("dd/MM/yy").toStdString())) {
        QMessageBox::warning(this, "Reminder", "The reminder data is not valid.");
        return;
    }

    if (!reminderData_.UpdateReminder(updatedReminder)) {
        QMessageBox::warning(this, "Reminder", "Could not update the reminder in reminders.json.");
        return;
    }

    refreshReminderItem(item, updatedReminder);
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
    presentWindow(notesWindow_);
}

void homeScreen::openSchedule()
{
    if (scheduleWindow_ == nullptr) {
        scheduleWindow_ = new schedule(this);
    }

    scheduleWindow_->load();
    hide();
    presentWindow(scheduleWindow_);
}

void homeScreen::openCompletedItems()
{
    QDialog dialog(this);
    dialog.setWindowTitle("Completed items");
    dialog.resize(760, 420);

    QVBoxLayout* rootLayout = new QVBoxLayout(&dialog);
    QLabel* titleLabel = new QLabel("Completed tasks and reminders", &dialog);
    QHBoxLayout* listsLayout = new QHBoxLayout();
    QListWidget* completedTasksList = new QListWidget(&dialog);
    QListWidget* completedRemindersList = new QListWidget(&dialog);
    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);

    titleLabel->setStyleSheet("font-size: 20px; font-weight: 700;");
    completedTasksList->setMinimumWidth(320);
    completedRemindersList->setMinimumWidth(320);

    listsLayout->addWidget(completedTasksList);
    listsLayout->addWidget(completedRemindersList);
    rootLayout->addWidget(titleLabel);
    rootLayout->addLayout(listsLayout);
    rootLayout->addWidget(buttonBox);

    auto reloadLists = [&]() {
        taskData_.Load();
        reminderData_.Load();
        completedTasksList->clear();
        completedRemindersList->clear();

        for (const Task& task : taskData_.Data()) {
            if (!task.GetStatus()) {
                continue;
            }

            QListWidgetItem* item = new QListWidgetItem(taskTextFromTask(task), completedTasksList);
            item->setData(Qt::UserRole, task.GetTaskId());
        }

        for (const Reminder& reminder : reminderData_.Data()) {
            if (!reminder.GetStatus()) {
                continue;
            }

            QListWidgetItem* item = new QListWidgetItem(reminderTextFromReminder(reminder), completedRemindersList);
            item->setData(Qt::UserRole, reminder.GetTaskId());
        }
    };

    reloadLists();

    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    connect(completedTasksList, &QListWidget::itemDoubleClicked, &dialog, [&](QListWidgetItem* item) {
        if (item == nullptr) {
            return;
        }

        const int id = item->data(Qt::UserRole).toInt();
        Task* task = taskData_.FindById(id);
        if (task == nullptr) {
            QMessageBox::warning(&dialog, "Completed task", "The selected task could not be found.");
            return;
        }

        const CompletedItemAction action = askCompletedItemAction(
            &dialog,
            "Completed task",
            "Do you want to reopen or delete this completed task?");

        if (action == CompletedItemAction::Reopen) {
            Task updatedTask = *task;
            updatedTask.ReopenTask();

            if (!taskData_.UpdateTask(updatedTask)) {
                QMessageBox::warning(&dialog, "Completed task", "Could not reopen the task.");
                return;
            }

            reloadLists();
            loadStoredData();
            return;
        }

        if (action == CompletedItemAction::Delete) {
            if (!confirmDeleteAction(&dialog, "Delete completed task", "Do you really want to delete this completed task?")) {
                return;
            }

            if (!taskData_.RemoveTask(id)) {
                QMessageBox::warning(&dialog, "Completed task", "Could not delete the task.");
                return;
            }

            reloadLists();
            loadStoredData();
        }
    });

    connect(completedRemindersList, &QListWidget::itemDoubleClicked, &dialog, [&](QListWidgetItem* item) {
        if (item == nullptr) {
            return;
        }

        const int id = item->data(Qt::UserRole).toInt();
        Reminder* reminder = reminderData_.FindById(id);
        if (reminder == nullptr) {
            QMessageBox::warning(&dialog, "Completed reminder", "The selected reminder could not be found.");
            return;
        }

        const CompletedItemAction action = askCompletedItemAction(
            &dialog,
            "Completed reminder",
            "Do you want to reopen or delete this completed reminder?");

        if (action == CompletedItemAction::Reopen) {
            Reminder updatedReminder = *reminder;
            updatedReminder.ReopenTask();

            if (!reminderData_.UpdateReminder(updatedReminder)) {
                QMessageBox::warning(&dialog, "Completed reminder", "Could not reopen the reminder.");
                return;
            }

            reloadLists();
            loadStoredData();
            return;
        }

        if (action == CompletedItemAction::Delete) {
            if (!confirmDeleteAction(&dialog, "Delete completed reminder", "Do you really want to delete this completed reminder?")) {
                return;
            }

            if (!reminderData_.RemoveReminder(id)) {
                QMessageBox::warning(&dialog, "Completed reminder", "Could not delete the reminder.");
                return;
            }

            reloadLists();
            loadStoredData();
        }
    });

    dialog.exec();
}
