/* Extended Module Player
 * Copyright (C) 1996-2012 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include "common.h"

#ifdef __AMIGA__
#include <sys/unistd.h>
#endif

/* TODO: use .ini file in Windows */

static char drive_id[32];
static char instrument_path[256];


static void delete_spaces(char *l)
{
    char *s;

    for (s = l; *s; s++) {
	if ((*s == ' ') || (*s == '\t')) {
	    memmove (s, s + 1, strlen (s));
	    s--;
	}
    }
}


static int get_yesno(char *s)
{
    return !(strncmp (s, "y", 1) && strncmp (s, "o", 1));
}


int _xmp_read_rc(struct xmp_context *ctx)
{
    struct xmp_options *o = &ctx->o;
    FILE *rc;
    char myrc[PATH_MAX];
    char *hash, *var, *val, line[256];
    char cparm[512];

#if defined __EMX__
    char myrc2[PATH_MAX];
    char *home = getenv("HOME");

    snprintf(myrc, PATH_MAX, "%s\\.xmp\\xmp.conf", home);

    if ((rc = fopen(myrc2, "r")) == NULL) {
	if ((rc = fopen(myrc, "r")) == NULL) {
	    if ((rc = fopen("xmp.conf", "r")) == NULL) {
		return -1;
	    }
	}
    }
#elif defined __AMIGA__
    strncpy(myrc, "PROGDIR:xmp.conf", PATH_MAX);

    if ((rc = fopen(myrc, "r")) == NULL)
	return -1;
#elif defined WIN32
    char *home = getenv("SystemRoot");

    snprintf(myrc, PATH_MAX, "%s/xmp.ini", home);

    if ((rc = fopen(myrc, "r")) == NULL)
	return -1;
#elif defined ANDROID
    if ((rc = fopen("/sdcard/xmp/xmp.conf", "r")) == NULL)
	return -1;
#else
    char *home = getenv("HOME");

    snprintf(myrc, PATH_MAX, "%s/.xmp/xmp.conf", home);
    /*snprintf(myrc2, PATH_MAX, "%s/.xmprc", home); -- deprecated */

    if ((rc = fopen(myrc, "r")) == NULL) {
	if ((rc = fopen(SYSCONFDIR "/xmp.conf", "r")) == NULL) {
	    return -1;
	}
    }
#endif

    while (!feof (rc)) {
	memset (line, 0, 256);
	fscanf (rc, "%255[^\n]", line);
	fgetc (rc);

	/* Delete comments */
	if ((hash = strchr (line, '#')))
	    *hash = 0;

	delete_spaces (line);

	if (!(var = strtok (line, "=\n")))
	    continue;

	val = strtok (NULL, " \t\n");

#define getval_yn(w,x,y) { \
	if (!strcmp(var,x)) { if (get_yesno (val)) w |= (y); \
	    else w &= ~(y); continue; } }

#define getval_no(x,y) { \
	if (!strcmp(var,x)) { y = atoi (val); continue; } }

	getval_yn(o->flags, "8bit", XMP_CTL_8BIT);
	getval_yn(o->flags, "interpolate", XMP_CTL_ITPT);
	getval_yn(o->flags, "loop", XMP_CTL_LOOP);
	getval_yn(o->flags, "reverse", XMP_CTL_REVERSE);
	getval_yn(o->flags, "pan", XMP_CTL_DYNPAN);
	getval_yn(o->flags, "filter", XMP_CTL_FILTER);
	getval_yn(o->outfmt, "mono", XMP_FMT_MONO);
	getval_no("amplify", o->amplify);
	getval_no("mix", o->mix);
	getval_no("srate", o->freq);
	getval_no("time", o->time);

	/* don't parse clickfilter in general config */
	/* don't parse vblank in general config */

	if (!strcmp (var, "driver")) {
	    strncpy (drive_id, val, 31);
	    o->drv_id = drive_id;
	    continue;
	}

	if (!strcmp (var, "amplify")) {
	    o->amplify = atoi (val);
	    continue;
	}

	if (!strcmp (var, "bits")) {
	    o->resol = atoi (val);
	    if (o->resol != 16 || o->resol != 8 || o->resol != 0)
		o->resol = 16;	/* #?# FIXME resol==0 -> U_LAW mode ? */
	    continue;
	}

	if (!strcmp (var, "instrument_path")) {
	    strncpy (instrument_path, val, 256);
	    o->ins_path = instrument_path;
	    continue;
	}
	/* If the line does not match any of the previous parameter,
	 * send it to the device driver
	 */
	snprintf(cparm, 512, "%s=%s", var, val);
	xmp_set_driver_parameter(&ctx->o, cparm);
    }

    fclose (rc);

    return 0;
}
