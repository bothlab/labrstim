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

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "settingsdialog.h"

#include <QMessageBox>
#include <QScrollBar>
#include <QLabel>
#include <QtSerialPort>
#include <QSvgRenderer>

#include "labrstimclient.h"


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    m_client = new LabrstimClient(this);
    m_settingsDlg = new SettingsDialog(this);

    ui->actionConnect->setEnabled(true);
    ui->actionDisconnect->setEnabled(false);
    ui->actionQuit->setEnabled(true);
    ui->actionConfigure->setEnabled(true);

    // be safe on what is selected
    ui->stackedWidget->setCurrentIndex(0);
    ui->stimTypeComboBox->setCurrentIndex(0);
    m_client->setMode(LabrstimClient::ModeSwr);

    m_status = new QLabel;
    ui->statusBar->addWidget(m_status);

    connect(ui->actionConnect, &QAction::triggered, this, &MainWindow::openSerialPort);
    connect(ui->actionDisconnect, &QAction::triggered, this, &MainWindow::closeSerialPort);
    connect(ui->actionQuit, &QAction::triggered, this, &MainWindow::close);
    connect(ui->actionConfigure, &QAction::triggered, m_settingsDlg, &SettingsDialog::show);
    connect(ui->actionAbout, &QAction::triggered, this, &MainWindow::about);

    connect(m_client, &LabrstimClient::error, this, &MainWindow::handleError);
    connect(m_client, &LabrstimClient::newRawData, this, &MainWindow::readRawData);
    connect(m_client, &LabrstimClient::stimulationFinished, this, &MainWindow::onStimulationFinished);

    // we can't run until we connected the stimulator device
    setRunning(false);
    ui->actionRun->setEnabled(false);

    // collapse the log view
    QList<int> list;
    list.append(1);
    list.append(0);
    ui->splitter->setSizes(list);

    // create our waiting animation
    ui->runIndicatorWidget->load(QStringLiteral(":/graphics/running.svg"));

    // set default values (which should trigger value-change slots to set the values on m_client)
    ui->samplingRateSpinBox->setValue(200000);
    ui->trialDurationTimeEdit->setTime(QTime(0, 7, 0));
    ui->pulseDurationSpinBox->setValue(20);
    ui->laserIntensitySpinBox->setValue(2);
    ui->minimumIntervalSpinBox->setValue(10);
    ui->maximumIntervalSpinBox->setValue(20);

    ui->swrPowerThresholdDoubleSpinBox->setValue(3);
    ui->thetaPhaseSpinBox->setValue(90);
    ui->trainFrequencySpinBox->setValue(6);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::openSerialPort()
{
    setRunInfoVisible(true);

    if (m_client->open(m_settingsDlg->portName())) {
        ui->actionConnect->setEnabled(false);
        ui->actionDisconnect->setEnabled(true);
        ui->actionConfigure->setEnabled(false);
        setStatusMessage(QStringLiteral("Connected to %1 (%2)")
                         .arg(m_settingsDlg->portName())
                         .arg(m_client->clientVersion()));
        ui->actionRun->setEnabled(true);
    } else {
        setStatusMessage("Connection error");
        ui->actionRun->setEnabled(false);
    }

    setRunInfoVisible(false);
}

void MainWindow::closeSerialPort()
{
    m_client->close();
    ui->actionConnect->setEnabled(true);
    ui->actionDisconnect->setEnabled(false);
    ui->actionConfigure->setEnabled(true);
    setStatusMessage("Disconnected");
}

void MainWindow::readRawData(const QString& data)
{
    ui->logViewBox->insertPlainText(QString(data));
    auto vbar = ui->logViewBox->verticalScrollBar();
    vbar->setValue(vbar->maximum());
}

void MainWindow::handleError(const QString& message)
{
    QMessageBox::critical(this, "Critical Error", message);
}

void MainWindow::onStimulationFinished()
{
    setRunning(false);
}

void MainWindow::setStatusMessage(const QString &msg)
{
    m_status->setText(msg);
}

void MainWindow::setRunInfoVisible(bool shown)
{
    ui->runIndicatorWidget->setVisible(shown);
    QApplication::processEvents();
}

void MainWindow::setRunning(bool running)
{
    ui->actionStop->setEnabled(running);
    ui->actionRun->setEnabled(!running);

    ui->stackedWidget->setEnabled(!running);
    ui->generalWidget->setEnabled(!running);

    setRunInfoVisible(running);
}

void MainWindow::about()
{
    QMessageBox::about(this, "About LaBrStim Client", "This is just a dummy test tool for now.");
}

void MainWindow::on_actionRun_triggered()
{
    if (m_client->runStimulation())
        setRunning(true);
}

void MainWindow::on_actionStop_triggered()
{
    if (m_client->stopStimulation())
        setRunning(false);
}

void MainWindow::on_stimTypeComboBox_currentIndexChanged(int index)
{
    ui->randomIntervalCheckBox->setEnabled(true);
    ui->randomIntervalLabel->setEnabled(true);
    switch (index) {
        case 0:
            m_client->setMode(LabrstimClient::ModeSwr);
            ui->randomIntervalCheckBox->setEnabled(false);
            ui->randomIntervalLabel->setEnabled(false);
            break;
        case 1:
            m_client->setMode(LabrstimClient::ModeTheta);
            break;
        case 2:
            m_client->setMode(LabrstimClient::ModeTrain);
            break;
        default:
            qWarning() << "Unknown mode selected!";
            m_client->setMode(LabrstimClient::ModeUnknown);
    }
}

void MainWindow::on_trialDurationTimeEdit_timeChanged(const QTime &time)
{
    m_client->setTrialDuration(QTime(0, 0).secsTo(time));
}

void MainWindow::on_pulseDurationSpinBox_valueChanged(double arg1)
{
    m_client->setPulseDuration(arg1);
}

void MainWindow::on_laserIntensitySpinBox_valueChanged(double arg1)
{
    m_client->setLaserIntensity(arg1);
}

void MainWindow::on_randomIntervalCheckBox_toggled(bool checked)
{
    m_client->setRandomIntervals(checked);
}

void MainWindow::on_minimumIntervalSpinBox_valueChanged(double arg1)
{
    m_client->setMinimumInterval(arg1);
    if (ui->maximumIntervalSpinBox->value() <= arg1) {
        ui->maximumIntervalSpinBox->setValue(arg1 + 1);
    }
}

void MainWindow::on_maximumIntervalSpinBox_valueChanged(double arg1)
{
    m_client->setMaximumInterval(arg1);
    if (ui->minimumIntervalSpinBox->value() >= arg1) {
        ui->minimumIntervalSpinBox->setValue(arg1 - 1);
    }
}

void MainWindow::on_swrRefractoryTimeSpinBox_valueChanged(double arg1)
{
    m_client->setSwrRefractoryTime(arg1);
}

void MainWindow::on_swrPowerThresholdDoubleSpinBox_valueChanged(double arg1)
{
    m_client->setSwrPowerThreshold(arg1);
}

void MainWindow::on_convolutionPeakThresholdSpinBox_valueChanged(double arg1)
{
    m_client->setConvolutionPeakThreshold(arg1);
}

void MainWindow::on_thetaPhaseSpinBox_valueChanged(double arg1)
{
    m_client->setThetaPhase(arg1);
}

void MainWindow::on_trainFrequencySpinBox_valueChanged(double arg1)
{
    m_client->setTrainFrequency(arg1);
}

void MainWindow::on_samplingRateSpinBox_valueChanged(int arg1)
{
    m_client->setSamplingFrequency(arg1);
}
