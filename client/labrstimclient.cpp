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

#include "labrstimclient.h"

#include <QSerialPort>
#include <QCoreApplication>
#include <QThread>
#include <QDebug>

LabrstimClient::LabrstimClient(QObject *parent)
    : QObject(parent),
      m_running(false)
{
    m_serial = new QSerialPort(this);

    connect(m_serial, static_cast<void (QSerialPort::*)(QSerialPort::SerialPortError)> (&QSerialPort::error),
            this, &LabrstimClient::handleError);

    connect(m_serial, &QSerialPort::readyRead, this, &LabrstimClient::readData);

    // default values
    m_trialDuration = 0;
    m_pulseDuration = 0;
    m_laserIntensity = 0;

    m_randomIntervals = 0;
    m_minimumInterval = 0;
    m_maximumInterval = 0;

    m_swrRefractoryTime = 0;
    m_swrPowerThreshold = 0;
    m_convolutionPeakThreshold = 0;
    m_swrDelayStimulation = false;
}

LabrstimClient::~LabrstimClient()
{
    // ensure we are stopped
    this->disconnect();
    if (m_serial->isOpen())
        sendRequest("STOP");
}

QString LabrstimClient::lastError() const
{
    return m_lastError;
}

QString LabrstimClient::sendRequest(const QString &req)
{
    m_lastError = QString();
    m_lastResult = QString();
    m_serial->write(QStringLiteral("%1\n").arg(req).toUtf8());
    emit newRawData(QStringLiteral("=> %1\n").arg(req));

    for (uint i = 1; i < 24; i++) {
        QCoreApplication::processEvents();

        if (m_lastResult.endsWith("\n")) {
            return getLastResult();
        }

        QThread::msleep(50 * i);
    }

    // we didn't get a reply
    emitError(QStringLiteral("No reply received in time (Request: %1).").arg(req));
    return QString();
}

QString LabrstimClient::clientVersion() const
{
    return m_clientVersion;
}

void LabrstimClient::setMode(LabrstimClient::Mode mode)
{
    m_mode = mode;
}

void LabrstimClient::setTrialDuration(double val)
{
    m_trialDuration = val;
}

void LabrstimClient::setPulseDuration(double val)
{
    m_pulseDuration = val;
}

void LabrstimClient::setLaserIntensity(double val)
{
    m_laserIntensity = val;
}

void LabrstimClient::setRandomIntervals(bool random)
{
    m_randomIntervals = random;
}

void LabrstimClient::setMinimumInterval(double min)
{
    m_minimumInterval = min;
}

void LabrstimClient::setMaximumInterval(double max)
{
    m_maximumInterval = max;
}

void LabrstimClient::setSwrRefractoryTime(double val)
{
    m_swrRefractoryTime = val;
}

void LabrstimClient::setSwrPowerThreshold(double val)
{
    m_swrPowerThreshold = val;
}

void LabrstimClient::setConvolutionPeakThreshold(double val)
{
    m_convolutionPeakThreshold = val;
}

void LabrstimClient::setSwrDelayStimulation(bool delay)
{
    m_swrDelayStimulation = delay;
}

bool LabrstimClient::open(const QString &portName)
{
    // just make sure the port isn't already open
    close();

    // set general port settings suitable for the device
    m_serial->setPortName(portName);
    m_serial->setBaudRate(QSerialPort::Baud115200);
    m_serial->setDataBits(QSerialPort::Data8);
    m_serial->setStopBits(QSerialPort::OneStop);

    if (!m_serial->open(QIODevice::ReadWrite)) {
        emitError("Connection error");
        return false;
    }

    // send some stuff to check the connection
    m_serial->write("NOOP\n"); // this does not reply
    if (sendRequest("PING") != "PONG") {
        emitError("Unable to communicate with the device.");
        close();
        return false;
    }

    // request device software version
    auto ver = sendRequest("VERSION");
    if (ver.isEmpty()) {
        emitError("Could not determine client version.");
        close();
        return false;
    }
    if (!ver.startsWith("VERSION ")) {
        emitError("Version check failed: The reply was invalid.");
        close();
        return false;
    }
    m_clientVersion = ver.remove(0, 8);

    return true;
}

void LabrstimClient::close()
{
    if (m_serial->isOpen())
        m_serial->close();
}

bool LabrstimClient::runStimulation()
{
    if (m_running) {
        emitError("Already running.");
        return false;
    }

    auto command = QStringLiteral("RUN");
    switch (m_mode) {
        case ModeSwr:
            command.append(" swr");
            break;
        case ModeTheta:
            command.append(" theta");
            break;
        case ModeTrain:
            command.append(" train");
            break;
        default:
            emitError("No valid stimulation mode set.");
            return false;
    }

    command.append(QString(" %1").arg(m_trialDuration));
    command.append(QString(" %1").arg(m_pulseDuration));
    command.append(QString(" %1").arg(m_laserIntensity));

    // SWR-specific settings
    if (m_swrRefractoryTime != 0)
        command.append(QString(" -f %1").arg(m_swrRefractoryTime));
    if (m_swrPowerThreshold != 0)
        command.append(QString(" -s %1").arg(m_swrPowerThreshold));

    command.append(QString(" -C %1").arg(m_convolutionPeakThreshold));

    if (m_swrDelayStimulation) {
        command.append(QString(" -m %1").arg(m_minimumInterval));
        command.append(QString(" -M %1").arg(m_maximumInterval));
    }

    auto res = sendRequest(command);
    if (res != "OK") {
        emitError(QStringLiteral("Unable to start stimulation."));
        return false;
    }

    m_running = true;
    return true;
}

bool LabrstimClient::stopStimulation()
{
    if (!m_running) {
        emitError("Can not stop a stimulation that is not running.");
        return false;
    }

    auto res = sendRequest("STOP");
    if ((res != "OK") && (!res.startsWith("FINISHED"))) {
        emitError(QStringLiteral("Unable to stop stimulation."));
        return false;
    }

    m_running = false;
    return true;
}

void LabrstimClient::readData()
{
    auto data = m_serial->readAll();
    emit newRawData(data);

    // quick & dirty
    m_lastResultBuf.append(QString(data));
    if (m_lastResultBuf.endsWith("\n")) {
        m_lastResult = m_lastResultBuf;
        m_lastResultBuf = QString();

        if (m_lastResult.startsWith("ERROR")) {
            emitError(getLastResult());
            if (m_running)
                emit stimulationFinished();
            m_running = false;
        } else if (m_lastResult.startsWith("FINISHED")) {
            if (m_running)
                emit stimulationFinished();
            m_running = false;
        }
    }
}

void LabrstimClient::handleError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::ResourceError) {
        emitError(m_serial->errorString());
        close();
    }
}

QString LabrstimClient::getLastResult()
{
    auto res = m_lastResult.replace("\n", "").replace("\\n", "\n");
    m_lastResult = QString();
    return res;
}

void LabrstimClient::emitError(const QString &message)
{
    m_lastError = message;
    emit error(m_lastError);
}
