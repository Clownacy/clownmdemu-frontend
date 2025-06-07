#ifndef DEBUG_CPU_H
#define DEBUG_CPU_H

#include <QDialog>

namespace Ui {
class DebugCPU;
}

class DebugCPU : public QDialog
{
    Q_OBJECT

public:
    explicit DebugCPU(QWidget *parent = nullptr);
    ~DebugCPU();

private:
    Ui::DebugCPU *ui;
};

#endif // DEBUG_CPU_H
