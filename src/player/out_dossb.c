/*
 * out_dossb.c -- DOS SoundBlaster output
 *
 * Copyright (C) WildMidi Developers 2020
 *
 * This file is part of WildMIDI.
 *
 * WildMIDI is free software: you can redistribute and/or modify the player
 * under the terms of the GNU General Public License and you can redistribute
 * and/or modify the library under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation, either version 3 of
 * the licenses, or(at your option) any later version.
 *
 * WildMIDI is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License and
 * the GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License and the
 * GNU Lesser General Public License along with WildMIDI.  If not,  see
 * <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#ifdef AUDIODRV_DOSSB

#include <stdio.h>
#include <string.h>

#include "wildplay.h"
#include "dossb.h"

/* SoundBlaster/Pro/16/AWE32 driver for DOS -- adapted from
 * libMikMod,  written by Andrew Zabolotny <bit@eltech.ru>,
 * further fixes by O.Sezer <sezero@users.sourceforge.net>.
 * Timer callback functionality replaced by a push mechanism
 * to keep the wildmidi player changes to a minimum, for now.
 */

/* The last buffer byte filled with sound */
static unsigned int buff_tail = 0;

static int write_sb_common(void *data, int siz) {
    unsigned int dma_size, dma_pos;
    unsigned int cnt;

    sb_query_dma(&dma_size, &dma_pos);
    /* There isn't much sense in filling less than 256 bytes */
    dma_pos &= ~255;

    /* If nothing to mix, quit */
    if (buff_tail == dma_pos)
        return 0;

    /* If DMA pointer still didn't wrapped around ... */
    if (dma_pos > buff_tail) {
        if ((cnt = dma_pos - buff_tail) > (unsigned int)siz)
            cnt = siz;
        memcpy(sb.dma_buff->linear + buff_tail, data, cnt);
        buff_tail += cnt;
        /* If we arrived right to the DMA buffer end, jump to the beginning */
        if (buff_tail >= dma_size)
            buff_tail = 0;
    } else {
        /* If wrapped around, fill first to the end of buffer */
        if ((cnt = dma_size - buff_tail) > (unsigned int)siz)
            cnt = siz;
        memcpy(sb.dma_buff->linear + buff_tail, data, cnt);
        buff_tail += cnt;
        siz -= cnt;
        if (!siz)
            return cnt;

        /* Now fill from buffer beginning to current DMA pointer */
        if (dma_pos > (unsigned int)siz)
            dma_pos = siz;
        data = (char *)data + cnt;
        cnt += dma_pos;

        memcpy(sb.dma_buff->linear, data, dma_pos);
        buff_tail = dma_pos;
    }

    return cnt;
}

static int write_sb_output(void *data, int siz) {
    int i;

    if (sb.caps & SBMODE_16BITS) {
        /* libWildMidi sint16 stereo -> SB16 sint16 stereo */
        /* do nothing, we can do 16 bit stereo by default */
    } else if (sb.caps & SBMODE_STEREO) {
        /* libWildMidi sint16 stereo -> SB uint8 stereo */
        int16_t *src = (int16_t *) data;
        uint8_t *dst = (uint8_t *) data;
        i = (siz /= 2);
        for (; i >= 0; --i) {
            *dst++ = (*src++ >> 8) + 128;
        }
    } else {
        /* libWildMidi sint16 stereo -> SB uint8 mono */
        int16_t *src = (int16_t *) data;
        uint8_t *dst = (uint8_t *) data;
        i = (siz /= 4); int val;
        for (; i >= 0; --i) {
            /* do a cheap (left+right)/2 */
            val  = *src++;
            val += *src++;
            *dst++ = (val >> 9) + 128;
        }
    }
    while (1) {
        i = write_sb_common(data, siz);
        if ((siz -= i) <= 0)
            return 0;

        data = (char *)data + i;
        /*usleep(100);*/
    }
}

static void pause_sb_output(void) {
    if (sb.caps & SBMODE_16BITS) {
        // 16 bit
        memset(sb.dma_buff->linear, 0, sb.dma_buff->size);
    } else {
        // 8 bit
        memset(sb.dma_buff->linear, 0x80, sb.dma_buff->size);
    }
}

static void resume_sb_output(void) {}

static void close_sb_output(void) {
    sb.timer_callback = NULL;
    sb_output(FALSE);
    sb_stop_dma();
    sb_close();
}

static int open_sb_output(const char *output, unsigned int *rate) {
    WMPLAY_UNUSED(output);

    if (!sb_open()) {
        fprintf(stderr, "Sound Blaster initialization failed.\n");
        return -1;
    }

    if (*rate < 4000) *rate = 4000;
    if (sb.caps & SBMODE_STEREO) {
        if (*rate > sb.maxfreq_stereo)
            *rate = sb.maxfreq_stereo;
    } else {
        if (*rate > sb.maxfreq_mono)
            *rate = sb.maxfreq_mono;
    }

    /* Enable speaker output */
    sb_output(TRUE);

    /* Set our routine to be called during SB IRQs */
    buff_tail = 0;
    sb.timer_callback = NULL;/* see above  */

    /* Start cyclic DMA transfer */
    if (!sb_start_dma(((sb.caps & SBMODE_16BITS) ? SBMODE_16BITS | SBMODE_SIGNED : 0) |
                            (sb.caps & SBMODE_STEREO), *rate)) {
        sb_output(FALSE);
        sb_close();
        fprintf(stderr, "Sound Blaster: DMA start failed.\n");
        return -1;
    }

    return 0;
}

audiodrv_info audiodrv_dossb = {
    "dossb",
    "DOS SoundBlaster output",
    open_sb_output,
    write_sb_output,
    close_sb_output,
    pause_sb_output,
    resume_sb_output
};

#endif /* AUDIODRV_DOSSB */
