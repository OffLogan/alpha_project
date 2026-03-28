//ReminderGestor.h
//Header file for the reminder data gestion class

#ifndef REMINDERGESTOR_H
#define REMINDERGESTOR_H

#include "reminder.h"
#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

class ReminderData{
    private:
        std::vector<Reminder> reminders_;
        std::unordered_map<int, std::size_t> indexById_;
        std::string filePath_;

        void RebuildIndex();
    public:
        explicit ReminderData(const std::string& filePath = "data/reminders.json");
        std::vector<Reminder> Data() const;
        bool Load();
        bool Update();
        int NextId() const;
        bool AddReminder(const Reminder& reminder);
        bool RemoveReminder(const int id);
        bool UpdateReminder(const Reminder& reminder);
        Reminder* FindById(int id);
        const Reminder* FindById(int id) const;
};

#endif
