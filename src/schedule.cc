#include "../include/schedule.h"
#include "../include/appPaths.h"
#include "ui_schedule.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidgetItem>

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
    if (parentWidget() != nullptr) {
        parentWidget()->show();
    }

    hide();
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

bool schedule::writeSchedule(bool showSuccessMessage)
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

    auto writeJsonFile = [&](const QString& filePath) {
        QDir().mkpath(QFileInfo(filePath).path());
        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            return false;
        }

        const qint64 writtenBytes = file.write(payload);
        file.close();
        return writtenBytes != -1;
    };

    const QString filePath = writableDataFilePath("data/schedule.json");
    if (!writeJsonFile(filePath)) {
        QMessageBox::warning(this, "Schedule", "Could not save the schedule.");
        return false;
    }

    const QString legacyPath = legacyReadableDataFilePath("data/schedule.json");
    if (!legacyPath.isEmpty() && legacyPath != filePath && !writeJsonFile(legacyPath)) {
        QMessageBox::warning(this, "Schedule", "Could not save the schedule.");
        return false;
    }

    if (showSuccessMessage) {
        QMessageBox::information(this, "Schedule", "Schedule saved successfully.");
    }

    return true;
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
    QFile file(scheduleFilePath());
    const QSignalBlocker blocker(ui->scheduleTableWidget);
    ui->scheduleTableWidget->setRowCount(0);

    if (!file.exists()) {
        return;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "Schedule", "Could not open the saved schedule.");
        return;
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!document.isObject()) {
        return;
    }

    const QJsonArray items = document.object().value("items").toArray();

    for (const QJsonValue& value : items) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject item = value.toObject();
        appendRow(item.value("day").toString(),
                  item.value("time").toString(),
                  item.value("subject").toString(),
                  item.value("location").toString());
    }
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
