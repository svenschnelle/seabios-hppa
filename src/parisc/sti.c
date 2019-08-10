// VGA / STI code for parisc architecture
//
// Copyright (C) 2017  Helge Deller <deller@gmx.de>
//
// This file may be distributed under the terms of the GNU LGPLv3 license.

#include "autoconf.h"
#include "types.h"
#include <stdint.h>
#include "std/optionrom.h"
#include "hw/pci.h" // pci_config_readl
#include "hw/pci_regs.h" // PCI_BASE_ADDRESS_0
#include "vgahw.h"
#include "parisc/sticore.h"

#define REGION_MAX 8
/****************************************************************
 * PCI Data
 ****************************************************************/
#if 0
struct pci_data __VISIBLE rom_pci_data = {
    .signature = PCI_ROM_SIGNATURE,
    .vendor = CONFIG_VGA_VID,
    .device = CONFIG_VGA_DID,
    .dlen = 0x18,
    .class_hi = 0x300,
    .irevision = 1,
    .type = PCIROM_CODETYPE_X86,
    .indicator = 0x80,
};
#endif

int32_t __attribute__((section(".text.stifunc.init_graph")))
sti_init_graph(struct sti_init_flags *flags,
			  struct sti_init_inptr *in,
			  struct sti_init_outptr *out,
			  struct sti_glob_cfg *cfg)
{
	(void)in;
	(void)flags;
	out->errno = 0;
	cfg->text_planes = 1;
	cfg->onscreen_x = 1280;
	cfg->onscreen_y = 1024;
	cfg->offscreen_x = 0;
	cfg->offscreen_y = 0;
	cfg->total_x = 1280;
	cfg->total_y = 1024;
	return 0;
}

int32_t __attribute__((section(".text.stifunc.inq_conf")))
sti_inq_conf(struct sti_conf_flags *flags,
			struct sti_conf_inptr *in,
			struct sti_conf_outptr *out,
			struct sti_glob_cfg *cfg)
{

	(void)in;
	(void)flags;
	out->errno = 0;
	out->onscreen_x = cfg->onscreen_x;
	out->onscreen_y = cfg->onscreen_y;
	out->total_x = cfg->total_x;
	out->total_y = cfg->total_y;
	out->bits_per_pixel = 8;
	out->planes = 1;
	out->dev_name[0] = 'Q';
	out->dev_name[1] = 'E';
	out->dev_name[2] = 'M';
	out->dev_name[3] = 'U';
	out->dev_name[4] = ' ';
	out->dev_name[5] = 'V';
	out->dev_name[6] = 'G';
	out->dev_name[7] = 'A';
	out->dev_name[8] = '\0';
	return 0;
}

int32_t __attribute__((section(".text.stifunc.bmove")))
sti_bmove(struct sti_blkmv_flags *flags,
			 struct sti_blkmv_inptr *in,
			 struct sti_blkmv_outptr *out,
			 struct sti_glob_cfg *cfg)
{
	uint8_t *fb = (unsigned char *)cfg->region_ptrs[1];
	int line, column, columnincr, lineincr, endline, startcolumn, endcolumn;
	uint8_t *srcline, *dstline;
#if 0
	if (in->width == cfg->total_x &&
		in->dest_y < in->src_y &&
		in->src_x == 0 && in->dest_x == 0) {
		/* common case: scrolling the whole screen */
		return sti_bmove_fast(cfg, in->src_y, in->dest_y, flags->clear);
	}
#endif
	if (in->dest_y > in->src_y) {
		/* move down */
		line = in->height;
		endline = -1;
		lineincr = -1;
	} else {
		/* move up */
		line = 0;
		endline = in->height;
		lineincr = 1;
	}

	if (in->dest_x > in->src_x) {
		/* move right */
		startcolumn = in->width-1;
		endcolumn = -1;
		columnincr = -1;
	} else {
		/* move left */
		startcolumn = 0;
		endcolumn = in->width;
		columnincr = 1;
	}

	srcline = &fb[(in->src_y + line) * cfg->total_x];
	dstline = &fb[(in->dest_y + line) * cfg->total_x];

	for(; line != endline; line += lineincr) {
		if (flags->clear) {
			for(column = startcolumn; column != endcolumn; column += columnincr)
				dstline[in->dest_x + column] = 0;
		} else {
			for(column = startcolumn; column != endcolumn; column += columnincr)
				dstline[in->dest_x + column] = srcline[in->src_x + column];
		}
		srcline += cfg->total_x;
		dstline += cfg->total_x;
	}
	out->errno = 0;
	return 0;
}

static const int color_map[8] __attribute__((section(".text.stifunc.color_map"))) = {
	0, 7, 4, 6, 2, 3, 1, 5
};

int __attribute__((section(".text.stifunc.unpmv")))
sti_font_unpmv(struct sti_font_flags *flags,
			  struct sti_font_inptr *in,
			  struct sti_font_outptr *out,
			  struct sti_glob_cfg *cfg) {
	int x, y, fgcolor, bgcolor;
	unsigned char *fb = (unsigned char *)cfg->region_ptrs[1];
	unsigned char *src = (unsigned char *)in->font_start_addr + 16 + (in->index * 16);
	(void)out;
	(void)flags;

	if (!in->font_start_addr)
		return 0;

	fgcolor = color_map[in->fg_color & 7];
	bgcolor = color_map[in->bg_color & 7];

	for(x = 0; x < 8; x++) {
		for(y = 0; y <  16; y++) {
			if (src[y] & (1 << (7-x)))
				fb[in->dest_x + x + (cfg->total_x * (in->dest_y + y))] = fgcolor;
			else
				fb[in->dest_x + x + (cfg->total_x * (in->dest_y + y))] = bgcolor;
		}
	}
	return 0;
}


struct font  {
	struct sti_rom_font hdr;
	char font[16*256];
} __attribute__((packed));

const struct font _font __attribute__((section(".text.stifunc.font"))) = {
	.hdr = {
		.first_char = 0,
		.last_char = 0,
		.width = 8,
		.height = 16,
		.font_type = 1,
		.bytes_per_char = 16,
		.underline_height = 1,
		.underline_pos = 15,
	},
	.font = {
		0x00, 0x48, 0x68, 0x58, 0x48, 0x48, 0x00, 0x12, 0x12, 0x12, 0x12, 0x0c, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x38, 0x40, 0x30, 0x08, 0x70, 0x00, 0x12, 0x12, 0x1e, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x38, 0x40, 0x30, 0x08, 0x70, 0x00, 0x22, 0x14, 0x08, 0x14, 0x22, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x78, 0x40, 0x70, 0x40, 0x78, 0x00, 0x22, 0x14, 0x08, 0x14, 0x22, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x78, 0x40, 0x70, 0x40, 0x78, 0x00, 0x3e, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x78, 0x40, 0x70, 0x40, 0x78, 0x00, 0x1c, 0x22, 0x22, 0x2a, 0x1c, 0x04, 0x00, 0x00, 0x00,
		0x00, 0x30, 0x48, 0x78, 0x48, 0x48, 0x00, 0x12, 0x14, 0x18, 0x14, 0x12, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x38, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0xfe, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x70, 0x48, 0x70, 0x48, 0x70, 0x00, 0x0e, 0x10, 0x0c, 0x02, 0x1c, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x48, 0x48, 0x78, 0x48, 0x48, 0x00, 0x3e, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x40, 0x40, 0x40, 0x40, 0x78, 0x00, 0x1e, 0x10, 0x1c, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x44, 0x44, 0x28, 0x28, 0x10, 0x00, 0x3e, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x78, 0x40, 0x70, 0x40, 0x40, 0x00, 0x1e, 0x10, 0x1c, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x38, 0x40, 0x40, 0x40, 0x38, 0x00, 0x1c, 0x12, 0x1c, 0x14, 0x12, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x38, 0x40, 0x30, 0x08, 0x70, 0x00, 0x0c, 0x12, 0x12, 0x12, 0x0c, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x38, 0x40, 0x30, 0x08, 0x70, 0x00, 0x0e, 0x04, 0x04, 0x04, 0x0e, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x70, 0x48, 0x48, 0x48, 0x70, 0x00, 0x10, 0x10, 0x10, 0x10, 0x1e, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x70, 0x48, 0x48, 0x48, 0x70, 0x00, 0x04, 0x0c, 0x04, 0x04, 0x0e, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x70, 0x48, 0x48, 0x48, 0x70, 0x0c, 0x12, 0x02, 0x0c, 0x10, 0x1e, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x70, 0x48, 0x48, 0x48, 0x70, 0x00, 0x1c, 0x02, 0x0c, 0x02, 0x1c, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x70, 0x48, 0x48, 0x48, 0x70, 0x00, 0x14, 0x14, 0x1e, 0x04, 0x04, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x48, 0x68, 0x58, 0x48, 0x48, 0x00, 0x12, 0x14, 0x18, 0x14, 0x12, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x38, 0x40, 0x30, 0x08, 0x70, 0x00, 0x22, 0x14, 0x08, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x78, 0x40, 0x70, 0x40, 0x78, 0x00, 0x1c, 0x12, 0x1c, 0x12, 0x1c, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x38, 0x40, 0x40, 0x40, 0x38, 0x00, 0x12, 0x1a, 0x16, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x78, 0x40, 0x70, 0x40, 0x78, 0x00, 0x22, 0x36, 0x2a, 0x22, 0x22, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x38, 0x40, 0x30, 0x08, 0x70, 0x00, 0x1c, 0x12, 0x1c, 0x12, 0x1c, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x78, 0x40, 0x70, 0x40, 0x78, 0x00, 0x0e, 0x10, 0x10, 0x10, 0x0e, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x78, 0x40, 0x70, 0x40, 0x40, 0x00, 0x0e, 0x10, 0x0c, 0x02, 0x1c, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x38, 0x40, 0x58, 0x48, 0x38, 0x00, 0x0e, 0x10, 0x0c, 0x02, 0x1c, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x70, 0x48, 0x70, 0x50, 0x48, 0x00, 0x0e, 0x10, 0x0c, 0x02, 0x1c, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x48, 0x48, 0x48, 0x48, 0x30, 0x00, 0x0e, 0x10, 0x0c, 0x02, 0x1c, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x28, 0x28, 0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x24, 0x24, 0x7e, 0x24, 0x24, 0x24, 0x24, 0x7e, 0x24, 0x24, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x10, 0x38, 0x54, 0x50, 0x30, 0x18, 0x14, 0x54, 0x38, 0x10, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x24, 0x54, 0x28, 0x08, 0x10, 0x10, 0x20, 0x24, 0x4a, 0x44, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x30, 0x48, 0x48, 0x50, 0x20, 0x50, 0x8a, 0x84, 0x8c, 0x72, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x08, 0x10, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x08, 0x10, 0x10, 0x20, 0x20, 0x20, 0x20, 0x10, 0x10, 0x08, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x20, 0x10, 0x10, 0x08, 0x08, 0x08, 0x08, 0x10, 0x10, 0x20, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x10, 0x54, 0x38, 0x7c, 0x38, 0x54, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x10, 0x10, 0x10, 0xfe, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x30, 0x10, 0x20, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x30, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x04, 0x04, 0x08, 0x08, 0x10, 0x10, 0x20, 0x20, 0x40, 0x40, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x18, 0x24, 0x42, 0x42, 0x5a, 0x5a, 0x42, 0x42, 0x24, 0x18, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x08, 0x18, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x1c, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x3c, 0x42, 0x02, 0x02, 0x0c, 0x10, 0x20, 0x40, 0x40, 0x7e, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x3c, 0x42, 0x02, 0x02, 0x1c, 0x02, 0x02, 0x02, 0x42, 0x3c, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x04, 0x0c, 0x14, 0x24, 0x44, 0x7e, 0x04, 0x04, 0x04, 0x04, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x7e, 0x40, 0x40, 0x40, 0x7c, 0x02, 0x02, 0x02, 0x42, 0x3c, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x3c, 0x42, 0x40, 0x40, 0x7c, 0x42, 0x42, 0x42, 0x42, 0x3c, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x7e, 0x02, 0x02, 0x04, 0x08, 0x08, 0x10, 0x10, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x3c, 0x42, 0x42, 0x42, 0x3c, 0x42, 0x42, 0x42, 0x42, 0x3c, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x3c, 0x42, 0x42, 0x42, 0x3e, 0x02, 0x02, 0x02, 0x42, 0x3c, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x30, 0x00, 0x00, 0x00, 0x30, 0x30, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x30, 0x00, 0x00, 0x00, 0x30, 0x30, 0x10, 0x20, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x08, 0x10, 0x20, 0x40, 0x20, 0x10, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x7e, 0x00, 0x00, 0x7e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x20, 0x10, 0x08, 0x04, 0x08, 0x10, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x3c, 0x42, 0x02, 0x02, 0x04, 0x08, 0x10, 0x10, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x3c, 0x42, 0x4e, 0x52, 0x52, 0x52, 0x4c, 0x40, 0x42, 0x3c, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x3c, 0x42, 0x42, 0x42, 0x7e, 0x42, 0x42, 0x42, 0x42, 0x42, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x7c, 0x42, 0x42, 0x42, 0x7c, 0x42, 0x42, 0x42, 0x42, 0x7c, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x3c, 0x42, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x42, 0x3c, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x7c, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x7c, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x7e, 0x40, 0x40, 0x40, 0x7c, 0x40, 0x40, 0x40, 0x40, 0x7e, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x7e, 0x40, 0x40, 0x40, 0x78, 0x40, 0x40, 0x40, 0x40, 0x40, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x3c, 0x42, 0x40, 0x40, 0x40, 0x4e, 0x42, 0x42, 0x42, 0x3c, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x42, 0x42, 0x42, 0x42, 0x7e, 0x42, 0x42, 0x42, 0x42, 0x42, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x38, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x38, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x0e, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x44, 0x38, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x42, 0x42, 0x44, 0x48, 0x50, 0x70, 0x48, 0x44, 0x42, 0x42, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x7e, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x42, 0x42, 0x66, 0x5a, 0x5a, 0x42, 0x42, 0x42, 0x42, 0x42, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x42, 0x62, 0x62, 0x52, 0x52, 0x4a, 0x4a, 0x46, 0x46, 0x42, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x3c, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x3c, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x7c, 0x42, 0x42, 0x42, 0x7c, 0x40, 0x40, 0x40, 0x40, 0x40, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x3c, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x4a, 0x3c, 0x04, 0x02, 0x00, 0x00,
		0x00, 0x00, 0x7c, 0x42, 0x42, 0x42, 0x7c, 0x50, 0x48, 0x44, 0x42, 0x42, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x3c, 0x42, 0x40, 0x40, 0x3c, 0x02, 0x02, 0x02, 0x42, 0x3c, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0xfe, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x3c, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x82, 0x82, 0x44, 0x44, 0x44, 0x28, 0x28, 0x28, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x42, 0x42, 0x42, 0x42, 0x42, 0x5a, 0x5a, 0x66, 0x42, 0x42, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x42, 0x42, 0x42, 0x24, 0x18, 0x18, 0x24, 0x42, 0x42, 0x42, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x82, 0x82, 0x44, 0x44, 0x28, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x7e, 0x02, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x40, 0x7e, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x3c, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x3c, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x40, 0x40, 0x20, 0x20, 0x10, 0x10, 0x08, 0x08, 0x04, 0x04, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x3c, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x3c, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x10, 0x28, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00,
		0x00, 0x20, 0x10, 0x08, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x02, 0x3e, 0x42, 0x42, 0x3e, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x40, 0x40, 0x40, 0x40, 0x7c, 0x42, 0x42, 0x42, 0x42, 0x7c, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x42, 0x40, 0x40, 0x42, 0x3c, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x02, 0x02, 0x02, 0x02, 0x3e, 0x42, 0x42, 0x42, 0x42, 0x3e, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x42, 0x7e, 0x40, 0x42, 0x3c, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x1c, 0x22, 0x20, 0x20, 0x20, 0x78, 0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3e, 0x42, 0x42, 0x42, 0x42, 0x3e, 0x02, 0x02, 0x3c, 0x00,
		0x00, 0x00, 0x40, 0x40, 0x40, 0x40, 0x7c, 0x42, 0x42, 0x42, 0x42, 0x42, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x30, 0x10, 0x10, 0x10, 0x10, 0x38, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x0c, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x18, 0x00,
		0x00, 0x00, 0x40, 0x40, 0x40, 0x40, 0x44, 0x48, 0x50, 0x68, 0x44, 0x42, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x18, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x1c, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xec, 0x92, 0x92, 0x92, 0x92, 0x92, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5c, 0x62, 0x42, 0x42, 0x42, 0x42, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x42, 0x42, 0x42, 0x42, 0x3c, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5c, 0x62, 0x42, 0x42, 0x42, 0x7c, 0x40, 0x40, 0x40, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3a, 0x46, 0x42, 0x42, 0x42, 0x3e, 0x02, 0x02, 0x02, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x6c, 0x32, 0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x40, 0x3c, 0x02, 0x42, 0x3c, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x7c, 0x10, 0x10, 0x10, 0x10, 0x0c, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x42, 0x42, 0x42, 0x42, 0x42, 0x3e, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x82, 0x44, 0x44, 0x28, 0x28, 0x10, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x92, 0x92, 0x92, 0x92, 0x92, 0x6c, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x42, 0x24, 0x18, 0x18, 0x24, 0x42, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x42, 0x42, 0x42, 0x42, 0x42, 0x3e, 0x02, 0x02, 0x3c, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7e, 0x04, 0x08, 0x10, 0x20, 0x7e, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x0c, 0x10, 0x10, 0x10, 0x10, 0x20, 0x10, 0x10, 0x10, 0x0c, 0x00, 0x00, 0x00, 0x00,
		0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
		0x00, 0x00, 0x30, 0x08, 0x08, 0x08, 0x08, 0x04, 0x08, 0x08, 0x08, 0x30, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x32, 0x4c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x2a, 0x54, 0x2a, 0x54, 0x2a, 0x54, 0x2a, 0x54, 0x2a, 0x54, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x38, 0x40, 0x40, 0x40, 0x38, 0x00, 0x10, 0x10, 0x10, 0x10, 0x1e, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x70, 0x20, 0x20, 0x20, 0x70, 0x00, 0x22, 0x22, 0x14, 0x14, 0x08, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x70, 0x48, 0x70, 0x48, 0x70, 0x00, 0x0c, 0x10, 0x16, 0x12, 0x0c, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x70, 0x20, 0x20, 0x20, 0x70, 0x00, 0x1c, 0x12, 0x1c, 0x12, 0x1c, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x48, 0x48, 0x48, 0x48, 0x30, 0x00, 0x10, 0x10, 0x10, 0x10, 0x1e, 0x00, 0xff, 0x00, 0x00,
		0x00, 0x70, 0x20, 0x20, 0x20, 0x70, 0x00, 0x22, 0x22, 0x14, 0x14, 0x08, 0x00, 0xff, 0x00, 0x00,
		0x00, 0x70, 0x48, 0x70, 0x48, 0x70, 0x00, 0x0c, 0x10, 0x16, 0x12, 0x0c, 0x00, 0xff, 0x00, 0x00,
		0x00, 0x70, 0x20, 0x20, 0x20, 0x70, 0x00, 0x1c, 0x12, 0x1c, 0x12, 0x1c, 0x00, 0xff, 0x00, 0x00,
		0x00, 0xa8, 0xa8, 0xa8, 0xa8, 0x50, 0x00, 0x12, 0x12, 0x1e, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x70, 0x48, 0x70, 0x50, 0x48, 0x00, 0x1c, 0x12, 0x12, 0x12, 0x1c, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x48, 0x48, 0x30, 0x10, 0x10, 0x00, 0x1e, 0x10, 0x1c, 0x10, 0x1e, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x30, 0x40, 0x58, 0x48, 0x30, 0x00, 0x1c, 0x12, 0x1c, 0x14, 0x12, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x38, 0x40, 0x40, 0x40, 0x38, 0x00, 0x12, 0x12, 0x0c, 0x04, 0x04, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x70, 0x48, 0x70, 0x48, 0x70, 0x00, 0x12, 0x12, 0x12, 0x12, 0x0c, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x88, 0xd8, 0xa8, 0x88, 0x88, 0x00, 0x0c, 0x10, 0x16, 0x12, 0x0c, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x70, 0x48, 0x70, 0x48, 0x70, 0x00, 0x12, 0x14, 0x18, 0x14, 0x12, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x30, 0x48, 0x38, 0x08, 0x30, 0x00, 0x0c, 0x12, 0x12, 0x12, 0x0c, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x30, 0x48, 0x38, 0x08, 0x30, 0x00, 0x04, 0x0c, 0x04, 0x04, 0x0e, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x30, 0x48, 0x38, 0x08, 0x30, 0x00, 0x0c, 0x12, 0x04, 0x08, 0x1e, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x30, 0x48, 0x38, 0x08, 0x30, 0x00, 0x1c, 0x02, 0x1c, 0x02, 0x1c, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x30, 0x48, 0x38, 0x08, 0x30, 0x00, 0x14, 0x14, 0x1e, 0x04, 0x04, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x30, 0x48, 0x38, 0x08, 0x30, 0x00, 0x1e, 0x10, 0x1c, 0x02, 0x1c, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x30, 0x48, 0x38, 0x08, 0x30, 0x00, 0x0c, 0x10, 0x1c, 0x12, 0x0c, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x30, 0x48, 0x38, 0x08, 0x30, 0x00, 0x1e, 0x02, 0x04, 0x08, 0x10, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x30, 0x48, 0x38, 0x08, 0x30, 0x00, 0x0c, 0x12, 0x0c, 0x12, 0x0c, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x30, 0x48, 0x38, 0x08, 0x30, 0x00, 0x0c, 0x12, 0x0e, 0x02, 0x0c, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x30, 0x48, 0x78, 0x08, 0x30, 0x00, 0x0c, 0x12, 0x1e, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x30, 0x48, 0x38, 0x08, 0x30, 0x00, 0x1c, 0x12, 0x1c, 0x12, 0x1c, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x30, 0x48, 0x38, 0x08, 0x30, 0x00, 0x0e, 0x10, 0x10, 0x10, 0x0e, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x30, 0x48, 0x38, 0x08, 0x30, 0x00, 0x1c, 0x12, 0x12, 0x12, 0x1c, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x30, 0x48, 0x38, 0x08, 0x30, 0x00, 0x1e, 0x10, 0x1c, 0x10, 0x1e, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x30, 0x48, 0x38, 0x08, 0x30, 0x00, 0x1e, 0x10, 0x1c, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x10, 0x08, 0x00, 0x3c, 0x42, 0x42, 0x42, 0x7e, 0x42, 0x42, 0x42, 0x42, 0x00, 0x00, 0x00, 0x00,
		0x08, 0x14, 0x00, 0x3c, 0x42, 0x42, 0x42, 0x7e, 0x42, 0x42, 0x42, 0x42, 0x00, 0x00, 0x00, 0x00,
		0x10, 0x08, 0x00, 0x7e, 0x40, 0x40, 0x40, 0x78, 0x40, 0x40, 0x40, 0x7e, 0x00, 0x00, 0x00, 0x00,
		0x10, 0x28, 0x00, 0x7e, 0x40, 0x40, 0x40, 0x78, 0x40, 0x40, 0x40, 0x7e, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x24, 0x00, 0x7e, 0x40, 0x40, 0x40, 0x78, 0x40, 0x40, 0x40, 0x7e, 0x00, 0x00, 0x00, 0x00,
		0x10, 0x28, 0x00, 0x38, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x38, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x28, 0x00, 0x38, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x38, 0x00, 0x00, 0x00, 0x00,
		0x10, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x20, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x10, 0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x32, 0x4c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x10, 0x08, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x3c, 0x00, 0x00, 0x00, 0x00,
		0x10, 0x28, 0x00, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x3c, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x0c, 0x12, 0x10, 0x3c, 0x10, 0x3c, 0x10, 0x70, 0x91, 0x6e, 0x00, 0x00, 0x00, 0x00,
		0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x70, 0x48, 0x70, 0x48, 0x70, 0x00, 0x04, 0x0c, 0x04, 0x04, 0x0e, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x02, 0x06, 0x0e, 0x1e, 0x3e, 0x1e, 0x0e, 0x06, 0x02, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x18, 0x24, 0x24, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x3c, 0x42, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x42, 0x3c, 0x10, 0x08, 0x10, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x42, 0x40, 0x40, 0x42, 0x3c, 0x10, 0x08, 0x10, 0x00,
		0x32, 0x4c, 0x00, 0x42, 0x62, 0x52, 0x52, 0x4a, 0x4a, 0x46, 0x42, 0x42, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x32, 0x4c, 0x00, 0x5c, 0x62, 0x42, 0x42, 0x42, 0x42, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x10, 0x00, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x10, 0x00, 0x10, 0x10, 0x10, 0x20, 0x40, 0x40, 0x42, 0x3c, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x41, 0x22, 0x1c, 0x22, 0x22, 0x22, 0x22, 0x1c, 0x22, 0x41, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x0c, 0x12, 0x10, 0x10, 0x3c, 0x10, 0x10, 0x70, 0x91, 0x6e, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x82, 0x44, 0x28, 0x10, 0x7c, 0x10, 0x7c, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x1c, 0x20, 0x20, 0x10, 0x18, 0x24, 0x24, 0x18, 0x08, 0x04, 0x38, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x07, 0x08, 0x10, 0x10, 0x7c, 0x10, 0x10, 0x10, 0x20, 0xc0, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x08, 0x3e, 0x49, 0x48, 0x48, 0x49, 0x3e, 0x08, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x08, 0x14, 0x00, 0x3c, 0x02, 0x3e, 0x42, 0x42, 0x3e, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x08, 0x14, 0x00, 0x3c, 0x42, 0x7e, 0x40, 0x40, 0x3e, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x10, 0x28, 0x00, 0x3c, 0x42, 0x42, 0x42, 0x42, 0x3c, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x10, 0x28, 0x00, 0x42, 0x42, 0x42, 0x42, 0x42, 0x3e, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x08, 0x10, 0x00, 0x3c, 0x02, 0x3e, 0x42, 0x42, 0x3e, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x08, 0x10, 0x00, 0x3c, 0x42, 0x7e, 0x40, 0x40, 0x3e, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x08, 0x10, 0x00, 0x3c, 0x42, 0x42, 0x42, 0x42, 0x3c, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x08, 0x10, 0x00, 0x42, 0x42, 0x42, 0x42, 0x42, 0x3e, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x10, 0x08, 0x00, 0x3c, 0x02, 0x3e, 0x42, 0x42, 0x3e, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x10, 0x08, 0x00, 0x3c, 0x42, 0x7e, 0x40, 0x40, 0x3e, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x10, 0x08, 0x00, 0x3c, 0x42, 0x42, 0x42, 0x42, 0x3c, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x10, 0x08, 0x00, 0x42, 0x42, 0x42, 0x42, 0x42, 0x3e, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x24, 0x00, 0x3c, 0x02, 0x3e, 0x42, 0x42, 0x3e, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x24, 0x00, 0x3c, 0x42, 0x7e, 0x40, 0x40, 0x3e, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x24, 0x00, 0x3c, 0x42, 0x42, 0x42, 0x42, 0x3c, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x24, 0x00, 0x42, 0x42, 0x42, 0x42, 0x42, 0x3e, 0x00, 0x00, 0x00, 0x00,
		0x18, 0x24, 0x18, 0x3c, 0x42, 0x42, 0x42, 0x7e, 0x42, 0x42, 0x42, 0x42, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x10, 0x28, 0x00, 0x30, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x3d, 0x42, 0x46, 0x4a, 0x4a, 0x52, 0x52, 0x62, 0x42, 0xbc, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x3f, 0x48, 0x48, 0x48, 0x7e, 0x48, 0x48, 0x48, 0x48, 0x4f, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x08, 0x14, 0x08, 0x00, 0x3c, 0x02, 0x3e, 0x42, 0x42, 0x3e, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x08, 0x10, 0x00, 0x30, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3d, 0x42, 0x4e, 0x72, 0x42, 0xbc, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3e, 0x09, 0x3f, 0x48, 0x49, 0x3e, 0x00, 0x00, 0x00, 0x00,
		0x24, 0x00, 0x3c, 0x42, 0x42, 0x42, 0x7e, 0x42, 0x42, 0x42, 0x42, 0x42, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x20, 0x10, 0x00, 0x30, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00,
		0x24, 0x00, 0x3c, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x3c, 0x00, 0x00, 0x00, 0x00,
		0x24, 0x00, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x3c, 0x00, 0x00, 0x00, 0x00,
		0x08, 0x10, 0x00, 0x7e, 0x40, 0x40, 0x40, 0x78, 0x40, 0x40, 0x40, 0x7e, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x28, 0x00, 0x30, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x18, 0x24, 0x44, 0x48, 0x70, 0x48, 0x44, 0x44, 0x64, 0x58, 0x00, 0x00, 0x00, 0x00,
		0x10, 0x28, 0x00, 0x3c, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x3c, 0x00, 0x00, 0x00, 0x00,
		0x08, 0x10, 0x00, 0x3c, 0x42, 0x42, 0x42, 0x7e, 0x42, 0x42, 0x42, 0x42, 0x00, 0x00, 0x00, 0x00,
		0x32, 0x4c, 0x00, 0x3c, 0x42, 0x42, 0x42, 0x7e, 0x42, 0x42, 0x42, 0x42, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x32, 0x4c, 0x00, 0x3c, 0x02, 0x3e, 0x42, 0x42, 0x3e, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x7c, 0x42, 0x42, 0x42, 0xe2, 0x42, 0x42, 0x42, 0x42, 0x7c, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x02, 0x07, 0x02, 0x3e, 0x42, 0x42, 0x42, 0x42, 0x3c, 0x00, 0x00, 0x00, 0x00,
		0x08, 0x10, 0x00, 0x38, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x38, 0x00, 0x00, 0x00, 0x00,
		0x20, 0x10, 0x00, 0x38, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x38, 0x00, 0x00, 0x00, 0x00,
		0x08, 0x10, 0x00, 0x3c, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x3c, 0x00, 0x00, 0x00, 0x00,
		0x10, 0x08, 0x00, 0x3c, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x3c, 0x00, 0x00, 0x00, 0x00,
		0x32, 0x4c, 0x00, 0x3c, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x3c, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x32, 0x4c, 0x00, 0x3c, 0x42, 0x42, 0x42, 0x42, 0x3c, 0x00, 0x00, 0x00, 0x00,
		0x28, 0x10, 0x00, 0x3c, 0x42, 0x40, 0x40, 0x3c, 0x02, 0x02, 0x42, 0x3c, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x28, 0x10, 0x00, 0x3c, 0x40, 0x3c, 0x02, 0x42, 0x3c, 0x00, 0x00, 0x00, 0x00,
		0x08, 0x10, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x3c, 0x00, 0x00, 0x00, 0x00,
		0x24, 0x00, 0x41, 0x41, 0x22, 0x14, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x24, 0x00, 0x42, 0x42, 0x42, 0x42, 0x42, 0x3e, 0x02, 0x02, 0x3c, 0x00,
		0x00, 0x00, 0x38, 0x10, 0x10, 0x1c, 0x12, 0x12, 0x1c, 0x10, 0x10, 0x38, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x10, 0x10, 0x1c, 0x12, 0x12, 0x1c, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x78, 0x40, 0x70, 0x40, 0x40, 0x00, 0x0c, 0x12, 0x04, 0x08, 0x1e, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x78, 0x40, 0x70, 0x40, 0x40, 0x00, 0x1c, 0x02, 0x0c, 0x02, 0x1c, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x78, 0x40, 0x70, 0x40, 0x40, 0x00, 0x14, 0x14, 0x1e, 0x04, 0x04, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x70, 0x20, 0x20, 0x20, 0x70, 0x00, 0x0c, 0x12, 0x12, 0x12, 0x0c, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x20, 0x60, 0x22, 0x24, 0x28, 0x14, 0x2c, 0x54, 0x1e, 0x04, 0x0e, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x20, 0x60, 0x22, 0x24, 0x28, 0x10, 0x2c, 0x52, 0x04, 0x08, 0x1e, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x3c, 0x02, 0x3e, 0x42, 0x3e, 0x00, 0x7e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x3c, 0x42, 0x42, 0x42, 0x3c, 0x00, 0x7e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x09, 0x12, 0x24, 0x48, 0x24, 0x12, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x48, 0x24, 0x12, 0x09, 0x12, 0x24, 0x48, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x10, 0x10, 0x7c, 0x10, 0x10, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x7e, 0x5a, 0x5a, 0x56, 0x4e, 0x56, 0x5a, 0x5a, 0x7e, 0x00, 0x00, 0x00, 0x00, 0x00,
	}
};


volatile const uint32_t __attribute__((section(".text.sti.regionlist"))) region_list[REGION_MAX]  = { 0x8100, 0xd000, 0, 0, 0, 0, 0, 0 };

struct sti_rom _sti_rom __attribute__((section(".text.sti.header"))) = {
	.type = { 0x03, 0x03, 0x03, 0x03 },
	.num_mons = 1,
	.revno = { 0x0b, 0x0f },
	.graphics_id = { 0x1234567, 0x98765432 },
	//»     .statesize =  &state_size,
	.last_addr = 0xfffff,
	//»     .mon_tbl_addr = &montable,
};

const int sti_rom_end __attribute__((section(".text.sti.end"))) = 0xaa55;
extern void handle_100e(struct bregs *regs);

void parisc_teletype_output(struct bregs *regs)
{
	// re-read PCI addresses. Linux kernel reconfigures those at boot.
	parisc_vga_mem = pci_config_readl(VgaBDF, PCI_BASE_ADDRESS_0);
	parisc_vga_mem &= PCI_BASE_ADDRESS_MEM_MASK;
	VBE_framebuffer = parisc_vga_mem;
	parisc_vga_mmio = pci_config_readl(VgaBDF, PCI_BASE_ADDRESS_2);
	parisc_vga_mmio &= PCI_BASE_ADDRESS_MEM_MASK;

	handle_100e(regs);
}

void init_sti(void)
{
    _sti_rom.init_graph = (uint32_t)sti_init_graph - (uint32_t)&_sti_rom;
    _sti_rom.font_unpmv = (uint32_t)sti_font_unpmv - (uint32_t)&_sti_rom;
    _sti_rom.block_move = (uint32_t)sti_bmove - (uint32_t)&_sti_rom;
    _sti_rom.inq_conf = (uint32_t)sti_inq_conf - (uint32_t)&_sti_rom;
    _sti_rom.region_list = (uint32_t)region_list - (uint32_t)&_sti_rom;
    _sti_rom.font_start = (uint32_t)&_font - (uint32_t)&_sti_rom;
    _sti_rom.last_addr = (uint32_t)&sti_rom_end + 2 - (uint32_t)&_sti_rom;
}
