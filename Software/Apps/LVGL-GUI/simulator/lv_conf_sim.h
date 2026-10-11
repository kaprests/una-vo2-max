// Simulator-only LVGL config: the SDK's, with the pool scaled for 64-bit pointers.
// LV_MEM_SIZE is sized for the watch's 32-bit CPU; on a 64-bit host the same
// screens need roughly twice the pool, so the unscaled value runs out.
#include UNA_SDK_LV_CONF
#undef  LV_MEM_SIZE
#define LV_MEM_SIZE (40U * 1024U * sizeof(void*) / 4U)
