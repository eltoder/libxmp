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

#include <assert.h>

#include "loader.h"
#include "iff.h"
#include "period.h"

#define MAGIC_D_T_	MAGIC4('D','.','T','.')


static int dt_test(HIO_HANDLE *, char *, const int);
static int dt_load (struct module_data *, HIO_HANDLE *, const int);

const struct format_loader dt_loader = {
	"Digital Tracker",
	dt_test,
	dt_load
};

static int dt_test(HIO_HANDLE *f, char *t, const int start)
{
	if (hio_read32b(f) != MAGIC_D_T_)
		return -1;

	hio_read32b(f);			/* chunk size */
	hio_read16b(f);			/* type */
	hio_read16b(f);			/* 0xff then mono */
	hio_read16b(f);			/* reserved */
	hio_read16b(f);			/* tempo */
	hio_read16b(f);			/* bpm */
	hio_read32b(f);			/* undocumented */

	read_title(f, t, 32);

	return 0;
}


struct local_data {
    int pflag, sflag;
    int realpat;
    int last_pat;
    int insnum;
};


static int get_d_t_(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	int b;

	hio_read16b(f);			/* type */
	hio_read16b(f);			/* 0xff then mono */
	hio_read16b(f);			/* reserved */
	mod->spd = hio_read16b(f);
	if ((b = hio_read16b(f)) > 0)	/* RAMBO.DTM has bpm 0 */
		mod->bpm = b;
	hio_read32b(f);			/* undocumented */

	hio_read(mod->name, 32, 1, f);
	set_type(m, "Digital Tracker DTM");

	MODULE_INFO();

	return 0;
}

static int get_s_q_(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	int i, maxpat;

	mod->len = hio_read16b(f);
	mod->rst = hio_read16b(f);
	hio_read32b(f);	/* reserved */

	for (maxpat = i = 0; i < 128; i++) {
		mod->xxo[i] = hio_read8(f);
		if (mod->xxo[i] > maxpat)
			maxpat = mod->xxo[i];
	}
	mod->pat = maxpat + 1;

	return 0;
}

static int get_patt(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	struct local_data *data = (struct local_data *)parm;

	mod->chn = hio_read16b(f);
	data->realpat = hio_read16b(f);
	mod->trk = mod->chn * mod->pat;

	return 0;
}

static int get_inst(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	int i, c2spd;
	uint8 name[30];

	mod->ins = mod->smp = hio_read16b(f);

	D_(D_INFO "Instruments    : %d ", mod->ins);

	if (instrument_init(mod) < 0)
		return -1;

	for (i = 0; i < mod->ins; i++) {
		int fine, replen, flag;

		if (subinstrument_alloc(mod, i, 1) < 0)
			return -1;

		hio_read32b(f);		/* reserved */
		mod->xxs[i].len = hio_read32b(f);
		mod->xxi[i].nsm = !!mod->xxs[i].len;
		fine = hio_read8s(f);	/* finetune */
		mod->xxi[i].sub[0].vol = hio_read8(f);
		mod->xxi[i].sub[0].pan = 0x80;
		mod->xxs[i].lps = hio_read32b(f);
		replen = hio_read32b(f);
		mod->xxs[i].lpe = mod->xxs[i].lps + replen - 1;
		mod->xxs[i].flg = replen > 2 ?  XMP_SAMPLE_LOOP : 0;

		hio_read(name, 22, 1, f);
		instrument_name(mod, i, name, 22);

		flag = hio_read16b(f);	/* bit 0-7:resol 8:stereo */
		if ((flag & 0xff) > 8) {
			mod->xxs[i].flg |= XMP_SAMPLE_16BIT;
			mod->xxs[i].len >>= 1;
			mod->xxs[i].lps >>= 1;
			mod->xxs[i].lpe >>= 1;
		}

		hio_read32b(f);		/* midi note (0x00300000) */
		c2spd = hio_read32b(f);	/* frequency */
		c2spd_to_note(c2spd, &mod->xxi[i].sub[0].xpo, &mod->xxi[i].sub[0].fin);

		/* It's strange that we have both c2spd and finetune */
		mod->xxi[i].sub[0].fin += fine;

		mod->xxi[i].sub[0].sid = i;

		D_(D_INFO "[%2X] %-22.22s %05x%c%05x %05x %c%c %2db V%02x F%+03d %5d",
			i, mod->xxi[i].name,
			mod->xxs[i].len,
			mod->xxs[i].flg & XMP_SAMPLE_16BIT ? '+' : ' ',
			mod->xxs[i].lps,
			replen,
			mod->xxs[i].flg & XMP_SAMPLE_LOOP ? 'L' : ' ',
			flag & 0x100 ? 'S' : ' ',
			flag & 0xff,
			mod->xxi[i].sub[0].vol,
			fine,
			c2spd);
	}

	return 0;
}

static int get_dapt(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	struct local_data *data = (struct local_data *)parm;
	int pat, i, j, k;
	struct xmp_event *event;
	int rows;

	if (!data->pflag) {
		D_(D_INFO "Stored patterns: %d", mod->pat);
		data->pflag = 1;
		data->last_pat = 0;

		if (pattern_init(mod) < 0)
			return -1;
	}

	hio_read32b(f);	/* 0xffffffff */
	pat = hio_read16b(f);
	rows = hio_read16b(f);

	for (i = data->last_pat; i <= pat; i++) {
		if (pattern_tracks_alloc(mod, i, rows) < 0)
			return -1;
	}
	data->last_pat = pat + 1;

	for (j = 0; j < rows; j++) {
		for (k = 0; k < mod->chn; k++) {
			uint8 a, b, c, d;

			event = &EVENT(pat, k, j);
			a = hio_read8(f);
			b = hio_read8(f);
			c = hio_read8(f);
			d = hio_read8(f);
			if (a) {
				a--;
				event->note = 12 * (a >> 4) + (a & 0x0f) + 12;
			}
			event->vol = (b & 0xfc) >> 2;
			event->ins = ((b & 0x03) << 4) + (c >> 4);
			event->fxt = c & 0xf;
			event->fxp = d;
		}
	}

	return 0;
}

static int get_dait(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	struct local_data *data = (struct local_data *)parm;

	if (!data->sflag) {
		D_(D_INFO "Stored samples : %d ", mod->smp);
		data->sflag = 1;
		data->insnum = 0;
	}

	if (size > 2) {
		int ret = load_sample(m, f, SAMPLE_FLAG_BIGEND,
			&mod->xxs[mod->xxi[data->insnum].sub[0].sid], NULL);

		if (ret < 0)
			return -1;
	}

	data->insnum++;

	return 0;
}

static int dt_load(struct module_data *m, HIO_HANDLE *f, const int start)
{
	iff_handle handle;
	struct local_data data;
	int ret, i;

	LOAD_INIT();

	data.pflag = data.sflag = 0;
	
	handle = iff_new();
	if (handle == NULL)
		return -1;

	/* IFF chunk IDs */
	ret = iff_register(handle, "D.T.", get_d_t_);
	ret |= iff_register(handle, "S.Q.", get_s_q_);
	ret |= iff_register(handle, "PATT", get_patt);
	ret |= iff_register(handle, "INST", get_inst);
	ret |= iff_register(handle, "DAPT", get_dapt);
	ret |= iff_register(handle, "DAIT", get_dait);

	if (ret != 0)
		return -1;

	/* Load IFF chunks */
	ret = iff_load(handle, m, f , &data);
	iff_release(handle);
	if (ret < 0)
		return -1;

	//fix rest patterns
	for (i = data.last_pat; i < m->mod.pat; i++) {
		if (pattern_tracks_alloc(&m->mod, i, 64) < 0)
			return -1;
	}

	return 0;
}

