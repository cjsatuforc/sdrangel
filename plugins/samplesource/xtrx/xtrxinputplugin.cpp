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

#include "xtrxinputplugin.h"

#include <QtPlugin>
#include <QAction>

#include <regex>
#include <string>

#include "xtrx_api.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "device/devicesourceapi.h"

#include "xtrxinputgui.h"

const PluginDescriptor XTRXInputPlugin::m_pluginDescriptor = {
    QString("XTRX Input"),
    QString("0.0.1"),
    QString("(c) Sergey Kostanbaev, Fairwaves"),
    QString("https://github.com/xtrx-sdr/sdrangel"),
    true,
    QString("https://github.com/xtrx-sdr/sdrangel")
};

const QString XTRXInputPlugin::m_hardwareID = "XTRX";
const QString XTRXInputPlugin::m_deviceTypeID = XTRX_DEVICE_TYPE_ID;

XTRXInputPlugin::XTRXInputPlugin(QObject* parent) :
    QObject(parent)
{
}

const PluginDescriptor& XTRXInputPlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void XTRXInputPlugin::initPlugin(PluginAPI* pluginAPI)
{
    pluginAPI->registerSampleSource(m_deviceTypeID, this);
}

PluginInterface::SamplingDevices XTRXInputPlugin::enumSampleSources()
{
    int nbDevices = 1;
    SamplingDevices result;
    const std::string deviceList[] = { "/dev/xtrx0" };

    for (int i = 0; i < nbDevices; i++)
    {
        //std::string serial("N/D");
        //findSerial((const char *) deviceList[i], serial);

        DeviceXTRXParams XTRXParams;
        XTRXParams.open(deviceList[i].c_str());
        XTRXParams.close();

        for (unsigned int j = 0; j < XTRXParams.m_nbRxChannels; j++)
        {
            qDebug("XTRXInputPlugin::enumSampleSources: device #%d channel %u: %s", i, j, (char *) deviceList[i].c_str());
            QString displayedName(QString("XTRX[%1:%2] %3").arg(i).arg(j).arg("<unknown>"));
            result.append(SamplingDevice(displayedName,
                                         m_hardwareID,
                                         m_deviceTypeID,
                                         QString(deviceList[i].c_str()),
                                         i,
                                         PluginInterface::SamplingDevice::PhysicalDevice,
                                         true,
                                         XTRXParams.m_nbRxChannels,
                                         j));
        }
    }

    return result;
}

PluginInstanceGUI* XTRXInputPlugin::createSampleSourcePluginInstanceGUI(
        const QString& sourceId,
        QWidget **widget,
        DeviceUISet *deviceUISet)
{
    if(sourceId == m_deviceTypeID)
    {
        XTRXInputGUI* gui = new XTRXInputGUI(deviceUISet);
        *widget = gui;
        return gui;
    }
    else
    {
        return 0;
    }
}

bool XTRXInputPlugin::findSerial(const char *lmsInfoStr, std::string& serial)
{
    std::regex serial_reg("serial=([0-9,A-F]+)");
    std::string input(lmsInfoStr);
    std::smatch result;
    std::regex_search(input, result, serial_reg);

    if (result[1].str().length()>0)
    {
        serial = result[1].str();
        return true;
    }
    else
    {
        return false;
    }
}

DeviceSampleSource *XTRXInputPlugin::createSampleSourcePluginInstanceInput(const QString& sourceId, DeviceSourceAPI *deviceAPI)
{
    if (sourceId == m_deviceTypeID)
    {
        XTRXInput* input = new XTRXInput(deviceAPI);
        return input;
    }
    else
    {
        return 0;
    }
}

