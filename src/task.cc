//Task.cc
//Implementation file for the task class

#include "../include/task.h"


//Constructors
Task::Task() //Main constructors, once you tap on create task this will be used and then the modifiers will modify the task's data
    :   name_(""), description_(""), status_(false)
    {
        //Must think about how the id will be set after I implement the data structures
    }

Task::Task(const std::string& name, const std::string& description)
    :   name_(name), description_(description), status_(false)
    {
        //Same for this constructors
    }


//Observers
const int Task::GetTaskId() const {return id_;}
const std::string Task::GetName() const {return name_;}
const std::string Task::GetDescription() const {return description_;}

//Modifiers
bool Task::SetId(const int id){
    if(id<=0){
        return false;
    }
    id_ = id;
    return true;
};

bool Task::SetName(const std::string& name){
    if(name.empty()){
        return false;
    }
    name_ = name;
    return true;
};

bool Task::SetDescription(const std::string& description){
    if(description.empty()){
        return false;
    }
    description_ = description;
    return true;
};

void Task::CompleteTask(){
    status_ = true; //Will be the function used when you click complete task
};
