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

#include "win_compat.h"
#include <algorithm>

std::string native_path(std::string path)
{
#if !defined(_WIN32)
    std::replace(path.begin(), path.end(), '\\', '/');
#endif
    return path;
}

std::wstring native_path(std::wstring path)
{
#if !defined(_WIN32)
    std::replace(path.begin(), path.end(), L'\\', L'/');
#endif
    return path;
}

#if defined(_WIN32)

std::string wstring_to_utf8(const std::wstring &ws)
{
    int size = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string out(size > 0 ? size - 1 : 0, '\0');
    if (size > 1)
        WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, &out[0], size, nullptr, nullptr);
    return out;
}

std::wstring utf8_to_wstring(const std::string &s)
{
    int size = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring out(size > 0 ? size - 1 : 0, L'\0');
    if (size > 1)
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &out[0], size);
    return out;
}

#else

#include <cerrno>
#include <ctime>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/mount.h>
#include <sys/sysmacros.h>
#include <linux/fs.h>
#include <mntent.h>
#include <openssl/evp.h>
#include <vector>

/*
 * String conversion
 */

std::string wstring_to_utf8(const wchar_t *ws)
{
    std::string out;
    if (!ws)
        return out;

    for (; *ws; ws++)
    {
        uint32_t c = (uint32_t)*ws;
        if (c < 0x80)
            out += (char)c;
        else if (c < 0x800) {
            out += (char)(0xC0 | (c >> 6));
            out += (char)(0x80 | (c & 0x3F));
        }
        else if (c < 0x10000) {
            out += (char)(0xE0 | (c >> 12));
            out += (char)(0x80 | ((c >> 6) & 0x3F));
            out += (char)(0x80 | (c & 0x3F));
        }
        else {
            out += (char)(0xF0 | (c >> 18));
            out += (char)(0x80 | ((c >> 12) & 0x3F));
            out += (char)(0x80 | ((c >> 6) & 0x3F));
            out += (char)(0x80 | (c & 0x3F));
        }
    }
    return out;
}

std::string wstring_to_utf8(const std::wstring &ws)
{
    return wstring_to_utf8(ws.c_str());
}

std::wstring utf8_to_wstring(const char *s)
{
    std::wstring out;
    if (!s)
        return out;

    const unsigned char *p = (const unsigned char *)s;
    while (*p)
    {
        uint32_t c = *p;
        int extra = 0;
        if (c >= 0xF0 && c < 0xF8) { c &= 0x07; extra = 3; }
        else if (c >= 0xE0) { c &= 0x0F; extra = 2; }
        else if (c >= 0xC0) { c &= 0x1F; extra = 1; }
        else if (c >= 0x80) { out += (wchar_t)c; p++; continue; } // Invalid lead byte, keep as is (latin1)
        p++;
        for (int i = 0; i < extra; i++, p++)
        {
            if ((*p & 0xC0) != 0x80)
                break;
            c = (c << 6) | (*p & 0x3F);
        }
        out += (wchar_t)c;
    }
    return out;
}

std::wstring utf8_to_wstring(const std::string &s)
{
    return utf8_to_wstring(s.c_str());
}

/*
 * Errors
 */

DWORD GetLastError() { return (DWORD)errno; }
void SetLastError(DWORD err) { errno = (int)err; }

/*
 * Handles. A HANDLE stores (fd + 1) so that a null handle never maps to a valid fd (stdin)
 */

static inline int handle_to_fd(HANDLE h)
{
    if (h == INVALID_HANDLE_VALUE)
        return -1;
    return (int)((intptr_t)h - 1);
}

static inline HANDLE fd_to_handle(int fd)
{
    if (fd < 0)
        return INVALID_HANDLE_VALUE;
    return (HANDLE)((intptr_t)fd + 1);
}

static bool is_block_device(int fd)
{
    struct stat st;
    return !fstat(fd, &st) && S_ISBLK(st.st_mode);
}

static bool get_device_size(int fd, uint64_t *size)
{
    struct stat st;
    if (fstat(fd, &st))
        return false;

    if (S_ISBLK(st.st_mode))
        return !ioctl(fd, BLKGETSIZE64, size);

    *size = (uint64_t)st.st_size;
    return true;
}

HANDLE CreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD, void *, DWORD dwCreationDisposition, DWORD, HANDLE)
{
    std::string path = wstring_to_utf8(lpFileName);

    int flags = O_CLOEXEC;
    bool rd = dwDesiredAccess & GENERIC_READ, wr = dwDesiredAccess & GENERIC_WRITE;
    if (rd && wr) flags |= O_RDWR;
    else if (wr) flags |= O_WRONLY;
    else flags |= O_RDONLY;

    // Block devices cannot be created nor truncated
    struct stat st;
    bool isBlk = !stat(path.c_str(), &st) && S_ISBLK(st.st_mode);
    if (!isBlk)
    {
        switch (dwCreationDisposition) {
        case CREATE_NEW:    flags |= O_CREAT | O_EXCL; break;
        case CREATE_ALWAYS: flags |= O_CREAT | O_TRUNC; break;
        case OPEN_ALWAYS:   flags |= O_CREAT; break;
        default: break;
        }
    }

    int fd = open(path.c_str(), flags, 0644);

    // Windows lets a read/write handle be opened on read-only media (writes will fail later).
    // Mimic this by falling back to a read-only descriptor.
    if (fd < 0 && rd && wr && (errno == EROFS || errno == EACCES || errno == EPERM)
            && (isBlk || dwCreationDisposition == OPEN_EXISTING))
    {
        int err = errno;
        fd = open(path.c_str(), O_CLOEXEC | O_RDONLY);
        if (fd < 0) errno = err;
    }

    return fd_to_handle(fd);
}

BOOL ReadFile(HANDLE hFile, void *lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead, LPOVERLAPPED)
{
    int fd = handle_to_fd(hFile);
    if (lpNumberOfBytesRead) *lpNumberOfBytesRead = 0;
    if (fd < 0) { errno = EBADF; return FALSE; }

    DWORD total = 0;
    while (total < nNumberOfBytesToRead)
    {
        ssize_t r = read(fd, (char *)lpBuffer + total, nNumberOfBytesToRead - total);
        if (r < 0) {
            if (errno == EINTR) continue;
            if (lpNumberOfBytesRead) *lpNumberOfBytesRead = total;
            return FALSE;
        }
        if (r == 0) break; // EOF
        total += (DWORD)r;
    }
    if (lpNumberOfBytesRead) *lpNumberOfBytesRead = total;
    return TRUE;
}

BOOL WriteFile(HANDLE hFile, const void *lpBuffer, DWORD nNumberOfBytesToWrite, LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED)
{
    int fd = handle_to_fd(hFile);
    if (lpNumberOfBytesWritten) *lpNumberOfBytesWritten = 0;
    if (fd < 0) { errno = EBADF; return FALSE; }

    DWORD total = 0;
    while (total < nNumberOfBytesToWrite)
    {
        ssize_t w = write(fd, (const char *)lpBuffer + total, nNumberOfBytesToWrite - total);
        if (w < 0) {
            if (errno == EINTR) continue;
            if (lpNumberOfBytesWritten) *lpNumberOfBytesWritten = total;
            return FALSE;
        }
        if (w == 0) break;
        total += (DWORD)w;
    }
    if (lpNumberOfBytesWritten) *lpNumberOfBytesWritten = total;
    return TRUE;
}

BOOL SetFilePointerEx(HANDLE hFile, LARGE_INTEGER liDistanceToMove, PLARGE_INTEGER lpNewFilePointer, DWORD dwMoveMethod)
{
    int fd = handle_to_fd(hFile);
    if (fd < 0) { errno = EBADF; return FALSE; }

    int whence = dwMoveMethod == FILE_CURRENT ? SEEK_CUR : dwMoveMethod == FILE_END ? SEEK_END : SEEK_SET;
    off_t res = lseek(fd, (off_t)liDistanceToMove.QuadPart, whence);
    if (res == (off_t)-1)
        return FALSE;

    if (lpNewFilePointer)
        lpNewFilePointer->QuadPart = (int64_t)res;
    return TRUE;
}

BOOL GetFileSizeEx(HANDLE hFile, PLARGE_INTEGER lpFileSize)
{
    int fd = handle_to_fd(hFile);
    if (fd < 0) { errno = EBADF; return FALSE; }

    uint64_t size = 0;
    if (!get_device_size(fd, &size))
        return FALSE;

    lpFileSize->QuadPart = (int64_t)size;
    return TRUE;
}

BOOL GetHandleInformation(HANDLE hObject, LPDWORD lpdwFlags)
{
    int fd = handle_to_fd(hObject);
    if (fd < 0)
        return FALSE;

    int flags = fcntl(fd, F_GETFD);
    if (flags == -1)
        return FALSE;

    if (lpdwFlags) *lpdwFlags = 0;
    return TRUE;
}

BOOL CloseHandleImpl(HANDLE hObject)
{
    int fd = handle_to_fd(hObject);
    if (fd < 0) { errno = EBADF; return FALSE; }
    return close(fd) == 0;
}

// Unmount every mounted filesystem whose source device is "dev" (or one of its partitions if "whole_disk")
static bool unmount_device(dev_t dev, bool whole_disk)
{
    FILE *mtab = setmntent("/proc/self/mounts", "r");
    if (!mtab)
        return false;

    std::vector<std::string> targets;
    struct mntent *ent;
    while ((ent = getmntent(mtab)) != nullptr)
    {
        struct stat st;
        if (strncmp(ent->mnt_fsname, "/dev/", 5) || stat(ent->mnt_fsname, &st) || !S_ISBLK(st.st_mode))
            continue;

        bool match = st.st_rdev == dev;
        if (!match && whole_disk)
        {
            // Check if mounted device is a partition of dev, through sysfs
            char sys_path[128];
            snprintf(sys_path, sizeof(sys_path), "/sys/dev/block/%u:%u/partition", major(st.st_rdev), minor(st.st_rdev));
            if (!access(sys_path, F_OK))
            {
                snprintf(sys_path, sizeof(sys_path), "/sys/dev/block/%u:%u/../dev", major(st.st_rdev), minor(st.st_rdev));
                FILE *f = fopen(sys_path, "r");
                unsigned int maj, min;
                if (f && fscanf(f, "%u:%u", &maj, &min) == 2)
                    match = makedev(maj, min) == dev;
                if (f) fclose(f);
            }
        }
        if (match)
            targets.push_back(ent->mnt_dir);
    }
    endmntent(mtab);

    bool result = true;
    for (auto &t : targets)
        if (umount2(t.c_str(), 0))
            result = false;

    return result;
}

BOOL DeviceIoControl(HANDLE hDevice, DWORD dwIoControlCode, void *, DWORD, void *lpOutBuffer,
                     DWORD nOutBufferSize, LPDWORD lpBytesReturned, LPOVERLAPPED)
{
    int fd = handle_to_fd(hDevice);
    if (lpBytesReturned) *lpBytesReturned = 0;
    if (fd < 0) { errno = EBADF; return FALSE; }

    struct stat st;
    if (fstat(fd, &st))
        return FALSE;
    bool isBlk = S_ISBLK(st.st_mode);

    switch (dwIoControlCode) {
    case IOCTL_DISK_GET_DRIVE_GEOMETRY: {
        if (!isBlk || nOutBufferSize < sizeof(DISK_GEOMETRY)) { errno = ENOTTY; return FALSE; }
        uint64_t size = 0;
        int sector_size = 512;
        if (ioctl(fd, BLKGETSIZE64, &size))
            return FALSE;
        ioctl(fd, BLKSSZGET, &sector_size);
        auto pdg = reinterpret_cast<DISK_GEOMETRY *>(lpOutBuffer);
        pdg->MediaType = 0;
        pdg->BytesPerSector = (DWORD)sector_size;
        pdg->TracksPerCylinder = 255;
        pdg->SectorsPerTrack = 63;
        pdg->Cylinders.QuadPart = (int64_t)(size / ((uint64_t)255 * 63 * sector_size));
        if (lpBytesReturned) *lpBytesReturned = sizeof(DISK_GEOMETRY);
        return TRUE;
    }
    case IOCTL_DISK_GET_LENGTH_INFO: {
        if (!isBlk || nOutBufferSize < sizeof(GET_LENGTH_INFORMATION)) { errno = ENOTTY; return FALSE; }
        uint64_t size = 0;
        if (ioctl(fd, BLKGETSIZE64, &size))
            return FALSE;
        reinterpret_cast<GET_LENGTH_INFORMATION *>(lpOutBuffer)->Length.QuadPart = (int64_t)size;
        if (lpBytesReturned) *lpBytesReturned = sizeof(GET_LENGTH_INFORMATION);
        return TRUE;
    }
    case IOCTL_DISK_IS_WRITABLE: {
        if (!isBlk) { errno = ENOTTY; return FALSE; }
        int ro = 0;
        if (ioctl(fd, BLKROGET, &ro) || ro)
            return FALSE;
        // Check permissions on the underlying device node
        char fd_path[64];
        snprintf(fd_path, sizeof(fd_path), "/proc/self/fd/%d", fd);
        return access(fd_path, W_OK) == 0;
    }
    case FSCTL_LOCK_VOLUME:
    case FSCTL_UNLOCK_VOLUME:
        return TRUE;
    case FSCTL_DISMOUNT_VOLUME:
        if (!isBlk) { errno = ENOTTY; return FALSE; }
        return unmount_device(st.st_rdev, false);
    case IOCTL_STORAGE_EJECT_MEDIA:
        if (!isBlk) { errno = ENOTTY; return FALSE; }
        fsync(fd);
        ioctl(fd, BLKFLSBUF, 0);
        return TRUE;
    default:
        errno = ENOTTY;
        return FALSE;
    }
}

BOOL LockFileEx(HANDLE hFile, DWORD dwFlags, DWORD, DWORD, DWORD, LPOVERLAPPED)
{
    int fd = handle_to_fd(hFile);
    if (fd < 0) { errno = EBADF; return FALSE; }

    int op = dwFlags & LOCKFILE_EXCLUSIVE_LOCK ? LOCK_EX : LOCK_SH;
    if (dwFlags & LOCKFILE_FAIL_IMMEDIATELY)
        op |= LOCK_NB;
    return flock(fd, op) == 0;
}

DWORD GetFileAttributesW(LPCWSTR lpFileName)
{
    std::string path = wstring_to_utf8(lpFileName);
    struct stat st;
    if (stat(path.c_str(), &st))
        return INVALID_FILE_ATTRIBUTES;

    DWORD attr = 0;
    if (S_ISDIR(st.st_mode))
        attr |= FILE_ATTRIBUTE_DIRECTORY;
    if (access(path.c_str(), W_OK))
        attr |= FILE_ATTRIBUTE_READONLY;
    if (!attr)
        attr = FILE_ATTRIBUTE_NORMAL;
    return attr;
}

BOOL GetDiskFreeSpaceW(LPCWSTR lpRootPathName, LPDWORD lpSectorsPerCluster, LPDWORD lpBytesPerSector,
                       LPDWORD lpNumberOfFreeClusters, LPDWORD lpTotalNumberOfClusters)
{
    std::string path = lpRootPathName && *lpRootPathName ? wstring_to_utf8(lpRootPathName) : std::string(".");
    struct statvfs sv;
    if (statvfs(path.c_str(), &sv))
        return FALSE;

    // Scale "sectors per cluster" so that cluster counts fit in 32 bits
    uint64_t total = sv.f_blocks, avail = sv.f_bavail, spc = 1;
    while (total / spc > 0xFFFFFFFFULL)
        spc <<= 1;

    if (lpSectorsPerCluster) *lpSectorsPerCluster = (DWORD)spc;
    if (lpBytesPerSector) *lpBytesPerSector = (DWORD)sv.f_frsize;
    if (lpNumberOfFreeClusters) *lpNumberOfFreeClusters = (DWORD)(avail / spc);
    if (lpTotalNumberOfClusters) *lpTotalNumberOfClusters = (DWORD)(total / spc);
    return TRUE;
}

BOOL CreateDirectoryA(LPCSTR lpPathName, void *)
{
    return mkdir(native_path(std::string(lpPathName)).c_str(), 0755) == 0;
}

BOOL CreateDirectoryW(LPCWSTR lpPathName, void *)
{
    return CreateDirectoryA(wstring_to_utf8(lpPathName).c_str(), nullptr);
}

DWORD GetModuleFileNameW(HMODULE, LPWSTR lpFilename, DWORD nSize)
{
    char buf[MAX_PATH];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len < 0 || !nSize)
        return 0;
    buf[len] = '\0';
    std::wstring ws = utf8_to_wstring(buf);
    wcsncpy(lpFilename, ws.c_str(), nSize - 1);
    lpFilename[nSize - 1] = L'\0';
    return (DWORD)wcslen(lpFilename);
}

int MultiByteToWideChar(UINT, DWORD, LPCSTR lpMultiByteStr, int cbMultiByte, LPWSTR lpWideCharStr, int cchWideChar)
{
    std::string in = cbMultiByte < 0 ? std::string(lpMultiByteStr) : std::string(lpMultiByteStr, (size_t)cbMultiByte);
    std::wstring out = utf8_to_wstring(in);
    int required = (int)out.size() + (cbMultiByte < 0 ? 1 : 0);
    if (!cchWideChar || !lpWideCharStr)
        return required;
    int count = std::min(required, cchWideChar);
    wmemcpy(lpWideCharStr, out.c_str(), (size_t)count);
    if (cbMultiByte < 0 && count > 0)
        lpWideCharStr[count - 1] = L'\0';
    return count;
}

void Sleep(DWORD dwMilliseconds)
{
    usleep((useconds_t)dwMilliseconds * 1000);
}

DWORD SetThreadExecutionState(DWORD esFlags)
{
    return esFlags;
}

// FILETIME is the number of 100ns intervals since 1601-01-01 (UTC)
#define FILETIME_UNIX_EPOCH 116444736000000000ULL

BOOL FileTimeToDosDateTime(const FILETIME *lpFileTime, WORD *lpFatDate, WORD *lpFatTime)
{
    uint64_t ft = ((uint64_t)lpFileTime->dwHighDateTime << 32) | lpFileTime->dwLowDateTime;
    if (ft < FILETIME_UNIX_EPOCH)
        return FALSE;
    time_t t = (time_t)((ft - FILETIME_UNIX_EPOCH) / 10000000ULL);
    struct tm tm;
    gmtime_r(&t, &tm);
    if (tm.tm_year < 80 || tm.tm_year > 207)
        return FALSE;
    *lpFatDate = (WORD)(((tm.tm_year - 80) << 9) | ((tm.tm_mon + 1) << 5) | tm.tm_mday);
    *lpFatTime = (WORD)((tm.tm_hour << 11) | (tm.tm_min << 5) | (tm.tm_sec / 2));
    return TRUE;
}

BOOL DosDateTimeToFileTime(WORD wFatDate, WORD wFatTime, LPFILETIME lpFileTime)
{
    struct tm tm;
    memset(&tm, 0, sizeof(tm));
    tm.tm_year = ((wFatDate >> 9) & 0x7F) + 80;
    tm.tm_mon = ((wFatDate >> 5) & 0x0F) - 1;
    tm.tm_mday = wFatDate & 0x1F;
    tm.tm_hour = (wFatTime >> 11) & 0x1F;
    tm.tm_min = (wFatTime >> 5) & 0x3F;
    tm.tm_sec = (wFatTime & 0x1F) * 2;
    time_t t = timegm(&tm);
    if (t == (time_t)-1)
        return FALSE;
    uint64_t ft = (uint64_t)t * 10000000ULL + FILETIME_UNIX_EPOCH;
    lpFileTime->dwLowDateTime = (DWORD)ft;
    lpFileTime->dwHighDateTime = (DWORD)(ft >> 32);
    return TRUE;
}

/*
 * Crypto (MD5)
 */

BOOL CryptAcquireContext(HCRYPTPROV *phProv, LPCWSTR, LPCWSTR, DWORD, DWORD)
{
    if (phProv) *phProv = 1;
    return TRUE;
}

BOOL CryptCreateHash(HCRYPTPROV, DWORD Algid, uintptr_t, DWORD, HCRYPTHASH *phHash)
{
    if (Algid != CALG_MD5 || !phHash)
        return FALSE;

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx || !EVP_DigestInit_ex(ctx, EVP_md5(), nullptr)) {
        if (ctx) EVP_MD_CTX_free(ctx);
        return FALSE;
    }
    *phHash = (HCRYPTHASH)ctx;
    return TRUE;
}

BOOL CryptHashData(HCRYPTHASH hHash, const BYTE *pbData, DWORD dwDataLen, DWORD)
{
    if (!hHash)
        return FALSE;
    return EVP_DigestUpdate((EVP_MD_CTX *)hHash, pbData, dwDataLen) == 1;
}

BOOL CryptGetHashParam(HCRYPTHASH hHash, DWORD dwParam, BYTE *pbData, DWORD *pdwDataLen, DWORD)
{
    if (!hHash || dwParam != HP_HASHVAL || !pdwDataLen || *pdwDataLen < 16)
        return FALSE;

    // Finalize a copy so that the hash object stays usable
    EVP_MD_CTX *copy = EVP_MD_CTX_new();
    if (!copy || !EVP_MD_CTX_copy_ex(copy, (EVP_MD_CTX *)hHash)) {
        if (copy) EVP_MD_CTX_free(copy);
        return FALSE;
    }
    unsigned int len = 0;
    BOOL res = EVP_DigestFinal_ex(copy, pbData, &len) == 1;
    EVP_MD_CTX_free(copy);
    *pdwDataLen = len;
    return res;
}

BOOL CryptDestroyHash(HCRYPTHASH hHash)
{
    if (!hHash)
        return FALSE;
    EVP_MD_CTX_free((EVP_MD_CTX *)hHash);
    return TRUE;
}

#endif // !_WIN32
