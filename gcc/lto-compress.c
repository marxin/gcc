/* LTO IL compression streams.

   Copyright (C) 2009-2019 Free Software Foundation, Inc.
   Contributed by Simon Baldwin <simonb@google.com>

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3, or (at your option)
any later version.

GCC is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
License for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING3.  If not see
<http://www.gnu.org/licenses/>.  */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "tree.h"
#include "gimple.h"
#include "cgraph.h"
#include "lto-streamer.h"
#include "lto-compress.h"
#include "timevar.h"
#include <zstd.h>

/* Compression stream structure, holds the flush callback and opaque token,
   the buffered data, and a note of whether compressing or uncompressing.  */

struct lto_compression_stream
{
  void (*callback) (const char *, unsigned, void *);
  void *opaque;
  char *buffer;
  size_t bytes;
  size_t allocation;
  bool is_compression;
};

static const size_t MIN_STREAM_ALLOCATION = 1024;

/* Return a zstd compression level that zstd will not reject.  Normalizes
   the compression level from the command line flag, clamping non-default
   values to the appropriate end of their valid range.  */

static int
lto_normalized_zstd_level (void)
{
  int level = flag_lto_compression_level;

  if (level != ZSTD_CLEVEL_DEFAULT)
    {
      if (level < 1)
	level = 1;
      else if (level > ZSTD_maxCLevel ())
	level = ZSTD_maxCLevel ();
    }

  return level;
}

/* Create a new compression stream, with CALLBACK flush function passed
   OPAQUE token, IS_COMPRESSION indicates if compressing or uncompressing.  */

static struct lto_compression_stream *
lto_new_compression_stream (void (*callback) (const char *, unsigned, void *),
			    void *opaque, bool is_compression)
{
  struct lto_compression_stream *stream
    = (struct lto_compression_stream *) xmalloc (sizeof (*stream));

  memset (stream, 0, sizeof (*stream));
  stream->callback = callback;
  stream->opaque = opaque;
  stream->is_compression = is_compression;

  return stream;
}

/* Append NUM_CHARS from address BASE to STREAM.  */

static void
lto_append_to_compression_stream (struct lto_compression_stream *stream,
				  const char *base, size_t num_chars)
{
  size_t required = stream->bytes + num_chars;

  if (stream->allocation < required)
    {
      if (stream->allocation == 0)
	stream->allocation = MIN_STREAM_ALLOCATION;
      while (stream->allocation < required)
	stream->allocation *= 2;

      stream->buffer = (char *) xrealloc (stream->buffer, stream->allocation);
    }

  memcpy (stream->buffer + stream->bytes, base, num_chars);
  stream->bytes += num_chars;
}

/* Free the buffer and memory associated with STREAM.  */

static void
lto_destroy_compression_stream (struct lto_compression_stream *stream)
{
  free (stream->buffer);
  free (stream);
}

/* Return a new compression stream, with CALLBACK flush function passed
   OPAQUE token.  */

struct lto_compression_stream *
lto_start_compression (void (*callback) (const char *, unsigned, void *),
		       void *opaque)
{
  return lto_new_compression_stream (callback, opaque, true);
}

/* Append NUM_CHARS from address BASE to STREAM.  */

void
lto_compress_block (struct lto_compression_stream *stream,
		    const char *base, size_t num_chars)
{
  gcc_assert (stream->is_compression);

  lto_append_to_compression_stream (stream, base, num_chars);
  lto_stats.num_output_il_bytes += num_chars;
}

/* Finalize STREAM compression, and free stream allocations.  */

void
lto_end_compression (struct lto_compression_stream *stream)
{
  unsigned char *cursor = (unsigned char *) stream->buffer;
  size_t size = stream->bytes;

  timevar_push (TV_IPA_LTO_COMPRESS);
  size_t const outbuf_length = ZSTD_compressBound (size);
  char *outbuf = (char *) xmalloc (outbuf_length);

  size_t const csize = ZSTD_compress (outbuf, outbuf_length, cursor, size,
				      lto_normalized_zstd_level ());

  if (ZSTD_isError (csize))
    internal_error ("compressed stream: %s", ZSTD_getErrorName (csize));

  stream->callback (outbuf, csize, NULL);

  lto_destroy_compression_stream (stream);
  free (outbuf);
  timevar_pop (TV_IPA_LTO_COMPRESS);
}

/* Return a new uncompression stream, with CALLBACK flush function passed
   OPAQUE token.  */

struct lto_compression_stream *
lto_start_uncompression (void (*callback) (const char *, unsigned, void *),
			 void *opaque)
{
  return lto_new_compression_stream (callback, opaque, false);
}

/* Append NUM_CHARS from address BASE to STREAM.  */

void
lto_uncompress_block (struct lto_compression_stream *stream,
		      const char *base, size_t num_chars)
{
  gcc_assert (!stream->is_compression);

  lto_append_to_compression_stream (stream, base, num_chars);
  lto_stats.num_input_il_bytes += num_chars;
}

/* Finalize STREAM uncompression, and free stream allocations.

   Because of the way LTO IL streams are compressed, there may be several
   concatenated compressed segments in the accumulated data, so for this
   function we iterate decompressions until no data remains.  */

void
lto_end_uncompression (struct lto_compression_stream *stream)
{
  unsigned char *cursor = (unsigned char *) stream->buffer;
  size_t size = stream->bytes;

  timevar_push (TV_IPA_LTO_DECOMPRESS);
  unsigned long long const rsize = ZSTD_getFrameContentSize (cursor, size);
  if (rsize == ZSTD_CONTENTSIZE_ERROR)
    internal_error ("not compressed by zstd");
  else if (rsize == ZSTD_CONTENTSIZE_UNKNOWN)
    internal_error ("original size unknown");

  char *outbuf = (char *) xmalloc (rsize);
  size_t const dsize = ZSTD_decompress (outbuf, rsize, cursor, size);

  if (ZSTD_isError (dsize))
    internal_error ("decompressed stream: %s", ZSTD_getErrorName (dsize));

  stream->callback (outbuf, dsize, stream->opaque);

  lto_destroy_compression_stream (stream);
  free (outbuf);
  timevar_pop (TV_IPA_LTO_DECOMPRESS);
}
