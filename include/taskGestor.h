//TaskGestor.h
//Header file for the task data gestion class

#ifndef TASKGESTOR_H
#define TASKGESTOR_H

#include "task.h"
#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>


class TaskData{
    private:
        //Data structure to contains de tasks data
        std::vector<Task> tasks_;
        std::unordered_map<int, std::size_t> indexById_; //Storage the id plus the tasks instances
        std::string filePath_;

        void RebuildIndex();
    public:
        explicit TaskData(const std::string& filePath = "data/tasks.json");
        std::vector<Task> Data() const;
        bool Load();
        bool Update();
        int NextId() const;
        bool AddTask(const Task& task);
        bool RemoveTask(const int id);
        bool UpdateTask(const Task& task);
        Task* FindById(int id);
        const Task* FindById(int id) const;
};


#endif
