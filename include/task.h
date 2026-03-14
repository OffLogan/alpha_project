//Task.h
//Header file for the task class

#ifndef TASK_H
#define TASK_H

#include <string>

class Task{
    private:
        int id_;
        std::string name_;
        std::string description_;
        bool status_;
    public:
        //Constructors
        Task();
        Task(const std::string& name, const std::string& description);
        //Observers
        const int GetTaskId() const;
        const std::string GetName() const;
        const std::string GetDescription() const;
        //Modifiers
        bool SetId(const int id);
        bool SetName(const std::string& name);
        bool SetDescription(const std::string& description);
        void CompleteTask();
};




#endif