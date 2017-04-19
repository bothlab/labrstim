
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "settingsdialog.h"

#include <QMessageBox>
#include <QScrollBar>
#include <QLabel>
#include <QtSerialPort>
#include <QSvgRenderer>


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    m_serial = new QSerialPort(this);
    m_settingsDlg = new SettingsDialog(this);

    ui->actionConnect->setEnabled(true);
    ui->actionDisconnect->setEnabled(false);
    ui->actionQuit->setEnabled(true);
    ui->actionConfigure->setEnabled(true);

    // be safe on what is selected
    ui->stackedWidget->setCurrentIndex(0);
    ui->stimTypeComboBox->setCurrentIndex(0);

    m_status = new QLabel;
    ui->statusBar->addWidget(m_status);

    connect(ui->actionConnect, &QAction::triggered, this, &MainWindow::openSerialPort);
    connect(ui->actionDisconnect, &QAction::triggered, this, &MainWindow::closeSerialPort);
    connect(ui->actionQuit, &QAction::triggered, this, &MainWindow::close);
    connect(ui->actionConfigure, &QAction::triggered, m_settingsDlg, &SettingsDialog::show);
    connect(ui->actionAbout, &QAction::triggered, this, &MainWindow::about);

    connect(m_serial, static_cast<void (QSerialPort::*)(QSerialPort::SerialPortError)> (&QSerialPort::error),
            this, &MainWindow::handleError);

    connect(m_serial, &QSerialPort::readyRead, this, &MainWindow::readData);

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
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::openSerialPort()
{
    setRunInfoVisible(true);
    m_serial->setPortName(m_settingsDlg->portName());
    m_serial->setBaudRate(QSerialPort::Baud9600);
    m_serial->setDataBits(QSerialPort::Data8);
    m_serial->setStopBits(QSerialPort::OneStop);

    if (m_serial->open(QIODevice::ReadWrite)) {
        ui->actionConnect->setEnabled(false);
        ui->actionDisconnect->setEnabled(true);
        ui->actionConfigure->setEnabled(false);
        setStatusMessage(QStringLiteral("Connected to %1").arg(m_serial->portName()));
        ui->actionRun->setEnabled(true);
    } else {
        QMessageBox::critical(this, "Error", m_serial->errorString());
        setStatusMessage("Connection error");
        ui->actionRun->setEnabled(false);
    }

    setRunInfoVisible(false);
}

void MainWindow::closeSerialPort()
{
    if (m_serial->isOpen())
        m_serial->close();
    ui->actionConnect->setEnabled(true);
    ui->actionDisconnect->setEnabled(false);
    ui->actionConfigure->setEnabled(true);
    setStatusMessage("Disconnected");
}

void MainWindow::readData()
{
    auto data = m_serial->readAll();

    ui->logViewBox->insertPlainText(QString(data));
    auto vbar = ui->logViewBox->verticalScrollBar();
    vbar->setValue(vbar->maximum());
}

void MainWindow::handleError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::ResourceError) {
        QMessageBox::critical(this, "Critical Error", m_serial->errorString());
        closeSerialPort();
    }
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
    m_serial->write("\n");

    setRunning(true);
}

void MainWindow::on_actionStop_triggered()
{
    setRunning(false);
}
