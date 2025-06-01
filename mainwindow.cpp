#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , emulator(this)
{
    ui->setupUi(this);

    setCentralWidget(&emulator);
}

MainWindow::~MainWindow()
{
    delete ui;
}

