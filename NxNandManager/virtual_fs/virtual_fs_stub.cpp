/*
 * Copyright (c) 2021 eliboa (eliboa@gmail.com)
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

// Virtual filesystem stub for non-Windows platforms (Dokan is Windows only)

#if !defined(_WIN32)

#include "virtual_fs.h"

std::string dokanNtStatusToStr(NTSTATUS)
{
    return "Mounting partitions as virtual disk is not supported on this platform";
}

int installDokanDriver(bool)
{
    return ERR_DOKAN_DRIVER_NOT_FOUND;
}

namespace virtual_fs {

int virtual_fs::populate()
{
    return ERR_FAILED_TO_MOUNT_FS;
}

void virtual_fs::run()
{
    if (callback_func)
        callback_func(ERR_FAILED_TO_MOUNT_FS);

#if defined(ENABLE_GUI)
    emit dokan_callback((long)ERR_FAILED_TO_MOUNT_FS);
#endif
}

}  // namespace virtual_fs

#endif
