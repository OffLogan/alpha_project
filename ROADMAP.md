# KMN Space Roadmap

## Roadmap direction
The project is now beyond the very first prototype stage, so the roadmap is split into smaller, more coherent releases that build toward `0.1.0` instead of concentrating too much work in `0.0.3`.

## Target version: 0.0.3

### Goal
Improve the core daily workflow without taking on major structural changes.

### Planned work

1. Task and reminder management
- Edit existing tasks and reminders.
- Mark items as completed and reopen them.
- Add confirmation before deleting tasks and reminders from the home screen.

2. Visual clarity
- Highlight overdue reminders and entries due soon.
- Finish polishing small UI inconsistencies between main screens.

3. Reliability baseline
- Expand smoke tests for the most common create, edit and delete flows.

### Exit criteria
- Core task and reminder flows feel complete enough for everyday use.
- No risky delete actions remain without confirmation.
- Main screens feel visually consistent.

## Target version: 0.0.4

### Goal
Improve organization and navigation across the existing modules.

### Planned work

1. Task and reminder productivity improvements
- Filter by pending, completed, overdue and due soon.
- Sort reminders by due date.

2. Notes organization
- Rename folders.
- Move notes between folders without manual recreation.

3. Navigation improvements
- Add a first version of global search across notes, tasks, reminders and schedule entries.

4. User-facing documentation improvements
- Create the first proper project documentation baseline.
- Document the current modules and the main user flows available in the app.
- Add clearer usage documentation for notes, schedule, reminders and settings.
- Start documenting screenshots or examples for the most important screens if useful.
- Prepare the functional scope for a future language selector with Spanish support.

5. Layout responsiveness improvements
- Adjust the app screens so they behave correctly in fullscreen and larger window sizes.
- Reduce awkward empty areas, overlapping elements or badly stretched layouts when the window grows.

### Exit criteria
- Users can manage growing amounts of information without losing track of it.
- Notes structure becomes flexible enough for real academic use.

## Target version: 0.0.5

### Goal
Strengthen quality before the architectural jump.

### Planned work

1. Bug-hunt and hardening cycle
- Run an exhaustive review of manual flows: create, edit, delete, export, import and reopen.
- Review UI regressions between Home, Notes, Schedule and Settings.
- Review invalid or corrupted JSON handling.

2. Testing expansion
- Add persistence-focused tests for tasks, reminders, notes and settings.
- Add regression coverage for the most fragile UI-linked flows.

3. Search refinement
- Improve global search results and direct navigation.
- Optionally keep recent searches if the first search release proves useful.

4. Activity log
- Add a first app-wide activity log for the main modules.
- Record key actions performed in tasks, reminders, notes and schedule.
- Decide whether the log should be user-visible, developer-oriented, or both.
- Prepare the log structure so it can later move cleanly to the database model.

5. Technical documentation
- Document the current architecture, persistence model and known limitations before the storage migration.
- Leave a clearer base for the future database transition.

### Exit criteria
- No reproducible crash in core flows.
- No silent data loss during save, delete or import actions.
- Test coverage is clearly better than in `0.0.2`.

## Target version: 0.1.0

### Goal
Ship the first clearly mature version of KMN Space with stronger persistence and a safer data model.

### Planned work

1. Introduce a local database layer
- Replace JSON-file persistence with a structured local database.
- Define stable schemas and relationships for tasks, reminders, notes, folders and schedule blocks.

2. Migration and compatibility
- Import existing user data from current JSON files on first launch after upgrade.
- Keep a safe migration path and avoid silent data loss.

3. Data integrity improvements
- Use transactional writes for create, edit and delete flows.
- Reduce the risk of partial writes or inconsistent in-memory state.

4. Final stabilization for 0.1.0
- Expand automated tests around migrations, CRUD operations and corrupted-data recovery.
- Validate that all existing product flows keep working after the storage migration.

5. Mature software documentation
- Prepare proper software documentation for both users and developers.
- Document installation, application structure, storage model, migration behavior and maintenance guidance.
- Leave the project in a state where a new contributor can understand and extend it more easily.

6. Language support planning
- Add a settings-based language extension after the English-first interface is stable.
- Plan Spanish as the first additional language option.

### Exit criteria
- Existing users can upgrade without losing their data.
- Persistence is significantly more robust than the JSON-based model.
- KMN Space is stable enough to be presented as `0.1.0` instead of another `0.0.x`.

## Recommended implementation order

1. Finish `0.0.3` workflow improvements.
2. Deliver `0.0.4` organization and navigation features.
3. Use `0.0.5` as the hardening pass before storage migration.
4. Deliver database migration and final stabilization in `0.1.0`.
