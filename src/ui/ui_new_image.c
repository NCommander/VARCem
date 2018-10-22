/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Generic code support for the New Floppy Image dialog.
 *
 * NOTE:	Most of this code should be moved to the Floppy image file
 *		format handlers, and re-integrated with that code. This is
 *		just the wrong place for it..
 *
 * Version:	@(#)ui_new_image.c	1.0.4	2018/10/05
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2018 Fred N. van Kempen.
 *		Copyright 2018 Miran Grca.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free  Software  Foundation; either  version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is  distributed in the hope that it will be useful, but
 * WITHOUT   ANY  WARRANTY;  without  even   the  implied  warranty  of
 * MERCHANTABILITY  or FITNESS  FOR A PARTICULAR  PURPOSE. See  the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the:
 *
 *   Free Software Foundation, Inc.
 *   59 Temple Place - Suite 330
 *   Boston, MA 02111-1307
 *   USA.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>
#include "../emu.h"
#include "../random.h"
#include "../ui/ui.h"
#include "../plat.h"
#include "../devices/scsi/scsi_device.h"
#include "../devices/disk/zip.h"


typedef struct {
    int8_t	hole;
    int8_t	sides;
    int8_t	data_rate;
    int8_t	encoding;
    int8_t	rpm;
    uint8_t	tracks;
    int8_t	sectors;		/* for IMG and Japanese FDI only */
    int8_t	sector_len;		/* for IMG and Japanese FDI only */

    uint8_t	media_desc;
    int8_t	spc;
    int8_t	num_fats;
    int8_t	spfat;
    uint8_t	root_dir_entries;
} disk_size_t;


static const disk_size_t disk_sizes[14] = {
    {	0,  1, 2, 1, 0,  40,  8, 2, 0xfe, 2, 2,  1, 112 },	/* 160k */
    {	0,  1, 2, 1, 0,  40,  9, 2, 0xfc, 2, 2,  1, 112 },	/* 180k */
    {	0,  2, 2, 1, 0,  40,  8, 2, 0xff, 2, 2,  1, 112 },	/* 320k */
    {	0,  2, 2, 1, 0,  40,  9, 2, 0xfd, 2, 2,  2, 112 },	/* 360k */
    {	0,  2, 2, 1, 0,  80,  8, 2, 0xfb, 2, 2,  2, 112 },	/* 640k */
    {	0,  2, 2, 1, 0,  80,  9, 2, 0xf9, 2, 2,  3, 112 },	/* 720k */
    {	1,  2, 0, 1, 1,  80, 15, 2, 0xf9, 1, 2,  7, 224 },	/* 1.2M */
    {	1,  2, 0, 1, 1,  77,  8, 3, 0xfe, 1, 2,  2, 192 },	/* 1.25M */
    {	1,  2, 0, 1, 0,  80, 18, 2, 0xf0, 1, 2,  9, 224 },	/* 1.44M */
    {	1,  2, 0, 1, 0,  80, 21, 2, 0xf0, 2, 2,  5,  16 },	/* DMF 1024 */
    {	1,  2, 0, 1, 0,  80, 21, 2, 0xf0, 4, 2,  3,  16 },	/* DMF 2048 */
    {	2,  2, 3, 1, 0,  80, 36, 2, 0xf0, 2, 2,  9, 240 },	/* 2.88M */
    {	0, 64, 0, 0, 0,  96, 32, 2,    0, 0, 0,  0,   0 },	/* ZIP 100 */
    {	0, 64, 0, 0, 0, 239, 32, 2,    0, 0, 0,  0,   0 }	/* ZIP 250 */
};


/* This function should be moved to the D86F format handler. */
int
floppy_create_86f(const wchar_t *fn, int8_t ds, int8_t rpm_mode)
{
    const disk_size_t *dp = &disk_sizes[ds];
    uint8_t *empty;
    FILE *f;

    uint32_t magic = 0x46423638;
    uint16_t version = 0x020B;
    uint16_t dflags = 0;
    uint16_t tflags = 0;
    uint32_t index_hole_pos = 0;
    uint32_t tarray[512];
    uint32_t array_size, array_size2;
    uint32_t track_base, track_size;
    int i;
    uint32_t shift = 0;

    dflags = 0;					/* Has surface data? - Assume no for now. */
    dflags |= (dp->hole << 1);			/* Hole */
    dflags |= ((dp->sides - 1) << 3);		/* Sides. */
    dflags |= (0 << 4);				/* Write protect? - Assume no for now. */
    dflags |= (rpm_mode << 5);			/* RPM mode. */
    dflags |= (0 << 7);				/* Has extra bit cells? - Assume no for now. */

    tflags = dp->data_rate;			/* Data rate. */
    tflags |= (dp->encoding << 3);		/* Encoding. */
    tflags |= (dp->rpm << 5);			/* RPM. */

    switch (dp->hole) {
	case 0:
	case 1:
	default:
		switch(rpm_mode) {
			case 1:
				array_size = 25250;
				break;
			case 2:
				array_size = 25374;
				break;
			case 3:
				array_size = 25750;
				break;
			default:
				array_size = 25000;
				break;
		}
		break;
	case 2:
		switch(rpm_mode) {
			case 1:
				array_size = 50500;
				break;
			case 2:
				array_size = 50750;
				break;
			case 3:
				array_size = 51000;
				break;
			default:
				array_size = 50000;
				break;
		}
		break;
    }

    array_size2 = (array_size << 3);
    array_size = (array_size2 >> 4) << 1;
    if (array_size2 & 15)
	array_size += 2;

    f = plat_fopen(fn, L"wb");
    if (f == NULL)
	return 0;

    empty = (uint8_t *)mem_alloc(array_size);
    memset(tarray, 0x00, 2048);
    memset(empty, 0x00, array_size);

    fwrite(&magic, 4, 1, f);
    fwrite(&version, 2, 1, f);
    fwrite(&dflags, 2, 1, f);

    track_size = array_size + 6;

    track_base = 8 + ((dp->sides == 2) ? 2048 : 1024);

    for (i = 0; i < dp->tracks * dp->sides; i++)
    if (dp->tracks <= 43)
	shift = 1;

    for (i = 0; i < (dp->tracks * dp->sides) << shift; i++)
	tarray[i] = track_base + (i * track_size);

    fwrite(tarray, 1, (dp->sides == 2) ? 2048 : 1024, f);

    for (i = 0; i < (dp->tracks * dp->sides) << shift; i++) {
	fwrite(&tflags, 2, 1, f);
	fwrite(&index_hole_pos, 4, 1, f);
	fwrite(empty, 1, array_size, f);
    }

    free(empty);

    fclose(f);

    return 1;
}


/* This function should be moved to the Floppy Image format handler. */
int
floppy_create_image(const wchar_t *fn, int8_t ds, int8_t is_fdi)
{
    const disk_size_t *dp = &disk_sizes[ds];
    uint8_t *empty;
    FILE *f;

    uint32_t total_size = 0;
    uint32_t total_sectors = 0;
    uint32_t sector_bytes = 0;
    uint32_t root_dir_bytes = 0;
    uint32_t fat_size = 0;
    uint32_t fat1_offs = 0;
    uint32_t fat2_offs = 0;
    uint32_t zero_bytes = 0;
    uint16_t base = 0x1000;
    
    f = plat_fopen(fn, L"wb");
    if (f == NULL)
	return 0;

    sector_bytes = (128 << dp->sector_len);
    total_sectors = dp->sides * dp->tracks * dp->sectors;
    if (total_sectors > ZIP_SECTORS)
	total_sectors = ZIP_250_SECTORS;
    total_size = total_sectors * sector_bytes;
    root_dir_bytes = (dp->root_dir_entries << 5);
    fat_size = (dp->spfat * sector_bytes);
    fat1_offs = sector_bytes;
    fat2_offs = fat1_offs + fat_size;
    zero_bytes = fat2_offs + fat_size + root_dir_bytes;

    if (is_fdi) {
	empty = (uint8_t *)mem_alloc(base);
	memset(empty, 0x00, base);

	*(uint32_t *) &(empty[0x08]) = (uint32_t)base;
	*(uint32_t *) &(empty[0x0C]) = total_size;
	*(uint16_t *) &(empty[0x10]) = (uint16_t)sector_bytes;
	*(uint8_t *)  &(empty[0x14]) = (uint8_t)dp->sectors;
	*(uint8_t *)  &(empty[0x18]) = (uint8_t)dp->sides;
	*(uint8_t *)  &(empty[0x1C]) = (uint8_t)dp->tracks;

	fwrite(empty, 1, base, f);

	free(empty);
    }

    empty = (uint8_t *)mem_alloc(total_size);
    memset(empty, 0x00, zero_bytes);

    memset(empty + zero_bytes, 0xF6, total_size - zero_bytes);

    empty[0x00] = 0xEB;			/* jump to make DOS happy */
    empty[0x01] = 0x58;
    empty[0x02] = 0x90;

    empty[0x03] = (uint8_t)'V';		/* OEM ID */
    empty[0x04] = (uint8_t)'A';
    empty[0x05] = (uint8_t)'R';
    empty[0x06] = (uint8_t)'C';
    empty[0x07] = (uint8_t)'E';
    empty[0x08] = (uint8_t)'M';
    empty[0x09] = (uint8_t)'5';
    empty[0x0A] = (uint8_t)'0';

    *(uint16_t *) &(empty[0x0B]) = (uint16_t) sector_bytes;
    *(uint8_t  *) &(empty[0x0D]) = (uint8_t)  dp->spc;
    *(uint16_t *) &(empty[0x0E]) = (uint16_t) 1;
    *(uint8_t  *) &(empty[0x10]) = (uint8_t)  dp->num_fats;
    *(uint16_t *) &(empty[0x11]) = (uint16_t) dp->root_dir_entries;
    *(uint16_t *) &(empty[0x13]) = (uint16_t) total_sectors;
    *(uint8_t *)  &(empty[0x15]) = (uint8_t)  dp->media_desc;
    *(uint16_t *) &(empty[0x16]) = (uint16_t) dp->spfat;
    *(uint8_t *)  &(empty[0x18]) = (uint8_t)  dp->sectors;
    *(uint8_t *)  &(empty[0x1A]) = (uint8_t)  dp->sides;

    empty[0x26] = 0x29;		/* ')' followed by random serial number */
    empty[0x27] = random_generate();
    empty[0x28] = random_generate();
    empty[0x29] = random_generate();
    empty[0x2A] = random_generate();

    memset(&(empty[0x2B]), 0x20, 11);

    empty[0x36] = (uint8_t)'F';
    empty[0x37] = (uint8_t)'A';
    empty[0x38] = (uint8_t)'T';
    empty[0x39] = (uint8_t)'1';
    empty[0x3A] = (uint8_t)'2';
    empty[0x3B] = (uint8_t)' ';
    empty[0x3C] = (uint8_t)' ';
    empty[0x3D] = (uint8_t)' ';

    empty[0x1FE] = 0x55;
    empty[0x1FF] = 0xAA;

    empty[fat1_offs + 0x00] = empty[fat2_offs + 0x00] = empty[0x15];
    empty[fat1_offs + 0x01] = empty[fat2_offs + 0x01] = 0xFF;
    empty[fat1_offs + 0x02] = empty[fat2_offs + 0x02] = 0xFF;

    fwrite(empty, 1, total_size, f);

    free(empty);

    fclose(f);

    return 1;
}


/* This function should be moved to the ZIP drive module. */
int
zip_create_image(const wchar_t *fn, int8_t ds, int8_t is_zdi)
{
    const disk_size_t *dp = &disk_sizes[ds];
    uint8_t *empty;
    FILE *f;

    uint32_t total_size = 0;
    uint32_t total_sectors = 0;
    uint32_t sector_bytes = 0;
    uint32_t root_dir_bytes = 0;
    uint32_t fat_size = 0;
    uint32_t fat1_offs = 0;
    uint32_t fat2_offs = 0;
    uint32_t zero_bytes = 0;
    uint16_t base = 0x1000;
    uint32_t pbar_max = 0;
    uint32_t i;
    
    f = plat_fopen(fn, L"wb");
    if (f == NULL)
	return 0;

    sector_bytes = (128 << dp->sector_len);
    total_sectors = dp->sides * dp->tracks * dp->sectors;
    if (total_sectors > ZIP_SECTORS)
	total_sectors = ZIP_250_SECTORS;
    total_size = total_sectors * sector_bytes;
    root_dir_bytes = (dp->root_dir_entries << 5);
    fat_size = (dp->spfat * sector_bytes);
    fat1_offs = sector_bytes;
    fat2_offs = fat1_offs + fat_size;
    zero_bytes = fat2_offs + fat_size + root_dir_bytes;

    pbar_max = total_size;
    if (is_zdi)
	pbar_max += base;
    pbar_max >>= 11;
    pbar_max--;

    //FIXME: we will implement a ProgressBar handler.  --FvK
#if 0
    h = GetDlgItem(hwnd, IDC_COMBO_RPM_MODE);
    EnableWindow(h, FALSE);
    ShowWindow(h, SW_HIDE);
    h = GetDlgItem(hwnd, IDT_1751);
    EnableWindow(h, FALSE);
    ShowWindow(h, SW_HIDE);
    h = GetDlgItem(hwnd, IDC_PBAR_IMG_CREATE);
    SendMessage(h, PBM_SETRANGE32, (WPARAM) 0, (LPARAM) pbar_max);
    SendMessage(h, PBM_SETPOS, (WPARAM) 0, (LPARAM) 0);
    EnableWindow(h, TRUE);
    ShowWindow(h, SW_SHOW);
    h = GetDlgItem(hwnd, IDT_1757);
    EnableWindow(h, TRUE);
    ShowWindow(h, SW_SHOW);

    h = GetDlgItem(hwnd, IDC_PBAR_IMG_CREATE);
#endif
    pbar_max++;

    if (is_zdi) {
	empty = (uint8_t *)mem_alloc(base);
	memset(empty, 0x00, base);

	*(uint32_t *) &(empty[0x08]) = (uint32_t) base;
	*(uint32_t *) &(empty[0x0C]) = total_size;
	*(uint16_t *) &(empty[0x10]) = (uint16_t) sector_bytes;
	*(uint8_t *)  &(empty[0x14]) = (uint8_t)  dp->sectors;
	*(uint8_t *)  &(empty[0x18]) = (uint8_t)  dp->sides;
	*(uint8_t *)  &(empty[0x1C]) = (uint8_t)  dp->tracks;

	fwrite(empty, 1, 2048, f);
#if 0
	SendMessage(h, PBM_SETPOS, (WPARAM) 1, (LPARAM) 0);
#endif

	fwrite(&empty[0x0800], 1, 2048, f);

	free(empty);

#if 0
	SendMessage(h, PBM_SETPOS, (WPARAM) 2, (LPARAM) 0);
#endif
	pbar_max -= 2;
    }

    empty = (uint8_t *)mem_alloc(total_size);
    memset(empty, 0x00, zero_bytes);

    if (total_sectors == ZIP_SECTORS) {
	/* ZIP 100 */
	/* MBR */
	*(uint64_t *) &(empty[0x0000]) = 0x0000030000025245LL;
	*(uint64_t *) &(empty[0x0008]) = 0x0000000000000000LL;
	*(uint64_t *) &(empty[0x0010]) = 0x0900E90300000100LL;
	*(uint64_t *) &(empty[0x0018]) = 0x726F70726F430100LL;
	*(uint64_t *) &(empty[0x0020]) = 0x202D206E6F697461LL;
	*(uint64_t *) &(empty[0x0028]) = 0x30392F33322F3131LL;

	*(uint64_t *) &(empty[0x01AE]) = 0x0116010100E905E2LL;
	*(uint64_t *) &(empty[0x01B6]) = 0x226BEDCE014E0135LL;
	*(uint64_t *) &(empty[0x01BE]) = 0x5E203F0600010180LL;
	*(uint64_t *) &(empty[0x01C6]) = 0x0002FE6000000020LL;

	*(uint16_t *) &(empty[0x01FE]) = 0xAA55;

	/* 4 sectors filled with 0xFA */
	memset(&(empty[0x0200]), 0xFA, 0x0800);

	/* Iomega_Reserved sector */
	*(uint64_t *) &(empty[0x0A00]) = 0x0500000000004D50LL;
	*(uint64_t *) &(empty[0x0A08]) = 0xAFF9010051060100LL;

	*(uint64_t *) &(empty[0x0A30]) = 0x525F6167656D6F49LL;
	*(uint64_t *) &(empty[0x0A38]) = 0x0064657672657365LL;

	*(uint64_t *) &(empty[0x0A54]) = 0x03000000AFF90100LL;

	/* 26 sectors filled with 0x48 */
	memset(&(empty[0x0C00]), 0x48, 0x3400);

	/* Boot sector */
	*(uint64_t *) &(empty[0x4000]) = 0x584F4236389058EBLL;
	*(uint64_t *) &(empty[0x4008]) = 0x0001040200302E35LL;
	*(uint64_t *) &(empty[0x4010]) = 0x00C0F80000020002LL;
	*(uint64_t *) &(empty[0x4018]) = 0x0000002000400020LL;
	*(uint32_t *) &(empty[0x4020]) = 0x0002FFE0;
	*(uint16_t *) &(empty[0x4024]) = 0x0080;

	empty[0x4026] = 0x29;	/* ')' followed by random serial number */
	empty[0x4027] = random_generate();
	empty[0x4028] = random_generate();
	empty[0x4029] = random_generate();
	empty[0x402A] = random_generate();

	memset(&(empty[0x402B]), 0x00, 0x000B);
	memset(&(empty[0x4036]), 0x20, 0x0008);

	empty[0x4036] = (uint8_t)'F';
	empty[0x4037] = (uint8_t)'A';
	empty[0x4038] = (uint8_t)'T';
	empty[0x4039] = (uint8_t)'1';
	empty[0x403A] = (uint8_t)'6';

	empty[0x41FE] = 0x55;
	empty[0x41FF] = 0xAA;

	empty[0x4200] = empty[0x1C200] = empty[0x4015];
	empty[0x4201] = empty[0x1C201] = 0xFF;
	empty[0x4202] = empty[0x1C202] = 0xFF;
	empty[0x4203] = empty[0x1C203] = 0xFF;

	/* Root directory = 0x34200
	   Data = 0x38200 */
    } else {
	/* ZIP 250 */
	/* MBR */
	*(uint64_t *) &(empty[0x0000]) = 0x2054524150492EEBLL;
	*(uint64_t *) &(empty[0x0008]) = 0x3930302065646F63LL;
	*(uint64_t *) &(empty[0x0010]) = 0x67656D6F49202D20LL;
	*(uint64_t *) &(empty[0x0018]) = 0x726F70726F432061LL;
	*(uint64_t *) &(empty[0x0020]) = 0x202D206E6F697461LL;
	*(uint64_t *) &(empty[0x0028]) = 0x30392F33322F3131LL;

	*(uint64_t *) &(empty[0x01AE]) = 0x0116010100E900E9LL;
	*(uint64_t *) &(empty[0x01B6]) = 0x2E32A7AC014E0135LL;

	*(uint64_t *) &(empty[0x01EE]) = 0xEE203F0600010180LL;
	*(uint64_t *) &(empty[0x01F6]) = 0x000777E000000020LL;
	*(uint16_t *) &(empty[0x01FE]) = 0xAA55;

	/* 31 sectors filled with 0x48 */
	memset(&(empty[0x0200]), 0x48, 0x3E00);

	/* The second sector begins with some strange data
	   in my reference image. */
	*(uint64_t *) &(empty[0x0200]) = 0x3831393230334409LL;
	*(uint64_t *) &(empty[0x0208]) = 0x6A57766964483130LL;
	*(uint64_t *) &(empty[0x0210]) = 0x3C3A34676063653FLL;
	*(uint64_t *) &(empty[0x0218]) = 0x586A56A8502C4161LL;
	*(uint64_t *) &(empty[0x0220]) = 0x6F2D702535673D6CLL;
	*(uint64_t *) &(empty[0x0228]) = 0x255421B8602D3456LL;
	*(uint64_t *) &(empty[0x0230]) = 0x577B22447B52603ELL;
	*(uint64_t *) &(empty[0x0238]) = 0x46412CC871396170LL;
	*(uint64_t *) &(empty[0x0240]) = 0x704F55237C5E2626LL;
	*(uint64_t *) &(empty[0x0248]) = 0x6C7932C87D5C3C20LL;
	*(uint64_t *) &(empty[0x0250]) = 0x2C50503E47543D6ELL;
	*(uint64_t *) &(empty[0x0258]) = 0x46394E807721536ALL;
	*(uint64_t *) &(empty[0x0260]) = 0x505823223F245325LL;
	*(uint64_t *) &(empty[0x0268]) = 0x365C79B0393B5B6ELL;

	/* Boot sector */
	*(uint64_t *) &(empty[0x4000]) = 0x584F4236389058EBLL;
	*(uint64_t *) &(empty[0x4008]) = 0x0001080200302E35LL;
	*(uint64_t *) &(empty[0x4010]) = 0x00EFF80000020002LL;
	*(uint64_t *) &(empty[0x4018]) = 0x0000002000400020LL;
	*(uint32_t *) &(empty[0x4020]) = 0x000777E0;
	*(uint16_t *) &(empty[0x4024]) = 0x0080;

	empty[0x4026] = 0x29;	/* ')' followed by random serial number */
	empty[0x4027] = random_generate();
	empty[0x4028] = random_generate();
	empty[0x4029] = random_generate();
	empty[0x402A] = random_generate();

	memset(&(empty[0x402B]), 0x00, 0x000B);
	memset(&(empty[0x4036]), 0x20, 0x0008);

	empty[0x4036] = (uint8_t)'F';
	empty[0x4037] = (uint8_t)'A';
	empty[0x4038] = (uint8_t)'T';
	empty[0x4039] = (uint8_t)'1';
	empty[0x403A] = (uint8_t)'6';

	empty[0x41FE] = 0x55;
	empty[0x41FF] = 0xAA;

	empty[0x4200] = empty[0x22000] = empty[0x4015];
	empty[0x4201] = empty[0x22001] = 0xFF;
	empty[0x4202] = empty[0x22002] = 0xFF;
	empty[0x4203] = empty[0x22003] = 0xFF;

	/* Root directory = 0x3FE00
	   Data = 0x38200 */
    }

    for (i = 0; i < pbar_max; i++) {
	fwrite(&empty[i << 11], 1, 2048, f);
#if 0
	SendMessage(h, PBM_SETPOS, (WPARAM) i + 2, (LPARAM) 0);
#endif
    }

    free(empty);

    fclose(f);

    return 1;
}
