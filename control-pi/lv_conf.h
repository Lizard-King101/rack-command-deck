#if 1 /* Set to 1 to enable */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

#define LV_COLOR_DEPTH 32

/*-- Memory --*/
#define LV_MEM_SIZE (32 * 1024 * 1024U) /* Room for optional full-HD GIF screensavers */
#define LV_MEM_POOL_INCLUDE <stdlib.h>
#define LV_MEM_POOL_ALLOC   malloc
#define LV_MEM_POOL_FREE    free

/*-- HAL --*/
#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE <sys/time.h>
#define LV_TICK_CUSTOM_SYS_TIME_EXPR \
    ({struct timeval tv; gettimeofday(&tv, NULL); \
      (uint32_t)(tv.tv_sec * 1000 + tv.tv_usec / 1000);})

/*-- Logging --*/
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 1

/*-- Features --*/
#define LV_USE_FLEX   1
#define LV_USE_GRID   1
#define LV_USE_LABEL  1
#define LV_USE_BTN    1
#define LV_USE_BAR    1
#define LV_USE_ARC    1
#define LV_USE_CHART  1
#define LV_USE_TABLE  1
#define LV_USE_SCROLL 1
#define LV_USE_SNAPSHOT 1
#define LV_USE_GIF 1
#define LV_GIF_CACHE_DECODE_DATA 0
#define LV_USE_FFMPEG 1
#define LV_FFMPEG_DUMP_FORMAT 0
#define LV_USE_FS_STDIO 'A'
#define LV_FS_STDIO_LETTER 'A'
#define LV_FS_STDIO_PATH ""
#define LV_FS_STDIO_CACHE_SIZE 0

/*-- Fonts --*/
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_48 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

/*-- Linux drivers (hardware) --*/
#define LV_USE_LINUX_FBDEV  1
#define LV_USE_EVDEV        1

/*-- SDL2 driver (emulator) --*/
#define LV_USE_SDL          1

#define LV_USE_DRAW_SW 1
#define LV_USE_DRAW_SW_ASM      LV_DRAW_SW_ASM_NONE
#define LV_USE_NATIVE_HELIUM_ASM 0

#endif /* LV_CONF_H */
#endif /* Set to 1 */
