/*
 * Copyright (c) 2015 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions: *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

struct drm_i915_mocs_entry {
	uint32_t control_value;
	uint16_t l3cc_value;
};

#define LE_CACHEABILITY(value)	((value) << 0)
#define LE_TGT_CACHE(value)	((value) << 2)
#define LE_LRUM(value)		((value) << 4)
#define LE_AOM(value)		((value) << 6)
#define LE_RSC(value)		((value) << 7)
#define LE_SCC(value)		((value) << 8)
#define LE_PFM(value)		((value) << 11)
#define LE_SCF(value)		((value) << 14)
#define L3_ESC(value)		((value) << 0)
#define L3_SCC(value)		((value) << 1)
#define L3_CACHEABILITY(value)	((value) << 4)
#define LE_PAGETABLE		0
#define LE_UC			1
#define LE_WT			2
#define LE_WB			3
#define L3_DIRECT		0
#define L3_UC			1
#define L3_RESERVED		2
#define L3_WB			3
#define MESA_ELLC		0
#define MESA_LLC		1
#define LLC_ELLC		2

#define MOCS_DEF_skylake_mocs_table static const struct drm_i915_mocs_entry MESA_MOCS[] = {\
	/* { 0x00000009, 0x0010 } */\
	{ (LE_CACHEABILITY(LE_UC) | LE_TGT_CACHE(LLC_ELLC) | LE_LRUM(0) |\
	   LE_AOM(0) | LE_RSC(0) | LE_SCC(0) | LE_PFM(0) | LE_SCF(0)),\
	  (L3_ESC(0) | L3_SCC(0) | L3_CACHEABILITY(L3_UC)) },\
	/* { 0x00000038, 0x0030 } */\
	{ (LE_CACHEABILITY(LE_PAGETABLE) | LE_TGT_CACHE(LLC_ELLC) | LE_LRUM(3) |\
	   LE_AOM(0) | LE_RSC(0) | LE_SCC(0) | LE_PFM(0) | LE_SCF(0)),\
	  (L3_ESC(0) | L3_SCC(0) | L3_CACHEABILITY(L3_WB)) },\
	/* { 0x0000003b, 0x0030 } */\
	{ (LE_CACHEABILITY(LE_WB) | LE_TGT_CACHE(LLC_ELLC) | LE_LRUM(3) |\
	   LE_AOM(0) | LE_RSC(0) | LE_SCC(0) | LE_PFM(0) | LE_SCF(0)),\
	  (L3_ESC(0) | L3_SCC(0) | L3_CACHEABILITY(L3_WB)) }\
}

#define MOCS_DEF_broxton_mocs_table static const struct drm_i915_mocs_entry MESA_MOCS[] = {\
	/* { 0x00000009, 0x0010 } */\
	{ (LE_CACHEABILITY(LE_UC) | LE_TGT_CACHE(LLC_ELLC) | LE_LRUM(0) |\
	   LE_AOM(0) | LE_RSC(0) | LE_SCC(0) | LE_PFM(0) | LE_SCF(0)),\
	  (L3_ESC(0) | L3_SCC(0) | L3_CACHEABILITY(L3_UC)) },\
	/* { 0x00000038, 0x0030 } */\
	{ (LE_CACHEABILITY(LE_PAGETABLE) | LE_TGT_CACHE(LLC_ELLC) | LE_LRUM(3) |\
	   LE_AOM(0) | LE_RSC(0) | LE_SCC(0) | LE_PFM(0) | LE_SCF(0)),\
	  (L3_ESC(0) | L3_SCC(0) | L3_CACHEABILITY(L3_WB)) },\
	/* { 0x0000003b, 0x0030 } */\
	{ (LE_CACHEABILITY(LE_WB) | LE_TGT_CACHE(LLC_ELLC) | LE_LRUM(3) |\
	   LE_AOM(0) | LE_RSC(0) | LE_SCC(0) | LE_PFM(0) | LE_SCF(0)),\
	  (L3_ESC(0) | L3_SCC(0) | L3_CACHEABILITY(L3_WB)) }\
}
