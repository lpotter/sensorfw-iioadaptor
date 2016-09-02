/**
   @file iioadaptor.cpp
   @brief IioAdaptor based on SysfsAdaptor

   <p>
   Copyright (C) 2009-2010 Nokia Corporation
   Copyright (C) 2012 Tuomas Kulve
   Copyright (C) 2012 Srdjan Markovic

   @author Tuomas Kulve <tuomas@kulve.fi>

   Sensord is free software; you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License
   version 2.1 as published by the Free Software Foundation.

   Sensord is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with Sensord.  If not, see <http://www.gnu.org/licenses/>.
   </p>
*/
#include <errno.h>

#include <iio.h>
#include <libudev.h>

#include <logging.h>
#include <config.h>
#include <datatypes/utils.h>
#include <unistd.h>

#include "iioadaptor.h"
#include <sensord-qt5/sysfsadaptor.h>
#include <sensord-qt5/deviceadaptorringbuffer.h>
#include <QTextStream>
#include <QDir>
#include <QTimer>

#include <sensord-qt5/deviceadaptor.h>
#include "datatypes/orientationdata.h"

IioAdaptor::IioAdaptor(const QString &id/*, int type*/) :
        SysfsAdaptor(id, SysfsAdaptor::IntervalMode, true),
        deviceId(id),
        scale(-1)
{
    //sensorType = (IioAdaptor::IioSensorType)type;

    sensordLogD() << "Creating IioAdaptor with id: " << id;
//    QTimer::singleShot(100, this,SLOT(setup()));
    setup();
}

IioAdaptor::~IioAdaptor()
{
    if (iioBuffer_)
        delete iioBuffer_;
}

void IioAdaptor::setup()
{
    if (deviceId.startsWith("accel"))
        dev_accl_ = sensorExists(IioAdaptor::IIO_ACCELEROMETER);
    else if (deviceId.startsWith("gyro"))
        dev_gyro_ = -1;/*sensorExists(IIO_GYROSCOPE);*/
    else if (deviceId.startsWith("mag"))
        dev_magn_ = -1;/*sensorExists(IIO_MAGNETOMETER);*/

    qWarning() << Q_FUNC_INFO << dev_accl_;

    if (dev_accl_ != -1) {
        iioBuffer_ = new DeviceAdaptorRingBuffer<TimedXyzData>(1);
        QString desc = "Industrial I/O accelerometer (" +  devices_[dev_accl_].name +")";
        qWarning() << Q_FUNC_INFO << "Accelerometer found";
        setAdaptedSensor("accelerometer", desc, iioBuffer_);
    }

    // Disable and then enable devices to make sure they allow changing settings
//    for (i = 0; i < IIO_MAX_DEVICES; ++i) {
//        if (dev_accl_ == i || dev_gyro_ == i || dev_magn_ == i) {
//            deviceEnable(i, false);
//            deviceEnable(i, true);
        //    addDevice(dev_accl_);
//        }
//    }
    introduceAvailableDataRange(DataRange(0, 65535, 1));
    introduceAvailableInterval(DataRange(10, 586, 0));
    setDefaultInterval(10);
}

int IioAdaptor::addDevice(int device) {
    
    QDir dir(devicePath);
    if (!dir.exists()) {
        qWarning() << Q_FUNC_INFO << "Directory ??? doesn't exists";
        return 1;
    }

    QStringList filters;
    filters << "*_raw";
    dir.setNameFilters(filters);

    QFileInfoList list = dir.entryInfoList();
    qWarning() << Q_FUNC_INFO << dir;

    for (int i = 0; i < list.size(); ++i) {
        QFileInfo fileInfo = list.at(i);
        qWarning() << Q_FUNC_INFO << "adding device " << fileInfo.filePath()
                   << " as channel" << device*IIO_MAX_DEVICE_CHANNELS+i;
        addPath(fileInfo.filePath(),device*IIO_MAX_DEVICE_CHANNELS+i);
    }
    
    return 0;
}

// accel_3d
int IioAdaptor::findSensor(const QString &sensorName)
{
    qWarning() << Q_FUNC_INFO << sensorName;

    static struct iio_context *ctx = iio_create_default_context();
    if (!ctx) {
        qWarning() << "no context";
    }
    struct iio_device *iiodev = iio_context_find_device(ctx, sensorName.toLocal8Bit());
    if (!iiodev) {
        qWarning() << "no iio_device";
    } else {
        QString iiodevice(iio_device_get_id(iiodev));
        int index = iiodevice.right(1).toInt();

        qWarning() << iiodevice
                   << iio_device_get_name(iiodev);
        devicePath = QString(IIO_SYSFS_BASE) + iiodevice + "/";

        int channels = iio_device_get_channels_count(iiodev);

        qWarning() << "name" << iio_device_get_name(iiodev)
                   << "Channels" << channels
                   << "devicePath" << devicePath;
        const char *attr;


        for (int j = 0; j < channels; j++) {
            const char *chId;
            struct iio_channel *chn = iio_device_get_channel(iiodev, j);
            chId = iio_channel_get_id(chn);
            qWarning() << "channel id:" << chId;

            attr = iio_channel_find_attr(chn,"raw");

            QString filePath = devicePath + iio_channel_attr_get_filename(chn, attr);
//            QString filePath = "/sys/bus/iio/devices/"+iiodevice+"/"+iio_channel_attr_get_filename(chn, attr);
            qWarning() <<"filepath"<< filePath;
            addPath(filePath, j);

            //            const struct iio_device *trig;
            //            qWarning() << "is trigger" << iio_device_is_trigger(iiodev)
            //                       << iio_device_get_trigger(iiodev, &trig);

        }

        if (scale < 0) {

//            double val;
//          //  iio_channel_attr_read(chn, "scale", buf, sizeof(buf));
//            iio_channel_attr_read_double(chn, "scale", &val);
//            scale = val;
            qWarning() << "scale" << scale;
        }

        return index;
    }
    return -1;
}
/*
 * als
 * accel_3d
 * gyro_3d
 * magn_3d
 * incli_3d
 * dev_rotation
 *
 * */
int IioAdaptor::sensorExists(IioAdaptor::IioSensorType sensor)
{
    QString sensorName;
    switch (sensor) {
    case IIO_ACCELEROMETER:
        sensorName = QStringLiteral("accel_3d");
        break;
    case IIO_GYROSCOPE:
        sensorName = QStringLiteral("gyro_3d");
        break;
    case IIO_MAGNETOMETER:
        sensorName = QStringLiteral("magn_3d");
        break;
    default:
        return -1;
    }
    if (!sensorName.isEmpty())
        return findSensor(sensorName);
    else
        return -1;
}

bool IioAdaptor::deviceEnable(int device, int enable)
{
    QString pathEnable = devicePath + "/buffer/enable";
    QString pathLength = devicePath + "/buffer/length";

    if (enable) {
        // FIXME: should enable sensors for this device? Assuming enabled already
        devices_[device].name = deviceGetName(device);
        devices_[device].channels = scanElementsEnable(device, enable);
        sysfsWriteInt(pathLength, IIO_BUFFER_LEN);
        sysfsWriteInt(pathEnable, enable);
    } else {
        sysfsWriteInt(pathEnable, enable);
        scanElementsEnable(device, enable);
        // FIXME: should disable sensors for this device?
    }

    return true;
}

QString IioAdaptor::deviceGetName(int device)
{
    QString pathDeviceName = devicePath + "/name";
    qWarning() << Q_FUNC_INFO << pathDeviceName;

    return sysfsReadString(pathDeviceName);
}

bool IioAdaptor::sysfsWriteInt(QString filename, int val)
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        sensordLogW() << "Failed to open " << filename;
        return false;
    }

    QTextStream out(&file);
    out << val << "\n";

    file.close();

	return true;
}

QString IioAdaptor::sysfsReadString(QString filename)
{

    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        sensordLogW() << "Failed to open " << filename;
        return QString();
    }

    QTextStream in(&file);
    QString line = in.readLine();

    if (line.endsWith("\n")) {
        line.chop(1);
    }

    file.close();

	return line;
}

int IioAdaptor::sysfsReadInt(QString filename)
{
    QString string = sysfsReadString(filename);

    bool ok;
    int value = string.toInt(&ok);

    if (!ok) {
        sensordLogW() << "Failed to parse '" << string << "' to int from file " << filename;
    }

	return value;
}

// Return the number of channels
int IioAdaptor::scanElementsEnable(int device, int enable)
{
    QString elementsPath = devicePath + "/scan_elements";

    qWarning() << Q_FUNC_INFO << elementsPath;

                  QDir dir(elementsPath);
    if (!dir.exists()) {
        sensordLogW() << "Directory " << elementsPath << " doesn't exist";
        return 0;
    }

    // Find all the *_en file and write 0/1 to it
    QStringList filters;
    filters << "*_en";
    dir.setNameFilters(filters);

    QFileInfoList list = dir.entryInfoList();
    for (int i = 0; i < list.size(); ++i) {
        QFileInfo fileInfo = list.at(i);

        if (enable) {
            QString base = fileInfo.filePath();
            // Remove the _en
            base.chop(3);

            int index = sysfsReadInt(base + "_index");
            int bytes = deviceChannelParseBytes(base + "_type");

            devices_[device].channel_bytes[index] = bytes;
        }

        sysfsWriteInt(fileInfo.filePath(), enable);
    }

    return list.size();
}


int IioAdaptor::deviceChannelParseBytes(QString filename)
{
    QString type = sysfsReadString(filename);

    if (type.compare("le:s16/16>>0") == 0) {
        return 2;
    } else if (type.compare("le:s32/32>>0") == 0) {
        return 4;
    } else if (type.compare("le:s64/64>>0") == 0) {
        return 8;
    } else {
        sensordLogW() << "ERROR: invalid type from file " << filename << ": " << type;
    }

    return 0;
}


void IioAdaptor::processSample(int fileId, int fd)
{
    char buf[256];
    int readBytes;
    int result;
    int channel = fileId%IIO_MAX_DEVICE_CHANNELS;
    int device = (fileId-channel)/IIO_MAX_DEVICE_CHANNELS;

    readBytes = read(fd, buf, sizeof(buf));

    if (readBytes <= 0) {
        sensordLogW() << "read():" << strerror(errno);
        return;
    }
    result = atoi(buf)  * scale * 1000;
    qWarning() << "Read " << result << " from device " << device << ", channel " << channel;
qWarning() << result * scale;

    // FIXME: shouldn't hardcode
    // FIXME: Find a mapping between channels and actual devices (0, 1 and 2 are probably wrong)
    if (device == 0) {
        TimedXyzData* accl = iioBuffer_->nextSlot();
        switch(channel){
        case 0:
            accl->x_=result;
            break;

        case 1:
            accl->y_ = result;
            break;

        case 2:
            accl->z_ = result;
            break;

        default:
            return;
        }
        iioBuffer_->commit();
        iioBuffer_->wakeUpReaders();
    }
}

bool IioAdaptor::setInterval(const unsigned int value, const int sessionId)
{
    if (mode() == SysfsAdaptor::IntervalMode)
        return SysfsAdaptor::setInterval(value, sessionId);

    sensordLogD() << "Ignoring setInterval for " << value;

    return true;
}

unsigned int IioAdaptor::interval() const
{
    int value = 100;
    sensordLogD() << "Returning dummy value in interval(): " << value;

    return value;
}


/* Emacs indentatation information
   Local Variables:
   indent-tabs-mode:nil
   tab-width:4
   c-basic-offset:4
   End:
*/
