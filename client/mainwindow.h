
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QtCore/QtGlobal>

#include <QMainWindow>
#include <QtSerialPort/QSerialPort>

class QLabel;
namespace Ui {
class MainWindow;
}

class SettingsDialog;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void openSerialPort();
    void closeSerialPort();
    void about();
    void readData();

    void handleError(QSerialPort::SerialPortError error);

private:
    void setStatusMessage(const QString &msg);

    Ui::MainWindow *ui;
    QLabel *m_status;
    SettingsDialog *m_settingsDlg;
    QSerialPort *m_serial;
};

#endif // MAINWINDOW_H
