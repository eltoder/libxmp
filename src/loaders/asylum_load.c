/* Extended Module Player
 * Copyright (C) 1996-2014 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/*
 * Based on AMF->MOD converter written by Mr. P / Powersource, 1995
 */

#include "loader.h"
#include "period.h"

static int asylum_test(HIO_HANDLE *, char *, const int);
static int asylum_load(struct module_data *, HIO_HANDLE *, const int);

const struct format_loader asylum_loader = {
	"Asylum Music Format v1.0",
	asylum_test,
	asylum_load
};

static int asylum_test(HIO_HANDLE *f, char *t, const int start)
{
	char buf[32];

	if (hio_read(buf, 1, 32, f) < 32)
		return -1;

	if (memcmp(buf, "ASYLUM Music Format V1.0\0\0\0\0\0\0\0\0", 32))
		return -1;

	read_title(f, t, 0);

	return 0;
}

static int asylum_load(struct module_data *m, HIO_HANDLE *f, const int start)
{
	struct xmp_module *mod = &m->mod;
	struct xmp_event *event;
	int i, j;

	LOAD_INIT();

	hio_seek(f, 32, SEEK_CUR);			/* skip magic */
	mod->spd = hio_read8(f);			/* initial speed */
	mod->bpm = hio_read8(f);			/* initial BPM */
	mod->ins = hio_read8(f);			/* number of instruments */
	mod->pat = hio_read8(f);			/* number of patterns */
	mod->len = hio_read8(f);			/* module length */
	hio_read8(f);

	hio_read(mod->xxo, 1, mod->len, f);	/* read orders */
	hio_seek(f, start + 294, SEEK_SET);

	mod->chn = 8;
	mod->smp = mod->ins;
	mod->trk = mod->pat * mod->chn;

	snprintf(mod->type, XMP_NAME_SIZE, "Asylum Music Format v1.0");

	MODULE_INFO();

	if (instrument_init(mod) < 0)
		return -1;

	/* Read and convert instruments and samples */
	for (i = 0; i < mod->ins; i++) {
		uint8 insbuf[37];

		if (subinstrument_alloc(mod, i, 1) < 0)	
			return -1;

		hio_read(insbuf, 1, 37, f);
		instrument_name(mod, i, insbuf, 22);
		mod->xxi[i].sub[0].fin = (int8)(insbuf[22] << 4);
		mod->xxi[i].sub[0].vol = insbuf[23];
		mod->xxi[i].sub[0].xpo = (int8)insbuf[24];
		mod->xxi[i].sub[0].pan = 0x80;
		mod->xxi[i].sub[0].sid = i;

		mod->xxs[i].len = readmem32l(insbuf + 25);
		mod->xxs[i].lps = readmem32l(insbuf + 29);
		mod->xxs[i].lpe = mod->xxs[i].lps + readmem32l(insbuf + 33);
		
		mod->xxs[i].flg = mod->xxs[i].lpe > 2 ? XMP_SAMPLE_LOOP : 0;

		D_(D_INFO "[%2X] %-22.22s %04x %04x %04x %c V%02x %d", i,
		   mod->xxi[i].name, mod->xxs[i].len, mod->xxs[i].lps,
		   mod->xxs[i].lpe,
		   mod->xxs[i].flg & XMP_SAMPLE_LOOP ? 'L' : ' ',
		   mod->xxi[i].sub[0].vol, mod->xxi[i].sub[0].fin);
	}

	hio_seek(f, 37 * (64 - mod->ins), SEEK_CUR);

	D_(D_INFO "Module length: %d", mod->len);

	if (pattern_init(mod) < 0)
		return -1;

	/* Read and convert patterns */
	D_(D_INFO "Stored patterns: %d", mod->pat);

	for (i = 0; i < mod->pat; i++) {
		if (pattern_tracks_alloc(mod, i, 64) < 0)
			return -1;

		for (j = 0; j < 64 * mod->chn; j++) {
			uint8 note;

			event = &EVENT(i, j % mod->chn, j / mod->chn);
			memset(event, 0, sizeof(struct xmp_event));
			note = hio_read8(f);

			if (note != 0) {
				event->note = note + 13;
			}

			event->ins = hio_read8(f);
			event->fxt = hio_read8(f);
			event->fxp = hio_read8(f);
		}
	}

	/* Read samples */
	D_(D_INFO "Stored samples: %d", mod->smp);

	for (i = 0; i < mod->ins; i++) {
		if (mod->xxs[i].len > 1) {
			if (load_sample(m, f, 0, &mod->xxs[i], NULL) < 0)
				return -1;
			mod->xxi[i].nsm = 1;
		}
	}

	return 0;
}
