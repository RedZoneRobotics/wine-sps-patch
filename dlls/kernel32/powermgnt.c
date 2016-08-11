/*
 * Copyright 1995 Thomas Sandford (tdgsandf@prds-grn.demon.co.uk)
 * Copyright 2003 Dimitrie O. Paun
 * Copyright 2016 John P. Lettman (jlettman@redzone.com) RedZone Robotics, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <stdarg.h>
#include <stdio.h>
#include <dirent.h>
#include <memory.h>
#include <string.h>
#include <math.h> // floor

#include "windef.h"
#include "winbase.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(powermgnt);

enum ACLineStatus
{
    Offline = 0,
    Online = 1,
    AC_Unknown = 255
};

enum BatteryFlag
{
    High = 1,
    Low = 2,
    Critical = 4,
    Charging = 8,
    NoSystemBattery = 128,
    BAT_Unknown = 255
};

const char* PATH_SYS_POWER_SUPPLY = "/sys/class/power_supply/";

static int _read_sys_power_supply_int(char* device, char* file)
{
    char* path;
    char fd_contents[255];

    path = malloc(strlen(PATH_SYS_POWER_SUPPLY) + 1 +
                  strlen(device) + 1 + strlen(file));

    if(path == NULL)
    {
        return -1;
    }

    sprintf(path, "%s/%s/%s", PATH_SYS_POWER_SUPPLY, device, file);
    FILE* fd = fopen(path, "r");

    if(fd <= 0)
    {
        free(path);
        return -1;
    }

    fgets(fd_contents, 255, fd);
    fclose(fd);

    if(fd_contents[0] != NULL)
    {
        return atoi(fd_contents);
    }
    else
    {
        return -1;
    }
}


/******************************************************************************
 *           GetDevicePowerState   (KERNEL32.@)
 */
BOOL WINAPI GetDevicePowerState(HANDLE hDevice, BOOL* pfOn)
{
    WARN("(hDevice %p pfOn %p): stub\n", hDevice, pfOn);
    return TRUE; /* no information */
}

/***********************************************************************
 *           GetSystemPowerStatus      (KERNEL32.@)
 */
BOOL WINAPI GetSystemPowerStatus(LPSYSTEM_POWER_STATUS ps)
{
    WARN("(%p): stub, harmless.\n", ps);

    if (ps)
    {
        // Set defaults
        ps->ACLineStatus        = 255;
        ps->BatteryFlag         = 255;
        ps->BatteryLifePercent  = 255;
        ps->Reserved1           = 0;
        ps->BatteryLifeTime     = ~0u;
        ps->BatteryFullLifeTime = ~0u;


        DIR* power_supply_dir = opendir(PATH_SYS_POWER_SUPPLY);

        if(power_supply_dir)
        {
            struct dirent* dir_entry;

            while((dir_entry = readdir(power_supply_dir)) != NULL)
            {
                if(strncmp(dir_entry->d_name, "BAT", 3) == 0)
                {
                    // Check if the battery is not present
                    if(_read_sys_power_supply_int(dir_entry->d_name, "present") == 0)
                    {
                        if(ps->BatteryFlag == BAT_Unknown)
                        {
                            ps->BatteryFlag = NoSystemBattery;
                        }

                        continue; // skip
                    }

                    // Add energy amounts
                    ps->BatteryFullLifeTime +=
                        _read_sys_power_supply_int(dir_entry->d_name, "energy_full");

                    ps->BatteryLifeTime +=
                        _read_sys_power_supply_int(dir_entry->d_name, "energy_now");
                }

                else if(strncmp(dir_entry->d_name, "AC", 2) == 0)
                {
                    switch(_read_sys_power_supply_int(dir_entry->d_name, "online"))
                    {
                    case 0:
                        ps->ACLineStatus = Offline;
                        break;

                    case 1:
                        ps->ACLineStatus = Online;
                        break;

                    default:
                        ps->ACLineStatus = AC_Unknown;
                        break;
                    }
                }
            } // while((dir_entry = readdir(power_supply_dir)) != NULL)

            if(ps->BatteryLifeTime != 0 && ps->BatteryFullLifeTime != 0)
            {
                // Calculate percentage
                double battery_percentage =
                    ((double)ps->BatteryLifeTime /
                     (double)ps->BatteryFullLifeTime) * 100.0;

                ps->BatteryLifePercent = (unsigned char)floor(battery_percentage);

                // Calculate flags
                if(ps->ACLineStatus == Online)
                {
                    ps->BatteryFlag = Charging;
                }
                else if(ps->BatteryLifePercent < 10)
                {
                    ps->BatteryFlag = Critical;
                }
                else if(ps->BatteryLifePercent < 25)
                {
                    ps->BatteryFlag = Low;
                }
                else
                {
                    ps->BatteryFlag = High;
                }
            }
        }

        return TRUE;
    }
    return FALSE;
}

/***********************************************************************
 *           IsSystemResumeAutomatic   (KERNEL32.@)
 */
BOOL WINAPI IsSystemResumeAutomatic(void)
{
    WARN("(): stub, harmless.\n");
    return FALSE;
}

/***********************************************************************
 *           RequestWakeupLatency      (KERNEL32.@)
 */
BOOL WINAPI RequestWakeupLatency(LATENCY_TIME latency)
{
    WARN("(): stub, harmless.\n");
    return TRUE;
}

/***********************************************************************
 *           SetSystemPowerState      (KERNEL32.@)
 */
BOOL WINAPI SetSystemPowerState(BOOL suspend_or_hibernate,
                                  BOOL force_flag)
{
    WARN("(): stub, harmless.\n");
    return TRUE;
}

/***********************************************************************
 * SetThreadExecutionState (KERNEL32.@)
 *
 * Informs the system that activity is taking place for
 * power management purposes.
 */
EXECUTION_STATE WINAPI SetThreadExecutionState(EXECUTION_STATE flags)
{
    static EXECUTION_STATE current =
        ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED | ES_USER_PRESENT;
    EXECUTION_STATE old = current;

    WARN("(0x%x): stub, harmless.\n", flags);

    if (!(current & ES_CONTINUOUS) || (flags & ES_CONTINUOUS))
        current = flags;
    return old;
}

/***********************************************************************
 *           PowerCreateRequest      (KERNEL32.@)
 */
HANDLE WINAPI PowerCreateRequest(REASON_CONTEXT *context)
{
    FIXME("(%p): stub\n", context);

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return INVALID_HANDLE_VALUE;
}

/***********************************************************************
 *           PowerSetRequest      (KERNEL32.@)
 */
BOOL WINAPI PowerSetRequest(HANDLE request, POWER_REQUEST_TYPE type)
{
    FIXME("(%p, %u): stub\n", request, type);

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}

/***********************************************************************
 *           PowerClearRequest      (KERNEL32.@)
 */
BOOL WINAPI PowerClearRequest(HANDLE request, POWER_REQUEST_TYPE type)
{
    FIXME("(%p, %u): stub\n", request, type);

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}
