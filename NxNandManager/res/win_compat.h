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

/*
 * Minimal Win32 API emulation layer for non-Windows (POSIX/Linux) builds.
 * On Windows, this header simply includes the native Windows headers.
 */

#ifndef __win_compat_h__
#define __win_compat_h__

#if defined(_WIN32)

#include <windows.h>
#include <winioctl.h>
#include <Wincrypt.h>
#include <tchar.h>
#include <string>

#else

extern "C++" {

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cwchar>
#include <string>

// Types
typedef uint32_t DWORD;
typedef DWORD* LPDWORD;
typedef DWORD* PDWORD;
typedef int BOOL;
typedef BOOL* PBOOL;
typedef unsigned char BYTE;
typedef BYTE* LPBYTE;
typedef BYTE* PBYTE;
typedef uint16_t WORD;
typedef uint16_t USHORT;
typedef uint32_t UINT;
typedef uint32_t ULONG;
typedef int32_t LONG;
typedef int64_t LONGLONG;
typedef uint64_t ULONGLONG;
typedef uint64_t ULONG64;
typedef uint64_t QWORD;
typedef char CHAR;
typedef wchar_t WCHAR;
#ifndef _TCHAR_DEFINED
#define _TCHAR_DEFINED
typedef wchar_t TCHAR;
#endif
typedef WCHAR* LPWSTR;
typedef const WCHAR* LPCWSTR;
typedef TCHAR* LPTSTR;
typedef const TCHAR* LPCTSTR;
typedef char* LPSTR;
typedef const char* LPCSTR;
typedef void* LPVOID;
typedef void* HANDLE;
typedef void* HMODULE;
typedef HANDLE* PHANDLE;
typedef int32_t NTSTATUS;
typedef uintptr_t HCRYPTPROV;
typedef uintptr_t HCRYPTHASH;

typedef union _LARGE_INTEGER {
    struct {
        DWORD LowPart;
        LONG HighPart;
    };
    int64_t QuadPart;
} LARGE_INTEGER, *PLARGE_INTEGER;

typedef struct _FILETIME {
    DWORD dwLowDateTime;
    DWORD dwHighDateTime;
} FILETIME, *PFILETIME, *LPFILETIME;

typedef struct _OVERLAPPED {
    DWORD Offset;
    DWORD OffsetHigh;
    HANDLE hEvent;
} OVERLAPPED, *LPOVERLAPPED;

typedef struct _DISK_GEOMETRY {
    LARGE_INTEGER Cylinders;
    int MediaType;
    DWORD TracksPerCylinder;
    DWORD SectorsPerTrack;
    DWORD BytesPerSector;
} DISK_GEOMETRY, *PDISK_GEOMETRY;

typedef struct _GET_LENGTH_INFORMATION {
    LARGE_INTEGER Length;
} GET_LENGTH_INFORMATION;

// Constants
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef MAX_PATH
#define MAX_PATH 4096
#endif
#ifndef _MAX_PATH
#define _MAX_PATH MAX_PATH
#endif
#define WINAPI
#define CONST const

#define GENERIC_READ                0x80000000
#define GENERIC_WRITE               0x40000000
#define FILE_SHARE_READ             0x00000001
#define FILE_SHARE_WRITE            0x00000002
#define CREATE_NEW                  1
#define CREATE_ALWAYS               2
#define OPEN_EXISTING               3
#define OPEN_ALWAYS                 4
#define FILE_FLAG_NO_BUFFERING      0x20000000
#define FILE_FLAG_SEQUENTIAL_SCAN   0x08000000
#define FILE_ATTRIBUTE_READONLY     0x00000001
#define FILE_ATTRIBUTE_HIDDEN       0x00000002
#define FILE_ATTRIBUTE_SYSTEM       0x00000004
#define FILE_ATTRIBUTE_DIRECTORY    0x00000010
#define FILE_ATTRIBUTE_ARCHIVE      0x00000020
#define FILE_ATTRIBUTE_NORMAL       0x00000080
#define FILE_ATTRIBUTE_VIRTUAL      0x00010000
#define INVALID_FILE_ATTRIBUTES     ((DWORD)-1)
#define FILE_BEGIN                  0
#define FILE_CURRENT                1
#define FILE_END                    2
#define INVALID_SET_FILE_POINTER    ((DWORD)-1)
#define INVALID_HANDLE_VALUE        ((HANDLE)(intptr_t)-1)
#define LOCKFILE_FAIL_IMMEDIATELY   0x00000001
#define LOCKFILE_EXCLUSIVE_LOCK     0x00000002
#define ERROR_SUCCESS               0

// IOCTL codes (values are internal to this layer)
#define IOCTL_DISK_GET_DRIVE_GEOMETRY   0x00070000
#define IOCTL_DISK_IS_WRITABLE          0x00070024
#define IOCTL_DISK_GET_LENGTH_INFO      0x0007405C
#define IOCTL_STORAGE_EJECT_MEDIA       0x002D4808
#define FSCTL_LOCK_VOLUME               0x00090018
#define FSCTL_UNLOCK_VOLUME             0x0009001C
#define FSCTL_DISMOUNT_VOLUME           0x00090020

// Power management (no-op)
#define ES_CONTINUOUS               0x80000000
#define ES_SYSTEM_REQUIRED          0x00000001
#define ES_AWAYMODE_REQUIRED        0x00000040

// Crypto (MD5 only)
#define PROV_RSA_FULL               1
#define CRYPT_VERIFYCONTEXT         0xF0000000
#define CALG_MD5                    0x00008003
#define HP_HASHVAL                  0x0002

// Code pages
#define CP_ACP                      0
#define CP_UTF8                     65001

// Macros
#define TEXT(x) L##x
#ifndef _T
#define _T(x) L##x
#endif
#define ZeroMemory(p, s) memset((p), 0, (s))
#define ARRAYSIZE(a) (sizeof(a) / sizeof((a)[0]))
#define lstrcmp wcscmp
#define lstrlen wcslen
#define strcpy_s strcpy
#define sprintf_s snprintf
#define swprintf_s swprintf
#define memcpy_s(dest, destsz, src, count) memcpy((dest), (src), (count))

#define DEFINE_ENUM_FLAG_OPERATORS(ENUMTYPE) \
inline ENUMTYPE operator | (ENUMTYPE a, ENUMTYPE b) { return ENUMTYPE(((int)a) | ((int)b)); } \
inline ENUMTYPE &operator |= (ENUMTYPE &a, ENUMTYPE b) { return (ENUMTYPE &)(((int &)a) |= ((int)b)); } \
inline ENUMTYPE operator & (ENUMTYPE a, ENUMTYPE b) { return ENUMTYPE(((int)a) & ((int)b)); } \
inline ENUMTYPE &operator &= (ENUMTYPE &a, ENUMTYPE b) { return (ENUMTYPE &)(((int &)a) &= ((int)b)); } \
inline ENUMTYPE operator ~ (ENUMTYPE a) { return ENUMTYPE(~((int)a)); } \
inline ENUMTYPE operator ^ (ENUMTYPE a, ENUMTYPE b) { return ENUMTYPE(((int)a) ^ ((int)b)); } \
inline ENUMTYPE &operator ^= (ENUMTYPE &a, ENUMTYPE b) { return (ENUMTYPE &)(((int &)a) ^= ((int)b)); }

// String conversion helpers (UTF-8 <-> wchar_t)
std::string wstring_to_utf8(const std::wstring &ws);
std::string wstring_to_utf8(const wchar_t *ws);
std::wstring utf8_to_wstring(const std::string &s);
std::wstring utf8_to_wstring(const char *s);

// Errors
DWORD GetLastError();
void SetLastError(DWORD err);

// File I/O
HANDLE CreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, void *lpSecurityAttributes,
                   DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);
#define CreateFile CreateFileW
BOOL ReadFile(HANDLE hFile, void *lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped);
BOOL WriteFile(HANDLE hFile, const void *lpBuffer, DWORD nNumberOfBytesToWrite, LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED lpOverlapped);
BOOL SetFilePointerEx(HANDLE hFile, LARGE_INTEGER liDistanceToMove, PLARGE_INTEGER lpNewFilePointer, DWORD dwMoveMethod);
BOOL GetFileSizeEx(HANDLE hFile, PLARGE_INTEGER lpFileSize);
BOOL GetHandleInformation(HANDLE hObject, LPDWORD lpdwFlags);
BOOL CloseHandleImpl(HANDLE hObject);
// Closing a handle also invalidates the variable holding it: POSIX reuses file descriptors
// aggressively, so a stale handle could otherwise end up closing an unrelated file.
inline BOOL CloseHandle(HANDLE &hObject) { BOOL r = CloseHandleImpl(hObject); hObject = INVALID_HANDLE_VALUE; return r; }
inline BOOL CloseHandle(HANDLE &&hObject) { return CloseHandleImpl(hObject); }
BOOL DeviceIoControl(HANDLE hDevice, DWORD dwIoControlCode, void *lpInBuffer, DWORD nInBufferSize, void *lpOutBuffer,
                     DWORD nOutBufferSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped);
BOOL LockFileEx(HANDLE hFile, DWORD dwFlags, DWORD dwReserved, DWORD nNumberOfBytesToLockLow,
                DWORD nNumberOfBytesToLockHigh, LPOVERLAPPED lpOverlapped);
DWORD GetFileAttributesW(LPCWSTR lpFileName);
BOOL GetDiskFreeSpaceW(LPCWSTR lpRootPathName, LPDWORD lpSectorsPerCluster, LPDWORD lpBytesPerSector,
                       LPDWORD lpNumberOfFreeClusters, LPDWORD lpTotalNumberOfClusters);
#define GetDiskFreeSpace GetDiskFreeSpaceW
BOOL CreateDirectoryW(LPCWSTR lpPathName, void *lpSecurityAttributes);
BOOL CreateDirectoryA(LPCSTR lpPathName, void *lpSecurityAttributes);
DWORD GetModuleFileNameW(HMODULE hModule, LPWSTR lpFilename, DWORD nSize);
#define GetModuleFileName GetModuleFileNameW

// Misc
int MultiByteToWideChar(UINT CodePage, DWORD dwFlags, LPCSTR lpMultiByteStr, int cbMultiByte, LPWSTR lpWideCharStr, int cchWideChar);
void Sleep(DWORD dwMilliseconds);
DWORD SetThreadExecutionState(DWORD esFlags);
BOOL FileTimeToDosDateTime(const FILETIME *lpFileTime, WORD *lpFatDate, WORD *lpFatTime);
BOOL DosDateTimeToFileTime(WORD wFatDate, WORD wFatTime, LPFILETIME lpFileTime);

// Crypto (MD5 through OpenSSL)
BOOL CryptAcquireContext(HCRYPTPROV *phProv, LPCWSTR szContainer, LPCWSTR szProvider, DWORD dwProvType, DWORD dwFlags);
BOOL CryptCreateHash(HCRYPTPROV hProv, DWORD Algid, uintptr_t hKey, DWORD dwFlags, HCRYPTHASH *phHash);
BOOL CryptHashData(HCRYPTHASH hHash, const BYTE *pbData, DWORD dwDataLen, DWORD dwFlags);
BOOL CryptGetHashParam(HCRYPTHASH hHash, DWORD dwParam, BYTE *pbData, DWORD *pdwDataLen, DWORD dwFlags);
BOOL CryptDestroyHash(HCRYPTHASH hHash);

} // extern "C++"

#endif // !_WIN32

#if defined(_WIN32)
#define PATH_SEPARATOR "\\"
#else
#define PATH_SEPARATOR "/"
#endif

extern "C++" {
// Converts a path using Windows separators to the native separator
std::string native_path(std::string path);
std::wstring native_path(std::wstring path);
#if defined(_WIN32)
std::string wstring_to_utf8(const std::wstring &ws);
std::wstring utf8_to_wstring(const std::string &s);
#endif
}

#endif // __win_compat_h__
