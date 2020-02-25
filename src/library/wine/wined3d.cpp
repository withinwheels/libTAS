/*
    Copyright 2015-2020 Clément Gallet <clement.gallet@ens-lyon.org>

    This file is part of libTAS.

    libTAS is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    libTAS is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with libTAS.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "wined3d.h"
#include "winehook.h"
#include "../hookpatch.h"
#include "../logging.h"
#include "../checkpoint/ThreadSync.h"
// #include <sys/mman.h>

namespace libtas {

namespace orig {

static void* __stdcall __attribute__((noinline)) wined3d_texture_get_resource(void *texture)
{
    static long x__ = 0;
    x__++;
    x__++;
    if (x__==2) {
        debuglog(LCF_HOOK | LCF_ERROR, "Function got called before it was set up!");
    }
    x__++;
    x__++;
    return nullptr;
}

unsigned long __attribute__((noinline)) wined3d_swapchain_present(void *swapchain, const void *src_rect,
        const void *dst_rect, void *dst_window_override, unsigned int swap_interval, unsigned int flags)
{
    static long x__ = 0;
    x__++;
    x__++;
    if (x__==2) {
        debuglog(LCF_HOOK | LCF_ERROR, "Function got called before it was set up!");
    }
    x__++;
    x__++;
    return 0;
}

}

void* __stdcall wined3d_texture_get_resource(void *texture)
{
    DEBUGLOGCALL(LCF_WINE);
    if (shared_config.game_specific_sync & SharedConfig::GC_SYNC_WITNESS) {
        ThreadSync::detSignal(true);
    }
    return orig::wined3d_texture_get_resource(texture);
}

unsigned long wined3d_swapchain_present(void *swapchain, const void *src_rect,
        const void *dst_rect, void *dst_window_override, unsigned int swap_interval, unsigned int flags)
{
    DEBUGLOGCALL(LCF_WINE);
    if (shared_config.game_specific_sync & SharedConfig::GC_SYNC_WITNESS) {
        ThreadSync::detInit();
    }
    return orig::wined3d_swapchain_present(swapchain, src_rect, dst_rect, dst_window_override, swap_interval, flags);
}

void hook_wined3d()
{
    HOOK_PATCH_ORIG(wined3d_texture_get_resource, "wined3d.dll.so");
    HOOK_PATCH_ORIG(wined3d_swapchain_present, "wined3d.dll.so");
}


}
