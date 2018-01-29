/*
 hap.c
 
 Copyright (c) 2011-2013, Tom Butterworth and Vidvox LLC. All rights reserved.
 
 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:
 
 * Redistributions of source code must retain the above copyright
 notice, this list of conditions and the following disclaimer.
 
 * Redistributions in binary form must reproduce the above copyright
 notice, this list of conditions and the following disclaimer in the
 documentation and/or other materials provided with the distribution.
 
 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "hap.h"
#include "hap_old_images.h"
#include <stdlib.h>
#include <stdint.h>

/*
 IMPORTANT: This file decodes a legacy format which never became official. You shouldn't have to use it.
 */

/*
 Hap Constants
 First four bits represent the compressor
 */
#define kHapCompressorComplex 0xC

/*
 Hap Frame Section Types
 */
#define kHapSectionMultipleImages 0x0D
#define kHapSectionDecodeInstructionsContainer 0x01
#define kHapSectionChunkSecondStageCompressorTable 0x02
#define kHapSectionChunkSizeTable 0x03
#define kHapSectionChunkOffsetTable 0x04
#define kHapSectionFrameDimensions 0x05

// These read little-endian values on big or little-endian architectures
static unsigned int hap_old_read_3_byte_uint(const void *buffer)
{
    return (*(uint8_t *)buffer) + ((*(((uint8_t *)buffer) + 1)) << 8) + ((*(((uint8_t *)buffer) + 2)) << 16);
}

static unsigned int hap_old_read_4_byte_uint(const void *buffer)
{
    return (*(uint8_t *)buffer) + ((*(((uint8_t *)buffer) + 1)) << 8) + ((*(((uint8_t *)buffer) + 2)) << 16) + ((*(((uint8_t *)buffer) + 3)) << 24);
}

#define hap_old_top_4_bits(x) (((x) & 0xF0) >> 4)

static int hap_old_read_section_header(const void *buffer, uint32_t buffer_length, uint32_t *out_header_length, uint32_t *out_section_length, unsigned int *out_section_type)
{
    /*
     Verify buffer is big enough to contain a four-byte header
     */
    if (buffer_length < 4U)
    {
        return HapResult_Bad_Frame;
    }

    /*
     The first three bytes are the length of the section (not including the header) or zero
     if the length is stored in the last four bytes of an eight-byte header
     */
    *out_section_length = hap_old_read_3_byte_uint(buffer);

    /*
     If the first three bytes are zero, the size is in the following four bytes
     */
    if (*out_section_length == 0U)
    {
        /*
         Verify buffer is big enough to contain an eight-byte header
         */
        if (buffer_length < 8U)
        {
            return HapResult_Bad_Frame;
        }
        *out_section_length = hap_old_read_4_byte_uint(((uint8_t *)buffer) + 4U);
        *out_header_length = 8U;
    }
    else
    {
        *out_header_length = 4U;
    }

    /*
     The fourth byte stores the section type
     */
    *out_section_type = *(((uint8_t *)buffer) + 3U);
    
    /*
     Verify the section does not extend beyond the buffer
     */
    if (*out_header_length + *out_section_length > buffer_length)
    {
        return HapResult_Bad_Frame;
    }

    return HapResult_No_Error;
}

// Parses the Decode Instructions Container inside a Complex section and returns a HapResult error code
static unsigned int hap_old_parse_complex(const void *input_buffer, size_t input_buffer_bytes,
                                      int *chunk_count,
                                      const void **compressors_table,
                                      const void **chunk_sizes_table,
                                      const void **chunk_offsets_table,
                                      const void **frame_dimensions,
                                      const char **texture_data)
{
    uint32_t section_header_length;
    uint32_t section_length;
    unsigned int section_type;
    unsigned int result;

    *chunk_count = 0;
    *compressors_table = NULL;
    *chunk_sizes_table = NULL;
    *chunk_offsets_table = NULL;
    *frame_dimensions = NULL;
    *texture_data = NULL;

    result = hap_old_read_section_header(input_buffer, input_buffer_bytes, &section_header_length, &section_length, &section_type);

    /*
     The top-level section should contain a Decode Instructions Container followed by frame data
     */
    if (result == HapResult_No_Error && section_type != kHapSectionDecodeInstructionsContainer)
    {
        result = HapResult_Bad_Frame;
    }

    if (result != HapResult_No_Error)
    {
        return result;
    }

    /*
     Frame data follows immediately after the Decode Instructions Container
     */
    *texture_data = ((const char *)input_buffer) + section_header_length + section_length;

    /*
     Now unpack the Decode Instructions Container
     */
    input_buffer = ((uint8_t *)input_buffer) + section_header_length;
    input_buffer_bytes = section_length;

    /*
     Step through the sections inside the Decode Instructions Container
     */
    while (input_buffer_bytes > 0) {
        unsigned int section_chunk_count = 0;

        result = hap_old_read_section_header(input_buffer, input_buffer_bytes, &section_header_length, &section_length, &section_type);
        if (result != HapResult_No_Error)
        {
            return result;
        }
        input_buffer = ((uint8_t *)input_buffer) + section_header_length;
        switch (section_type) {
            case kHapSectionChunkSecondStageCompressorTable:
                *compressors_table = input_buffer;
                section_chunk_count = section_length;
                break;
            case kHapSectionChunkSizeTable:
                *chunk_sizes_table = input_buffer;
                section_chunk_count = section_length / 4;
                break;
            case kHapSectionChunkOffsetTable:
                *chunk_offsets_table = input_buffer;
                section_chunk_count = section_length / 4;
                break;
            case kHapSectionFrameDimensions:
                *frame_dimensions = input_buffer;
                break;
            default:
                // Ignore unrecognized sections
                break;
        }

        /*
         If we calculated a chunk count and already have one, make sure they match
         */
        if (section_chunk_count != 0)
        {
            if (*chunk_count != 0 && section_chunk_count != *chunk_count)
            {
                return HapResult_Bad_Frame;
            }
            *chunk_count = section_chunk_count;
        }

        input_buffer = ((uint8_t *)input_buffer) + section_length;
        input_buffer_bytes -= section_header_length + section_length;
    }

    /*
     The Chunk Second-Stage Compressor Table and Chunk Size Table are required
     */
    if (*compressors_table == NULL || *chunk_sizes_table == NULL)
    {
        return HapResult_Bad_Frame;
    }
    return HapResult_No_Error;
}

static int hap_old_get_section_at_index(const void *input_buffer, uint32_t input_buffer_bytes,
                                    unsigned int index,
                                    const void **section, uint32_t *section_length, unsigned int *section_type)
{
    int result;
    uint32_t section_header_length;

    result = hap_old_read_section_header(input_buffer, input_buffer_bytes, &section_header_length, section_length, section_type);

    if (result != HapResult_No_Error)
    {
        return result;
    }

    if (*section_type == kHapSectionMultipleImages)
    {
        /*
         Step through until we find the section at index
         */
        size_t offset = 0;
        size_t top_section_length = *section_length;
        input_buffer = ((uint8_t *)input_buffer) + section_header_length;
        section_header_length = 0;
        *section_length = 0;
        for (int i = 0; i <= index; i++) {
            offset += section_header_length + *section_length;
            if (offset >= top_section_length)
            {
                return HapResult_Bad_Arguments;
            }
            result = hap_old_read_section_header(((uint8_t *)input_buffer) + offset,
                                             top_section_length - offset,
                                             &section_header_length,
                                             section_length,
                                             section_type);
            if (result != HapResult_No_Error)
            {
                return result;
            }
        }
        offset += section_header_length;
        *section = ((uint8_t *)input_buffer) + offset;
        return HapResult_No_Error;
    }
    else if (index == 0)
    {
        /*
         A single-texture frame with the texture as the top section.
         */
        *section = ((uint8_t *)input_buffer) + section_header_length;
        return HapResult_No_Error;
    }
    else
    {
        *section = NULL;
        *section_length = 0;
        *section_type = 0;
        return HapResult_Bad_Arguments;
    }
}

unsigned int HapOldGetFrameDimensions(const void *inputBuffer, unsigned long inputBufferBytes, unsigned int *outWidth, unsigned int *outHeight)
{
    unsigned int result = HapResult_No_Error;
    /*
     Check arguments
     */
    if (inputBuffer == NULL
        || outWidth == NULL
        || outHeight == NULL
        )
    {
        return HapResult_Bad_Arguments;
    }
    /*
     Zero return values in case we don't find any
     */
    *outWidth = *outHeight = 0;

    /*
     All textures in a frame are required to have the same dimensions, so return the first dimensions present.
     */
    unsigned int count;
    result = HapGetFrameTextureCount(inputBuffer, inputBufferBytes, &count);
    for (unsigned int i = 0; i < count && result == HapResult_No_Error; i++)
    {
        const void *section;
        uint32_t section_length;
        unsigned int section_type;
        /*
         Locate the section at the given index, which will either be the top-level section in a single texture image, or one of the
         sections inside a multi-image top-level section.
         */
        result = hap_old_get_section_at_index(inputBuffer, inputBufferBytes, i, &section, &section_length, &section_type);
        if (result == HapResult_No_Error)
        {
            /*
             What is the compressor for this section
             */
            unsigned int compressor = hap_old_top_4_bits(section_type);
            /*
             Only Complex compressor type can store dimensions
             */
            if (compressor == kHapCompressorComplex)
            {
                int chunk_count = 0;
                const void *compressors = NULL;
                const void *chunk_sizes = NULL;
                const void *chunk_offsets = NULL;
                const void *dimensions = NULL;
                const char *texture_data = NULL;

                result = hap_old_parse_complex(section, section_length, &chunk_count, &compressors, &chunk_sizes, &chunk_offsets, &dimensions, &texture_data);
                if (result == HapResult_No_Error && dimensions != NULL)
                {
                    *outWidth = hap_old_read_4_byte_uint(dimensions);
                    *outHeight = hap_old_read_4_byte_uint(((uint8_t *)dimensions) + 4);
                    return result;
                }
            }
        }
    }
    return result;
}
