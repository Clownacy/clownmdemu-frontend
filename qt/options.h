#ifndef OPTIONS_H
#define OPTIONS_H

#include "ui_options.h"

#include "emulator.h"

class Options : public QDialog
{
	Q_OBJECT

private:
	Ui::Options ui;
	Emulator::Configuration &configuration;

public:
	explicit Options(Emulator::Configuration &configuration, QWidget *parent = nullptr);
};

#endif // OPTIONS_H
