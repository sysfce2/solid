/*
    Copyright 2010 Mario Bensi <mbensi@ipsquad.net>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) version 3, or any
    later version accepted by the membership of KDE e.V. (or its
    successor approved by the membership of KDE e.V.), which shall
    act as a proxy defined in Section 6 of version 3 of the license.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library. If not, see <http://www.gnu.org/licenses/>.
*/

#include "fstabstorageaccess.h"
#include "fstabwatcher.h"
#include <solid/devices/backends/fstab/fstabdevice.h>
#include <solid/devices/backends/fstab/fstabhandling.h>
#include <solid/devices/backends/fstab/fstabservice.h>
#include <QtCore/QStringList>

#include <QTimer>

#define MTAB "/etc/mtab"

using namespace Solid::Backends::Fstab;

FstabStorageAccess::FstabStorageAccess(Solid::Backends::Fstab::FstabDevice *device) :
    QObject(device),
    m_fstabDevice(device)
{
    QStringList currentMountPoints = FstabHandling::currentMountPoints(device->device());
    if (currentMountPoints.isEmpty()) {
        QStringList mountPoints = FstabHandling::mountPoints(device->device());
        m_filePath = mountPoints.isEmpty() ? QString() : mountPoints.first();
        m_isAccessible = false;
    } else {
        m_filePath = currentMountPoints.first();
        m_isAccessible = true;
    }
    m_isIgnored = FstabHandling::options(device->device()).contains(QLatin1String("x-gvfs-hide"));

    connect(device, SIGNAL(mtabChanged(QString)), this, SLOT(onMtabChanged(QString)));
    QTimer::singleShot(0, this, SLOT(connectDBusSignals()));
}

FstabStorageAccess::~FstabStorageAccess()
{
}

void FstabStorageAccess::connectDBusSignals()
{
    m_fstabDevice->registerAction("setup", this,
                                  SLOT(slotSetupRequested()),
                                  SLOT(slotSetupDone(int,QString)));

    m_fstabDevice->registerAction("teardown", this,
                                  SLOT(slotTeardownRequested()),
                                  SLOT(slotTeardownDone(int,QString)));
}

const Solid::Backends::Fstab::FstabDevice *FstabStorageAccess::fstabDevice() const
{
    return m_fstabDevice;
}

bool FstabStorageAccess::isAccessible() const
{
    return m_isAccessible;
}

QString FstabStorageAccess::filePath() const
{
    return m_filePath;
}

bool FstabStorageAccess::isIgnored() const
{
    return m_isIgnored;
}

bool FstabStorageAccess::setup()
{
    if (filePath().isEmpty()) {
        return false;
    }
    m_fstabDevice->broadcastActionRequested("setup");
    return FstabHandling::callSystemCommand("mount", {filePath()}, this, [this](QProcess *process) {
        if (process->exitCode() == 0) {
            m_fstabDevice->broadcastActionDone("setup", Solid::NoError, QString());
        } else {
            m_fstabDevice->broadcastActionDone("setup", Solid::UnauthorizedOperation, process->readAllStandardError());
        }
    });
}

void FstabStorageAccess::slotSetupRequested()
{
    emit setupRequested(m_fstabDevice->udi());
}

bool FstabStorageAccess::teardown()
{
    if (filePath().isEmpty()) {
        return false;
    }
    m_fstabDevice->broadcastActionRequested("teardown");
    return FstabHandling::callSystemCommand("umount", {filePath()}, this, [this](QProcess *process) {
        if (process->exitCode() == 0) {
            m_fstabDevice->broadcastActionDone("teardown", Solid::NoError, QString());
        } else {
            m_fstabDevice->broadcastActionDone("teardown", Solid::UnauthorizedOperation, process->readAllStandardError());
        }
    });
}

void FstabStorageAccess::slotTeardownRequested()
{
    emit teardownRequested(m_fstabDevice->udi());
}

void FstabStorageAccess::slotSetupDone(int error, const QString &errorString)
{
    emit setupDone(static_cast<Solid::ErrorType>(error), errorString, m_fstabDevice->udi());
}

void FstabStorageAccess::slotTeardownDone(int error, const QString &errorString)
{
    emit teardownDone(static_cast<Solid::ErrorType>(error), errorString, m_fstabDevice->udi());
}

void FstabStorageAccess::onMtabChanged(const QString &device)
{
    QStringList currentMountPoints = FstabHandling::currentMountPoints(device);
    if (currentMountPoints.isEmpty()) {
        // device umounted
        m_filePath = FstabHandling::mountPoints(device).first();
        m_isAccessible = false;
        emit accessibilityChanged(false, QString(FSTAB_UDI_PREFIX) + "/" + device);
    } else {
        // device added
        m_filePath = currentMountPoints.first();
        m_isAccessible = true;
        emit accessibilityChanged(true, QString(FSTAB_UDI_PREFIX) + "/" + device);
    }
}
