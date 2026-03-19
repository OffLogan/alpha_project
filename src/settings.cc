//Settings.cc
//Implementation file for the settings class

#include "../include/settings.h"
#include "ui_settings.h"

settings::settings(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::settings)
{
    ui->setupUi(this);
}

settings::~settings()
{
    delete ui;
}
