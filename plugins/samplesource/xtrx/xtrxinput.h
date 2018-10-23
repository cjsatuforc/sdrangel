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

#ifndef PLUGINS_SAMPLESOURCE_XTRXINPUT_XTRXINPUT_H_
#define PLUGINS_SAMPLESOURCE_XTRXINPUT_XTRXINPUT_H_

#include <QString>
#include <stdint.h>

#include "dsp/devicesamplesource.h"
#include "xtrx/devicextrxshared.h"
#include "xtrxinputsettings.h"

class DeviceSourceAPI;
class XTRXInputThread;
struct DeviceXTRXParams;
class FileRecord;

class XTRXInput : public DeviceSampleSource
{
public:
    class MsgConfigureXTRX : public Message {
        MESSAGE_CLASS_DECLARATION

        public:
            const XTRXInputSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureXTRX* create(const XTRXInputSettings& settings, bool force)
        {
            return new MsgConfigureXTRX(settings, force);
        }

    private:
        XTRXInputSettings m_settings;
        bool m_force;

        MsgConfigureXTRX(const XTRXInputSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    class MsgGetStreamInfo : public Message {
        MESSAGE_CLASS_DECLARATION

        public:
            static MsgGetStreamInfo* create()
        {
            return new MsgGetStreamInfo();
        }

    private:
        MsgGetStreamInfo() :
            Message()
        { }
    };

    class MsgGetDeviceInfo : public Message {
        MESSAGE_CLASS_DECLARATION

        public:
            static MsgGetDeviceInfo* create()
        {
            return new MsgGetDeviceInfo();
        }

    private:
        MsgGetDeviceInfo() :
            Message()
        { }
    };

    class MsgReportStreamInfo : public Message {
        MESSAGE_CLASS_DECLARATION

        public:
            bool     getSuccess() const { return m_success; }
        bool     getActive() const { return m_active; }
        uint32_t getFifoFilledCount() const { return m_fifoFilledCount; }
        uint32_t getFifoSize() const { return m_fifoSize; }
        uint32_t getUnderrun() const { return m_underrun; }
        uint32_t getOverrun() const { return m_overrun; }
        uint32_t getDroppedPackets() const { return m_droppedPackets; }
        float    getSampleRate() const { return m_sampleRate; }
        float    getLinkRate() const { return m_linkRate; }
        uint64_t getTimestamp() const { return m_timestamp; }

        static MsgReportStreamInfo* create(
                bool     success,
                bool     active,
                uint32_t fifoFilledCount,
                uint32_t fifoSize,
                uint32_t underrun,
                uint32_t overrun,
                uint32_t droppedPackets,
                float    sampleRate,
                float    linkRate,
                uint64_t timestamp
                )
        {
            return new MsgReportStreamInfo(
                        success,
                        active,
                        fifoFilledCount,
                        fifoSize,
                        underrun,
                        overrun,
                        droppedPackets,
                        sampleRate,
                        linkRate,
                        timestamp
                        );
        }

    private:
        bool     m_success;
        // everything from lms_stream_status_t
        bool     m_active; //!< Indicates whether the stream is currently active
        uint32_t m_fifoFilledCount; //!< Number of samples in FIFO buffer
        uint32_t m_fifoSize; //!< Size of FIFO buffer
        uint32_t m_underrun; //!< FIFO underrun count
        uint32_t m_overrun; //!< FIFO overrun count
        uint32_t m_droppedPackets; //!< Number of dropped packets by HW
        float    m_sampleRate; //!< Sampling rate of the stream
        float    m_linkRate; //!< Combined data rate of all stream of the same direction (TX or RX)
        uint64_t m_timestamp; //!< Current HW timestamp

        MsgReportStreamInfo(
                bool     success,
                bool     active,
                uint32_t fifoFilledCount,
                uint32_t fifoSize,
                uint32_t underrun,
                uint32_t overrun,
                uint32_t droppedPackets,
                float    sampleRate,
                float    linkRate,
                uint64_t timestamp
                ) :
            Message(),
            m_success(success),
            m_active(active),
            m_fifoFilledCount(fifoFilledCount),
            m_fifoSize(fifoSize),
            m_underrun(underrun),
            m_overrun(overrun),
            m_droppedPackets(droppedPackets),
            m_sampleRate(sampleRate),
            m_linkRate(linkRate),
            m_timestamp(timestamp)
        { }
    };

    class MsgStartStop : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        bool getStartStop() const { return m_startStop; }

        static MsgStartStop* create(bool startStop) {
            return new MsgStartStop(startStop);
        }

    protected:
        bool m_startStop;

        MsgStartStop(bool startStop) :
            Message(),
            m_startStop(startStop)
        { }
    };

    class MsgFileRecord : public Message {
        MESSAGE_CLASS_DECLARATION

        public:
            bool getStartStop() const { return m_startStop; }

        static MsgFileRecord* create(bool startStop) {
            return new MsgFileRecord(startStop);
        }

    protected:
        bool m_startStop;

        MsgFileRecord(bool startStop) :
            Message(),
            m_startStop(startStop)
        { }
    };

    XTRXInput(DeviceSourceAPI *deviceAPI);
    virtual ~XTRXInput();
    virtual void destroy();

    virtual void init();
    virtual bool start();
    virtual void stop();

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);

    virtual void setMessageQueueToGUI(MessageQueue *queue) { m_guiMessageQueue = queue; }
    virtual const QString& getDeviceDescription() const;
    virtual int getSampleRate() const;
    virtual quint64 getCenterFrequency() const;
    virtual void setCenterFrequency(qint64 centerFrequency);

    virtual bool handleMessage(const Message& message);

    std::size_t getChannelIndex();
    void getLORange(float& minF, float& maxF, float& stepF) const;
    void getSRRange(float& minF, float& maxF, float& stepF) const;
    void getLPRange(float& minF, float& maxF, float& stepF) const;
    uint32_t getHWLog2Decim() const;

    void apply_gain_auto(double gain);
    void apply_gain_lna(double gain);
    void apply_gain_tia(double gain);
    void apply_gain_pga(double gain);

private:
    DeviceSourceAPI *m_deviceAPI;
    QMutex m_mutex;
    XTRXInputSettings m_settings;
    XTRXInputThread* m_XTRXInputThread;
    QString m_deviceDescription;
    bool m_running;
    DeviceXTRXShared m_deviceShared;
    bool m_channelAcquired;

    FileRecord *m_fileSink; //!< File sink to record device I/Q output

    bool openDevice();
    void closeDevice();
    bool acquireChannel();
    void releaseChannel();
    void suspendRxBuddies();
    void resumeRxBuddies();
    void suspendTxBuddies();
    void resumeTxBuddies();
    bool applySettings(const XTRXInputSettings& settings, bool force = false, bool forceNCOFrequency = false);
};

#endif /* PLUGINS_SAMPLESOURCE_XTRXINPUT_XTRXINPUT_H_ */
