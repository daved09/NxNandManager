/*
 * Copyright (c) 2019 eliboa
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef __win_ioctl_h__
#define __win_ioctl_h__

#include <stdio.h>
#include <string>
#include <sys/types.h>
#include "win_compat.h"
#include "types.h"
#include <iostream>
#include <fstream>
#include <wchar.h>
#include <sstream>
#include <algorithm>
#include <vector>
#include "utils.h"

typedef struct {
    bool removableMedia;
    std::wstring volumeName;
    DWORD diskNumber = 0;
    u64 diskSize = 0;
    u64 diskStartOffset = 0;
    u64 volumeTotalBytes = 0;
    u64 volumeFreeBytes = 0;
    std::string serialNumber;
    std::string vId;
    std::string pId;
    u64 size = 0;
    std::wstring mountPt;
} volumeDescriptor;

typedef struct {
    bool removableMedia;
    DWORD diskNumber;
    std::string vId;
    std::string pId;
    std::string serialNumber;
    u64 size;
    std::vector<volumeDescriptor> volumes;
} diskDescriptor;

bool operator == (diskDescriptor a, diskDescriptor b);
bool operator == (volumeDescriptor a, volumeDescriptor b);

bool GetVolumeMountPoint(wchar_t* letter, LPWSTR volumeName);
bool GetVolumeDescriptor(volumeDescriptor* vd, LPWSTR volumeName);
void GetVolumes(std::vector<volumeDescriptor> *volumes);
void GetDisks(std::vector<diskDescriptor> *disks);
bool GetVolumeName(DWORD diskNumber, u64 startOffset, WCHAR *pVolumeName, size_t size = MAX_PATH);
bool DisMountVolume(volumeDescriptor vd);
bool DisMountAllVolumes(diskDescriptor dd);
bool GetAvailableMountPoint(TCHAR *DriveLetter);
std::vector<TCHAR> GetAvailableMountPoints();
bool isAvailableMountPoint(TCHAR *DriveLetter);
// Path to a physical drive from its disk number (\\.\PhysicalDriveX on Windows, /dev/xxx on Linux)
std::wstring GetPhysicalDrivePath(DWORD diskNumber);
// Disk number from a physical drive path. Returns false if path is not a physical drive
bool GetDiskNumberFromPath(const std::wstring &path, DWORD *diskNumber);
// Label for a volume mount point (drive letter on Windows, directory on Linux)
std::wstring GetMountPointLabel(const std::wstring &mountPt);
// Ask the OS to re-read the partition table of a physical drive (no-op on Windows)
bool RefreshPartitionTable(const std::wstring &drivePath);

#endif
