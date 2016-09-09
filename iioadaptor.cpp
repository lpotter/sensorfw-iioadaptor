/**
   @file iioadaptor.cpp
   @brief IioAdaptor based on SysfsAdaptor

   <p>
   Copyright (C) 2009-2010 Nokia Corporation
   Copyright (C) 2012 Tuomas Kulve
   Copyright (C) 2012 Srdjan Markovic
   Copyright (C) 2016 Canonical

   @author Tuomas Kulve <tuomas@kulve.fi>
   @author Lorn Potter <lorn.potter@canonical.com>

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
#include <time.h>

#include "iioadaptor.h"
#include <sensord-qt5/sysfsadaptor.h>
#include <sensord-qt5/deviceadaptorringbuffer.h>
#include <QTextStream>
#include <QDir>
#include <QTimer>
#include <QDirIterator>
#include <qmath.h>

#include <sensord-qt5/deviceadaptor.h>
#include "datatypes/orientationdata.h"


//#define BIT(x) (1 << (x))
//inline unsigned int set_bit_range(int start, int end)
//{
//    int i;
//    unsigned int value = 0;

//    for (i = start; i < end; ++i)
//        value |= BIT(i);

//    return value;
//}
//                                     // 32 / 8
//                                     // realbits  offset
//inline float convert_from_vtf_format(int size, int exponent, unsigned int value)
//{
//    int divider = 1;
//    int i;
//    float sample;
//    float mul = 1.0;

//    value = value & set_bit_range(0, size * 8);

//    if (value & BIT(size*8-1)) {
//        value =  ((1LL << (size * 8)) - value);
//        mul = -1.0;
//    }

//    sample = value * 1.0;

//    if (exponent < 0) {
//        exponent = abs(exponent);
//        for (i = 0; i < exponent; ++i)
//            divider = divider * 10;

//        return mul * sample/divider;
//    }

//    return mul * sample * qPow(10.0, exponent);
//}


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
    if (iioXyzBuffer_)
        delete iioXyzBuffer_;
    if (alsBuffer_)
        delete alsBuffer_;
    if (magnetometerBuffer_)
        delete magnetometerBuffer_;
}

void IioAdaptor::setup()
{
    if (deviceId.startsWith("accel")) {
        dev_accl_ = sensorExists(IioAdaptor::IIO_ACCELEROMETER);
        if (dev_accl_!= -1) {
            sensorType = IioAdaptor::IIO_ACCELEROMETER;
            iioXyzBuffer_ = new DeviceAdaptorRingBuffer<TimedXyzData>(1);
            QString desc = "Industrial I/O accelerometer (" +  devices_[dev_accl_].name +")";
            qWarning() << Q_FUNC_INFO << "Accelerometer found";
            setAdaptedSensor("accelerometer", desc, iioXyzBuffer_);
            setDescription(desc);
        }
    }
    else if (deviceId.startsWith("gyro")) {
        dev_accl_ = sensorExists(IIO_GYROSCOPE);
        if (dev_accl_!= -1) {
            sensorType = IioAdaptor::IIO_GYROSCOPE;
            iioXyzBuffer_ = new DeviceAdaptorRingBuffer<TimedXyzData>(1);
            QString desc = "Industrial I/O gyroscope (" +  devices_[dev_accl_].name +")";
            setAdaptedSensor("gyroscope", desc, iioXyzBuffer_);
            setDescription(desc);
        }
    }
    else if (deviceId.startsWith("mag")) {
        dev_accl_ = sensorExists(IIO_MAGNETOMETER);
        if (dev_accl_!= -1) {
            sensorType = IioAdaptor::IIO_MAGNETOMETER;
            magnetometerBuffer_ = new DeviceAdaptorRingBuffer<CalibratedMagneticFieldData>(1);
            QString desc = "Industrial I/O magnetometer (" +  devices_[dev_accl_].name +")";
            setAdaptedSensor("magnetometer", desc, magnetometerBuffer_);
            //        overflowLimit_ = Config::configuration()->value<int>("magnetometer/overflow_limit", 8000);
            setDescription(desc);
        }
    }
    else if (deviceId.startsWith("als")) {
        dev_accl_ = sensorExists(IIO_ALS);
        if (dev_accl_!= -1) {
            sensorType = IioAdaptor::IIO_ALS;
            alsBuffer_ = new DeviceAdaptorRingBuffer<TimedUnsigned>(1);
            QString desc = "Industrial I/O light sensor (" +  devices_[dev_accl_].name +")";
            setDescription(desc);
            setAdaptedSensor("als", desc, alsBuffer_);
        }
    }

    qWarning() << Q_FUNC_INFO << dev_accl_;

//    if (dev_accl_ != -1) {
//        iioBuffer_ = new DeviceAdaptorRingBuffer<TimedXyzData>(1);
//        QString desc = "Industrial I/O accelerometer (" +  devices_[dev_accl_].name +")";
//        qWarning() << Q_FUNC_INFO << "Accelerometer found";
//        setAdaptedSensor("accelerometer", desc, iioBuffer_);
//    }

    // Disable and then enable devices to make sure they allow changing settings
//    for (i = 0; i < IIO_MAX_DEVICES; ++i) {
//        if (dev_accl_ == i || dev_gyro_ == i || dev_magn_ == i) {
    deviceEnable(dev_accl_, false);
    deviceEnable(dev_accl_, true);
//    }
    introduceAvailableDataRange(DataRange(0, 65535, 1));
    introduceAvailableInterval(DataRange(0, 586, 0));
    setDefaultInterval(10);
}

// accel_3d
int IioAdaptor::findSensor(const QString &sensorName)
{
    qWarning() << Q_FUNC_INFO << sensorName;

    udev_list_entry *devices;
    udev_list_entry *dev_list_entry;
    udev_device *dev;
    struct udev *udevice = 0;
    struct udev_enumerate *enumerate = 0;

    if (!udevice)
        udevice = udev_new();

    enumerate = udev_enumerate_new(udevice);
    udev_enumerate_add_match_subsystem(enumerate, "iio");

    udev_enumerate_scan_devices(enumerate);
    devices = udev_enumerate_get_list_entry(enumerate);

    QString eventPath;
    int index = 0;
    bool ok2;

    udev_list_entry_foreach(dev_list_entry, devices) {
        const char *path;
        path = udev_list_entry_get_name(dev_list_entry);

        dev = udev_device_new_from_syspath(udevice, path);
        if (qstrcmp(udev_device_get_subsystem(dev), "iio") == 0) {
            QString name = QString::fromLatin1(udev_device_get_sysattr_value(dev,"name"));

            if (name == sensorName) {
                struct udev_list_entry *sysattr;
                int j = 0;
                QString eventName = QString::fromLatin1(udev_device_get_sysname(dev));
                devicePath = QString::fromLatin1(udev_device_get_syspath(dev)) +"/";
                index = eventName.right(1).toInt(&ok2);
                //qWarning() << Q_FUNC_INFO << "syspath" << devicePath;

                udev_list_entry_foreach(sysattr, udev_device_get_sysattr_list_entry(dev)) {
                    const char *name;
                    const char *value;
                    bool ok;
                    name = udev_list_entry_get_name(sysattr);
                    value = udev_device_get_sysattr_value(dev, name);
                    if (value == NULL)
                        continue;
    //
                    qWarning() << "attr" << name << value;

                    QString attributeName(name);
                    if (attributeName.endsWith("scale")) {
                        double num = QString(value).toDouble(&ok);
                        if (ok) {
                            scale = num;
                            qWarning() << "scale is" << scale;
                        }
                    } else if (attributeName.endsWith("offset")) {
                        double num = QString(value).toDouble(&ok);
                        if (ok)
                            offset = num;
                        qWarning() << "offset is" << value;
                    } else if (attributeName.endsWith("frequency")) {
                        double num = QString(value).toDouble(&ok);
                        if (ok)
                            frequency = num;
                        qWarning() << "frequency is" << value;
                    } else if (attributeName.endsWith("raw")) {
                        qWarning() << "adding to paths:" << devicePath
                                   << attributeName << index;

                        addPath(devicePath + attributeName, j);
                        j++;
                    }
                }

    // in_rot_from_north_magnetic_tilt_comp_raw ?

                // type
                break;
            }
        }
        udev_device_unref(dev);
    }
    udev_enumerate_unref(enumerate);

    if (ok2)
        return index;
    else
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
    case IIO_ALS:
        sensorName = QStringLiteral("als");
        break;
        /*
          case IIO_ROTATION:
        sensorName = QStringLiteral("dev_rotation");
          case IIO_TILT:
        sensorName = QStringLiteral("incli_3d");
        */
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
    qWarning() << Q_FUNC_INFO << device << enable;

    QString pathEnable = devicePath + "buffer/enable";
    QString pathLength = devicePath + "buffer/length";

    if (enable) {
        // FIXME: should enable sensors for this device? Assuming enabled already
        devices_[device].name = deviceGetName(device);
        numChannels = scanElementsEnable(device, enable);
        devices_[device].channels = numChannels;
        sysfsWriteInt(pathLength, IIO_BUFFER_LEN);
        sysfsWriteInt(pathEnable, enable);
    } else {
        sysfsWriteInt(pathEnable, enable);
        scanElementsEnable(device, enable);
        // FIXME: should disable sensors for this device?
    }

    return true;
}

QString IioAdaptor::deviceGetName(int /*device*/)
{
    QString pathDeviceName = devicePath + "name";
    qWarning() << Q_FUNC_INFO << pathDeviceName;

    return sysfsReadString(pathDeviceName);
}

bool IioAdaptor::sysfsWriteInt(QString filename, int val)
{
    qWarning() << Q_FUNC_INFO << filename << val;
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
    QString elementsPath = devicePath + "scan_elements";

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
qWarning() << Q_FUNC_INFO << list.size();

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
    int device = (fileId - channel)/IIO_MAX_DEVICE_CHANNELS;

    qWarning() << Q_FUNC_INFO << "fileId" << fileId << "channel" << channel << "device" << device;

    if (device == 0) {
        readBytes = read(fd, buf, sizeof(buf));

        if (readBytes <= 0) {
            sensordLogW() << "read():" << strerror(errno);
            return;
        }

        result = atoi(buf);

        qWarning() << "Read " << result
                   << " from device " << device
                   << ", channel " << channel;

        switch(channel) {
        case 0: { //x
            switch (sensorType) {
            case IioAdaptor::IIO_ACCELEROMETER:
            case IioAdaptor::IIO_GYROSCOPE:
                timedData = iioXyzBuffer_->nextSlot();
                result = (result * scale) * 100;
                timedData->x_= -result;
                break;
            case IioAdaptor::IIO_MAGNETOMETER:
                calData = magnetometerBuffer_->nextSlot();
                result = (result * scale) * 100;
                calData->rx_ = result;
                break;
            case IioAdaptor::IIO_ALS:
                uData = alsBuffer_->nextSlot();
                result = (result * scale);
                uData->value_ = result;
                qWarning() << "ALS" << result << uData->value_;
                break;
            default:
                break;
            };
        }
            break;

        case 1: { //y
            switch (sensorType) {
            case IioAdaptor::IIO_ACCELEROMETER:
            case IioAdaptor::IIO_GYROSCOPE:
                timedData = iioXyzBuffer_->nextSlot();
                result = (result * scale) * 100;
                timedData->y_ = -result;
                break;
            case IioAdaptor::IIO_MAGNETOMETER:
                calData = magnetometerBuffer_->nextSlot();
                result = (result * scale) * 100;
                calData->y_ = result;
                break;
            default:
                break;
            };
        }
            break;

        case 2: {// z
            switch (sensorType) {
            case IioAdaptor::IIO_ACCELEROMETER:
            case IioAdaptor::IIO_GYROSCOPE:
                timedData = iioXyzBuffer_->nextSlot();
                result = (result * scale) * 100;
                timedData->z_ = -result;
                break;
            case IioAdaptor::IIO_MAGNETOMETER:
                calData = magnetometerBuffer_->nextSlot();
                result = (result * scale) * 100;
                calData->rz_ = result;
                break;
            default:
                break;
            };
        }
            break;
        };

        qWarning() << Q_FUNC_INFO << channel << devices_[device].channels
                   << "numChannels" << numChannels;

        if (channel == numChannels - 1) {

            switch (sensorType) {
            case IioAdaptor::IIO_ACCELEROMETER:
            case IioAdaptor::IIO_GYROSCOPE:
                timedData->timestamp_ = Utils::getTimeStamp();
                iioXyzBuffer_->commit();
                iioXyzBuffer_->wakeUpReaders();
                break;
            case IioAdaptor::IIO_MAGNETOMETER:
                calData->timestamp_ = Utils::getTimeStamp();
                magnetometerBuffer_->commit();
                magnetometerBuffer_->wakeUpReaders();
                break;
            case IioAdaptor::IIO_ALS:
                uData->timestamp_ = Utils::getTimeStamp();
                alsBuffer_->commit();
                alsBuffer_->wakeUpReaders();
                qWarning() << "XXXXXXXXXXXXXXX<<<<<<<<<<<<<<<>>>>>>>>>>>>>>>>>>>>>>";
                break;
            default:
                break;
            };
        }
    }
}

bool IioAdaptor::setInterval(const unsigned int value, const int sessionId)
{
    if (mode() == SysfsAdaptor::IntervalMode)
        return SysfsAdaptor::setInterval(value, sessionId);

    sensordLogD() << "Ignoring setInterval for " << value;

    return true;
}

//unsigned int IioAdaptor::interval() const
//{
//    int value = 100;
//    sensordLogD() << "Returning dummy value in interval(): " << value;

//    return value;
//}

//quint64 IioAdaptor::getTimestamp()
//{
//    struct timespec tv;
//    int ok;
//    ok = clock_gettime(CLOCK_MONOTONIC, &tv);
//    Q_ASSERT(ok == 0);
//    Q_UNUSED(ok);

//    quint64 result = (tv.tv_sec * 1000000ULL) + (tv.tv_nsec * 0.001); // scale to microseconds
//    return result;
//}

bool IioAdaptor::startSensor()
{
    qWarning() << Q_FUNC_INFO;
    deviceEnable(dev_accl_, true);
    return SysfsAdaptor::startSensor();
}

void IioAdaptor::stopSensor()
{
    qWarning() << Q_FUNC_INFO;
    deviceEnable(dev_accl_, false);
    SysfsAdaptor::stopSensor();
}


/* Emacs indentatation information
   Local Variables:
   indent-tabs-mode:nil
   tab-width:4
   c-basic-offset:4
   End:
*/
