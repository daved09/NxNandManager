#include "win_ioctl.h"

#if defined(_WIN32)

bool compareVolByInt(const volumeDescriptor& a, const volumeDescriptor b)
{
    return a.diskNumber < b.diskNumber;
}

bool GetVolumeMountPoint(wchar_t* letter, LPWSTR volumeName)
{
    TCHAR I;
    TCHAR Drive[] = TEXT("a:\\");
    TCHAR Volume[MAX_PATH];
    wcscat(volumeName, L"\\\0");
    for (I = TEXT('a'); I < TEXT('z'); I++)
    {
        Drive[0] = I;
        if (GetVolumeNameForVolumeMountPoint(Drive, Volume, MAX_PATH) && !lstrcmp(Volume, volumeName))
        {
            *letter = I;
            return true;
        }
    }
    return false;
}

bool GetVolumeDescriptor(volumeDescriptor* vd, LPWSTR VolumeName)
{
    DWORD dwBytesReturned;
    HANDLE hHandle = CreateFile(VolumeName, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (hHandle == INVALID_HANDLE_VALUE)
        return false;

    DISK_GEOMETRY pdg;
    if (!DeviceIoControl(hHandle, IOCTL_DISK_GET_DRIVE_GEOMETRY, NULL, 0, &pdg, sizeof(pdg), &dwBytesReturned, (LPOVERLAPPED)NULL))
        return false;

    STORAGE_PROPERTY_QUERY query;
    DWORD cbBytesReturned = 0;
    char local_buffer[10000];
    memset((void *)& query, 0, sizeof(query));
    query.PropertyId = StorageDeviceProperty;
    query.QueryType = PropertyStandardQuery;
    memset(local_buffer, 0, sizeof(local_buffer));

    if (!DeviceIoControl(hHandle, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query), &local_buffer[0], sizeof(local_buffer), &cbBytesReturned, nullptr))
        return false;

    STORAGE_DEVICE_DESCRIPTOR * descrip = (STORAGE_DEVICE_DESCRIPTOR *)& local_buffer;

    char productId[1000];
    char vendorId[1000];
    char serialNumber[1000];
    flipAndCodeBytes(local_buffer, descrip->VendorIdOffset, 0, vendorId);
    flipAndCodeBytes(local_buffer, descrip->ProductIdOffset, 0, productId);
    flipAndCodeBytes(local_buffer, descrip->SerialNumberOffset, 0, serialNumber);

    VOLUME_DISK_EXTENTS volumeDiskExtents;

    if (!DeviceIoControl(hHandle, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, NULL, 0, &volumeDiskExtents, sizeof(volumeDiskExtents), &dwBytesReturned, NULL))
        return false;

    PDISK_EXTENT pDiskExtent = &volumeDiskExtents.Extents[0];

    vd->removableMedia = descrip->RemovableMedia;
    vd->volumeName = std::wstring(VolumeName);
    vd->diskNumber = pDiskExtent->DiskNumber;
    vd->pId = std::string(productId);
    vd->vId = std::string(vendorId);    
    if (!vd->pId.find("UMS disk") && !vd->vId.find("Linux"))
        vd->vId.append(" (memloader)");
    vd->serialNumber = std::string(serialNumber);
    vd->diskSize = pdg.Cylinders.QuadPart * (ULONG)pdg.TracksPerCylinder * (ULONG)pdg.SectorsPerTrack * (ULONG)pdg.BytesPerSector;
    vd->diskStartOffset = pDiskExtent->StartingOffset.QuadPart;

    wchar_t mp;
    if (GetVolumeMountPoint(&mp, VolumeName))
        vd->mountPt = mp;

    GET_LENGTH_INFORMATION gli;
    if (DeviceIoControl(hHandle, IOCTL_DISK_GET_LENGTH_INFO, NULL, 0, &gli, sizeof(gli), &dwBytesReturned, NULL))
    {
        vd->size = gli.Length.QuadPart;
    }



    DWORD dwSectPerClust, dwBytesPerSect, dwFreeClusters, dwTotalClusters;
    if(GetDiskFreeSpace(VolumeName, &dwSectPerClust, &dwBytesPerSect, &dwFreeClusters, &dwTotalClusters))
    {
        vd->volumeTotalBytes = (u64)dwTotalClusters * dwSectPerClust * dwBytesPerSect;
        vd->volumeFreeBytes = (u64)dwFreeClusters * dwSectPerClust * dwBytesPerSect;
    }

    if (!vd->volumeTotalBytes)
        vd->volumeTotalBytes = pDiskExtent->ExtentLength.QuadPart;

    return true;
}

void GetVolumes(std::vector<volumeDescriptor> *volumes)
{
    volumes->clear();
    HANDLE hVol = INVALID_HANDLE_VALUE;
    HANDLE hHandle = INVALID_HANDLE_VALUE;
    DWORD dwBytesReturned;
    WCHAR  VolumeName[MAX_PATH] = L"";

    hVol = FindFirstVolumeW(VolumeName, ARRAYSIZE(VolumeName));
    if (hVol == INVALID_HANDLE_VALUE)
        return;

    do {
        if (hHandle != INVALID_HANDLE_VALUE) CloseHandle(hHandle);

        size_t Index = wcslen(VolumeName) - 1;

        if (VolumeName[0] != L'\\' || VolumeName[1] != L'\\' || VolumeName[2] != L'?' || VolumeName[3] != L'\\' || VolumeName[Index] != L'\\')
            continue;

        VolumeName[Index] = L'\0'; //remove trailing backslash

        volumeDescriptor vd;
        if (GetVolumeDescriptor(&vd, VolumeName))
            volumes->push_back(vd);

    } while (FindNextVolumeW(hVol, VolumeName, ARRAYSIZE(VolumeName)));
    FindVolumeClose(hVol);
    if (hHandle != INVALID_HANDLE_VALUE) CloseHandle(hHandle);

    // Get offline drives (memloader sometimes mounts NAND as on offline drive)
    for (int drive = 0; drive < 26; drive++)
    {
        bool found = false;
        for (volumeDescriptor vol : *volumes) if (vol.diskNumber == drive) found = true;
        if (found)
            continue;

        swprintf_s(VolumeName, MAX_PATH, L"\\\\.\\PhysicalDrive%d", drive);

        volumeDescriptor vd;
        if (GetVolumeDescriptor(&vd, VolumeName))
            volumes->push_back(vd);
    }

    std::sort(volumes->begin(), volumes->end(), compareVolByInt);
}

void GetDisks(std::vector<diskDescriptor> *disks)
{
    disks->clear();
    std::vector<volumeDescriptor> volumes;
    GetVolumes(&volumes);
    for (volumeDescriptor vol : volumes)
    {
        diskDescriptor* disk = nullptr;
        if (disks->size() > 0)
        {
            for (int i(0); i <= disks->size() - 1; i++) 
            {
                if (disks->at(i).diskNumber == vol.diskNumber)
                {
                    disk = &disks->at(i);
                }
            }
        }
        if (nullptr == disk)
        {
            diskDescriptor new_disk;
            new_disk.diskNumber = vol.diskNumber;
            new_disk.pId = vol.pId;
            new_disk.vId = vol.vId;
            new_disk.serialNumber = vol.serialNumber;
            new_disk.size = vol.diskSize;
            new_disk.removableMedia = vol.removableMedia;
            disks->push_back(new_disk);
            disk = &disks->back();
        }

        if (vol.volumeName.find(L"\\\\.\\PhysicalDrive"))
            disk->volumes.push_back(vol);
    }
}

bool operator == (diskDescriptor a, diskDescriptor b)
{
    if (a.serialNumber == b.serialNumber && a.volumes.size() == b.volumes.size())
        return true;
    else
        return false;
};

bool operator == (const volumeDescriptor a, const volumeDescriptor b)
{
    if (a.volumeName == b.volumeName)
        return true;
    else
        return false;
};

bool GetVolumeName(DWORD diskNumber, u64 startOffset, WCHAR *pVolumeName, size_t size)
{
    std::vector<volumeDescriptor> volumes;
    GetVolumes(&volumes);
    for (volumeDescriptor vol : volumes)
    {
        if (vol.diskNumber == diskNumber && vol.diskStartOffset == startOffset)
        {
            memcpy(pVolumeName, vol.volumeName.c_str(), size);
            dbg_wprintf(L"GetVolumeName(diskNumber = %I32d, offset = %I64d, WCHAR *pVolumeName = %s, size = %I32d\n", diskNumber, startOffset, vol.volumeName.c_str(), size);
            return true;
        }
    }
    return false;
}

bool DisMountVolume(volumeDescriptor vd)
{
    DWORD dwBytesReturned;
    HANDLE hHandle = CreateFile(vd.volumeName.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    
    if (hHandle == INVALID_HANDLE_VALUE)
        return false;

    bool result = DeviceIoControl(hHandle, FSCTL_DISMOUNT_VOLUME, NULL, 0, NULL, 0, &dwBytesReturned, NULL);
    
    CloseHandle(hHandle);

    return result;
}

bool DisMountAllVolumes(diskDescriptor dd)
{
    bool result = true;
    for (volumeDescriptor vol : dd.volumes)
    {
        if (!DisMountVolume(vol))
            result = false;
    }
    return result;
}

bool GetAvailableMountPoint(TCHAR *DriveLetter)
{
    TCHAR Buf[MAX_PATH];
    wchar_t Drive[4] = L"d:\\";
    // Look for existing or available mounting point
    for (wchar_t I = L'd'; I < L'z'; I++)
    {
        // Stamp the drive for the appropriate letter.
        Drive[0] = I;
        bool bFlag = GetVolumeNameForVolumeMountPoint(
            Drive,     // input volume mount point or directory
            Buf,       // output volume name buffer
            MAX_PATH); // size of volume name buffer
        if (!bFlag)
        {
            DriveLetter[0] = I;
            return true;
        }
    }
    return false;
}

std::vector<TCHAR> GetAvailableMountPoints()
{
    std::vector<TCHAR> mount_points;
    TCHAR Buf[MAX_PATH];
    wchar_t Drive[4] = L"d:\\";
    // Look for existing or available mounting point
    for (wchar_t I = L'd'; I < L'z'; I++)
    {
        // Stamp the drive for the appropriate letter.
        Drive[0] = I;
        bool bFlag = GetVolumeNameForVolumeMountPoint(
            Drive,     // input volume mount point or directory
            Buf,       // output volume name buffer
            MAX_PATH); // size of volume name buffer
        if (!bFlag)
        {
            mount_points.push_back(I);
        }
    }
    return mount_points;
}

bool isAvailableMountPoint(TCHAR *DriveLetter)
{
    TCHAR Buf[MAX_PATH];
    wchar_t Drive[4] = L" :\\";
    Drive[0] = DriveLetter[0];
    return !GetVolumeNameForVolumeMountPoint(Drive, Buf, MAX_PATH);

}

std::wstring GetPhysicalDrivePath(DWORD diskNumber)
{
    wchar_t path[MAX_PATH];
    swprintf_s(path, MAX_PATH, L"\\\\.\\PhysicalDrive%d", diskNumber);
    return std::wstring(path);
}

bool GetDiskNumberFromPath(const std::wstring &path, DWORD *diskNumber)
{
    std::wstring drive(path);
    std::transform(drive.begin(), drive.end(), drive.begin(), ::toupper);
    std::size_t pos = drive.find(L"PHYSICALDRIVE");
    if (pos == std::string::npos)
        return false;

    *diskNumber = (DWORD)stoi(drive.substr(pos + 13, 2));
    return true;
}

bool RefreshPartitionTable(const std::wstring &)
{
    return true;
}

std::wstring GetMountPointLabel(const std::wstring &mountPt)
{
    std::wstring label(mountPt);
    std::transform(label.begin(), label.end(), label.begin(), ::toupper);
    return label.append(L":");
}

#else // Linux

#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <mntent.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/sysmacros.h>
#include <sys/ioctl.h>
#include <linux/fs.h>

static std::string readSysAttr(const std::string &path)
{
    std::ifstream f(path);
    std::string value;
    if (f.is_open())
        std::getline(f, value);
    return trim(value);
}

static u64 readSysU64(const std::string &path)
{
    std::string v = readSysAttr(path);
    return v.length() ? std::strtoull(v.c_str(), nullptr, 10) : 0;
}

static bool readSysDev(const std::string &path, DWORD *dev)
{
    unsigned int maj, min;
    std::string v = readSysAttr(path);
    if (sscanf(v.c_str(), "%u:%u", &maj, &min) != 2)
        return false;
    *dev = (DWORD)makedev(maj, min);
    return true;
}

// Mount directory for a given device path (first match in /proc/mounts)
static std::string getMountDir(const std::string &devPath)
{
    struct stat dev_st;
    if (stat(devPath.c_str(), &dev_st))
        return "";

    std::string dir;
    FILE *mtab = setmntent("/proc/self/mounts", "r");
    if (!mtab)
        return dir;
    struct mntent *ent;
    while ((ent = getmntent(mtab)) != nullptr)
    {
        struct stat st;
        if (!stat(ent->mnt_fsname, &st) && S_ISBLK(st.st_mode) && st.st_rdev == dev_st.st_rdev)
        {
            dir = ent->mnt_dir;
            break;
        }
    }
    endmntent(mtab);
    return dir;
}

static bool isRemovableDisk(const std::string &name, const std::string &sysPath)
{
    if (readSysU64(sysPath + "/removable"))
        return true;

    // USB mass storage (memloader/hekate UMS, card readers) are often not flagged as removable
    char real[PATH_MAX];
    if (realpath(sysPath.c_str(), real) && std::string(real).find("/usb") != std::string::npos)
        return true;

    // Loop devices (disk images attached with losetup)
    if (!name.compare(0, 4, "loop"))
        return true;

    return false;
}

static void fillVolumeDescriptor(volumeDescriptor *vd, const diskDescriptor &disk, const std::string &devPath, u64 startOffset, u64 size)
{
    vd->removableMedia = disk.removableMedia;
    vd->volumeName = utf8_to_wstring(devPath);
    vd->diskNumber = disk.diskNumber;
    vd->pId = disk.pId;
    vd->vId = disk.vId;
    vd->serialNumber = disk.serialNumber;
    vd->diskSize = disk.size;
    vd->diskStartOffset = startOffset;
    vd->size = size;
    vd->volumeTotalBytes = size;
    vd->volumeFreeBytes = 0;

    std::string mountDir = getMountDir(devPath);
    if (mountDir.length())
    {
        vd->mountPt = utf8_to_wstring(mountDir);
        struct statvfs sv;
        if (!statvfs(mountDir.c_str(), &sv))
        {
            vd->volumeTotalBytes = (u64)sv.f_blocks * sv.f_frsize;
            vd->volumeFreeBytes = (u64)sv.f_bavail * sv.f_frsize;
        }
    }
}

void GetDisks(std::vector<diskDescriptor> *disks)
{
    disks->clear();
    DIR *dir = opendir("/sys/block");
    if (!dir)
        return;

    std::vector<std::string> names;
    struct dirent *ent;
    while ((ent = readdir(dir)) != nullptr)
        if (ent->d_name[0] != '.')
            names.push_back(ent->d_name);
    closedir(dir);
    std::sort(names.begin(), names.end());

    for (auto &name : names)
    {
        // Skip virtual devices that can't contain a NAND
        if (!name.compare(0, 3, "ram") || !name.compare(0, 4, "zram") || !name.compare(0, 2, "sr"))
            continue;

        std::string sysPath = "/sys/block/" + name;
        u64 size = readSysU64(sysPath + "/size") * 512;
        if (!size)
            continue;

        diskDescriptor disk;
        if (!readSysDev(sysPath + "/dev", &disk.diskNumber))
            continue;
        disk.size = size;
        disk.removableMedia = isRemovableDisk(name, sysPath);
        disk.vId = readSysAttr(sysPath + "/device/vendor");
        disk.pId = readSysAttr(sysPath + "/device/model");
        disk.serialNumber = readSysAttr(sysPath + "/device/serial");
        if (!name.compare(0, 4, "loop"))
        {
            disk.vId = "Loop device";
            disk.pId = readSysAttr(sysPath + "/loop/backing_file");
        }
        if (!disk.pId.find("UMS disk") && !disk.vId.find("Linux"))
            disk.vId.append(" (memloader)");
        if (!disk.serialNumber.length())
            disk.serialNumber = name;

        // Partitions
        std::vector<std::string> parts;
        if ((dir = opendir(sysPath.c_str())) != nullptr)
        {
            while ((ent = readdir(dir)) != nullptr)
                if (ent->d_name[0] != '.' && !access((sysPath + "/" + ent->d_name + "/partition").c_str(), F_OK))
                    parts.push_back(ent->d_name);
            closedir(dir);
        }
        std::sort(parts.begin(), parts.end(), [](const std::string &a, const std::string &b) {
            return a.length() != b.length() ? a.length() < b.length() : a < b;
        });

        for (auto &part : parts)
        {
            std::string partSys = sysPath + "/" + part;
            volumeDescriptor vd;
            fillVolumeDescriptor(&vd, disk, "/dev/" + part, readSysU64(partSys + "/start") * 512, readSysU64(partSys + "/size") * 512);
            disk.volumes.push_back(vd);
        }

        disks->push_back(disk);
    }
}

void GetVolumes(std::vector<volumeDescriptor> *volumes)
{
    volumes->clear();
    std::vector<diskDescriptor> disks;
    GetDisks(&disks);
    for (auto &disk : disks)
    {
        if (!disk.volumes.size())
        {
            // Disk without partition table, list the whole disk as a volume
            volumeDescriptor vd;
            fillVolumeDescriptor(&vd, disk, wstring_to_utf8(GetPhysicalDrivePath(disk.diskNumber)), 0, disk.size);
            volumes->push_back(vd);
        }
        for (auto &vol : disk.volumes)
            volumes->push_back(vol);
    }
}

bool GetVolumeDescriptor(volumeDescriptor* vd, LPWSTR volumeName)
{
    std::string path = wstring_to_utf8(volumeName);
    struct stat st;
    if (stat(path.c_str(), &st))
        return false;

    // A directory (mount point) was provided, look for underlying device
    if (S_ISDIR(st.st_mode))
    {
        std::vector<volumeDescriptor> volumes;
        GetVolumes(&volumes);
        char real[PATH_MAX];
        std::string dirPath = realpath(path.c_str(), real) ? std::string(real) : path;
        for (auto &vol : volumes) if (vol.mountPt.length() && wstring_to_utf8(vol.mountPt) == dirPath)
        {
            *vd = vol;
            return true;
        }
        // Not a mounted volume, but still a valid directory
        struct statvfs sv;
        if (statvfs(path.c_str(), &sv))
            return false;
        vd->removableMedia = false;
        vd->volumeName = utf8_to_wstring(path);
        vd->mountPt = utf8_to_wstring(dirPath);
        vd->volumeTotalBytes = (u64)sv.f_blocks * sv.f_frsize;
        vd->volumeFreeBytes = (u64)sv.f_bavail * sv.f_frsize;
        vd->size = vd->volumeTotalBytes;
        return true;
    }

    if (!S_ISBLK(st.st_mode))
        return false;

    std::vector<volumeDescriptor> volumes;
    GetVolumes(&volumes);
    for (auto &vol : volumes)
    {
        struct stat vst;
        if (!stat(wstring_to_utf8(vol.volumeName).c_str(), &vst) && vst.st_rdev == st.st_rdev)
        {
            *vd = vol;
            return true;
        }
    }
    return false;
}

bool GetVolumeMountPoint(wchar_t* letter, LPWSTR volumeName)
{
    std::string dir = getMountDir(wstring_to_utf8(volumeName));
    if (!dir.length())
        return false;
    *letter = L'\0';
    return true;
}

std::wstring GetPhysicalDrivePath(DWORD diskNumber)
{
    char link[64], target[PATH_MAX];
    snprintf(link, sizeof(link), "/sys/dev/block/%u:%u", major(diskNumber), minor(diskNumber));
    ssize_t len = readlink(link, target, sizeof(target) - 1);
    if (len <= 0)
        return std::wstring();
    target[len] = '\0';
    return utf8_to_wstring(std::string("/dev/") + base_name(std::string(target)));
}

bool GetDiskNumberFromPath(const std::wstring &path, DWORD *diskNumber)
{
    struct stat st;
    if (stat(wstring_to_utf8(path).c_str(), &st) || !S_ISBLK(st.st_mode))
        return false;

    char sysPath[128];
    snprintf(sysPath, sizeof(sysPath), "/sys/dev/block/%u:%u/partition", major(st.st_rdev), minor(st.st_rdev));
    if (!access(sysPath, F_OK))
    {
        // Partition, get parent disk
        snprintf(sysPath, sizeof(sysPath), "/sys/dev/block/%u:%u/../dev", major(st.st_rdev), minor(st.st_rdev));
        return readSysDev(sysPath, diskNumber);
    }
    *diskNumber = (DWORD)st.st_rdev;
    return true;
}

std::wstring GetMountPointLabel(const std::wstring &mountPt)
{
    return mountPt;
}

bool RefreshPartitionTable(const std::wstring &drivePath)
{
    int fd = open(wstring_to_utf8(drivePath).c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return false;

    fsync(fd);
    bool res = false;
    // The kernel may still hold the device for a short while after the last write
    for (int i = 0; i < 10 && !(res = !ioctl(fd, BLKRRPART)); i++)
        usleep(200000);
    close(fd);
    return res;
}

bool DisMountVolume(volumeDescriptor vd)
{
    if (!vd.mountPt.length())
        return true;

    DWORD dwBytesReturned;
    HANDLE hHandle = CreateFile(vd.volumeName.c_str(), GENERIC_READ, FILE_SHARE_WRITE | FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hHandle == INVALID_HANDLE_VALUE)
        return false;

    bool result = DeviceIoControl(hHandle, FSCTL_DISMOUNT_VOLUME, NULL, 0, NULL, 0, &dwBytesReturned, NULL);
    CloseHandle(hHandle);
    return result;
}

bool DisMountAllVolumes(diskDescriptor dd)
{
    bool result = true;
    for (volumeDescriptor vol : dd.volumes)
        if (!DisMountVolume(vol))
            result = false;
    return result;
}

bool operator == (diskDescriptor a, diskDescriptor b)
{
    return a.serialNumber == b.serialNumber && a.volumes.size() == b.volumes.size();
}

bool operator == (const volumeDescriptor a, const volumeDescriptor b)
{
    return a.volumeName == b.volumeName;
}

bool GetVolumeName(DWORD diskNumber, u64 startOffset, WCHAR *pVolumeName, size_t size)
{
    std::vector<volumeDescriptor> volumes;
    GetVolumes(&volumes);
    for (volumeDescriptor vol : volumes)
    {
        if (vol.diskNumber == diskNumber && vol.diskStartOffset == startOffset)
        {
            wcsncpy(pVolumeName, vol.volumeName.c_str(), size - 1);
            pVolumeName[size - 1] = L'\0';
            return true;
        }
    }
    return false;
}

// Drive letters do not exist on Linux
bool GetAvailableMountPoint(TCHAR *) { return false; }
std::vector<TCHAR> GetAvailableMountPoints() { return std::vector<TCHAR>(); }
bool isAvailableMountPoint(TCHAR *) { return false; }

#endif
