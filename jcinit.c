/*
 * jcinit.c
 *
 * This file was part of the Independent JPEG Group's software:
 * Copyright (C) 1991-1997, Thomas G. Lane.
 * Lossless JPEG Modifications:
 * Copyright (C) 1999, Ken Murchison.
 * libjpeg-turbo Modifications:
 * Copyright (C) 2020, 2022, D. R. Commander.
 * For conditions of distribution and use, see the accompanying README.ijg
 * file.
 *
 * This file contains initialization logic for the JPEG compressor.
 * This routine is in charge of selecting the modules to be executed and
 * making an initialization call to each one.
 *
 * Logically, this code belongs in jcmaster.c.  It's split out because
 * linking this routine implies linking the entire compression library.
 * For a transcoding-only application, we want to be able to use jcmaster.c
 * without linking in the whole library.
 */

#define JPEG_INTERNALS
#include "jinclude.h"
#include "jpeglib.h"
#include "jpegapicomp.h"


/*
 * Master selection of compression modules.
 * This is done once at the start of processing an image.  We determine
 * which modules will be used and give them appropriate initialization calls.
 */

GLOBAL(void)
jinit_compress_master(j_compress_ptr cinfo)
{
  /* Initialize master control (includes parameter checking/processing) */
  jinit_c_master_control(cinfo, FALSE /* full compression */);

  if (cinfo->data_precision == 16) {
    ERREXIT1(cinfo, JERR_BAD_PRECISION, cinfo->data_precision);
  } else if (cinfo->data_precision == 12) {
    ERREXIT1(cinfo, JERR_BAD_PRECISION, cinfo->data_precision);
  }

  /* Preprocessing */
  if (!cinfo->raw_data_in) {
      jinit_color_converter(cinfo);
      jinit_downsampler(cinfo);
      jinit_c_prep_controller(cinfo, FALSE /* never need full buffer here */);
  }

  if (cinfo->master->lossless) {
#ifdef C_LOSSLESS_SUPPORTED
      jinit_lossless_compressor(cinfo);
    /* Entropy encoding: either Huffman or arithmetic coding. */
    if (cinfo->arith_code) {
      ERREXIT(cinfo, JERR_ARITH_NOTIMPL);
    } else {
      jinit_lhuff_encoder(cinfo);
    }

    /* Need a full-image difference buffer in any multi-pass mode. */
      jinit_c_diff_controller(cinfo, (boolean)(cinfo->num_scans > 1 ||
                                               cinfo->optimize_coding));
#else
    ERREXIT(cinfo, JERR_NOT_COMPILED);
#endif
  } else {
      jinit_forward_dct(cinfo);
    /* Entropy encoding: either Huffman or arithmetic coding. */
    if (cinfo->arith_code) {
#ifdef C_ARITH_CODING_SUPPORTED
      jinit_arith_encoder(cinfo);
#else
      ERREXIT(cinfo, JERR_ARITH_NOTIMPL);
#endif
    } else {
      if (cinfo->progressive_mode) {
#ifdef C_PROGRESSIVE_SUPPORTED
        jinit_phuff_encoder(cinfo);
#else
        ERREXIT(cinfo, JERR_NOT_COMPILED);
#endif
      } else
        jinit_huff_encoder(cinfo);
    }

    /* Need a full-image coefficient buffer in any multi-pass mode. */
      jinit_c_coef_controller(cinfo, (boolean)(cinfo->num_scans > 1 ||
                                               cinfo->optimize_coding));
  }

    jinit_c_main_controller(cinfo, FALSE /* never need full buffer here */);

  jinit_marker_writer(cinfo);

  /* We can now tell the memory manager to allocate virtual arrays. */
  (*cinfo->mem->realize_virt_arrays) ((j_common_ptr)cinfo);

  /* Write the datastream header (SOI) immediately.
   * Frame and scan headers are postponed till later.
   * This lets application insert special markers after the SOI.
   */
  (*cinfo->marker->write_file_header) (cinfo);
}
