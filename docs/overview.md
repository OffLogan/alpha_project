# KMN Space Overview

## What KMN Space is
KMN Space is a desktop academic workspace built with C++ and Qt.
It is designed to keep common student workflows in one place:
- tasks
- reminders
- notes
- study schedule
- application preferences

## Main modules

### Home
The home screen is the operational hub of the app.
It currently allows the user to:
- create tasks
- create reminders
- edit tasks and reminders
- mark tasks and reminders as completed
- delete tasks and reminders with confirmation
- filter tasks and reminders
- sort reminders by due date
- launch the global search
- navigate to Notes, Schedule and Settings

### Notes
The notes module provides a tree-based structure for folders and notes.
It currently allows the user to:
- create notes
- create folders
- rename folders
- move notes and folders
- open a note in the editor
- export notes as TXT or PDF

### Schedule
The schedule module stores class or study blocks in a table.
It currently allows the user to:
- create schedule entries
- edit entries directly in the table
- delete selected entries
- save the schedule
- import and export schedule data as JSON
- clear the full table with confirmation

### Settings
The settings dialog stores user preferences.
It currently allows the user to:
- enable or disable notifications
- enable or disable dark mode
- preview the theme change immediately
- view the current version
- view the local data path

## Current persistence model
KMN Space is currently local-first.
Data is stored as JSON files for:
- tasks
- reminders
- notes
- schedule

The app uses the writable application data directory returned by Qt.
Legacy project-local JSON files may still be read in compatibility scenarios.

## Project structure
- `app/`: application entry point
- `include/`: headers for the main modules
- `src/`: implementation files
- `ui/`: Qt Designer UI files
- `tests/`: smoke tests
- `data/`: project-local JSON seed files
- `docs/`: project documentation

## Current known limitations
- Persistence is still JSON-based and not transactional.
- Fullscreen and large-window behavior has improved, but still needs real runtime verification.
- Search is a first functional version and can be refined further.
- The Windows release flow is still pending.

