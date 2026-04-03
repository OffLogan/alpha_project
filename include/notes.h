#ifndef NOTES_H
#define NOTES_H

#include <QMainWindow>
#include <QResizeEvent>

#include "noteGestor.h"

class QTreeWidgetItem;
class notesEditor;

namespace Ui {
class notes;
}

class notes : public QMainWindow
{
    Q_OBJECT

public:
    explicit notes(QWidget *parent = nullptr);
    ~notes();
    void openSearchResult(int noteId);

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void goBackHome();
    void createNote();
    void createFolder();
    void renameSelectedFolder();
    void moveSelectedNote();
    void deleteSelectedNote();
    void downloadSelectedNoteAsText();
    void downloadSelectedNoteAsPdf();
    void openNote(QTreeWidgetItem *item, int column);
    void refreshTree();

private:
    void loadNotes();
    void updateResponsiveLayout();
    const Note* selectedNote();
    int selectedTreeItemId() const;
    int folderIdByName(const QString& folderName) const;

    Ui::notes *ui;
    NoteData noteData_;
    notesEditor *editorWindow_;
};

#endif // NOTES_H
