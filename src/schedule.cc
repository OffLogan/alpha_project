#include "../include/schedule.h"
#include "../include/appPaths.h"
#include "ui_schedule.h"

#include <QDir>
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

    ui->dayComboBox->addItems({"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"});
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
