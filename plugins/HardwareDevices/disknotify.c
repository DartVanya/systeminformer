/*
 * Copyright (c) 2022 Winsider Seminars & Solutions, Inc.  All rights reserved.
 *
 * This file is part of System Informer.
 *
 * Authors:
 *
 *     dmex    2016-2024
 *
 */

#include "devices.h"

BOOLEAN MainWndDeviceChangeRegistrationEnabled = FALSE;
PH_CALLBACK_REGISTRATION MainWndDeviceChangeEventRegistration;

VOID NTAPI HardwareDevicesDeviceChangeCallback(
    _In_ PVOID Parameter,
    _In_ PVOID Context
    )
{
    PMSG message = Parameter;

    if (message->message != WM_DEVICECHANGE)
        return;

    switch (message->wParam)
    {
    case DBT_DEVICEARRIVAL: // Drive letter added
    case DBT_DEVICEREMOVECOMPLETE: // Drive letter removed
        {
            PDEV_BROADCAST_HDR deviceBroadcast = (PDEV_BROADCAST_HDR)message->lParam;

            if (deviceBroadcast->dbch_devicetype == DBT_DEVTYP_VOLUME)
            {
                //PDEV_BROADCAST_VOLUME deviceVolume = (PDEV_BROADCAST_VOLUME)deviceBroadcast;

                PhAcquireQueuedLockShared(&DiskDevicesListLock);

                for (ULONG i = 0; i < DiskDevicesList->Count; i++)
                {
                    PDV_DISK_ENTRY entry;

                    entry = PhReferenceObjectSafe(DiskDevicesList->Items[i]);

                    if (!entry)
                        continue;

                    // Reset the DiskIndex so we can re-query the index on the next interval update.
                    entry->DiskIndex = ULONG_MAX;
                    // Reset the DiskIndexName so we can re-query the name on the next interval update.
                    PhClearReference(&entry->DiskIndexName);
                    // Submit the query to the work queue.
                    DiskDeviceQueueNameUpdate(entry);

                    PhDereferenceObjectDeferDelete(entry);
                }

                PhReleaseQueuedLockShared(&DiskDevicesListLock);
            }
        }
        break;
    }
}

VOID AddRemoveDeviceChangeCallback(
    VOID
    )
{
    // We get called during the plugin LoadCallback, don't do anything.
    if (!SystemInformer_GetWindowHandle())
        return;

    // Add the subclass only when disks are being monitored, remove when no longer needed.
    if (DiskDevicesList->Count)
    {
        if (!MainWndDeviceChangeRegistrationEnabled)
        {
            // We have a disk device, register for window events needed to detect drive letter changes.
            MainWndDeviceChangeRegistrationEnabled = TRUE;
            PhRegisterCallback(
                PhGetGeneralCallback(GeneralCallbackWindowNotifyEvent),
                HardwareDevicesDeviceChangeCallback,
                NULL,
                &MainWndDeviceChangeEventRegistration
                );
        }
    }
    else
    {
        if (MainWndDeviceChangeRegistrationEnabled)
        {
            // Remove the event callback after the user has removed the last disk device.
            PhUnregisterCallback(PhGetGeneralCallback(GeneralCallbackWindowNotifyEvent), &MainWndDeviceChangeEventRegistration);
            MainWndDeviceChangeRegistrationEnabled = FALSE;
        }
    }
}
