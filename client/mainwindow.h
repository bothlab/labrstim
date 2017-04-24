/*
 * Copyright (C) 2017 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QtCore/QtGlobal>

#include <QMainWindow>
#include <QtSerialPort/QSerialPort>

namespace Ui {
class MainWindow;
}
class QLabel;
class SettingsDialog;
class LabrstimClient;

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
    void readRawData(const QString &data);

    void handleError(const QString &message);

    void on_actionStop_triggered();

    void on_actionRun_triggered();

    void on_stimTypeComboBox_currentIndexChanged(int index);
    void on_trialDurationTimeEdit_timeChanged(const QTime &time);
    void on_pulseDurationSpinBox_valueChanged(double arg1);
    void on_laserIntensitySpinBox_valueChanged(double arg1);

    void on_randomIntervalCheckBox_toggled(bool checked);

    void on_minimumIntervalSpinBox_valueChanged(double arg1);

    void on_maximumIntervalSpinBox_valueChanged(double arg1);

    void on_swrRefractoryTimeSpinBox_valueChanged(double arg1);

    void on_swrPowerThresholdDoubleSpinBox_valueChanged(double arg1);

    void on_convolutionPeakThresholdSpinBox_valueChanged(double arg1);

    void on_stimDelayCheckBox_toggled(bool checked);

private:
    void setStatusMessage(const QString &msg);
    void setRunning(bool running);
    void setRunInfoVisible(bool shown);

    Ui::MainWindow *ui;
    QLabel *m_status;
    SettingsDialog *m_settingsDlg;
    LabrstimClient *m_client;
};

#endif // MAINWINDOW_H
