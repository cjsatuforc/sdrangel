///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2017 Sergey Kostanbaev, Fairwaves Inc.                          //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include "xtrxinputgui.h"

#include <QDebug>
#include <QMessageBox>

#include <algorithm>

#include "ui_xtrxinputgui.h"
#include "gui/colormapper.h"
#include "gui/glspectrum.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "device/devicesourceapi.h"
#include "device/deviceuiset.h"

XTRXInputGUI::XTRXInputGUI(DeviceUISet *deviceUISet, QWidget* parent) :
    QWidget(parent),
    ui(new Ui::XTRXInputGUI),
    m_deviceUISet(deviceUISet),
    m_settings(),
    m_sampleRate(0),
    m_lastEngineState((DSPDeviceSourceEngine::State)-1),
    m_doApplySettings(true),
    m_forceSettings(true),
    m_statusCounter(0),
    m_deviceStatusCounter(0)
{
    m_XTRXInput = (XTRXInput*) m_deviceUISet->m_deviceSourceAPI->getSampleSource();

    ui->setupUi(this);

    float minF, maxF, stepF;

    m_XTRXInput->getLORange(minF, maxF, stepF);
    ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->centerFrequency->setValueRange(7, ((uint32_t) minF)/1000, ((uint32_t) maxF)/1000); // frequency dial is in kHz

    m_XTRXInput->getSRRange(minF, maxF, stepF);
    ui->sampleRate->setColorMapper(ColorMapper(ColorMapper::GrayGreenYellow));
    ui->sampleRate->setValueRange(8, (uint32_t) minF, (uint32_t) maxF);

    m_XTRXInput->getLPRange(minF, maxF, stepF);
    ui->lpf->setColorMapper(ColorMapper(ColorMapper::GrayYellow));
    ui->lpf->setValueRange(6, (minF/1000)+1, maxF/1000);

    ui->lpFIR->setColorMapper(ColorMapper(ColorMapper::GrayYellow));
    ui->lpFIR->setValueRange(5, 1U, 56000U);

    ui->ncoFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));

    ui->channelNumberText->setText(tr("#%1").arg(m_XTRXInput->getChannelIndex()));

    ui->hwDecimLabel->setText(QString::fromUtf8("H\u2193"));
    ui->swDecimLabel->setText(QString::fromUtf8("S\u2193"));

    connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
    connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
    m_statusTimer.start(500);

    displaySettings();

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
}

XTRXInputGUI::~XTRXInputGUI()
{
    delete ui;
}

void XTRXInputGUI::destroy()
{
    delete this;
}

void XTRXInputGUI::setName(const QString& name)
{
    setObjectName(name);
}

QString XTRXInputGUI::getName() const
{
    return objectName();
}

void XTRXInputGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    sendSettings();
}

qint64 XTRXInputGUI::getCenterFrequency() const
{
    return m_settings.m_centerFrequency;
}

void XTRXInputGUI::setCenterFrequency(qint64 centerFrequency)
{
    m_settings.m_centerFrequency = centerFrequency;
    displaySettings();
    sendSettings();
}

QByteArray XTRXInputGUI::serialize() const
{
    return m_settings.serialize();
}

bool XTRXInputGUI::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        displaySettings();
        m_forceSettings = true;
        sendSettings();
        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

bool XTRXInputGUI::handleMessage(const Message& message)
{

    if (DeviceXTRXShared::MsgReportBuddyChange::match(message))
    {
        DeviceXTRXShared::MsgReportBuddyChange& report = (DeviceXTRXShared::MsgReportBuddyChange&) message;
        m_settings.m_devSampleRate = report.getDevSampleRate();
        m_settings.m_log2HardDecim = report.getLog2HardDecimInterp();

        if (report.getRxElseTx()) {
            m_settings.m_centerFrequency = report.getCenterFrequency();
        }

        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);

        return true;
    }
    else if (DeviceXTRXShared::MsgReportClockSourceChange::match(message))
    {
        DeviceXTRXShared::MsgReportClockSourceChange& report = (DeviceXTRXShared::MsgReportClockSourceChange&) message;
        m_settings.m_extClockFreq = report.getExtClockFeq();
        m_settings.m_extClock = report.getExtClock();

        blockApplySettings(true);
        ui->extClock->setExternalClockFrequency(m_settings.m_extClockFreq);
        ui->extClock->setExternalClockActive(m_settings.m_extClock);
        blockApplySettings(false);

        return true;
    }
    else if (XTRXInput::MsgReportStreamInfo::match(message))
    {
        XTRXInput::MsgReportStreamInfo& report = (XTRXInput::MsgReportStreamInfo&) message;

        if (report.getSuccess())
        {
            if (report.getActive()) {
                ui->streamStatusLabel->setStyleSheet("QLabel { background-color : green; }");
            } else {
                ui->streamStatusLabel->setStyleSheet("QLabel { background-color : blue; }");
            }

            ui->streamLinkRateText->setText(tr("%1 MB/s").arg(QString::number(report.getLinkRate() / 1000000.0f, 'f', 3)));

            if (report.getUnderrun() > 0) {
                ui->underrunLabel->setStyleSheet("QLabel { background-color : red; }");
            } else {
                ui->underrunLabel->setStyleSheet("QLabel { background:rgb(79,79,79); }");
            }

            if (report.getOverrun() > 0) {
                ui->overrunLabel->setStyleSheet("QLabel { background-color : red; }");
            } else {
                ui->overrunLabel->setStyleSheet("QLabel { background:rgb(79,79,79); }");
            }

            if (report.getDroppedPackets() > 0) {
                ui->droppedLabel->setStyleSheet("QLabel { background-color : red; }");
            } else {
                ui->droppedLabel->setStyleSheet("QLabel { background:rgb(79,79,79); }");
            }

            ui->fifoBar->setMaximum(report.getFifoSize());
            ui->fifoBar->setValue(report.getFifoFilledCount());
            ui->fifoBar->setToolTip(tr("FIFO fill %1/%2 samples").arg(QString::number(report.getFifoFilledCount())).arg(QString::number(report.getFifoSize())));
        }
        else
        {
            ui->streamStatusLabel->setStyleSheet("QLabel { background:rgb(79,79,79); }");
        }

        return true;
    }
    else if (DeviceXTRXShared::MsgReportDeviceInfo::match(message))
    {
        DeviceXTRXShared::MsgReportDeviceInfo& report = (DeviceXTRXShared::MsgReportDeviceInfo&) message;
        ui->temperatureText->setText(tr("%1C").arg(QString::number(report.getTemperature(), 'f', 0)));
        return true;
    }
    else if (XTRXInput::MsgStartStop::match(message))
    {
        XTRXInput::MsgStartStop& notif = (XTRXInput::MsgStartStop&) message;
        blockApplySettings(true);
        ui->startStop->setChecked(notif.getStartStop());
        blockApplySettings(false);

        return true;
    }
    return false;
}

void XTRXInputGUI::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        if (DSPSignalNotification::match(*message))
        {
            DSPSignalNotification* notif = (DSPSignalNotification*) message;
            m_sampleRate = notif->getSampleRate();
            m_deviceCenterFrequency = notif->getCenterFrequency();
            qDebug("XTRXInputGUI::handleInputMessages: DSPSignalNotification: SampleRate: %d, CenterFrequency: %llu", notif->getSampleRate(), notif->getCenterFrequency());
            updateSampleRateAndFrequency();

            delete message;
        }
        else
        {
            if (handleMessage(*message)) {
                delete message;
            }
        }
    }
}

void XTRXInputGUI::updateADCRate()
{
    uint32_t adcRate = m_settings.m_devSampleRate * (1<<m_settings.m_log2HardDecim);

    if (adcRate < 100000000) {
        ui->adcRateLabel->setText(tr("%1k").arg(QString::number(adcRate / 1000.0f, 'g', 5)));
    } else {
        ui->adcRateLabel->setText(tr("%1M").arg(QString::number(adcRate / 1000000.0f, 'g', 5)));
    }
}

void XTRXInputGUI::updateSampleRateAndFrequency()
{
    m_deviceUISet->getSpectrum()->setSampleRate(m_sampleRate);
    m_deviceUISet->getSpectrum()->setCenterFrequency(m_deviceCenterFrequency);
    ui->deviceRateLabel->setText(tr("%1k").arg(QString::number(m_sampleRate / 1000.0f, 'g', 5)));
}

void XTRXInputGUI::displaySettings()
{
    ui->extClock->setExternalClockFrequency(m_settings.m_extClockFreq);
    ui->extClock->setExternalClockActive(m_settings.m_extClock);

    ui->centerFrequency->setValue(m_settings.m_centerFrequency / 1000);
    ui->sampleRate->setValue(m_settings.m_devSampleRate);

    ui->dcOffset->setChecked(m_settings.m_dcBlock);
    ui->iqImbalance->setChecked(m_settings.m_iqCorrection);

    ui->hwDecim->setCurrentIndex(m_settings.m_log2HardDecim);
    ui->swDecim->setCurrentIndex(m_settings.m_log2SoftDecim);

    updateADCRate();

    ui->lpf->setValue(m_settings.m_lpfBW / 1000);

    ui->lpFIREnable->setChecked(m_settings.m_lpfFIREnable);
    ui->lpFIR->setValue(m_settings.m_lpfFIRBW / 1000);

    ui->gain->setValue(m_settings.m_gain);
    ui->gainText->setText(tr("%1").arg(m_settings.m_gain));

    ui->antenna->setCurrentIndex((int) m_settings.m_antennaPath);

    ui->gainMode->setCurrentIndex((int) m_settings.m_gainMode);
    ui->lnaGain->setValue(m_settings.m_lnaGain);
    ui->tiaGain->setCurrentIndex(m_settings.m_tiaGain - 1);
    ui->pgaGain->setValue(m_settings.m_pgaGain);

    if (m_settings.m_gainMode == XTRXInputSettings::GAIN_AUTO)
    {
        ui->gain->setEnabled(true);
        ui->lnaGain->setEnabled(false);
        ui->tiaGain->setEnabled(false);
        ui->pgaGain->setEnabled(false);
    }
    else
    {
        ui->gain->setEnabled(false);
        ui->lnaGain->setEnabled(true);
        ui->tiaGain->setEnabled(true);
        ui->pgaGain->setEnabled(true);
    }

    setNCODisplay();

    ui->ncoEnable->setChecked(m_settings.m_ncoEnable);
}

void XTRXInputGUI::setNCODisplay()
{
    int ncoHalfRange = (m_settings.m_devSampleRate * (1<<(m_settings.m_log2HardDecim)))/2;
    int lowBoundary = std::max(0, (int) m_settings.m_centerFrequency - ncoHalfRange);
    ui->ncoFrequency->setValueRange(7,
                                    lowBoundary/1000,
                                    (m_settings.m_centerFrequency + ncoHalfRange)/1000); // frequency dial is in kHz
    ui->ncoFrequency->setValue((m_settings.m_centerFrequency + m_settings.m_ncoFrequency)/1000);
}

void XTRXInputGUI::sendSettings()
{
    if(!m_updateTimer.isActive())
        m_updateTimer.start(100);
}

void XTRXInputGUI::updateHardware()
{
    if (m_doApplySettings)
    {
        qDebug() << "XTRXInputGUI::updateHardware";
        XTRXInput::MsgConfigureXTRX* message = XTRXInput::MsgConfigureXTRX::create(m_settings, m_forceSettings);
        m_XTRXInput->getInputMessageQueue()->push(message);
        m_forceSettings = false;
        m_updateTimer.stop();
    }
}

void XTRXInputGUI::updateStatus()
{
    int state = m_deviceUISet->m_deviceSourceAPI->state();

    if(m_lastEngineState != state)
    {
        switch(state)
        {
        case DSPDeviceSourceEngine::StNotStarted:
            ui->startStop->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
            break;
        case DSPDeviceSourceEngine::StIdle:
            ui->startStop->setStyleSheet("QToolButton { background-color : blue; }");
            break;
        case DSPDeviceSourceEngine::StRunning:
            ui->startStop->setStyleSheet("QToolButton { background-color : green; }");
            break;
        case DSPDeviceSourceEngine::StError:
            ui->startStop->setStyleSheet("QToolButton { background-color : red; }");
            QMessageBox::information(this, tr("Message"), m_deviceUISet->m_deviceSourceAPI->errorMessage());
            break;
        default:
            break;
        }

        m_lastEngineState = state;
    }

    if (m_statusCounter < 1)
    {
        m_statusCounter++;
    }
    else
    {
        XTRXInput::MsgGetStreamInfo* message = XTRXInput::MsgGetStreamInfo::create();
        m_XTRXInput->getInputMessageQueue()->push(message);
        m_statusCounter = 0;
    }

    if (m_deviceStatusCounter < 10)
    {
        m_deviceStatusCounter++;
    }
    else
    {
        if (m_deviceUISet->m_deviceSourceAPI->isBuddyLeader())
        {
            XTRXInput::MsgGetDeviceInfo* message = XTRXInput::MsgGetDeviceInfo::create();
            m_XTRXInput->getInputMessageQueue()->push(message);
        }

        m_deviceStatusCounter = 0;
    }
}

void XTRXInputGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void XTRXInputGUI::on_startStop_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        XTRXInput::MsgStartStop *message = XTRXInput::MsgStartStop::create(checked);
        m_XTRXInput->getInputMessageQueue()->push(message);
    }
}

void XTRXInputGUI::on_record_toggled(bool checked)
{
    if (checked) {
        ui->record->setStyleSheet("QToolButton { background-color : red; }");
    } else {
        ui->record->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
    }

    XTRXInput::MsgFileRecord* message = XTRXInput::MsgFileRecord::create(checked);
    m_XTRXInput->getInputMessageQueue()->push(message);
}

void XTRXInputGUI::on_centerFrequency_changed(quint64 value)
{
    m_settings.m_centerFrequency = value * 1000;
    setNCODisplay();
    sendSettings();
}

void XTRXInputGUI::on_ncoFrequency_changed(quint64 value)
{
    m_settings.m_ncoFrequency = (int64_t) value - (int64_t) m_settings.m_centerFrequency/1000;
    m_settings.m_ncoFrequency *= 1000;
    sendSettings();
}

void XTRXInputGUI::on_ncoEnable_toggled(bool checked)
{
    m_settings.m_ncoEnable = checked;
    sendSettings();
}

void XTRXInputGUI::on_ncoReset_clicked(bool checked __attribute__((unused)))
{
    m_settings.m_ncoFrequency = 0;
    ui->ncoFrequency->setValue(m_settings.m_centerFrequency/1000);
    sendSettings();
}

void XTRXInputGUI::on_dcOffset_toggled(bool checked)
{
    m_settings.m_dcBlock = checked;
    sendSettings();
}

void XTRXInputGUI::on_iqImbalance_toggled(bool checked)
{
    m_settings.m_iqCorrection = checked;
    sendSettings();
}

void XTRXInputGUI::on_sampleRate_changed(quint64 value)
{
    m_settings.m_devSampleRate = value;
    updateADCRate();
    setNCODisplay();
    sendSettings();}

void XTRXInputGUI::on_hwDecim_currentIndexChanged(int index)
{
    if ((index <0) || (index > 5))
        return;
    m_settings.m_log2HardDecim = index;
    updateADCRate();
    setNCODisplay();
    sendSettings();
}

void XTRXInputGUI::on_swDecim_currentIndexChanged(int index)
{
    if ((index <0) || (index > 6))
        return;
    m_settings.m_log2SoftDecim = index;
    sendSettings();
}

void XTRXInputGUI::on_lpf_changed(quint64 value)
{
    m_settings.m_lpfBW = value * 1000;
    sendSettings();
}

void XTRXInputGUI::on_lpFIREnable_toggled(bool checked)
{
    m_settings.m_lpfFIREnable = checked;
    sendSettings();
}

void XTRXInputGUI::on_lpFIR_changed(quint64 value)
{
    m_settings.m_lpfFIRBW = value * 1000;

    if (m_settings.m_lpfFIREnable) { // do not send the update if the FIR is disabled
        sendSettings();
    }
}

void XTRXInputGUI::on_gainMode_currentIndexChanged(int index)
{
    m_settings.m_gainMode = (XTRXInputSettings::GainMode) index;

    if (index == 0)
    {
        ui->gain->setEnabled(true);
        ui->lnaGain->setEnabled(false);
        ui->tiaGain->setEnabled(false);
        ui->pgaGain->setEnabled(false);
    }
    else
    {
        ui->gain->setEnabled(false);
        ui->lnaGain->setEnabled(true);
        ui->tiaGain->setEnabled(true);
        ui->pgaGain->setEnabled(true);
    }

    sendSettings();
}

void XTRXInputGUI::on_gain_valueChanged(int value)
{
    m_settings.m_gain = value;
    ui->gainText->setText(tr("%1").arg(m_settings.m_gain));
    sendSettings();
}

void XTRXInputGUI::on_lnaGain_valueChanged(int value)
{
    m_settings.m_lnaGain = value;
    ui->lnaGainText->setText(tr("%1").arg(m_settings.m_lnaGain));
    sendSettings();
}

void XTRXInputGUI::on_tiaGain_currentIndexChanged(int index)
{
    m_settings.m_tiaGain = index + 1;
    sendSettings();
}

void XTRXInputGUI::on_pgaGain_valueChanged(int value)
{
    m_settings.m_pgaGain = value;
    ui->pgaGainText->setText(tr("%1").arg(m_settings.m_pgaGain));
    sendSettings();
}

void XTRXInputGUI::on_antenna_currentIndexChanged(int index)
{
    m_settings.m_antennaPath = (xtrx_antenna_t) index;
    sendSettings();
}

void XTRXInputGUI::on_extClock_clicked()
{
    m_settings.m_extClock = ui->extClock->getExternalClockActive();
    m_settings.m_extClockFreq = ui->extClock->getExternalClockFrequency();
    qDebug("XTRXInputGUI::on_extClock_clicked: %u Hz %s", m_settings.m_extClockFreq, m_settings.m_extClock ? "on" : "off");
    sendSettings();
}

void XTRXInputGUI::on_pwrmode_currentIndexChanged(int index)
{
    m_settings.m_pwrmode = index;
    sendSettings();
}
