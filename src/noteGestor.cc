//NoteGestor.cc
//Implementation file for the note gestor class

#include "../include/noteGestor.h"
#include "../include/appPaths.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

NoteData::NoteData(const std::string& filePath)
    : notes_(), indexById_(), filePath_(filePath) {}

std::vector<Note> NoteData::Data() const
{
    return notes_;
}

void NoteData::RebuildIndex()
{
    indexById_.clear();

    for (std::size_t i = 0; i < notes_.size(); ++i) {
        indexById_[notes_[i].GetId()] = i;
    }
}

bool NoteData::Load()
{
    QFile file(readableDataFilePath(filePath_));

    if (!file.exists()) {
        notes_.clear();
        indexById_.clear();
        return Update();
    }

    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    const QByteArray rawData = file.readAll();
    file.close();

    if (rawData.trimmed().isEmpty()) {
        notes_.clear();
        indexById_.clear();
        return Update();
    }

    const QJsonDocument document = QJsonDocument::fromJson(rawData);
    if (!document.isObject()) {
        return false;
    }

    const QJsonValue itemsValue = document.object().value("items");
    if (!itemsValue.isArray()) {
        return false;
    }

    const QJsonArray items = itemsValue.toArray();
    std::vector<Note> loadedNotes;
    loadedNotes.reserve(static_cast<std::size_t>(items.size()));

    for (const QJsonValue& value : items) {
        if (!value.isObject()) {
            return false;
        }

        const QJsonObject noteObject = value.toObject();
        if (!noteObject.contains("id") || !noteObject.contains("title") ||
            !noteObject.contains("content") || !noteObject.contains("parentId")) {
            return false;
        }

        Note note;
        if (!note.SetId(noteObject.value("id").toInt())) {
            return false;
        }
        if (!note.SetParentId(noteObject.value("parentId").toInt())) {
            return false;
        }
        if (!note.SetTitle(noteObject.value("title").toString().toStdString())) {
            return false;
        }
        if (!note.SetContent(noteObject.value("content").toString().toStdString())) {
            return false;
        }
        note.SetIsFolder(noteObject.value("isFolder").toBool(false));

        loadedNotes.push_back(note);
    }

    notes_ = std::move(loadedNotes);
    RebuildIndex();
    return true;
}

bool NoteData::Update()
{
    QJsonArray items;

    for (const Note& note : notes_) {
        QJsonObject noteObject;
        noteObject.insert("id", note.GetId());
        noteObject.insert("parentId", note.GetParentId());
        noteObject.insert("title", QString::fromStdString(note.GetTitle()));
        noteObject.insert("content", QString::fromStdString(note.GetContent()));
        noteObject.insert("isFolder", note.IsFolder());
        items.append(noteObject);
    }

    QJsonObject rootObject;
    rootObject.insert("version", 1);
    rootObject.insert("items", items);

    const QJsonDocument document(rootObject);
    const QByteArray payload = document.toJson(QJsonDocument::Indented);

    auto writeJsonFile = [&](const QString& outputPath) {
        const QFileInfo fileInfo(outputPath);
        QDir().mkpath(fileInfo.path());

        QFile file(outputPath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            return false;
        }

        const qint64 writtenBytes = file.write(payload);
        file.close();
        return writtenBytes != -1;
    };

    const QString outputPath = writableDataFilePath(filePath_);
    if (!writeJsonFile(outputPath)) {
        return false;
    }

    const QString legacyPath = legacyReadableDataFilePath(filePath_);
    if (!legacyPath.isEmpty() && legacyPath != outputPath && !writeJsonFile(legacyPath)) {
        return false;
    }

    return true;
}

int NoteData::NextId() const
{
    int maxId = 0;

    for (const Note& note : notes_) {
        if (note.GetId() > maxId) {
            maxId = note.GetId();
        }
    }

    return maxId + 1;
}

bool NoteData::AddNote(const Note& note)
{
    if (note.GetId() <= 0 || indexById_.count(note.GetId()) > 0) {
        return false;
    }

    notes_.push_back(note);
    indexById_[note.GetId()] = notes_.size() - 1;
    return Update();
}

bool NoteData::RemoveNote(int id)
{
    const auto foundNote = indexById_.find(id);
    if (foundNote == indexById_.end()) {
        return false;
    }

    notes_.erase(notes_.begin() + static_cast<std::ptrdiff_t>(foundNote->second));
    RebuildIndex();
    return Update();
}

bool NoteData::RemoveNoteCascade(int id)
{
    if (indexById_.find(id) == indexById_.end()) {
        return false;
    }

    std::vector<int> idsToRemove = {id};

    for (std::size_t i = 0; i < idsToRemove.size(); ++i) {
        const int currentId = idsToRemove[i];

        for (const Note& note : notes_) {
            if (note.GetParentId() == currentId) {
                idsToRemove.push_back(note.GetId());
            }
        }
    }

    std::unordered_map<int, bool> removeById;
    for (const int noteId : idsToRemove) {
        removeById[noteId] = true;
    }

    std::vector<Note> remainingNotes;
    remainingNotes.reserve(notes_.size());

    for (const Note& note : notes_) {
        if (removeById.find(note.GetId()) == removeById.end()) {
            remainingNotes.push_back(note);
        }
    }

    notes_ = std::move(remainingNotes);
    RebuildIndex();
    return Update();
}

bool NoteData::UpdateNote(const Note& note)
{
    const auto foundNote = indexById_.find(note.GetId());
    if (foundNote == indexById_.end()) {
        return false;
    }

    notes_[foundNote->second] = note;
    return Update();
}

Note* NoteData::FindById(int id)
{
    const auto foundNote = indexById_.find(id);
    if (foundNote == indexById_.end()) {
        return nullptr;
    }

    return &notes_[foundNote->second];
}

const Note* NoteData::FindById(int id) const
{
    const auto foundNote = indexById_.find(id);
    if (foundNote == indexById_.end()) {
        return nullptr;
    }

    return &notes_[foundNote->second];
}

int NoteData::ParentIdForCreation(int selectedId) const
{
    if (selectedId <= 0) {
        return 0;
    }

    const Note* selectedNote = FindById(selectedId);
    if (selectedNote == nullptr) {
        return 0;
    }

    if (selectedNote->IsFolder()) {
        return selectedId;
    }

    return selectedNote->GetParentId();
}
