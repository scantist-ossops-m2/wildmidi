/*
 * out_alsa.c -- ALSA output
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

#ifdef AUDIODRV_ALSA

#include <stdlib.h>
#include <stdio.h>
#include <alsa/asoundlib.h>

#include "wildplay.h"

static int alsa_first_time = 1;
static snd_pcm_t *pcm = NULL;

static void close_alsa_output(void);

static int open_alsa_output(const char *pcmname, unsigned int *rate) {
    snd_pcm_hw_params_t *hw;
    snd_pcm_sw_params_t *sw;
    unsigned int alsa_buffer_time;
    unsigned int alsa_period_time;
    const unsigned int r = *rate;
    int err;

    if (!pcmname || !*pcmname) {
        pcmname = "default";
    }

    if ((err = snd_pcm_open(&pcm, pcmname, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        fprintf(stderr, "Error: audio open error: %s\r\n", snd_strerror(err));
        return -1;
    }

    snd_pcm_hw_params_alloca(&hw);

    if ((err = snd_pcm_hw_params_any(pcm, hw)) < 0) {
        fprintf(stderr, "ERROR: No configuration available for playback: %s\r\n", snd_strerror(err));
        goto fail;
    }

    if ((err = snd_pcm_hw_params_set_access(pcm, hw, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
        fprintf(stderr, "Cannot set access mode: %s.\r\n", snd_strerror(err));
        goto fail;
    }

    if (snd_pcm_hw_params_set_format(pcm, hw, SND_PCM_FORMAT_S16) < 0) {
        fprintf(stderr, "ALSA does not support 16bit signed audio for your soundcard\r\n");
        goto fail;
    }

    if (snd_pcm_hw_params_set_channels(pcm, hw, 2) < 0) {
        fprintf(stderr, "ALSA does not support stereo for your soundcard\r\n");
        goto fail;
    }

    if (snd_pcm_hw_params_set_rate_near(pcm, hw, rate, 0) < 0) {
        fprintf(stderr, "ALSA does not support %uHz for your soundcard\r\n", r);
        goto fail;
    }
    if (r != *rate) {
        fprintf(stderr, "ALSA: sample rate set to %uHz instead of %u\r\n", *rate, r);
    }

    alsa_buffer_time = 500000;
    alsa_period_time = 50000;

    if ((err = snd_pcm_hw_params_set_buffer_time_near(pcm, hw, &alsa_buffer_time, 0)) < 0) {
        fprintf(stderr, "Set buffer time failed: %s.\r\n", snd_strerror(err));
        goto fail;
    }

    if ((err = snd_pcm_hw_params_set_period_time_near(pcm, hw, &alsa_period_time, 0)) < 0) {
        fprintf(stderr, "Set period time failed: %s.\r\n", snd_strerror(err));
        goto fail;
    }

    if (snd_pcm_hw_params(pcm, hw) < 0) {
        fprintf(stderr, "Unable to install hw params\r\n");
        goto fail;
    }

    snd_pcm_sw_params_alloca(&sw);
    snd_pcm_sw_params_current(pcm, sw);
    if (snd_pcm_sw_params(pcm, sw) < 0) {
        fprintf(stderr, "Unable to install sw params\r\n");
        goto fail;
    }

    return (0);

fail:
    close_alsa_output();
    return -1;
}

static int write_alsa_output(void *data, int output_size) {
    const unsigned char *output_data = (unsigned char *)data;
    snd_pcm_uframes_t frames;
    int err;

    while (output_size > 0) {
        frames = snd_pcm_bytes_to_frames(pcm, output_size);
        if ((err = snd_pcm_writei(pcm, output_data, frames)) < 0) {
            if (snd_pcm_state(pcm) == SND_PCM_STATE_XRUN) {
                if ((err = snd_pcm_prepare(pcm)) < 0)
                    fprintf(stderr, "\nsnd_pcm_prepare() failed.\r\n");
                alsa_first_time = 1;
                continue;
            }
            return err;
        }

        output_size -= snd_pcm_frames_to_bytes(pcm, err);
        output_data += snd_pcm_frames_to_bytes(pcm, err);
        if (alsa_first_time) {
            alsa_first_time = 0;
            snd_pcm_start(pcm);
        }
    }
    return (0);
}

static void close_alsa_output(void) {
    if (!pcm)
        return;
    printf("Shutting down sound output\r\n");
    snd_pcm_close(pcm);
    pcm = NULL;
}

static void pause_alsa_output(void) {}
static void resume_alsa_output(void) {}

audiodrv_info audiodrv_alsa = {
    "alsa",
    "Advanced Linux Sound Architecture (ALSA) output",
    open_alsa_output,
    write_alsa_output,
    close_alsa_output,
    pause_alsa_output,
    resume_alsa_output
};

#endif
