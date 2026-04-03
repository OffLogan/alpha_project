#include "../include/homescreen.h"
#include "../include/notes.h"
#include "../include/settings.h"
#include "../include/schedule.h"
#include "../include/noteGestor.h"
#include "../include/appPaths.h"
#include "ui_homescreen.h"

#include <algorithm>
#include <vector>
#include <QComboBox>
#include <QFile>
#include <QMessageBox>
#include <QListWidget>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDate>
#include <QInputDialog>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMouseEvent>
#include <QPixmap>
#include <QPushButton>
#include <QSignalBlocker>
#include <QString>
#include <QStringList>
#include <QTextEdit>
#include <QVBoxLayout>
#include <Qt>

namespace {
enum class TaskListFilter {
    Pending,
    Completed
};

enum class ReminderListFilter {
    Pending,
    Completed,
    Overdue,
    DueSoon
};

enum class ReminderSortMode {
    DueSoonest,
    DueLatest
};

enum class SearchResultType {
    Task,
    Reminder,
    Note,
    ScheduleEntry
};

struct GlobalSearchResult {
    SearchResultType type;
    int id;
    QString title;
    QString subtitle;
    QString day;
    QString time;
    QString subject;
    QString location;
};

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

TaskListFilter taskListFilterFromIndex(int index)
{
    return index == 1 ? TaskListFilter::Completed : TaskListFilter::Pending;
}

ReminderListFilter reminderListFilterFromIndex(int index)
{
    switch (index) {
    case 1:
        return ReminderListFilter::Completed;
    case 2:
        return ReminderListFilter::Overdue;
    case 3:
        return ReminderListFilter::DueSoon;
    default:
        return ReminderListFilter::Pending;
    }
}

ReminderSortMode reminderSortModeFromIndex(int index)
{
    return index == 1 ? ReminderSortMode::DueLatest : ReminderSortMode::DueSoonest;
}

bool taskMatchesFilter(const Task& task, TaskListFilter filter)
{
    if (filter == TaskListFilter::Completed) {
        return task.GetStatus();
    }

    return !task.GetStatus();
}

bool reminderMatchesFilter(const Reminder& reminder, ReminderListFilter filter)
{
    switch (filter) {
    case ReminderListFilter::Completed:
        return reminder.GetStatus();
    case ReminderListFilter::Overdue:
        return !reminder.GetStatus() && isReminderOverdue(reminder);
    case ReminderListFilter::DueSoon:
        return !reminder.GetStatus() && isReminderDueSoon(reminder);
    case ReminderListFilter::Pending:
    default:
        return !reminder.GetStatus();
    }
}

bool reminderDateComesFirst(const Reminder& left,
                            const Reminder& right,
                            ReminderSortMode sortMode)
{
    const QDate leftDate = parseDueDate(QString::fromStdString(left.GetDue()));
    const QDate rightDate = parseDueDate(QString::fromStdString(right.GetDue()));

    if (leftDate.isValid() && rightDate.isValid() && leftDate != rightDate) {
        return sortMode == ReminderSortMode::DueSoonest ? leftDate < rightDate : leftDate > rightDate;
    }

    if (leftDate.isValid() != rightDate.isValid()) {
        return leftDate.isValid();
    }

    return QString::fromStdString(left.GetName()).localeAwareCompare(QString::fromStdString(right.GetName())) < 0;
}

QString normalizedSearchHaystack(const QString& value)
{
    return value.trimmed().toLower();
}

bool searchTextMatches(const QString& query, const QStringList& values)
{
    const QString normalizedQuery = normalizedSearchHaystack(query);
    if (normalizedQuery.isEmpty()) {
        return false;
    }

    for (const QString& value : values) {
        if (normalizedSearchHaystack(value).contains(normalizedQuery)) {
            return true;
        }
    }

    return false;
}

QString searchResultLabel(const GlobalSearchResult& result)
{
    switch (result.type) {
    case SearchResultType::Task:
        return QString("[Task] %1").arg(result.title);
    case SearchResultType::Reminder:
        return QString("[Reminder] %1").arg(result.title);
    case SearchResultType::Note:
        return QString("[Note] %1").arg(result.title);
    case SearchResultType::ScheduleEntry:
        return QString("[Schedule] %1").arg(result.title);
    }

    return result.title;
}

QString searchResultSecondaryText(const GlobalSearchResult& result)
{
    return result.subtitle.trimmed();
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
    : QMainWindow(parent)
    , ui(new Ui::homeScreen)
    , taskData_()
    , reminderData_()
    , notesWindow_(nullptr)
    , scheduleWindow_(nullptr)
{
    ui->setupUi(this);
    ui->centralwidget->installEventFilter(this);

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
    updateResponsiveLayout();
    loadStoredData();

    connect(ui->pushButton, &QPushButton::clicked, this, &homeScreen::addTask);
    connect(ui->pushButton_3, &QPushButton::clicked, this, &homeScreen::addReminder);
    connect(ui->searchButton, &QPushButton::clicked, this, &homeScreen::openGlobalSearch);
    connect(ui->pushButton_2, &QPushButton::clicked, this, &homeScreen::openSettings);
    connect(ui->pushButton_4, &QPushButton::clicked, this, &homeScreen::openNotes);
    connect(ui->pushButton_5, &QPushButton::clicked, this, &homeScreen::openSchedule);
    connect(ui->listWidget, &QListWidget::itemDoubleClicked, this, &homeScreen::handleTaskItemAction);
    connect(ui->listWidget_2, &QListWidget::itemDoubleClicked, this, &homeScreen::handleReminderItemAction);
    connect(ui->taskFilterComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        refreshVisibleLists();
    });
    connect(ui->reminderFilterComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        refreshVisibleLists();
    });
    connect(ui->reminderSortComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        refreshVisibleLists();
    });
    ui->lineEdit->setPlaceholderText("Name");
    ui->textEdit->setPlaceholderText("Description");
    ui->lineEdit_3->setPlaceholderText("Name");
    ui->textEdit_2->setPlaceholderText("Description");
    ui->lineEdit_5->setPlaceholderText("DD/MM/YY");
    ui->taskFilterComboBox->addItems({"Pending", "Completed"});
    ui->reminderFilterComboBox->addItems({"Pending", "Completed", "Overdue", "Due soon"});
    ui->reminderSortComboBox->addItems({"Due soonest", "Due latest"});
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

bool homeScreen::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == ui->centralwidget && event != nullptr && event->type() == QEvent::MouseButtonPress) {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        const QPoint listWidgetPosition = ui->listWidget->mapFrom(ui->centralwidget, mouseEvent->pos());
        const QPoint reminderListPosition = ui->listWidget_2->mapFrom(ui->centralwidget, mouseEvent->pos());

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
        return QMainWindow::eventFilter(watched, event);
    }

    return QMainWindow::eventFilter(watched, event);
}

void homeScreen::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);

    taskData_.Load();
    reminderData_.Load();
    updateResponsiveLayout();
    loadStoredData();
    updateLogoLabel();
}

void homeScreen::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    updateResponsiveLayout();
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

void homeScreen::updateResponsiveLayout()
{
    if (ui == nullptr) {
        return;
    }

    if (ui->centralwidget == nullptr) {
        return;
    }

    const int width = ui->centralwidget->width();
    const int height = ui->centralwidget->height();
    const int margin = 24;
    const int top = 24;
    const int navButtonWidth = 111;
    const int buttonHeight = 32;
    const int navGap = 18;
    const int columnGap = 44;
    const int leftWidth = std::max(270, std::min(340, static_cast<int>(width * 0.28)));
    const int rightStartX = margin + leftWidth + 54;
    const int rightWidth = std::max(420, width - rightStartX - margin);
    const int columnWidth = std::max(220, (rightWidth - columnGap) / 2);
    const int reminderX = rightStartX + columnWidth + columnGap;
    const int titleWidth = 280;
    const int listTop = 370;
    const int navY = height - 45 - buttonHeight;
    const int listHeight = std::max(210, navY - listTop - 16);

    ui->label->setGeometry(margin, top, leftWidth, leftWidth);
    ui->searchButton->setGeometry(width - margin - navButtonWidth, top + 6, navButtonWidth, buttonHeight);
    ui->label_8->setGeometry(rightStartX + std::max(0, (rightWidth - titleWidth) / 2), top, titleWidth, 61);

    ui->label_4->setGeometry(rightStartX + std::max(0, (columnWidth - 120) / 2), 100, 120, 20);
    ui->lineEdit->setGeometry(rightStartX, 130, columnWidth, 28);
    ui->textEdit->setGeometry(rightStartX, 165, columnWidth, 74);
    ui->pushButton->setGeometry(rightStartX + std::max(0, (columnWidth - 100) / 2), 270, 100, buttonHeight);

    ui->label_3->setGeometry(reminderX + std::max(0, (columnWidth - 120) / 2), 100, 120, 20);
    ui->lineEdit_3->setGeometry(reminderX, 130, columnWidth, 28);
    ui->textEdit_2->setGeometry(reminderX, 165, columnWidth, 74);
    ui->lineEdit_5->setGeometry(reminderX, 245, columnWidth, 28);
    ui->pushButton_3->setGeometry(reminderX + std::max(0, (columnWidth - 100) / 2), 270, 100, buttonHeight);

    ui->taskFilterComboBox->setGeometry(rightStartX + columnWidth - 131, 333, 131, 27);
    ui->listWidget->setGeometry(rightStartX, listTop, columnWidth, listHeight);

    const int reminderControlsGap = 10;
    const int reminderSortWidth = std::min(150, std::max(120, columnWidth / 2 - (reminderControlsGap / 2)));
    const int reminderFilterWidth = std::min(150, std::max(120, columnWidth - reminderSortWidth - reminderControlsGap));
    ui->reminderFilterComboBox->setGeometry(reminderX + columnWidth - reminderSortWidth - reminderControlsGap - reminderFilterWidth,
                                            333,
                                            reminderFilterWidth,
                                            27);
    ui->reminderSortComboBox->setGeometry(reminderX + columnWidth - reminderSortWidth,
                                          333,
                                          reminderSortWidth,
                                          27);
    ui->listWidget_2->setGeometry(reminderX, listTop, columnWidth, listHeight);

    ui->label_7->setGeometry(margin + std::max(0, (leftWidth - 140) / 2), listTop + 18, 140, 41);
    const int calendarWidth = leftWidth;
    const int calendarHeight = std::max(180, height - (listTop + 90) - 40);
    ui->frame->setGeometry(margin, height - 40 - calendarHeight, calendarWidth, calendarHeight);
    ui->calendarWidget->setGeometry(0, 0, calendarWidth, calendarHeight);

    const int navTotalWidth = (navButtonWidth * 3) + (navGap * 2);
    int navStartX = width - margin - navTotalWidth;
    ui->pushButton_4->setGeometry(navStartX, navY, navButtonWidth, buttonHeight);
    navStartX += navButtonWidth + navGap;
    ui->pushButton_5->setGeometry(navStartX, navY, navButtonWidth, buttonHeight);
    navStartX += navButtonWidth + navGap;
    ui->pushButton_2->setGeometry(navStartX, navY, navButtonWidth, buttonHeight);
}

void homeScreen::loadStoredData()
{
    ui->listWidget->clear();
    ui->listWidget_2->clear();

    const TaskListFilter taskFilter = taskListFilterFromIndex(ui->taskFilterComboBox->currentIndex());
    const ReminderListFilter reminderFilter = reminderListFilterFromIndex(ui->reminderFilterComboBox->currentIndex());
    const ReminderSortMode reminderSortMode = reminderSortModeFromIndex(ui->reminderSortComboBox->currentIndex());

    for (const Task& task : taskData_.Data()) {
        if (taskMatchesFilter(task, taskFilter)) {
            appendTaskItem(task);
        }
    }

    std::vector<Reminder> visibleReminders;
    visibleReminders.reserve(reminderData_.Data().size());

    for (const Reminder& reminder : reminderData_.Data()) {
        if (reminderMatchesFilter(reminder, reminderFilter)) {
            visibleReminders.push_back(reminder);
        }
    }

    std::sort(visibleReminders.begin(), visibleReminders.end(), [reminderSortMode](const Reminder& left, const Reminder& right) {
        return reminderDateComesFirst(left, right, reminderSortMode);
    });

    for (const Reminder& reminder : visibleReminders) {
        appendReminderItem(reminder);
    }
}

void homeScreen::refreshVisibleLists()
{
    loadStoredData();
}

void homeScreen::focusTaskResult(int taskId)
{
    const Task* task = taskData_.FindById(taskId);
    if (task == nullptr) {
        return;
    }

    ui->taskFilterComboBox->setCurrentIndex(task->GetStatus() ? 1 : 0);
    loadStoredData();

    for (int row = 0; row < ui->listWidget->count(); ++row) {
        QListWidgetItem* item = ui->listWidget->item(row);
        if (item != nullptr && item->data(Qt::UserRole).toInt() == taskId) {
            ui->listWidget->setCurrentItem(item);
            ui->listWidget->scrollToItem(item);
            return;
        }
    }
}

void homeScreen::focusReminderResult(int reminderId)
{
    const Reminder* reminder = reminderData_.FindById(reminderId);
    if (reminder == nullptr) {
        return;
    }

    ui->reminderFilterComboBox->setCurrentIndex(reminder->GetStatus() ? 1 : 0);
    loadStoredData();

    for (int row = 0; row < ui->listWidget_2->count(); ++row) {
        QListWidgetItem* item = ui->listWidget_2->item(row);
        if (item != nullptr && item->data(Qt::UserRole).toInt() == reminderId) {
            ui->listWidget_2->setCurrentItem(item);
            ui->listWidget_2->scrollToItem(item);
            return;
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

    loadStoredData();
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

    loadStoredData();
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
        loadStoredData();
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
        loadStoredData();
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
    loadStoredData();
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
        loadStoredData();
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
        loadStoredData();
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
    loadStoredData();
}

void homeScreen::openSettings()
{
    settings settingsDialog(this);
    settingsDialog.exec();
}

void homeScreen::openGlobalSearch()
{
    bool accepted = false;
    const QString query = QInputDialog::getText(
        this,
        "Global search",
        "Search across notes, tasks, reminders and schedule:",
        QLineEdit::Normal,
        "",
        &accepted
    ).trimmed();

    if (!accepted || query.isEmpty()) {
        return;
    }

    taskData_.Load();
    reminderData_.Load();

    std::vector<GlobalSearchResult> results;

    for (const Task& task : taskData_.Data()) {
        if (!searchTextMatches(query, {
                QString::fromStdString(task.GetName()),
                QString::fromStdString(task.GetDescription())
            })) {
            continue;
        }

        GlobalSearchResult result;
        result.type = SearchResultType::Task;
        result.id = task.GetTaskId();
        result.title = QString::fromStdString(task.GetName()).trimmed().isEmpty()
            ? QString("Task %1").arg(task.GetTaskId())
            : QString::fromStdString(task.GetName()).trimmed();
        result.subtitle = task.GetStatus()
            ? QString("Completed task")
            : QString::fromStdString(task.GetDescription());
        results.push_back(result);
    }

    for (const Reminder& reminder : reminderData_.Data()) {
        if (!searchTextMatches(query, {
                QString::fromStdString(reminder.GetName()),
                QString::fromStdString(reminder.GetDescription()),
                QString::fromStdString(reminder.GetDue())
            })) {
            continue;
        }

        GlobalSearchResult result;
        result.type = SearchResultType::Reminder;
        result.id = reminder.GetTaskId();
        result.title = QString::fromStdString(reminder.GetName()).trimmed().isEmpty()
            ? QString("Reminder %1").arg(reminder.GetTaskId())
            : QString::fromStdString(reminder.GetName()).trimmed();
        result.subtitle = QString("Due %1").arg(QString::fromStdString(reminder.GetDue()));
        results.push_back(result);
    }

    NoteData searchNoteData;
    if (searchNoteData.Load()) {
        for (const Note& note : searchNoteData.Data()) {
            if (!searchTextMatches(query, {
                    QString::fromStdString(note.GetTitle()),
                    QString::fromStdString(note.GetContent())
                })) {
                continue;
            }

            GlobalSearchResult result;
            result.type = SearchResultType::Note;
            result.id = note.GetId();
            result.title = QString::fromStdString(note.GetTitle()).trimmed().isEmpty()
                ? (note.IsFolder() ? QString("Folder %1").arg(note.GetId()) : QString("Note %1").arg(note.GetId()))
                : QString::fromStdString(note.GetTitle()).trimmed();
            result.subtitle = note.IsFolder()
                ? QString("Folder")
                : QString::fromStdString(note.GetContent()).left(80);
            results.push_back(result);
        }
    }

    QFile scheduleFile(readableDataFilePath("data/schedule.json"));
    if (scheduleFile.open(QIODevice::ReadOnly)) {
        const QJsonDocument document = QJsonDocument::fromJson(scheduleFile.readAll());
        scheduleFile.close();

        QJsonArray items;
        if (document.isObject()) {
            items = document.object().value("items").toArray();
        } else if (document.isArray()) {
            items = document.array();
        }

        for (const QJsonValue& value : items) {
            if (!value.isObject()) {
                continue;
            }

            const QJsonObject item = value.toObject();
            const QString day = item.value("day").toString().trimmed();
            const QString time = item.value("time").toString().trimmed();
            const QString subject = item.value("subject").toString().trimmed();
            const QString location = item.value("location").toString().trimmed();

            if (!searchTextMatches(query, {day, time, subject, location})) {
                continue;
            }

            GlobalSearchResult result;
            result.type = SearchResultType::ScheduleEntry;
            result.id = 0;
            result.title = subject.isEmpty() ? QString("Schedule block") : subject;
            result.subtitle = QString("%1 | %2 | %3").arg(day, time, location);
            result.day = day;
            result.time = time;
            result.subject = subject;
            result.location = location;
            results.push_back(result);
        }
    }

    if (results.empty()) {
        QMessageBox::information(this, "Global search", "No results found.");
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle("Search results");
    dialog.resize(760, 420);

    QVBoxLayout* rootLayout = new QVBoxLayout(&dialog);
    QLabel* titleLabel = new QLabel(QString("Results for \"%1\"").arg(query), &dialog);
    QListWidget* resultsList = new QListWidget(&dialog);
    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);

    titleLabel->setWordWrap(true);
    titleLabel->setStyleSheet("font-size: 18px; font-weight: 600;");
    rootLayout->addWidget(titleLabel);
    rootLayout->addWidget(resultsList);
    rootLayout->addWidget(buttonBox);

    for (std::size_t i = 0; i < results.size(); ++i) {
        const GlobalSearchResult& result = results[i];
        QListWidgetItem* item = new QListWidgetItem(searchResultLabel(result), resultsList);
        item->setData(Qt::UserRole, static_cast<int>(i));
        item->setToolTip(searchResultSecondaryText(result));
        if (!searchResultSecondaryText(result).isEmpty()) {
            item->setText(QString("%1\n%2").arg(searchResultLabel(result), searchResultSecondaryText(result)));
        }
    }

    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(resultsList, &QListWidget::itemDoubleClicked, &dialog, [&](QListWidgetItem* item) {
        if (item == nullptr) {
            return;
        }

        const int resultIndex = item->data(Qt::UserRole).toInt();
        if (resultIndex < 0 || static_cast<std::size_t>(resultIndex) >= results.size()) {
            return;
        }

        const GlobalSearchResult& result = results[static_cast<std::size_t>(resultIndex)];
        if (result.type == SearchResultType::Task) {
            focusTaskResult(result.id);
            dialog.accept();
            return;
        }

        if (result.type == SearchResultType::Reminder) {
            focusReminderResult(result.id);
            dialog.accept();
            return;
        }

        if (result.type == SearchResultType::Note) {
            if (notesWindow_ == nullptr) {
                notesWindow_ = new notes(this);
            }

            hide();
            presentWindow(notesWindow_);
            notesWindow_->openSearchResult(result.id);
            dialog.accept();
            return;
        }

        if (result.type == SearchResultType::ScheduleEntry) {
            if (scheduleWindow_ == nullptr) {
                scheduleWindow_ = new schedule(this);
            }

            scheduleWindow_->load();
            hide();
            presentWindow(scheduleWindow_);
            scheduleWindow_->focusEntry(result.day, result.time, result.subject, result.location);
            dialog.accept();
        }
    });

    dialog.exec();
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
