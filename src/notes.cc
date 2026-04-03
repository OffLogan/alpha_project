#include "../include/homescreen.h"
#include "../include/notesEditor.h"
#include "../include/pdfGestor.h"
#include "notes.h"
#include "ui_notes.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QInputDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QVariant>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace {
QString downloadsFilePath(const QString& baseName, const QString& extension)
{
    QString downloadsDir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (downloadsDir.isEmpty()) {
        downloadsDir = QDir::homePath();
    }

    QDir directory(downloadsDir);
    const QString safeBaseName = baseName.trimmed().isEmpty() ? QString("note") : baseName;
    QString candidate = directory.filePath(QString("%1.%2").arg(safeBaseName, extension));
    int suffix = 1;

    while (QFileInfo::exists(candidate)) {
        candidate = directory.filePath(QString("%1_%2.%3").arg(safeBaseName).arg(suffix).arg(extension));
        ++suffix;
    }

    return candidate;
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

bool noteIsDescendantOf(const NoteData& noteData, int noteId, int potentialAncestorId)
{
    if (noteId <= 0 || potentialAncestorId <= 0) {
        return false;
    }

    const Note* current = noteData.FindById(noteId);
    std::unordered_set<int> visitedIds;

    while (current != nullptr && current->GetParentId() > 0) {
        if (!visitedIds.insert(current->GetId()).second) {
            return false;
        }

        if (current->GetParentId() == potentialAncestorId) {
            return true;
        }

        current = noteData.FindById(current->GetParentId());
    }

    return false;
}
}

notes::notes(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::notes)
    , noteData_()
    , editorWindow_(nullptr)
{
    ui->setupUi(this);

    connect(ui->pushButton, &QPushButton::clicked, this, &notes::goBackHome);
    connect(ui->newNoteButton, &QPushButton::clicked, this, &notes::createNote);
    connect(ui->newFolderButton, &QPushButton::clicked, this, &notes::createFolder);
    connect(ui->renameFolderButton, &QPushButton::clicked, this, &notes::renameSelectedFolder);
    connect(ui->moveNoteButton, &QPushButton::clicked, this, &notes::moveSelectedNote);
    connect(ui->deleteNoteButton, &QPushButton::clicked, this, &notes::deleteSelectedNote);
    connect(ui->downloadTxtButton, &QPushButton::clicked, this, &notes::downloadSelectedNoteAsText);
    connect(ui->downloadPdfButton, &QPushButton::clicked, this, &notes::downloadSelectedNoteAsPdf);
    connect(ui->treeWidget, &QTreeWidget::itemDoubleClicked, this, &notes::openNote);

    ui->treeWidget->setHeaderLabel("Notes");

    noteData_.Load();
    updateResponsiveLayout();
    loadNotes();
}

notes::~notes()
{
    delete ui;
}

void notes::goBackHome()
{
    hide();

    if (parentWidget() != nullptr) {
        presentWindow(parentWidget());
    }
}

void notes::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    updateResponsiveLayout();
}

void notes::createNote()
{
    Note note("", "", noteData_.ParentIdForCreation(selectedTreeItemId()));
    if (!note.SetId(noteData_.NextId())) {
        QMessageBox::warning(this, "Notes", "Could not generate a valid id for the note.");
        return;
    }

    if (!noteData_.AddNote(note)) {
        QMessageBox::warning(this, "Notes", "Could not save the note in notes.json.");
        return;
    }

    refreshTree();

    if (editorWindow_ == nullptr) {
        editorWindow_ = new notesEditor(this);
        connect(editorWindow_, &notesEditor::noteSaved, this, &notes::refreshTree);
    }

    editorWindow_->OpenNote(note.GetId(), &noteData_);
}

void notes::createFolder()
{
    bool accepted = false;
    const QString folderName = QInputDialog::getText(
        this,
        "New folder",
        "Folder name:",
        QLineEdit::Normal,
        "",
        &accepted
    ).trimmed();

    if (!accepted) {
        return;
    }

    if (folderName.isEmpty()) {
        QMessageBox::warning(this, "Notes", "The folder name cannot be empty.");
        return;
    }

    const QString parentFolderName = QInputDialog::getText(
        this,
        "Parent folder",
        "Parent folder name (optional):",
        QLineEdit::Normal,
        "",
        &accepted
    ).trimmed();

    if (!accepted) {
        return;
    }

    int parentId = 0;
    if (!parentFolderName.isEmpty()) {
        parentId = folderIdByName(parentFolderName);
        if (parentId < 0) {
            QMessageBox::warning(this, "Notes", "There is more than one folder with that name. Use a unique folder name first.");
            return;
        }

        if (parentId == 0) {
            QMessageBox::warning(this, "Notes", "The parent folder name was not found.");
            return;
        }
    }

    Note folder(folderName.toStdString(), "", parentId);
    folder.SetIsFolder(true);

    if (!folder.SetId(noteData_.NextId())) {
        QMessageBox::warning(this, "Notes", "Could not generate a valid id for the folder.");
        return;
    }

    if (!noteData_.AddNote(folder)) {
        QMessageBox::warning(this, "Notes", "Could not save the folder in notes.json.");
        return;
    }

    refreshTree();
}

void notes::renameSelectedFolder()
{
    QTreeWidgetItem *selectedItem = ui->treeWidget->currentItem();
    if (selectedItem == nullptr) {
        QMessageBox::warning(this, "Notes", "Select a folder first.");
        return;
    }

    if (!selectedItem->data(0, Qt::UserRole + 1).toBool()) {
        QMessageBox::warning(this, "Notes", "Only folders can be renamed with this action.");
        return;
    }

    const int folderId = selectedItem->data(0, Qt::UserRole).toInt();
    Note* folder = noteData_.FindById(folderId);
    if (folder == nullptr || !folder->IsFolder()) {
        QMessageBox::warning(this, "Notes", "The selected folder could not be found.");
        return;
    }

    bool accepted = false;
    const QString updatedName = QInputDialog::getText(
        this,
        "Rename folder",
        "New folder name:",
        QLineEdit::Normal,
        QString::fromStdString(folder->GetTitle()),
        &accepted
    ).trimmed();

    if (!accepted) {
        return;
    }

    if (updatedName.isEmpty()) {
        QMessageBox::warning(this, "Notes", "The folder name cannot be empty.");
        return;
    }

    Note updatedFolder = *folder;
    if (!updatedFolder.SetTitle(updatedName.toStdString())) {
        QMessageBox::warning(this, "Notes", "The folder name is not valid.");
        return;
    }

    if (!noteData_.UpdateNote(updatedFolder)) {
        QMessageBox::warning(this, "Notes", "Could not rename the selected folder.");
        return;
    }

    refreshTree();
}

void notes::moveSelectedNote()
{
    QTreeWidgetItem *selectedItem = ui->treeWidget->currentItem();
    if (selectedItem == nullptr) {
        QMessageBox::warning(this, "Notes", "Select a note or folder first.");
        return;
    }

    const int itemId = selectedItem->data(0, Qt::UserRole).toInt();
    Note* item = noteData_.FindById(itemId);
    if (item == nullptr) {
        QMessageBox::warning(this, "Notes", "The selected item could not be found.");
        return;
    }

    QStringList folderOptions;
    folderOptions << "Root";
    std::unordered_map<QString, int> folderIdByOption;
    folderIdByOption[QString("Root")] = 0;

    std::vector<Note> folders = noteData_.Data();
    std::sort(folders.begin(), folders.end(), [](const Note& left, const Note& right) {
        return QString::fromStdString(left.GetTitle()).localeAwareCompare(QString::fromStdString(right.GetTitle())) < 0;
    });

    for (const Note& candidate : folders) {
        if (!candidate.IsFolder()) {
            continue;
        }

        if (candidate.GetId() == item->GetId()) {
            continue;
        }

        if (noteIsDescendantOf(noteData_, candidate.GetId(), item->GetId())) {
            continue;
        }

        const QString folderTitle = QString::fromStdString(candidate.GetTitle()).trimmed().isEmpty()
            ? QString("Untitled folder %1").arg(candidate.GetId())
            : QString::fromStdString(candidate.GetTitle()).trimmed();
        const QString optionLabel = QString("%1 (id %2)").arg(folderTitle).arg(candidate.GetId());

        folderOptions << optionLabel;
        folderIdByOption[optionLabel] = candidate.GetId();
    }

    bool accepted = false;
    const QString selectedDestination = QInputDialog::getItem(
        this,
        item->IsFolder() ? "Move folder" : "Move note",
        "Destination folder:",
        folderOptions,
        0,
        false,
        &accepted
    );

    if (!accepted || selectedDestination.isEmpty()) {
        return;
    }

    const int destinationParentId = folderIdByOption[selectedDestination];
    if (destinationParentId == item->GetParentId()) {
        return;
    }

    Note updatedItem = *item;
    if (!updatedItem.SetParentId(destinationParentId)) {
        QMessageBox::warning(this, "Notes", "The destination folder is not valid.");
        return;
    }

    if (!noteData_.UpdateNote(updatedItem)) {
        QMessageBox::warning(this, "Notes", "Could not move the selected item.");
        return;
    }

    refreshTree();
}

void notes::deleteSelectedNote()
{
    QTreeWidgetItem *selectedItem = ui->treeWidget->currentItem();
    if (selectedItem == nullptr) {
        QMessageBox::warning(this, "Notes", "Select a note to delete.");
        return;
    }

    const int noteId = selectedItem->data(0, Qt::UserRole).toInt();
    if (noteId <= 0) {
        QMessageBox::warning(this, "Notes", "The selected note is not valid.");
        return;
    }

    const QMessageBox::StandardButton result = QMessageBox::question(
        this,
        "Delete note",
        "This will delete the selected note and all its child notes. Do you want to continue?",
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );

    if (result != QMessageBox::Yes) {
        return;
    }

    if (!noteData_.RemoveNoteCascade(noteId)) {
        QMessageBox::warning(this, "Notes", "The selected note could not be deleted.");
        return;
    }

    if (editorWindow_ != nullptr) {
        editorWindow_->hide();
    }

    refreshTree();
}

void notes::openNote(QTreeWidgetItem *item, int)
{
    if (item == nullptr) {
        return;
    }

    if (item->data(0, Qt::UserRole + 1).toBool()) {
        return;
    }

    const int noteId = item->data(0, Qt::UserRole).toInt();
    if (noteId <= 0) {
        return;
    }

    if (editorWindow_ == nullptr) {
        editorWindow_ = new notesEditor(this);
        connect(editorWindow_, &notesEditor::noteSaved, this, &notes::refreshTree);
    }

    editorWindow_->OpenNote(noteId, &noteData_);
}

void notes::refreshTree()
{
    noteData_.Load();
    loadNotes();
}

void notes::openSearchResult(int noteId)
{
    refreshTree();

    QTreeWidgetItemIterator iterator(ui->treeWidget);
    while (*iterator != nullptr) {
        QTreeWidgetItem* item = *iterator;
        if (item->data(0, Qt::UserRole).toInt() == noteId) {
            ui->treeWidget->setCurrentItem(item);
            ui->treeWidget->scrollToItem(item);

            if (!item->data(0, Qt::UserRole + 1).toBool()) {
                openNote(item, 0);
            }
            return;
        }
        ++iterator;
    }
}

void notes::downloadSelectedNoteAsText()
{
    const Note* note = selectedNote();
    if (note == nullptr) {
        return;
    }

    const QString title = QString::fromStdString(note->GetTitle()).trimmed().isEmpty()
        ? QString("note_%1").arg(note->GetId())
        : QString::fromStdString(note->GetTitle()).trimmed();
    const QString filePath = downloadsFilePath(title, "txt");

    QString errorMessage;
    if (!pdfGestor::exportNoteToText(filePath,
                                     title,
                                     QString::fromStdString(note->GetContent()),
                                     &errorMessage)) {
        QMessageBox::warning(this, "Notes", errorMessage);
        return;
    }

    QMessageBox::information(this, "Notes", QString("TXT file created in:\n%1").arg(filePath));
}

void notes::downloadSelectedNoteAsPdf()
{
    const Note* note = selectedNote();
    if (note == nullptr) {
        return;
    }

    const QString title = QString::fromStdString(note->GetTitle()).trimmed().isEmpty()
        ? QString("note_%1").arg(note->GetId())
        : QString::fromStdString(note->GetTitle()).trimmed();
    const QString filePath = downloadsFilePath(title, "pdf");

    QString errorMessage;
    if (!pdfGestor::exportNoteToPdf(filePath,
                                    title,
                                    QString::fromStdString(note->GetContent()),
                                    &errorMessage)) {
        QMessageBox::warning(this, "Notes", errorMessage);
        return;
    }

    QMessageBox::information(this, "Notes", QString("PDF file created in:\n%1").arg(filePath));
}

void notes::loadNotes()
{
    ui->treeWidget->clear();

    const std::vector<Note> storedNotes = noteData_.Data();
    std::unordered_map<int, QTreeWidgetItem*> itemsById;

    for (const Note& note : storedNotes) {
        QTreeWidgetItem *item = new QTreeWidgetItem();
        const QString title = QString::fromStdString(note.GetTitle()).trimmed();
        const QString fallbackTitle = note.IsFolder()
            ? QString("Untitled folder %1").arg(note.GetId())
            : QString("Untitled note %1").arg(note.GetId());
        const QString itemTitle = title.isEmpty() ? fallbackTitle : title;
        item->setText(0, note.IsFolder() ? QString("[Folder] %1").arg(itemTitle) : itemTitle);
        item->setData(0, Qt::UserRole, note.GetId());
        item->setData(0, Qt::UserRole + 1, note.IsFolder());
        itemsById[note.GetId()] = item;
    }

    for (const Note& note : storedNotes) {
        QTreeWidgetItem *item = itemsById[note.GetId()];
        const auto parentItem = itemsById.find(note.GetParentId());

        if (note.GetParentId() > 0 && parentItem != itemsById.end()) {
            parentItem->second->addChild(item);
        } else {
            ui->treeWidget->addTopLevelItem(item);
        }
    }

    ui->treeWidget->expandAll();
}

void notes::updateResponsiveLayout()
{
    if (ui == nullptr || ui->centralwidget == nullptr) {
        return;
    }

    const int width = ui->centralwidget->width();
    const int height = ui->centralwidget->height();
    const int margin = 30;
    const int top = 35;
    const int buttonY = 175;
    const int buttonHeight = 32;
    const int buttonGap = 14;
    const int homeWidth = 113;
    const int actionButtonCount = 7;
    const int availableButtonWidth = std::max(720, width - (margin * 2) - homeWidth - 32);
    const int actionButtonWidth = std::max(90, std::min(125, (availableButtonWidth - (buttonGap * (actionButtonCount - 1))) / actionButtonCount));

    ui->label->setGeometry(margin, top, std::min(420, width - (margin * 2)), 51);
    ui->label_2->setGeometry(margin, 85, std::min(520, width - (margin * 2)), 61);

    int actionX = margin;
    auto placeButton = [&](QPushButton* button) {
        if (button == nullptr) {
            return;
        }

        button->setGeometry(actionX, buttonY, actionButtonWidth, buttonHeight);
        actionX += actionButtonWidth + buttonGap;
    };

    placeButton(ui->newNoteButton);
    placeButton(ui->newFolderButton);
    placeButton(ui->renameFolderButton);
    placeButton(ui->moveNoteButton);
    placeButton(ui->deleteNoteButton);
    placeButton(ui->downloadTxtButton);

    if (ui->downloadPdfButton != nullptr) {
        ui->downloadPdfButton->setGeometry(actionX, buttonY, actionButtonWidth, buttonHeight);
    }

    const int treeTop = 235;
    const int bottomReserved = 88;
    ui->treeWidget->setGeometry(margin,
                                treeTop,
                                std::max(420, width - (margin * 2)),
                                std::max(260, height - treeTop - bottomReserved));

    ui->pushButton->setGeometry(width - margin - homeWidth,
                                height - 40 - buttonHeight,
                                homeWidth,
                                buttonHeight);
}

const Note* notes::selectedNote()
{
    QTreeWidgetItem *selectedItem = ui->treeWidget->currentItem();
    if (selectedItem == nullptr) {
        QMessageBox::warning(this, "Notes", "Select a note first.");
        return nullptr;
    }

    if (selectedItem->data(0, Qt::UserRole + 1).toBool()) {
        QMessageBox::warning(this, "Notes", "Folders cannot be exported.");
        return nullptr;
    }

    const int noteId = selectedItem->data(0, Qt::UserRole).toInt();
    if (noteId <= 0) {
        QMessageBox::warning(this, "Notes", "The selected note is not valid.");
        return nullptr;
    }

    const Note* note = noteData_.FindById(noteId);
    if (note == nullptr) {
        QMessageBox::warning(this, "Notes", "The selected note could not be found.");
    }

    return note;
}

int notes::selectedTreeItemId() const
{
    QTreeWidgetItem *selectedItem = ui->treeWidget->currentItem();
    if (selectedItem == nullptr) {
        return 0;
    }

    return selectedItem->data(0, Qt::UserRole).toInt();
}

int notes::folderIdByName(const QString& folderName) const
{
    int foundId = 0;

    for (const Note& note : noteData_.Data()) {
        if (!note.IsFolder()) {
            continue;
        }

        if (QString::fromStdString(note.GetTitle()).trimmed().compare(folderName, Qt::CaseInsensitive) != 0) {
            continue;
        }

        if (foundId != 0) {
            return -1;
        }

        foundId = note.GetId();
    }

    return foundId;
}
