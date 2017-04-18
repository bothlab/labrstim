
#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QtSerialPort/QSerialPort>

namespace Ui {
class SettingsDialog;
}

class QIntValidator;

QT_END_NAMESPACE

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    ~SettingsDialog();

    QString portName();

private slots:
    void showPortInfo(int idx);
    void apply();
    void checkCustomDevicePathPolicy(int idx);

private:
    void fillPortsParameters();
    void fillPortsInfo();
    void updateSettings();

private:
    Ui::SettingsDialog *ui;
};

#endif // SETTINGSDIALOG_H
