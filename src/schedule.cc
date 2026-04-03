#include "../include/schedule.h"
#include "../include/appPaths.h"
#include "ui_schedule.h"

#include <algorithm>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonObject>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStandardPaths>
#include <QTableWidgetItem>
#include <QVector>

namespace {
QString normalizedScheduleDay(const QString& rawDay)
{
    const QString day = rawDay.trimmed();

    if (day.compare("monday", Qt::CaseInsensitive) == 0 || day.compare("lunes", Qt::CaseInsensitive) == 0) {
        return "Monday";
    }
    if (day.compare("tuesday", Qt::CaseInsensitive) == 0 || day.compare("martes", Qt::CaseInsensitive) == 0) {
        return "Tuesday";
    }
    if (day.compare("wednesday", Qt::CaseInsensitive) == 0 || day.compare("miercoles", Qt::CaseInsensitive) == 0
        || day.compare("miércoles", Qt::CaseInsensitive) == 0) {
        return "Wednesday";
    }
    if (day.compare("thursday", Qt::CaseInsensitive) == 0 || day.compare("jueves", Qt::CaseInsensitive) == 0) {
        return "Thursday";
    }
    if (day.compare("friday", Qt::CaseInsensitive) == 0 || day.compare("viernes", Qt::CaseInsensitive) == 0) {
        return "Friday";
    }
    if (day.compare("saturday", Qt::CaseInsensitive) == 0 || day.compare("sabado", Qt::CaseInsensitive) == 0
        || day.compare("sábado", Qt::CaseInsensitive) == 0) {
        return "Saturday";
    }
    if (day.compare("sunday", Qt::CaseInsensitive) == 0 || day.compare("domingo", Qt::CaseInsensitive) == 0) {
        return "Sunday";
    }

    return day;
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

schedule::schedule(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::schedule)
{
    ui->setupUi(this);
    ui->centralwidget->installEventFilter(this);

    ui->dayComboBox->addItems({"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"});
    ui->dayComboBox->setStyleSheet(
        "QComboBox { padding-left: 10px; padding-right: 28px; }"
        "QComboBox QAbstractItemView { padding: 4px; }"
    );
    ui->scheduleTableWidget->setColumnCount(4);
    ui->scheduleTableWidget->setHorizontalHeaderLabels({"Day", "Time", "Subject", "Location"});
    ui->scheduleTableWidget->horizontalHeader()->setStretchLastSection(true);
    ui->scheduleTableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->scheduleTableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->scheduleTableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->scheduleTableWidget->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);

    connect(ui->pushButton_1, &QPushButton::clicked, this, &schedule::goBackHome);
    connect(ui->addEntryButton, &QPushButton::clicked, this, &schedule::addEntry);
    connect(ui->deleteEntryButton, &QPushButton::clicked, this, &schedule::deleteSelectedEntry);
    connect(ui->saveScheduleButton, &QPushButton::clicked, this, &schedule::saveSchedule);
    connect(ui->exportScheduleButton, &QPushButton::clicked, this, &schedule::exportSchedule);
    connect(ui->importScheduleButton, &QPushButton::clicked, this, &schedule::importSchedule);
    connect(ui->clearScheduleButton, &QPushButton::clicked, this, &schedule::clearSchedule);
    connect(ui->scheduleTableWidget, &QTableWidget::itemChanged, this, [this](QTableWidgetItem*) {
        writeSchedule(false);
    });

    updateResponsiveLayout();
    loadSchedule();
}

schedule::~schedule()
{
    delete ui;
}

void schedule::load()
{
    loadSchedule();
}

void schedule::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    updateResponsiveLayout();
}

void schedule::focusEntry(const QString& day,
                          const QString& time,
                          const QString& subject,
                          const QString& location)
{
    loadSchedule();

    for (int row = 0; row < ui->scheduleTableWidget->rowCount(); ++row) {
        const QString rowDay = ui->scheduleTableWidget->item(row, 0) != nullptr
            ? ui->scheduleTableWidget->item(row, 0)->text().trimmed()
            : QString();
        const QString rowTime = ui->scheduleTableWidget->item(row, 1) != nullptr
            ? ui->scheduleTableWidget->item(row, 1)->text().trimmed()
            : QString();
        const QString rowSubject = ui->scheduleTableWidget->item(row, 2) != nullptr
            ? ui->scheduleTableWidget->item(row, 2)->text().trimmed()
            : QString();
        const QString rowLocation = ui->scheduleTableWidget->item(row, 3) != nullptr
            ? ui->scheduleTableWidget->item(row, 3)->text().trimmed()
            : QString();

        if (rowDay.compare(day.trimmed(), Qt::CaseInsensitive) != 0 ||
            rowTime.compare(time.trimmed(), Qt::CaseInsensitive) != 0 ||
            rowSubject.compare(subject.trimmed(), Qt::CaseInsensitive) != 0 ||
            rowLocation.compare(location.trimmed(), Qt::CaseInsensitive) != 0) {
            continue;
        }

        ui->scheduleTableWidget->setCurrentCell(row, 0);
        ui->scheduleTableWidget->selectRow(row);
        ui->scheduleTableWidget->scrollToItem(ui->scheduleTableWidget->item(row, 0));
        return;
    }
}

bool schedule::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == ui->centralwidget && event != nullptr && event->type() == QEvent::MouseButtonPress) {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        const QPoint tablePosition = ui->scheduleTableWidget->mapFrom(ui->centralwidget, mouseEvent->pos());

        if (!ui->scheduleTableWidget->rect().contains(tablePosition)) {
            ui->scheduleTableWidget->clearSelection();
            ui->scheduleTableWidget->setCurrentCell(-1, -1);
        }
    }

    return QMainWindow::eventFilter(watched, event);
}

void schedule::goBackHome()
{
    hide();

    if (parentWidget() != nullptr) {
        presentWindow(parentWidget());
    }
}

void schedule::addEntry()
{
    const QString day = ui->dayComboBox->currentText().trimmed();
    const QString time = ui->timeLineEdit->text().trimmed();
    const QString subject = ui->subjectLineEdit->text().trimmed();
    const QString location = ui->locationLineEdit->text().trimmed();

    if (time.isEmpty() || subject.isEmpty()) {
        QMessageBox::warning(this, "Schedule", "Write at least the time and subject for the schedule block.");
        return;
    }

    appendRow(day, time, subject, location);
    clearInputs();
    writeSchedule(false);
}

void schedule::deleteSelectedEntry()
{
    const int row = ui->scheduleTableWidget->currentRow();
    if (row < 0) {
        QMessageBox::warning(this, "Schedule", "Select a row to delete.");
        return;
    }

    ui->scheduleTableWidget->removeRow(row);
    writeSchedule(false);
}

void schedule::saveSchedule()
{
    writeSchedule(true);
}

void schedule::exportSchedule()
{
    const QString exportPath = defaultExportSchedulePath();
    if (!writeScheduleToFile(exportPath)) {
        QMessageBox::warning(this, "Schedule", "Could not export the schedule.");
        return;
    }

    QMessageBox::information(this, "Schedule", QString("Schedule exported to:\n%1").arg(exportPath));
}

void schedule::importSchedule()
{
    QString downloadsDir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (downloadsDir.trimmed().isEmpty()) {
        downloadsDir = QDir::homePath();
    }

    const QString filePath = QFileDialog::getOpenFileName(
        this,
        "Import schedule",
        downloadsDir,
        "JSON files (*.json);;All files (*)"
    );

    if (filePath.trimmed().isEmpty()) {
        return;
    }

    if (!loadScheduleFromFile(filePath, true)) {
        return;
    }

    if (!writeSchedule(false)) {
        return;
    }

    QMessageBox::information(this, "Schedule", "Schedule imported successfully.");
}

bool schedule::writeSchedule(bool showSuccessMessage)
{
    const QString filePath = writableDataFilePath("data/schedule.json");
    if (!writeScheduleToFile(filePath)) {
        QMessageBox::warning(this, "Schedule", "Could not save the schedule.");
        return false;
    }

    const QString legacyPath = legacyReadableDataFilePath("data/schedule.json");
    if (!legacyPath.isEmpty() && legacyPath != filePath && !writeScheduleToFile(legacyPath)) {
        QMessageBox::warning(this, "Schedule", "Could not save the schedule.");
        return false;
    }

    if (showSuccessMessage) {
        QMessageBox::information(this, "Schedule", "Schedule saved successfully.");
    }

    return true;
}

bool schedule::writeScheduleToFile(const QString& filePath)
{
    QJsonArray items;

    for (int row = 0; row < ui->scheduleTableWidget->rowCount(); ++row) {
        QJsonObject item;
        item.insert("day", ui->scheduleTableWidget->item(row, 0) != nullptr
                               ? ui->scheduleTableWidget->item(row, 0)->text().trimmed()
                               : QString());
        item.insert("time", ui->scheduleTableWidget->item(row, 1) != nullptr
                                ? ui->scheduleTableWidget->item(row, 1)->text().trimmed()
                                : QString());
        item.insert("subject", ui->scheduleTableWidget->item(row, 2) != nullptr
                                   ? ui->scheduleTableWidget->item(row, 2)->text().trimmed()
                                   : QString());
        item.insert("location", ui->scheduleTableWidget->item(row, 3) != nullptr
                                    ? ui->scheduleTableWidget->item(row, 3)->text().trimmed()
                                    : QString());
        items.append(item);
    }

    QJsonObject root;
    root.insert("version", 1);
    root.insert("items", items);

    const QByteArray payload = QJsonDocument(root).toJson(QJsonDocument::Indented);

    QDir().mkpath(QFileInfo(filePath).path());
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    const qint64 writtenBytes = file.write(payload);
    file.close();
    return writtenBytes != -1;
}

void schedule::clearSchedule()
{
    const QMessageBox::StandardButton result = QMessageBox::question(
        this,
        "Clear schedule",
        "This will remove every schedule block from the table. Do you want to continue?",
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );

    if (result != QMessageBox::Yes) {
        return;
    }

    ui->scheduleTableWidget->setRowCount(0);
    writeSchedule(false);
}

void schedule::loadSchedule()
{
    loadScheduleFromFile(scheduleFilePath(), true);
}

bool schedule::loadScheduleFromFile(const QString& filePath, bool showErrorMessage)
{
    QFile file(filePath);
    if (!file.exists()) {
        return true;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        if (showErrorMessage) {
            QMessageBox::warning(this, "Schedule", "Could not open the schedule file.");
        }
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    file.close();

    if (parseError.error != QJsonParseError::NoError) {
        if (showErrorMessage) {
            QMessageBox::warning(
                this,
                "Schedule",
                QString("The selected file is not valid JSON.\n%1").arg(parseError.errorString())
            );
        }
        return false;
    }

    QJsonArray items;
    if (document.isObject()) {
        items = document.object().value("items").toArray();
    } else if (document.isArray()) {
        items = document.array();
    } else {
        if (showErrorMessage) {
            QMessageBox::warning(this, "Schedule", "The selected file does not contain a valid schedule structure.");
        }
        return false;
    }

    struct ScheduleRow {
        QString day;
        QString time;
        QString subject;
        QString location;
    };
    QVector<ScheduleRow> rows;

    for (const QJsonValue& value : items) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject item = value.toObject();
        const QString day = normalizedScheduleDay(item.value("day").toString());
        const QString time = item.value("time").toString().trimmed();
        const QString subject = item.value("subject").toString().trimmed();
        const QString location = item.value("location").toString().trimmed();

        if (day.isEmpty() || time.isEmpty() || subject.isEmpty()) {
            continue;
        }

        rows.push_back({
            day,
            time,
            subject,
            location
        });
    }

    const QSignalBlocker blocker(ui->scheduleTableWidget);
    ui->scheduleTableWidget->setRowCount(0);

    for (const ScheduleRow& row : rows) {
        appendRow(row.day, row.time, row.subject, row.location);
    }

    return true;
}

void schedule::appendRow(const QString& day,
                         const QString& time,
                         const QString& subject,
                         const QString& location)
{
    const int row = ui->scheduleTableWidget->rowCount();
    ui->scheduleTableWidget->insertRow(row);
    ui->scheduleTableWidget->setItem(row, 0, new QTableWidgetItem(day));
    ui->scheduleTableWidget->setItem(row, 1, new QTableWidgetItem(time));
    ui->scheduleTableWidget->setItem(row, 2, new QTableWidgetItem(subject));
    ui->scheduleTableWidget->setItem(row, 3, new QTableWidgetItem(location));
}

void schedule::clearInputs()
{
    ui->timeLineEdit->clear();
    ui->subjectLineEdit->clear();
    ui->locationLineEdit->clear();
    ui->timeLineEdit->setFocus();
}

void schedule::updateResponsiveLayout()
{
    if (ui == nullptr || ui->centralwidget == nullptr) {
        return;
    }

    const int width = ui->centralwidget->width();
    const int height = ui->centralwidget->height();
    const int margin = 30;
    const int top = 35;
    const int buttonHeight = 32;
    const int topRowY = 175;
    const int bottomRowY = height - 40 - buttonHeight;
    const int bottomGap = 18;
    const int homeWidth = 111;

    ui->titleLabel->setGeometry(margin, top, std::min(420, width - (margin * 2)), 51);
    ui->descriptionLabel->setGeometry(margin, 85, std::min(520, width - (margin * 2)), 61);

    const int rightLimit = width - margin;
    const int topGap = 18;
    const int topAvailable = std::max(820, rightLimit - margin);
    int dayWidth = 150;
    int timeWidth = 140;
    int subjectWidth = std::max(180, static_cast<int>(topAvailable * 0.2));
    int locationWidth = std::max(160, static_cast<int>(topAvailable * 0.18));
    int addWidth = 130;
    int deleteWidth = 150;

    const int totalRequested = dayWidth + timeWidth + subjectWidth + locationWidth + addWidth + deleteWidth + (topGap * 5);
    const int overflow = totalRequested - (rightLimit - margin);
    if (overflow > 0) {
        subjectWidth = std::max(160, subjectWidth - overflow / 2);
        locationWidth = std::max(140, locationWidth - overflow / 2);
    }

    int x = margin;
    ui->dayComboBox->setGeometry(x, topRowY, dayWidth, buttonHeight); x += dayWidth + topGap;
    ui->timeLineEdit->setGeometry(x, topRowY, timeWidth, buttonHeight); x += timeWidth + topGap;
    ui->subjectLineEdit->setGeometry(x, topRowY, subjectWidth, buttonHeight); x += subjectWidth + topGap;
    ui->locationLineEdit->setGeometry(x, topRowY, locationWidth, buttonHeight); x += locationWidth + topGap;
    ui->addEntryButton->setGeometry(x, topRowY, addWidth, buttonHeight); x += addWidth + topGap;
    ui->deleteEntryButton->setGeometry(x, topRowY, deleteWidth, buttonHeight);

    ui->scheduleTableWidget->setGeometry(margin,
                                         235,
                                         std::max(500, width - (margin * 2)),
                                         std::max(280, bottomRowY - 235 - 24));

    const int bottomButtonWidth = 111;
    const int totalBottomWidth = (bottomButtonWidth * 5) + (bottomGap * 4);
    int bottomStartX = std::max(margin, width - margin - totalBottomWidth);

    ui->saveScheduleButton->setGeometry(bottomStartX, bottomRowY, bottomButtonWidth, buttonHeight);
    bottomStartX += bottomButtonWidth + bottomGap;
    ui->exportScheduleButton->setGeometry(bottomStartX, bottomRowY, bottomButtonWidth, buttonHeight);
    bottomStartX += bottomButtonWidth + bottomGap;
    ui->importScheduleButton->setGeometry(bottomStartX, bottomRowY, bottomButtonWidth, buttonHeight);
    bottomStartX += bottomButtonWidth + bottomGap;
    ui->clearScheduleButton->setGeometry(bottomStartX, bottomRowY, bottomButtonWidth, buttonHeight);
    bottomStartX += bottomButtonWidth + bottomGap;
    ui->pushButton_1->setGeometry(bottomStartX, bottomRowY, homeWidth, buttonHeight);
}

QString schedule::scheduleFilePath() const
{
    return readableDataFilePath("data/schedule.json");
}

QString schedule::defaultExportSchedulePath() const
{
    QString downloadsDir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (downloadsDir.trimmed().isEmpty()) {
        downloadsDir = QDir::homePath();
    }

    QDir directory(downloadsDir);
    QString candidate = directory.filePath("schedule_export.json");
    int suffix = 1;

    while (QFileInfo::exists(candidate)) {
        candidate = directory.filePath(QString("schedule_export_%1.json").arg(suffix));
        ++suffix;
    }

    return candidate;
}
