#ifndef HARDDOOM_H
#define HARDDOOM_H

/* PCI ids */

#define HARDDOOM_VENDOR_ID			0x0666
#define HARDDOOM_DEVICE_ID			0x1993

/* Registers */

/* Enables active units of the device.  TLB is passive and doesn't have
 * an enable (disable XY or TEX instead).  FIFOs other than the main one
 * also don't have enables -- disable the source and/or destination unit
 * instead.  */
#define HARDDOOM_ENABLE				0x000
#define HARDDOOM_ENABLE_FETCH_CMD		0x00000001
#define HARDDOOM_ENABLE_FIFO			0x00000002
#define HARDDOOM_ENABLE_FE			0x00000004
#define HARDDOOM_ENABLE_XY			0x00000008
#define HARDDOOM_ENABLE_TEX			0x00000010
#define HARDDOOM_ENABLE_FLAT			0x00000020
#define HARDDOOM_ENABLE_FUZZ			0x00000040
#define HARDDOOM_ENABLE_SR			0x00000080
#define HARDDOOM_ENABLE_OG			0x00000100
#define HARDDOOM_ENABLE_SW			0x00000200
#define HARDDOOM_ENABLE_ALL			0x000003ff
/* Status of device units -- 1 means they have work to do.  */
#define HARDDOOM_STATUS				0x004
#define HARDDOOM_STATUS_FETCH_CMD		0x00000001
#define HARDDOOM_STATUS_FIFO			0x00000002
#define HARDDOOM_STATUS_FE			0x00000004
#define HARDDOOM_STATUS_XY			0x00000008
#define HARDDOOM_STATUS_TEX			0x00000010
#define HARDDOOM_STATUS_FLAT			0x00000020
#define HARDDOOM_STATUS_FUZZ			0x00000040
#define HARDDOOM_STATUS_SR			0x00000080
#define HARDDOOM_STATUS_OG			0x00000100
#define HARDDOOM_STATUS_SW			0x00000200
#define HARDDOOM_STATUS_FE2XY			0x00000400
#define HARDDOOM_STATUS_FE2TEX			0x00000800
#define HARDDOOM_STATUS_FE2FLAT			0x00001000
#define HARDDOOM_STATUS_FE2FUZZ			0x00002000
#define HARDDOOM_STATUS_FE2OG			0x00004000
#define HARDDOOM_STATUS_XY2SW			0x00008000
#define HARDDOOM_STATUS_XY2SR			0x00010000
#define HARDDOOM_STATUS_SR2OG			0x00020000
#define HARDDOOM_STATUS_TEX2OG			0x00040000
#define HARDDOOM_STATUS_FLAT2OG			0x00080000
#define HARDDOOM_STATUS_FUZZ2OG			0x00100000
#define HARDDOOM_STATUS_OG2SW			0x00200000
#define HARDDOOM_STATUS_OG2SW_C			0x00400000
#define HARDDOOM_STATUS_SW2XY			0x00800000
/* The reset register.  Punching 1 will clear all pending work.  There is
 * no reset for FETCH_CMD (initialize CMD_*_PTR instead).  */
#define HARDDOOM_RESET				0x004
#define HARDDOOM_RESET_FIFO			0x00000002
#define HARDDOOM_RESET_FE			0x00000004
#define HARDDOOM_RESET_XY			0x00000008
#define HARDDOOM_RESET_TEX			0x00000010
#define HARDDOOM_RESET_FLAT			0x00000020
#define HARDDOOM_RESET_FUZZ			0x00000040
#define HARDDOOM_RESET_SR			0x00000080
#define HARDDOOM_RESET_OG			0x00000100
#define HARDDOOM_RESET_SW			0x00000200
#define HARDDOOM_RESET_FE2XY			0x00000400
#define HARDDOOM_RESET_FE2TEX			0x00000800
#define HARDDOOM_RESET_FE2FLAT			0x00001000
#define HARDDOOM_RESET_FE2FUZZ			0x00002000
#define HARDDOOM_RESET_FE2OG			0x00004000
#define HARDDOOM_RESET_XY2SW			0x00008000
#define HARDDOOM_RESET_XY2SR			0x00010000
#define HARDDOOM_RESET_SR2OG			0x00020000
#define HARDDOOM_RESET_TEX2OG			0x00040000
#define HARDDOOM_RESET_FLAT2OG			0x00080000
#define HARDDOOM_RESET_FUZZ2OG			0x00100000
#define HARDDOOM_RESET_OG2SW			0x00200000
#define HARDDOOM_RESET_OG2SW_C			0x00400000
#define HARDDOOM_RESET_SW2XY			0x00800000
#define HARDDOOM_RESET_STATS			0x01000000
#define HARDDOOM_RESET_TLB			0x02000000
#define HARDDOOM_RESET_TEX_CACHE		0x04000000
#define HARDDOOM_RESET_FLAT_CACHE		0x08000000
#define HARDDOOM_RESET_ALL			0x0ffffffe
/* Interrupt status.  */
#define HARDDOOM_INTR				0x008
#define HARDDOOM_INTR_FENCE			0x00000001
#define HARDDOOM_INTR_PONG_SYNC			0x00000002
#define HARDDOOM_INTR_PONG_ASYNC		0x00000004
#define HARDDOOM_INTR_FE_ERROR			0x00000008
#define HARDDOOM_INTR_FIFO_OVERFLOW		0x00000010
#define HARDDOOM_INTR_SURF_DST_OVERFLOW		0x00000020
#define HARDDOOM_INTR_SURF_SRC_OVERFLOW		0x00000040
#define HARDDOOM_INTR_PAGE_FAULT_SURF_DST	0x00000080
#define HARDDOOM_INTR_PAGE_FAULT_SURF_SRC	0x00000100
#define HARDDOOM_INTR_PAGE_FAULT_TEXTURE	0x00000200
#define HARDDOOM_INTR_MASK			0x000003ff
/* And enable (same bitfields).  */
#define HARDDOOM_INTR_ENABLE			0x00c
/* The last value of processed FENCE command.  */
#define HARDDOOM_FENCE_LAST			0x010
#define HARDDOOM_FENCE_MASK			0x03ffffff
/* The value that will trigger a FENCE interrupt when used in FENCE command.  */
#define HARDDOOM_FENCE_WAIT			0x014
/* Command read pointer -- whenever not equal to CMD_WRITE_PTR, FETCH_CMD will
 * fetch command from here and increment.  */
#define HARDDOOM_CMD_READ_PTR			0x018
/* Command write pointer -- FETCH_CMD halts when it hits this address.  */
#define HARDDOOM_CMD_WRITE_PTR			0x01c


/* The FE (Front End) unit.  Its task is to digest FIFO commands into simple
 * commands for individual blocks (XY, TEX, FLAT, FUZZ, OG).  Since this is
 * quite a complicated task, it is a microcoded engine.  */

/* Front End microcode window.  Any access to _DATA will access the code RAM
 * cell selected by _ADDR and bump _ADDR by one.  Each cell is one 30-bit
 * instruction.  */
#define HARDDOOM_FE_CODE_ADDR			0x020
#define HARDDOOM_FE_CODE_WINDOW			0x024
#define HARDDOOM_FE_CODE_SIZE			0x00001000
/* Front End error reporting -- when the microcode detects an error in the
 * command stream, it triggers the FE_ERROR interrupt, writes the offending
 * command to _CMD, and writes the error code to _CODE.  */
#define HARDDOOM_FE_ERROR_CODE			0x028
/* Unknown command type.  */
#define HARDDOOM_FE_ERROR_CODE_RESERVED_TYPE	0x00000000
/* Known command type with non-0 value in reserved bits.  */
#define HARDDOOM_FE_ERROR_CODE_RESERVED_BITS	0x00000001
/* SURF_DIMS with WIDTH == 0.  */
#define HARDDOOM_FE_ERROR_CODE_SURF_WIDTH_ZERO	0x00000002
/* SURF_DIMS with HEIGHT == 0.  */
#define HARDDOOM_FE_ERROR_CODE_SURF_HEIGHT_ZERO	0x00000003
/* SURF_DIMS with WIDTH > 2048.  */
#define HARDDOOM_FE_ERROR_CODE_SURF_WIDTH_OVF	0x00000004
/* SURF_DIMS with HEIGHT > 2048.  */
#define HARDDOOM_FE_ERROR_CODE_SURF_HEIGHT_OVF	0x00000005
/* COPY_RECT or FILL_RECT destination X+WIDTH > SURF_DIMS.WIDTH.  */
#define HARDDOOM_FE_ERROR_CODE_RECT_DST_X_OVF	0x00000006
/* COPY_RECT or FILL_RECT destionation Y+HEIGHT > SURF_DIMS.HEIGHT.  */
#define HARDDOOM_FE_ERROR_CODE_RECT_DST_Y_OVF	0x00000007
/* COPY_RECT source X+WIDTH > SURF_DIMS.WIDTH.  */
#define HARDDOOM_FE_ERROR_CODE_RECT_SRC_X_OVF	0x00000008
/* COPY_RECT source Y+HEIGHT > SURF_DIMS.HEIGHT.  */
#define HARDDOOM_FE_ERROR_CODE_RECT_SRC_Y_OVF	0x00000009
/* DRAW_COLUMN XY_A.Y > XY_B.Y.  */
#define HARDDOOM_FE_ERROR_CODE_DRAW_COLUMN_REV	0x0000000a
/* DRAW_SPAN XY_A.X > XY_B.X.  */
#define HARDDOOM_FE_ERROR_CODE_DRAW_SPAN_REV	0x0000000b
#define HARDDOOM_FE_ERROR_CODE_MASK		0x00000fff
#define HARDDOOM_FE_ERROR_CMD			0x02c
/* Direct command submission (goes to FIFO bypassing FETCH_CMD).  */
#define HARDDOOM_FIFO_SEND			0x030
/* Read-only number of free slots in FIFO.  */
#define HARDDOOM_FIFO_FREE			0x030
/* The FE state -- microcode program counter and "waiting for FIFO" flag.  */
#define HARDDOOM_FE_STATE			0x034
#define HARDDOOM_FE_STATE_PC_MASK		0x00000fff
#define HARDDOOM_FE_STATE_WAIT_FIFO		0x00001000
#define HARDDOOM_FE_STATE_MASK			0x00001fff
/* Internal state of the main FIFO -- read and write pointers.
 * There are 0x200 entries, indexed by 10-bit indices (each entry is visible
 * under two indices).  Bits 0-9 is read pointer (index of the next entry to
 * be read by DRAW), 16-25 is write pointer (index of the next entry to be
 * written by FIFO_SEND).  FIFO is empty iff read == write, full iff read ==
 * write ^ 0x200.  Situations where ((write - read) & 0x3ff) > 0x200
 * are illegal and won't be reached in proper operation of the device.
 * Other FIFOs are similar, but with different sizes and entry types.
 */
#define HARDDOOM_FIFO_STATE			0x038
/* The window to the FIFO (internal).  When read, reads from READ_PTR,
 * and increments it.  When written, writes to WRITE_PTR, and increments
 * it.  If this causes a FIFO overflow/underflow, so be it.  */
#define HARDDOOM_FIFO_WINDOW			0x03c
#define HARDDOOM_FIFO_SIZE			0x00000200
/* The FE data memory window -- behaves like the code window.  Used by
 * the microcode to store data arrays.  */
#define HARDDOOM_FE_DATA_ADDR			0x040
#define HARDDOOM_FE_DATA_WINDOW			0x044
#define HARDDOOM_FE_DATA_SIZE			0x00000200
/* The FE -> XY command FIFO (see XYCMD definitions below).  */
#define HARDDOOM_FE2XY_STATE			0x048
#define HARDDOOM_FE2XY_WINDOW			0x04c
#define HARDDOOM_FE2XY_SIZE			0x00000080
/* The FE -> TEX command FIFO (see TEXCMD definitions below).  */
#define HARDDOOM_FE2TEX_STATE			0x050
#define HARDDOOM_FE2TEX_WINDOW			0x054
#define HARDDOOM_FE2TEX_SIZE			0x00000080
/* The FE -> FLAT command FIFO (see FLCMD definitions below).  */
#define HARDDOOM_FE2FLAT_STATE			0x058
#define HARDDOOM_FE2FLAT_WINDOW			0x05c
#define HARDDOOM_FE2FLAT_SIZE			0x00000080
/* The FE -> FUZZ command FIFO (see FZCMD definitions below).  */
#define HARDDOOM_FE2FUZZ_STATE			0x060
#define HARDDOOM_FE2FUZZ_WINDOW			0x064
#define HARDDOOM_FE2FUZZ_SIZE			0x00000080
/* The FE -> OG command FIFO (see OGCMD definitions below).  */
#define HARDDOOM_FE2OG_STATE			0x068
#define HARDDOOM_FE2OG_WINDOW			0x06c
#define HARDDOOM_FE2OG_SIZE			0x00000080
/* The FE registers, for use as variables by the microcode.  */
#define HARDDOOM_FE_REG(i)			(0x080 + (i) * 4)
#define HARDDOOM_FE_REG_NUM			0x20


/* The STATS unit.  */

/* The indices are listed below.  */
#define HARDDOOM_STATS(i)			(0x100 + (i) * 4)
#define HARDDOOM_STATS_NUM			0x40

/* The FE stats (counted by microcode).  */
/* A COPY_RECT command was processed as a series of horizontally drawn lines.  */
#define HARDDOOM_STAT_FE_COPY_RECT_HORIZONTAL	0x00
/* A line was drawn as part of the above.  */
#define HARDDOOM_STAT_FE_COPY_RECT_LINE		0x01
/* A COPY_RECT command was processed as a vertical series of blocks.  */
#define HARDDOOM_STAT_FE_COPY_RECT_VERTICAL	0x02
/* Like the above 3, but for FILL_RECT.  */
#define HARDDOOM_STAT_FE_FILL_RECT_HORIZONTAL	0x03
#define HARDDOOM_STAT_FE_FILL_RECT_LINE		0x04
#define HARDDOOM_STAT_FE_FILL_RECT_VERTICAL	0x05
/* A DRAW_LINE command was processed as mostly-horizontal.  */
#define HARDDOOM_STAT_FE_DRAW_LINE_HORIZONTAL	0x06
/* Ditto, mostly-vertical.  */
#define HARDDOOM_STAT_FE_DRAW_LINE_VERTICAL	0x07
/* A chunk of pixels (X×1 rectangle) was drawn for
 * a mostly-horizontal line.  */
#define HARDDOOM_STAT_FE_DRAW_LINE_H_CHUNK	0x08
/* A chunk of pixels (1×X rectangle) was drawn for
 * a mostly-vertical line.  */
#define HARDDOOM_STAT_FE_DRAW_LINE_V_CHUNK	0x09
/* A pixel was drawn for a mostly-horizontal line.  */
#define HARDDOOM_STAT_FE_DRAW_LINE_H_PIXEL	0x0a
/* A pixel was drawn for a mostly-vertical line.  */
#define HARDDOOM_STAT_FE_DRAW_LINE_V_PIXEL	0x0b
/* A DRAW_BACKGROUND command was processed.  */
#define HARDDOOM_STAT_FE_DRAW_BACKGROUND	0x0c
/* A batch of DRAW_COLUMN commands without FUZZ was processed (to count
 * invididual columns, see TEX_COLUMN counter).  */
#define HARDDOOM_STAT_FE_DRAW_COLUMN_TEX_BATCH	0x0d
/* A batch of DRAW_COLUMN commands with FUZZ was processed (to count
 * individual columns, see FUZZ_COLUMN counter).  */
#define HARDDOOM_STAT_FE_DRAW_COLUMN_FUZZ_BATCH	0x0e
/* A DRAW_SPAN command was processed.  */
#define HARDDOOM_STAT_FE_DRAW_SPAN		0x0f

/* Various FIFO flow statistics.  */
/* A command was sent to FE (from FIFO_SEND or FETCH_CMD).  */
#define HARDDOOM_STAT_FE_CMD			0x10
/* A command was sent to XY by FE.  */
#define HARDDOOM_STAT_XY_CMD			0x11
/* A command was sent to TEX by FE.  */
#define HARDDOOM_STAT_TEX_CMD			0x12
/* A command was sent to FLAT by FE.  */
#define HARDDOOM_STAT_FLAT_CMD			0x13
/* A command was sent to FUZZ by FE.  */
#define HARDDOOM_STAT_FUZZ_CMD			0x14
/* A command was sent to OG by FE.  */
#define HARDDOOM_STAT_OG_CMD			0x15
/* A command was sent to SW by OG.  */
#define HARDDOOM_STAT_SW_CMD			0x16
/* A block of pixels was read from a surface by SR and sent to OG.  */
#define HARDDOOM_STAT_SR_BLOCK			0x17
/* A block of pixels was textured by TEX and sent to OG.  */
#define HARDDOOM_STAT_TEX_BLOCK			0x18
/* A block of pixels was read or textured by FLAT and sent to OG.  */
#define HARDDOOM_STAT_FLAT_BLOCK		0x19
/* A block mask was prepared by FUZZ and sent to OG.  */
#define HARDDOOM_STAT_FUZZ_BLOCK		0x1a
/* A block of pixels was prepared by OG and sent to SW to be written to a surface.  */
#define HARDDOOM_STAT_SW_BLOCK			0x1b

/* TLB statistics.  */
/* TLB hits and misses for all 3 TLBs.  */
#define HARDDOOM_STAT_TLB_SURF_DST_HIT		0x1c
#define HARDDOOM_STAT_TLB_SURF_DST_MISS		0x1d
#define HARDDOOM_STAT_TLB_SURF_SRC_HIT		0x1e
#define HARDDOOM_STAT_TLB_SURF_SRC_MISS		0x1f
#define HARDDOOM_STAT_TLB_TEXTURE_HIT		0x20
#define HARDDOOM_STAT_TLB_TEXTURE_MISS		0x21
/* A SURF_*_PT command was processed by XY.  */
#define HARDDOOM_STAT_TLB_REBIND_SURF_DST	0x22
#define HARDDOOM_STAT_TLB_REBIND_SURF_SRC	0x23
/* A TEXTURE_PT command was processed by TEX.  */
#define HARDDOOM_STAT_TLB_REBIND_TEXTURE	0x24

/* XY statistics.  */
/* An INTERLOCK command was processed by XY.  */
#define HARDDOOM_STAT_XY_INTERLOCK		0x25

/* TEX statistics.  */
/* A START_COLUMN command was processed by TEX (equivalent to count of front
 * DRAW_COLUMN commands without FUZZ).  */
#define HARDDOOM_STAT_TEX_COLUMN		0x26
/* A pixel was textured by the TEX unit.  */
#define HARDDOOM_STAT_TEX_PIXEL			0x27
/* A texture cache hit on the currently textured pixel.  */
#define HARDDOOM_STAT_TEX_CACHE_HIT		0x28
/* A texture cache hit on a speculative pixel (causing it to be pre-textured).  */
#define HARDDOOM_STAT_TEX_CACHE_SPEC_HIT	0x29
/* A texture cache miss on the currently textured pixel (ie. a cache fill).  */
#define HARDDOOM_STAT_TEX_CACHE_MISS		0x2a
/* A texture cache miss on a speculative pixel (no cache fill).  */
#define HARDDOOM_STAT_TEX_CACHE_SPEC_MISS	0x2b

/* FLAT statistics.  */
/* A FLAT_ADDR command was processed by FLAT.  */
#define HARDDOOM_STAT_FLAT_REBIND		0x2c
/* A block was sent to OG by the READ_FLAT command.  */
#define HARDDOOM_STAT_FLAT_READ_BLOCK		0x2d
/* A block was sent to OG by the DRAW_SPAN command.  */
#define HARDDOOM_STAT_FLAT_SPAN_BLOCK		0x2e
/* A pixel was textured by the DRAW_SPAN command.  */
#define HARDDOOM_STAT_FLAT_SPAN_PIXEL		0x2f
/* A flat cache hit.  */
#define HARDDOOM_STAT_FLAT_CACHE_HIT		0x30
/* A flat cache miss (and fill).  */
#define HARDDOOM_STAT_FLAT_CACHE_MISS		0x31

/* FUZZ statistics.  */
/* A SET_COLUMN command was processed by FUZZ (equivalent to count of front
 * DRAW_COLUMN commands with FUZZ).  */
#define HARDDOOM_STAT_FUZZ_COLUMN		0x32

/* OG statistics.  */
/* A COLORMAP_ADDR command was processed by OG (ie. the main color map was
 * read from memory.  */
#define HARDDOOM_STAT_OG_COLORMAP_FETCH		0x33
/* A TRANSLATION_ADDR command was processed by OG (ie. the translation color
 * map was read from memory.  */
#define HARDDOOM_STAT_OG_TRANSLATION_FETCH	0x34
/* A block was sent to SW by the DRAW_BUF_* command.  */
#define HARDDOOM_STAT_OG_DRAW_BUF_BLOCK		0x35
/* A pixel was sent to SW by the DRAW_BUF_* command.  */
#define HARDDOOM_STAT_OG_DRAW_BUF_PIXEL		0x36
/* A block was sent to SW by the COPY_* command.  */
#define HARDDOOM_STAT_OG_COPY_BLOCK		0x37
/* A pixel was sent to SW by the COPY_* command.  */
#define HARDDOOM_STAT_OG_COPY_PIXEL		0x38
/* A pixel was sent to SW by the DRAW_FUZZ command.  */
#define HARDDOOM_STAT_OG_FUZZ_PIXEL		0x39
/* A block was processed by the translation color map.  */
#define HARDDOOM_STAT_OG_TRANSLATE_BLOCK	0x3a
/* A block was processed by the main color map, not including the FUZZ effect.  */
#define HARDDOOM_STAT_OG_COLORMAP_BLOCK		0x3b

/* SW statistics.  */
/* A FENCE command was processed by SW.  */
#define HARDDOOM_STAT_SW_FENCE			0x3c
/* A FENCE command was processed by SW that matched the FENCE_WAIT register.  */
#define HARDDOOM_STAT_SW_FENCE_INTR		0x3d
/* A pixel was written to memory by SW.  */
#define HARDDOOM_STAT_SW_PIXEL			0x3e
/* A contiguous group of pixels was written to memory by SW.  */
#define HARDDOOM_STAT_SW_XFER			0x3f


/* The XY unit.  Its responsibility is to translate (X, Y) coordinates in
 * the surfaces into physical addresses and to send them to SR and SW
 * units.  Since SR and SW operate on 64-pixel blocks, the X coordinates
 * here are 5-bit and are counted in blocks.  Widths are likewise 6-bit and
 * counted in blocks.  The FE unit requests translation in horizontal
 * or vertical batches, by giving the starting (X, Y) coordinate and
 * width or height of the batch.  The XY unit can have two batches
 * active at a moment -- one for the source surface and one for the
 * destination surface.  Also, the XY unit handles one half of the INTERLOCK
 * command (by listening on the SW2XY interface for receipt of the other
 * half by SW and blocking all operations until then).  */

/* Copy of the last processed SURF_DIMS command.  */
#define HARDDOOM_XY_SURF_DIMS			0x200
#define HARDDOOM_XY_SURF_DIMS_MASK		0x000fff3f
/* The command currently pending.  */
#define HARDDOOM_XY_CMD				0x204
/* The destination address command currently being executed.  */
#define HARDDOOM_XY_DST_CMD			0x208
/* The source address command currently being executed.  */
#define HARDDOOM_XY_SRC_CMD			0x20c
/* The XY -> SW FIFO (with physical addresses to be written by SW with data
 * prepared by OG).  */
#define HARDDOOM_XY2SW_STATE			0x240
#define HARDDOOM_XY2SW_WINDOW			0x244
#define HARDDOOM_XY2SW_SIZE			0x00000080
/* The XY -> SR FIFO (with physical addresses to be read by SR and sent
 * to OG for processing).  */
#define HARDDOOM_XY2SR_STATE			0x248
#define HARDDOOM_XY2SR_WINDOW			0x24c
#define HARDDOOM_XY2SR_SIZE			0x00000080


/* The SR (Surface Read) unit.  It takes physical addresses from the XY unit,
 * reads full blocks from them, and then submits them to the OG unit for
 * processing.  Stateless.  Used to read source data for COPY_RECT and to
 * read current data from the destination framebuffer to be modified by the
 * FUZZ effect.  */

/* The SR -> OG FIFO.  Each element is a 64-pixel block.  */
#define HARDDOOM_SR2OG_STATE			0x280
/* The window is 64 bytes long.  */
#define HARDDOOM_SR2OG_WINDOW			0x2c0
#define HARDDOOM_SR2OG_SIZE			0x00000020


/* The TLB unit.  Called by the XY and TEX units to translate virtual
 * addresses to physical.  */

/* The current page table for each buffer.  */
#define HARDDOOM_TLB_PT_SURF_DST		0x300
#define HARDDOOM_TLB_PT_SURF_SRC		0x304
#define HARDDOOM_TLB_PT_TEXTURE			0x308
#define HARDDOOM_TLB_PT_MASK			0xffffffc0
/* The single-entry TLBs, one for each paged buffer.  Bits 0 and 12+ are taken
 * straight from the PTE.  Bits 2-11 are the tag (bits 12-21 of the virtual
 * address, ie. page table index).  Bit 1 is the TLB valid bit.
 */
#define HARDDOOM_TLB_ENTRY_SURF_DST		0x310
#define HARDDOOM_TLB_ENTRY_SURF_SRC		0x314
#define HARDDOOM_TLB_ENTRY_TEXTURE		0x318
#define HARDDOOM_TLB_ENTRY_MASK			0xffffffff
#define HARDDOOM_TLB_ENTRY_PTE_VALID		0x00000001
#define HARDDOOM_TLB_ENTRY_VALID		0x00000002
#define HARDDOOM_TLB_ENTRY_IDX_MASK		0x00000ffc
#define HARDDOOM_TLB_ENTRY_IDX_SHIFT		2
#define HARDDOOM_TLB_ENTRY_PAGE_MASK		0xfffff000
/* The last virtual address looked up in the TLB -- useful in case of
 * page faults.  */
#define HARDDOOM_TLB_VADDR_SURF_DST		0x320
#define HARDDOOM_TLB_VADDR_SURF_SRC		0x324
#define HARDDOOM_TLB_VADDR_TEXTURE		0x328
#define HARDDOOM_TLB_VADDR_MASK			0x003fffc0
#define HARDDOOM_TLB_IDX_MASK			0x000003ff

/* The FLAT unit.  Gets flat coordinates and pixel count from the FE unit,
 * reads the texels from the flat, and outputs textured pixels to the OG unit.
 * The FE can request a single horizontal span at a time.  Also, can bypass
 * raw blocks from the flat directly to the OG (used for DRAW_BACKGROUND).  */

/* The current U and V coordinates (updated as pixels are textured).  */
#define HARDDOOM_FLAT_UCOORD			0x360
#define HARDDOOM_FLAT_VCOORD			0x364
/* The USTEP and VSTEP params are stored here.  */
#define HARDDOOM_FLAT_USTEP			0x368
#define HARDDOOM_FLAT_VSTEP			0x36c
#define HARDDOOM_FLAT_COORD_MASK		0x003fffff
/* The address of the current flat.  */
#define HARDDOOM_FLAT_ADDR			0x370
#define HARDDOOM_FLAT_ADDR_MASK			0xfffff000
/* The command in progress.  */
#define HARDDOOM_FLAT_CMD			0x374
/* The flat cache state.  It has one 64-byte cache line.  */
#define HARDDOOM_FLAT_CACHE_STATE		0x378
#define HARDDOOM_FLAT_CACHE_STATE_TAG_MASK	0x0000003f
#define HARDDOOM_FLAT_CACHE_STATE_VALID		0x00000100
#define HARDDOOM_FLAT_CACHE_STATE_MASK		0x0000013f
/* The FLAT -> OG FIFO.  Each element is a 64-pixel block.  If the DRAW_SPAN
 * width is not a multiple of 64, the last block is padded with random data.  */
#define HARDDOOM_FLAT2OG_STATE			0x37c
#define HARDDOOM_FLAT2OG_WINDOW			0x380
#define HARDDOOM_FLAT2OG_SIZE			0x00000020
#define HARDDOOM_FLAT_CACHE			0x3c0
#define HARDDOOM_FLAT_CACHE_SIZE		64


/* The SW unit.  Gets final pixel data and write masks from the OG unit,
 * and stores it to addresses received from the XY unit.  Also, as the last
 * unit in the pipeline, handles synchronization commands.  */

/* The command currently being processed.  */
#define HARDDOOM_SW_CMD				0x400
/* The SW -> XY pseudo-FIFO used for INTERLOCKs.  There is no actual data
 * payload being transmitted -- the FIFO state is simply the number of
 * INTERLOCKs sent by SW and not yet received by XY.  */
#define HARDDOOM_SW2XY_STATE			0x404
#define HARDDOOM_SW2XY_STATE_MASK		0x000000ff


/* The OG (output gather) unit.  Responsible for gathering pixel data from
 * FE, SR, TEX and FLAT, applying effects, preparing write masks, and sending
 * the final pixel data to SW.  The heart of this unit is a 4-block buffer.
 * The buffer blocks are aligned with destination surface blocks -- blocks
 * from SR and FLAT are rotated on input to match the destination alignment.  */

/* The write mask of the block to be rendered (64-bit register).  */
#define HARDDOOM_OG_MASK			0x480
/* The FUZZ effect mask received from the FUZZ unit (64-bit register).  */
#define HARDDOOM_OG_FUZZ_MASK			0x488
/* The command currently being processed.  */
#define HARDDOOM_OG_CMD				0x490
/* Misc state.  */
#define HARDDOOM_OG_MISC			0x494
/* The value of the last SRC_OFFSET command.  */
#define HARDDOOM_OG_MISC_SRC_OFFSET_MASK	0x0000003f
/* The current position in the buffer (exact interpretation depends
 * on the command).  */
#define HARDDOOM_OG_MISC_BUF_POS_MASK		0x0000ff00
#define HARDDOOM_OG_MISC_BUF_POS_SHIFT		8
/* The current state in the command execution state machine.  Every command
 * starts in the INIT state.  The exact interpretation depends on the command.  */
#define HARDDOOM_OG_MISC_STATE_INIT		0x00000000
#define HARDDOOM_OG_MISC_STATE_PREFILL		0x00010000
#define HARDDOOM_OG_MISC_STATE_RUNNING		0x00020000
#define HARDDOOM_OG_MISC_STATE_FILLED		0x00030000
#define HARDDOOM_OG_MISC_STATE_MASK		0x00030000
#define HARDDOOM_OG_MISC_MASK			0x0003ff3f
/* The OG -> SW command FIFO (contains 32-bit commands).  */
#define HARDDOOM_OG2SW_C_STATE			0x4a0
#define HARDDOOM_OG2SW_C_WINDOW			0x4a4
#define HARDDOOM_OG2SW_C_SIZE			0x00000020
/* The OG -> SW pixel data FIFO (each entry consists of a 64-byte block and
 * a 64-bit write mask.  */
#define HARDDOOM_OG2SW_STATE			0x4b4
#define HARDDOOM_OG2SW_M_WINDOW			0x4b8
#define HARDDOOM_OG2SW_D_WINDOW			0x4c0
#define HARDDOOM_OG2SW_SIZE			0x00000020
/* The buffer.  */
#define HARDDOOM_OG_BUF				0x500
#define HARDDOOM_OG_BUF_SIZE			0x00000100
/* The current color maps (they are eagerly read in their entirety when FE
 * requests so).  */
#define HARDDOOM_OG_COLORMAP			0x600
#define HARDDOOM_OG_TRANSLATION			0x700


/* The FUZZ unit.  Responsible for generating pixel masks for the FUZZ
 * effect.  Capable of operating on a block of columns at once, with
 * independent fuzz positions for each column.  The FE unit computes
 * and sets the initial fuzz position for each column, then sends FUZZ
 * the number of masks to generate, letting it step the positions while
 * the batch is being drawn.  */

/* The fuzz position for every column (64 single-byte registers).  */
#define HARDDOOM_FUZZ_POSITION			0x800
/* The command in progress.  */
#define HARDDOOM_FUZZ_CMD			0x840
/* The FUZZ -> OG FIFO.  Each entry is a 64-bit mask.  */
#define HARDDOOM_FUZZ2OG_STATE			0x844
#define HARDDOOM_FUZZ2OG_WINDOW			0x848
#define HARDDOOM_FUZZ2OG_SIZE			0x00000020


/* The TEX unit.  Can handle 64 DRAW_COLUMN commands concurrently, merging
 * them to a single stream of textured blocks.  To optimize cache usage,
 * when a pixel is textured, up to 0x10 pixels below it are speculatively
 * textured as well if the required texels are on the currently active
 * cache line.  */

/* A copy of the last submitted TEXTURE_DIMS command.  */
#define HARDDOOM_TEX_DIMS			0x850
#define HARDDOOM_TEX_DIMS_MASK			0x03fff3ff
/* The last submitted USTART command (will be copied to TEX_COLUMN_STATE
 * by START_COLUMN).  */
#define HARDDOOM_TEX_USTART			0x854
/* The last submitted USTEP command (will be copied to TEX_COLUMN_STEP
 * by START_COLUMN).  */
#define HARDDOOM_TEX_USTEP			0x858
#define HARDDOOM_TEX_COORD_MASK			0x03ffffff
/* The command currently being executed.  */
#define HARDDOOM_TEX_CMD			0x85c
/* The mask of currently active columns (64-bit register).  */
#define HARDDOOM_TEX_MASK			0x860
/* The texture cache -- a single 64-byte line.  */
#define HARDDOOM_TEX_CACHE_STATE		0x868
#define HARDDOOM_TEX_CACHE_STATE_TAG_MASK	0x0000ffff
#define HARDDOOM_TEX_CACHE_STATE_VALID		0x00010000
#define HARDDOOM_TEX_CACHE_STATE_MASK		0x0001ffff
/* The position of the current block in the speculative texturing buffers.  */
#define HARDDOOM_TEX_CACHE_STATE_SPEC_POS_MASK	0x00f00000
#define HARDDOOM_TEX_CACHE_STATE_SPEC_POS_SHIFT	20
/* The TEX -> OG FIFO.  Each entry consists of a 64-byte block and a 64-bit
 * active column mask.  */
#define HARDDOOM_TEX2OG_STATE			0x874
#define HARDDOOM_TEX2OG_M_WINDOW		0x878
#define HARDDOOM_TEX2OG_D_WINDOW		0x880
#define HARDDOOM_TEX2OG_SIZE			0x00000020
#define HARDDOOM_TEX_CACHE			0x8c0
#define HARDDOOM_TEX_CACHE_SIZE			64
/* The current texture coordinate for each column, and the number of
 * speculatively textured pixels available in the buffer.  */
#define HARDDOOM_TEX_COLUMN_STATE(i)		(0x900 + (i) * 4)
#define HARDDOOM_TEX_COLUMN_STATE_COORD_MASK	0x03ffffff
#define HARDDOOM_TEX_COLUMN_STATE_SPEC_MASK	0x7c000000
#define HARDDOOM_TEX_COLUMN_STATE_SPEC_SHIFT	26
#define HARDDOOM_TEX_COLUMN_STATE_MASK		0x7fffffff
/* The coordinate step for each column.  */
#define HARDDOOM_TEX_COLUMN_STEP(i)		(0xa00 + (i) * 4)
/* The starting texture offset for each column.  */
#define HARDDOOM_TEX_COLUMN_OFFSET(i)		(0xb00 + (i) * 4)
#define HARDDOOM_TEX_OFFSET_MASK		0x003fffff
/* The speculatively textured pixels for each column.  */
#define HARDDOOM_TEX_COLUMN_SPEC_DATA(i)	(0xc00 + (i) * 0x10)
#define HARDDOOM_TEX_COLUMN_SPEC_DATA_SIZE	0x10


/* The FETCH_CMD / FIFO / FE Commands.  */

/* Jump in the command buffer (only valid for FETCH_CMD).  */
#define HARDDOOM_CMD_TYPE_HI_JUMP		0x0

/* Surface to render to, all commands. */
#define HARDDOOM_CMD_TYPE_SURF_DST_PT		0x20
/* Source surface for COPY_RECT,  */
#define HARDDOOM_CMD_TYPE_SURF_SRC_PT		0x21
/* Texture for DRAW_COLUMN. */
#define HARDDOOM_CMD_TYPE_TEXTURE_PT		0x22
/* The flat for DRAW_BACKGROUND and DRAW_SPAN. */
#define HARDDOOM_CMD_TYPE_FLAT_ADDR		0x23
/* Fade/effect color map for DRAW_COLUMN and DRAW_SPAN, used iff DRAW_PARAMS_COLORMAP set. */
#define HARDDOOM_CMD_TYPE_COLORMAP_ADDR		0x24
/* Palette translation color map for DRAW_COLUMN and DRAW_SPAN, used iff DRAW_PARAMS_TRANSLATE set. */
#define HARDDOOM_CMD_TYPE_TRANSLATION_ADDR	0x25
/* Dimensions of all SURFs in use. */
#define HARDDOOM_CMD_TYPE_SURF_DIMS		0x26
/* Height and byte size of the texture for DRAW_COLUMN. */
#define HARDDOOM_CMD_TYPE_TEXTURE_DIMS		0x27
/* Solid fill color for FILL_RECT and DRAW_LINE. */
#define HARDDOOM_CMD_TYPE_FILL_COLOR		0x28
/* Flags for DRAW_COLUMN and DRAW_SPAN (FUZZ, TRANSLATE, COLORMAP).
 */
#define HARDDOOM_CMD_TYPE_DRAW_PARAMS		0x29
/* Destination rect top left corner for COPY_RECT, FILL_RECT.
 * First end point for DRAW_LINE.
 * Top end point for DRAW_COLUMN.
 * Left end point for DRAW_SPAN.
 */
#define HARDDOOM_CMD_TYPE_XY_A			0x2a
/* Source rect corner top left for COPY_RECT.
 * Second end point for DRAW_LINE.
 * Botoom end point for DRAW_COLUMN.
 * Right end point for DRAW_SPAN.
 */
#define HARDDOOM_CMD_TYPE_XY_B			0x2b
/* Tex coord start for DRAW_COLUMN (U), DRAW_SPAN (U+V). */
#define HARDDOOM_CMD_TYPE_USTART		0x2c
#define HARDDOOM_CMD_TYPE_VSTART		0x2d
/* Tex coord derivative for DRAW_COLUMN (U), DRAW_SPAN (U+V). */
#define HARDDOOM_CMD_TYPE_USTEP			0x2e
#define HARDDOOM_CMD_TYPE_VSTEP			0x2f

/* V_CopyRect: The usual blit.  Rectangle size passed directly.  */
#define HARDDOOM_CMD_TYPE_COPY_RECT		0x30
/* V_FillRect: The usual solid fill.  Rectangle size passed directly.  */
#define HARDDOOM_CMD_TYPE_FILL_RECT		0x31
/* V_DrawLine: The usual solid line. */
#define HARDDOOM_CMD_TYPE_DRAW_LINE		0x32
/* V_DrawBackground: Fill whole FB with repeated flat. */
#define HARDDOOM_CMD_TYPE_DRAW_BACKGROUND	0x33
/* R_DrawColumn. */
#define HARDDOOM_CMD_TYPE_DRAW_COLUMN		0x34
/* R_DrawSpan. */
#define HARDDOOM_CMD_TYPE_DRAW_SPAN		0x35

/* Set the sync counter.  */
#define HARDDOOM_CMD_TYPE_FENCE			0x3c
/* Trigger an interrupt once we're done with current work.  */
#define HARDDOOM_CMD_TYPE_PING_SYNC		0x3d
/* Trigger an interrupt nowr.  */
#define HARDDOOM_CMD_TYPE_PING_ASYNC		0x3e
/* Block further surface reads until inflight surface writes are complete.  */
#define HARDDOOM_CMD_TYPE_INTERLOCK		0x3f

#define HARDDOOM_DRAW_PARAMS_FUZZ		0x1
#define HARDDOOM_DRAW_PARAMS_TRANSLATE		0x2
#define HARDDOOM_DRAW_PARAMS_COLORMAP		0x4

#define HARDDOOM_CMD_JUMP(addr)			(HARDDOOM_CMD_TYPE_HI_JUMP << 30 | (addr) >> 2)
#define HARDDOOM_CMD_COPY_RECT(w, h)		(HARDDOOM_CMD_TYPE_COPY_RECT << 26 | (h) << 12 | (w))
#define HARDDOOM_CMD_FILL_RECT(w, h)		(HARDDOOM_CMD_TYPE_FILL_RECT << 26 | (h) << 12 | (w))
#define HARDDOOM_CMD_DRAW_LINE			(HARDDOOM_CMD_TYPE_DRAW_LINE << 26)
#define HARDDOOM_CMD_DRAW_BACKGROUND		(HARDDOOM_CMD_TYPE_DRAW_BACKGROUND << 26)
#define HARDDOOM_CMD_DRAW_COLUMN(iscale)	(HARDDOOM_CMD_TYPE_DRAW_COLUMN << 26 | (iscale))
#define HARDDOOM_CMD_DRAW_SPAN			(HARDDOOM_CMD_TYPE_DRAW_SPAN << 26)
#define HARDDOOM_CMD_SURF_DST_PT(addr)		(HARDDOOM_CMD_TYPE_SURF_DST_PT << 26 | (addr) >> 6)
#define HARDDOOM_CMD_SURF_SRC_PT(addr)		(HARDDOOM_CMD_TYPE_SURF_SRC_PT << 26 | (addr) >> 6)
#define HARDDOOM_CMD_TEXTURE_PT(addr)		(HARDDOOM_CMD_TYPE_TEXTURE_PT << 26 | (addr) >> 6)
#define HARDDOOM_CMD_FLAT_ADDR(addr)		(HARDDOOM_CMD_TYPE_FLAT_ADDR << 26 | (addr) >> 12)
#define HARDDOOM_CMD_COLORMAP_ADDR(addr)	(HARDDOOM_CMD_TYPE_COLORMAP_ADDR << 26 | (addr) >> 8)
#define HARDDOOM_CMD_TRANSLATION_ADDR(addr)	(HARDDOOM_CMD_TYPE_TRANSLATION_ADDR << 26 | (addr) >> 8)
#define HARDDOOM_CMD_SURF_DIMS(w, h)		(HARDDOOM_CMD_TYPE_SURF_DIMS << 26 | (w) >> 6 | (h) << 8)
#define HARDDOOM_CMD_TEXTURE_DIMS(sz, h)	(HARDDOOM_CMD_TYPE_TEXTURE_DIMS << 26 | ((sz) - 1) >> 8 << 12 | (h))
#define HARDDOOM_CMD_FILL_COLOR(color)		(HARDDOOM_CMD_TYPE_FILL_COLOR << 26 | (color))
#define HARDDOOM_CMD_DRAW_PARAMS(flags	)	(HARDDOOM_CMD_TYPE_DRAW_PARAMS << 26 | (flags))
#define HARDDOOM_CMD_XY_A(x, y)			(HARDDOOM_CMD_TYPE_XY_A << 26 | (y) << 12 | (x))
#define HARDDOOM_CMD_XY_B(x, y)			(HARDDOOM_CMD_TYPE_XY_B << 26 | (y) << 12 | (x))
#define HARDDOOM_CMD_USTART(arg)		(HARDDOOM_CMD_TYPE_USTART << 26 | (arg))
#define HARDDOOM_CMD_VSTART(arg)		(HARDDOOM_CMD_TYPE_VSTART << 26 | (arg))
#define HARDDOOM_CMD_USTEP(arg)			(HARDDOOM_CMD_TYPE_USTEP << 26 | (arg))
#define HARDDOOM_CMD_VSTEP(arg)			(HARDDOOM_CMD_TYPE_VSTEP << 26 | (arg))
#define HARDDOOM_CMD_FENCE(arg)			(HARDDOOM_CMD_TYPE_FENCE << 26 | (arg))
#define HARDDOOM_CMD_PING_SYNC			(HARDDOOM_CMD_TYPE_PING_SYNC << 26)
#define HARDDOOM_CMD_PING_ASYNC			(HARDDOOM_CMD_TYPE_PING_ASYNC << 26)
#define HARDDOOM_CMD_INTERLOCK			(HARDDOOM_CMD_TYPE_INTERLOCK << 26)

#define HARDDOOM_CMD_EXTR_TYPE_HI(cmd)		((cmd) >> 30 & 0x3)
#define HARDDOOM_CMD_EXTR_JUMP_ADDR(cmd)	((cmd) << 2 & 0xfffffffc)
#define HARDDOOM_CMD_EXTR_TYPE(cmd)		((cmd) >> 26 & 0x3f)
#define HARDDOOM_CMD_EXTR_RECT_WIDTH(cmd)	((cmd) & 0xfff)
#define HARDDOOM_CMD_EXTR_RECT_HEIGHT(cmd)	((cmd) >> 12 & 0xfff)
#define HARDDOOM_CMD_EXTR_COLUMN_OFFSET(cmd)	((cmd) & 0x3fffff)
#define HARDDOOM_CMD_EXTR_PT(cmd)		(((cmd) & 0x3ffffff) << 6)
#define HARDDOOM_CMD_EXTR_SURF_DIMS_WIDTH(cmd)	(((cmd) & 0x3f) << 6)
#define HARDDOOM_CMD_EXTR_SURF_DIMS_HEIGHT(cmd)	(((cmd) & 0xfff00) >> 8)
#define HARDDOOM_CMD_EXTR_TEXTURE_SIZE(cmd)	((((cmd) >> 12 & 0x3fff) + 1) << 8)
#define HARDDOOM_CMD_EXTR_TEXTURE_HEIGHT(cmd)	((cmd) & 0x3ff)
#define HARDDOOM_CMD_EXTR_FLAT_ADDR(cmd)	(((cmd) & 0xfffff) << 12)
#define HARDDOOM_CMD_EXTR_COLORMAP_ADDR(cmd)	(((cmd) & 0xffffff) << 8)
#define HARDDOOM_CMD_EXTR_FILL_COLOR(cmd)	((cmd) & 0xff)
#define HARDDOOM_CMD_EXTR_XY_X(cmd)		((cmd) & 0x7ff)
#define HARDDOOM_CMD_EXTR_XY_Y(cmd)		((cmd) >> 12 & 0x7ff)
#define HARDDOOM_CMD_EXTR_TEX_COORD(cmd)	((cmd) & 0x3ffffff)
#define HARDDOOM_CMD_EXTR_FENCE(cmd)		((cmd) & 0x3ffffff)


/* XY unit internal commands.  */

#define HARDDOOM_XYCMD_PARAMS_MASK		0x0fffffff
#define HARDDOOM_XYCMD_TYPE_SHIFT		28
/* These 3 correspond directly to the FE commands.  */
#define HARDDOOM_XYCMD_TYPE_SURF_DST_PT		0x1
#define HARDDOOM_XYCMD_TYPE_SURF_SRC_PT		0x2
#define HARDDOOM_XYCMD_TYPE_SURF_DIMS		0x3
/* Emits a series of addresses for a horizontal write to DST.  */
#define HARDDOOM_XYCMD_TYPE_WRITE_DST_H		0x4
/* Emits a series of addresses for a vertical write to DST.  */
#define HARDDOOM_XYCMD_TYPE_WRITE_DST_V		0x5
/* Emits a series of addresses for a horizontal read from SRC.  */
#define HARDDOOM_XYCMD_TYPE_READ_SRC_H		0x6
/* Emits a series of addresses for a vertical read from SRC.  */
#define HARDDOOM_XYCMD_TYPE_READ_SRC_V		0x7
/* Emits a series of addresses for a vertical read from DST.  */
#define HARDDOOM_XYCMD_TYPE_READ_DST_V		0x8
/* Emits a series of addresses for a vertical read and write to DST (sending
 * the same address stream to both SR and SW).  */
#define HARDDOOM_XYCMD_TYPE_RMW_DST_V		0x9
/* One half of the interlock -- makes XY wait for the interlock signal from SW.  */
#define HARDDOOM_XYCMD_TYPE_INTERLOCK		0xa

#define HARDDOOM_XYCMD_WRITE_DST_H(x, y, w)	(HARDDOOM_XYCMD_TYPE_WRITE_DST_H << 28 | ((w) & 0x3f) << 16 | ((y) & 0x7ff) << 5 | ((x) & 0x1f))
#define HARDDOOM_XYCMD_WRITE_DST_V(x, y, h)	(HARDDOOM_XYCMD_TYPE_WRITE_DST_V << 28 | ((h) & 0xfff) << 16 | ((y) & 0x7ff) << 5 | ((x) & 0x1f))
#define HARDDOOM_XYCMD_READ_DST_V(x, y, h)	(HARDDOOM_XYCMD_TYPE_READ_DST_V << 28 | ((h) & 0xfff) << 16 | ((y) & 0x7ff) << 5 | ((x) & 0x1f))
#define HARDDOOM_XYCMD_RMW_DST_V(x, y, h)	(HARDDOOM_XYCMD_TYPE_RMW_DST_V << 28 | ((h) & 0xfff) << 16 | ((y) & 0x7ff) << 5 | ((x) & 0x1f))
#define HARDDOOM_XYCMD_READ_SRC_H(x, y, w)	(HARDDOOM_XYCMD_TYPE_READ_SRC_H << 28 | ((w) & 0x3f) << 16 | ((y) & 0x7ff) << 5 | ((x) & 0x1f))
#define HARDDOOM_XYCMD_READ_SRC_V(x, y, h)	(HARDDOOM_XYCMD_TYPE_READ_SRC_V << 28 | ((h) & 0xfff) << 16 | ((y) & 0x7ff) << 5 | ((x) & 0x1f))

#define HARDDOOM_XYCMD_EXTR_TYPE(cmd)		((cmd) >> HARDDOOM_XYCMD_TYPE_SHIFT)
/* For all READ/WRITE/RMW commands -- start X coordinate in blocks.  */
#define HARDDOOM_XYCMD_EXTR_X(cmd)		((cmd) & 0x1f)
/* For all READ/WRITE/RMW commands -- start Y coordinate.  */
#define HARDDOOM_XYCMD_EXTR_Y(cmd)		((cmd) >> 5 & 0x7ff)
/* For horizontal READ/WRITE commands -- width in blocks.  */
#define HARDDOOM_XYCMD_EXTR_WIDTH(cmd)		((cmd) >> 16 & 0x3f)
/* For vertical READ/WRITE commands -- height.  */
#define HARDDOOM_XYCMD_EXTR_HEIGHT(cmd)		((cmd) >> 16 & 0xfff)


/* TEX unit internal commands.  */

#define HARDDOOM_TEXCMD_PARAMS_MASK		0x0fffffff
#define HARDDOOM_TEXCMD_TYPE_SHIFT		28
/* These correspond directly to the FE commands.  */
#define HARDDOOM_TEXCMD_TYPE_TEXTURE_PT		0x1
#define HARDDOOM_TEXCMD_TYPE_TEXTURE_DIMS	0x2
#define HARDDOOM_TEXCMD_TYPE_USTART		0x3
#define HARDDOOM_TEXCMD_TYPE_USTEP		0x4
/* Enables given column slot, sets its texture offset, copies the last
 * submitted USTEP value, and initializes current coordinate to the last
 * submitted USTART value.  */
#define HARDDOOM_TEXCMD_TYPE_START_COLUMN	0x5
/* Disables given column slot.  */
#define HARDDOOM_TEXCMD_TYPE_END_COLUMN		0x6
/* Emits a given number of blocks to OG.  The X parameter should always be 0
 * -- it is incremented internally by TEX afterwards, to keep track of which
 * pixels of a block have already been textured.  */
#define HARDDOOM_TEXCMD_TYPE_DRAW_TEX		0x7

#define HARDDOOM_TEXCMD_TEXTURE_PT(addr)	(HARDDOOM_TEXCMD_TYPE_TEXTURE_PT << 28 | (addr) >> 6)
#define HARDDOOM_TEXCMD_TEXTURE_DIMS(sz, h)	(HARDDOOM_TEXCMD_TYPE_TEXTURE_DIMS << 28 | ((sz) - 1) >> 8 << 12 | (h))
#define HARDDOOM_TEXCMD_USTART(arg)		(HARDDOOM_TEXCMD_TYPE_USTART << 28 | (arg))
#define HARDDOOM_TEXCMD_USTEP(arg)		(HARDDOOM_TEXCMD_TYPE_USTEP << 28 | (arg))
#define HARDDOOM_TEXCMD_START_COLUMN(x, offset)	(HARDDOOM_TEXCMD_TYPE_START_COLUMN << 28 | (offset) | (x) << 22)
#define HARDDOOM_TEXCMD_END_COLUMN(x)		(HARDDOOM_TEXCMD_TYPE_END_COLUMN << 28 | (x))
#define HARDDOOM_TEXCMD_DRAW_TEX(h, x)		(HARDDOOM_TEXCMD_TYPE_DRAW_TEX << 28 | (h) | (x) << 12)

#define HARDDOOM_TEXCMD_EXTR_TYPE(cmd)		((cmd) >> HARDDOOM_TEXCMD_TYPE_SHIFT)
#define HARDDOOM_TEXCMD_EXTR_START_X(cmd)	((cmd) >> 22 & 0x3f)
#define HARDDOOM_TEXCMD_EXTR_END_X(cmd)		((cmd) & 0x3f)
#define HARDDOOM_TEXCMD_EXTR_DRAW_X(cmd)	((cmd) >> 12 & 0x3f)
#define HARDDOOM_TEXCMD_EXTR_DRAW_HEIGHT(cmd)	((cmd) & 0xfff)


/* FLAT unit internal commands.  */

#define HARDDOOM_FLCMD_PARAMS_MASK		0x0fffffff
#define HARDDOOM_FLCMD_TYPE_SHIFT		28
/* Like the FE command.  */
#define HARDDOOM_FLCMD_TYPE_FLAT_ADDR		0x1
/* Like the FE commands, but will be destroyed by rendering (need to be re-submitted).  */
#define HARDDOOM_FLCMD_TYPE_USTART		0x2
#define HARDDOOM_FLCMD_TYPE_VSTART		0x3
/* Like the FE commands.  */
#define HARDDOOM_FLCMD_TYPE_USTEP		0x4
#define HARDDOOM_FLCMD_TYPE_VSTEP		0x5
/* Reads N lines of a flat in a loop, starting from the given Y coordinate.  */
#define HARDDOOM_FLCMD_TYPE_READ_FLAT		0x6
/* Emits a given number of textured pixels to the OG.  The X parameter should
 * always be 0 (it is incremented internally by the FLAT unit to keep track
 * of how many pixels of a block have been textured so far).  */
#define HARDDOOM_FLCMD_TYPE_DRAW_SPAN		0x7

#define HARDDOOM_FLCMD_FLAT_ADDR(addr)		(HARDDOOM_FLCMD_TYPE_FLAT_ADDR << 28 | (addr) >> 12)
#define HARDDOOM_FLCMD_USTART(arg)		(HARDDOOM_FLCMD_TYPE_USTART << 28 | (arg))
#define HARDDOOM_FLCMD_VSTART(arg)		(HARDDOOM_FLCMD_TYPE_VSTART << 28 | (arg))
#define HARDDOOM_FLCMD_USTEP(arg)		(HARDDOOM_FLCMD_TYPE_USTEP << 28 | (arg))
#define HARDDOOM_FLCMD_VSTEP(arg)		(HARDDOOM_FLCMD_TYPE_VSTEP << 28 | (arg))
#define HARDDOOM_FLCMD_READ_FLAT(h, y)		(HARDDOOM_FLCMD_TYPE_READ_FLAT << 28 | (h) | (y) << 12)
#define HARDDOOM_FLCMD_DRAW_SPAN(w, x)		(HARDDOOM_FLCMD_TYPE_DRAW_SPAN << 28 | (w) | (x) << 12)

#define HARDDOOM_FLCMD_EXTR_TYPE(cmd)		((cmd) >> HARDDOOM_FLCMD_TYPE_SHIFT)
#define HARDDOOM_FLCMD_EXTR_COORD(cmd)		((cmd) & 0x3fffff)
#define HARDDOOM_FLCMD_EXTR_READ_HEIGHT(cmd)	((cmd) & 0xfff)
#define HARDDOOM_FLCMD_EXTR_READ_Y(cmd)		((cmd) >> 12 & 0x3f)
#define HARDDOOM_FLCMD_EXTR_DRAW_WIDTH(cmd)	((cmd) & 0xfff)
#define HARDDOOM_FLCMD_EXTR_DRAW_X(cmd)		((cmd) >> 12 & 0x3f)


/* FUZZ unit internal commands.  */

#define HARDDOOM_FZCMD_PARAMS_MASK		0x0fffffff
#define HARDDOOM_FZCMD_TYPE_SHIFT		28
/* Sets the fuzz position for a given column.  */
#define HARDDOOM_FZCMD_TYPE_SET_COLUMN		0x1
/* Emits a given number of block masks to the OG.  */
#define HARDDOOM_FZCMD_TYPE_DRAW_FUZZ		0x2

#define HARDDOOM_FZCMD_SET_COLUMN(x, p)		(HARDDOOM_FZCMD_TYPE_SET_COLUMN << 28 | (x) << 6 | (p))
#define HARDDOOM_FZCMD_DRAW_FUZZ(h)		(HARDDOOM_FZCMD_TYPE_DRAW_FUZZ << 28 | (h))

#define HARDDOOM_FZCMD_EXTR_TYPE(cmd)		((cmd) >> HARDDOOM_FZCMD_TYPE_SHIFT)
#define HARDDOOM_FZCMD_EXTR_X(cmd)		((cmd) >> 6 & 0x3f)
#define HARDDOOM_FZCMD_EXTR_POS(cmd)		((cmd) & 0x3f)
#define HARDDOOM_FZCMD_EXTR_HEIGHT(cmd)		((cmd) & 0xfff)


/* OG unit internal commands.  */

#define HARDDOOM_OGCMD_PARAMS_MASK		0x03ffffff
#define HARDDOOM_OGCMD_TYPE_SHIFT		26
/* Tells SW to send the interlock signal on SW2XY.  */
#define HARDDOOM_OGCMD_TYPE_INTERLOCK		0x01
/* Like the FE command, passed to SW.  */
#define HARDDOOM_OGCMD_TYPE_FENCE		0x02
/* Like the FE PING_SYNC command, passed to SW.  */
#define HARDDOOM_OGCMD_TYPE_PING		0x03
/* Fills the buffer with a solid color.  */
#define HARDDOOM_OGCMD_TYPE_FILL_COLOR		0x04
/* Fetches the color map from the given address.  */
#define HARDDOOM_OGCMD_TYPE_COLORMAP_ADDR	0x05
/* Fetches the translation map from the given address.  */
#define HARDDOOM_OGCMD_TYPE_TRANSLATION_ADDR	0x06
/* Draws a horizontal stripe of pixels from buffer.  */
#define HARDDOOM_OGCMD_TYPE_DRAW_BUF_H		0x07
/* Draws a vertical stripe of pixels from buffer.  */
#define HARDDOOM_OGCMD_TYPE_DRAW_BUF_V		0x08
/* Draws a horizontal stripe of pixels from SR.  */
#define HARDDOOM_OGCMD_TYPE_COPY_H		0x09
/* Draws a vertical stripe of pixels from SR.  */
#define HARDDOOM_OGCMD_TYPE_COPY_V		0x0a
/* Sets the SR read offset.  */
#define HARDDOOM_OGCMD_TYPE_SRC_OFFSET		0x0b
/* Reads a block from FLAT to the buffer  */
#define HARDDOOM_OGCMD_TYPE_READ_FLAT		0x0c
/* Draws a horizontal stripe of pixels from FLAT.  */
#define HARDDOOM_OGCMD_TYPE_DRAW_SPAN		0x0d
/* Reads pixels from SR, applies FUZZ effect, draws them.  */
#define HARDDOOM_OGCMD_TYPE_DRAW_FUZZ		0x0e
/* Prepares for the above.  */
#define HARDDOOM_OGCMD_TYPE_INIT_FUZZ		0x0f
/* Flips the given column in FUZZ draw mask.  */
#define HARDDOOM_OGCMD_TYPE_FUZZ_COLUMN		0x10
/* Draws blocks straight from TEX.  */
#define HARDDOOM_OGCMD_TYPE_DRAW_TEX		0x11

#define HARDDOOM_OGCMD_INTERLOCK		(HARDDOOM_OGCMD_TYPE_INTERLOCK << 26)
#define HARDDOOM_OGCMD_FENCE(f)			(HARDDOOM_OGCMD_TYPE_FENCE << 26 | (f))
#define HARDDOOM_OGCMD_PING			(HARDDOOM_OGCMD_TYPE_PING << 26)
#define HARDDOOM_OGCMD_FILL_COLOR(color)	(HARDDOOM_OGCMD_TYPE_FILL_COLOR << 26 | (color))
#define HARDDOOM_OGCMD_COLORMAP_ADDR(addr)	(HARDDOOM_OGCMD_TYPE_COLORMAP_ADDR << 26 | (addr) >> 8)
#define HARDDOOM_OGCMD_TRANSLATION_ADDR(addr)	(HARDDOOM_OGCMD_TYPE_TRANSLATION_ADDR << 26 | (addr) >> 8)
#define HARDDOOM_OGCMD_DRAW_BUF_H(x, w)		(HARDDOOM_OGCMD_TYPE_DRAW_BUF_H << 26 | (x) | (w) << 6)
#define HARDDOOM_OGCMD_DRAW_BUF_V(x, w, h)	(HARDDOOM_OGCMD_TYPE_DRAW_BUF_V << 26 | (x) | (w) << 6 | (h) << 12)
#define HARDDOOM_OGCMD_COPY_H(x, w)		(HARDDOOM_OGCMD_TYPE_COPY_H << 26 | (x) | (w) << 6)
#define HARDDOOM_OGCMD_COPY_V(x, w, h)		(HARDDOOM_OGCMD_TYPE_COPY_V << 26 | (x) | (w) << 6 | (h) << 12)
#define HARDDOOM_OGCMD_SRC_OFFSET(x)		(HARDDOOM_OGCMD_TYPE_SRC_OFFSET << 26 | (x))
#define HARDDOOM_OGCMD_READ_FLAT		(HARDDOOM_OGCMD_TYPE_READ_FLAT << 26)
#define HARDDOOM_OGCMD_DRAW_SPAN(x, w, f)	(HARDDOOM_OGCMD_TYPE_DRAW_SPAN << 26 | (x) | (w) << 6 | (f) << 24)
#define HARDDOOM_OGCMD_DRAW_FUZZ(h, f)		(HARDDOOM_OGCMD_TYPE_DRAW_FUZZ << 26 | (h) | (f) << 24)
#define HARDDOOM_OGCMD_INIT_FUZZ		(HARDDOOM_OGCMD_TYPE_INIT_FUZZ << 26)
#define HARDDOOM_OGCMD_FUZZ_COLUMN(x)		(HARDDOOM_OGCMD_TYPE_FUZZ_COLUMN << 26 | (x))
#define HARDDOOM_OGCMD_DRAW_TEX(h, f)		(HARDDOOM_OGCMD_TYPE_DRAW_TEX << 26 | (h) | (f) << 24)

#define HARDDOOM_OGCMD_EXTR_TYPE(cmd)		((cmd) >> HARDDOOM_OGCMD_TYPE_SHIFT)
#define HARDDOOM_OGCMD_EXTR_X(cmd)		((cmd) & 0x3f)
#define HARDDOOM_OGCMD_EXTR_H_WIDTH(cmd)	((cmd) >> 6 & 0xfff)
#define HARDDOOM_OGCMD_EXTR_V_WIDTH(cmd)	((cmd) >> 6 & 0x3f)
#define HARDDOOM_OGCMD_EXTR_V_HEIGHT(cmd)	((cmd) >> 12 & 0xfff)
#define HARDDOOM_OGCMD_EXTR_TF_HEIGHT(cmd)	((cmd) & 0xfff)
#define HARDDOOM_OGCMD_EXTR_FLAGS(cmd)		((cmd) >> 24 & 3)

#define HARDDOOM_OGCMD_FLAG_TRANSLATE		1
#define HARDDOOM_OGCMD_FLAG_COLORMAP		2


/* SW unit internal commands.  */

#define HARDDOOM_SWCMD_PARAMS_MASK		0x03ffffff
#define HARDDOOM_SWCMD_TYPE_SHIFT		26
/* Will send the interlock signal on SW2XY.  */
#define HARDDOOM_SWCMD_TYPE_INTERLOCK		0x1
/* Like the FE command.  */
#define HARDDOOM_SWCMD_TYPE_FENCE		0x2
/* Like the FE PING_SYNC command.  */
#define HARDDOOM_SWCMD_TYPE_PING		0x3
/* Draws a given number of blocks.  */
#define HARDDOOM_SWCMD_TYPE_DRAW		0x4

#define HARDDOOM_SWCMD_INTERLOCK		(HARDDOOM_SWCMD_TYPE_INTERLOCK << 26)
#define HARDDOOM_SWCMD_FENCE(f)			(HARDDOOM_SWCMD_TYPE_FENCE << 26 | (f))
#define HARDDOOM_SWCMD_PING			(HARDDOOM_SWCMD_TYPE_PING << 26)
#define HARDDOOM_SWCMD_DRAW(b)			(HARDDOOM_SWCMD_TYPE_DRAW << 26 | (b))

#define HARDDOOM_SWCMD_EXTR_TYPE(cmd)		((cmd) >> HARDDOOM_SWCMD_TYPE_SHIFT)
#define HARDDOOM_SWCMD_EXTR_BLOCKS(cmd)		((cmd) & 0xfff)


/* Page tables */

#define HARDDOOM_PTE_VALID			0x00000001
#define HARDDOOM_PTE_PHYS_MASK			0xfffff000
#define HARDDOOM_PAGE_SHIFT			12
#define HARDDOOM_PAGE_SIZE			0x1000

/* Misc */

#define HARDDOOM_COORD_MASK			0x7ff
#define HARDDOOM_COLORMAP_SIZE			0x100
#define HARDDOOM_FLAT_SIZE			0x1000
/* The block size in pixels / columns -- the source and destination surfaces
 * are written in blocks of this many pixels, and texture/fuzz column batches
 * are made of this many columns.  Also happens to be the width of a flat.  */
#define HARDDOOM_BLOCK_SIZE			0x40

#endif
