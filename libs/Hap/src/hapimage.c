/*
 hapimage.c
 
 Copyright (c) 2011-2018, Tom Butterworth and Vidvox LLC. All rights reserved.
 
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

#include "hapimage.h"
#include <stdlib.h>
#include <stdint.h>

/*
 Hap Constants
 First four bits represent the compressor
 Second four bits represent the format
 */
#define kHapCompressorNone 0xA
#define kHapCompressorSnappy 0xB
#define kHapCompressorComplex 0xC

#define kHapFormatRGBDXT1 0xB
#define kHapFormatRGBADXT5 0xE
#define kHapFormatYCoCgDXT5 0xF
#define kHapFormatARGTC1 0x1

/*
 Hap Frame Section Types
 */
#define kHapSectionMultipleImages 0x0D
#define kHapSectionDimensions 0x05

// These read and write little-endian values on big or little-endian architectures
static unsigned int hapimage_read_3_byte_uint(const void *buffer)
{
    return (*(uint8_t *)buffer) + ((*(((uint8_t *)buffer) + 1)) << 8) + ((*(((uint8_t *)buffer) + 2)) << 16);
}

static void hapimage_write_3_byte_uint(void *buffer, unsigned int value)
{
    *(uint8_t *)buffer = value & 0xFF;
    *(((uint8_t *)buffer) + 1) = (value >> 8) & 0xFF;
    *(((uint8_t *)buffer) + 2) = (value >> 16) & 0xFF;
}

static unsigned int hapimage_read_4_byte_uint(const void *buffer)
{
    return (*(uint8_t *)buffer) + ((*(((uint8_t *)buffer) + 1)) << 8) + ((*(((uint8_t *)buffer) + 2)) << 16) + ((*(((uint8_t *)buffer) + 3)) << 24);
}

static void hapimage_write_4_byte_uint(const void *buffer, unsigned int value)
{
    *(uint8_t *)buffer = value & 0xFF;
    *(((uint8_t *)buffer) + 1) = (value >> 8) & 0xFF;
    *(((uint8_t *)buffer) + 2) = (value >> 16) & 0xFF;
    *(((uint8_t *)buffer) + 3) = (value >> 24) & 0xFF;
}

#define hapimage_top_4_bits(x) (((x) & 0xF0) >> 4)

#define hapimage_bottom_4_bits(x) ((x) & 0x0F)

static int hapimage_read_section_header(const void *buffer, uint32_t buffer_length, uint32_t *out_header_length, uint32_t *out_section_length, unsigned int *out_section_type)
{
    /*
     Verify buffer is big enough to contain a four-byte header
     */
    if (buffer_length < 4U)
    {
        return HapImageResult_Bad_Image;
    }

    /*
     The first three bytes are the length of the section (not including the header) or zero
     if the length is stored in the last four bytes of an eight-byte header
     */
    *out_section_length = hapimage_read_3_byte_uint(buffer);

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
            return HapImageResult_Bad_Image;
        }
        *out_section_length = hapimage_read_4_byte_uint(((uint8_t *)buffer) + 4U);
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
        return HapImageResult_Bad_Image;
    }

    return HapImageResult_No_Error;
}

static void hapimage_write_section_header(void *buffer, uint32_t section_length, unsigned int section_type)
{
    /*
     The first three bytes are the length of the section (not including the header) or zero
     if using an eight-byte header
     */
    hapimage_write_3_byte_uint(buffer, (unsigned int)section_length);
    
    /*
     The fourth byte stores the section type
     */
    *(((uint8_t *)buffer) + 3) = section_type;
}

static int hapimage_is_top_level_section(unsigned int section_type)
{
    if (section_type == kHapSectionMultipleImages)
    {
        return 1;
    }
    unsigned int pf = hapimage_bottom_4_bits(section_type);
    unsigned int ct = hapimage_top_4_bits(section_type);
    if ((pf == kHapFormatRGBDXT1 || pf == kHapFormatRGBADXT5 || pf == kHapFormatYCoCgDXT5 || pf == kHapFormatARGTC1) &&
        (ct == kHapCompressorNone || ct == kHapCompressorSnappy || ct == kHapCompressorComplex))
    {
        return 1;
    }
    return 0;
}

unsigned int HapImageRead(const void *inputBuffer, unsigned long inputBufferBytes,
                          unsigned int *width, unsigned int *height,
                          const void **frame, unsigned long *frameBytes)
{
    if (inputBufferBytes < 4U)
    {
        return HapImageResult_Buffer_Too_Small;
    }
    {
        // Check for the Hap Image signature
        const uint8_t *signature = inputBuffer;
        if (signature[0] != 0x88 || signature[1] != 0x48 || signature[2] != 0x61 || signature[3] != 0x70)
        {
            return HapImageResult_Bad_Image;
        }
        inputBuffer = signature + 4;
        inputBufferBytes -= 4;
    }
    *frame = NULL;
    void *dimensions = NULL;
    do {
        // Parse sections following the signature. Be prepared to skip any unexpected sections.
        uint32_t section_header_length;
        uint32_t section_length;
        unsigned int section_type;
        int result = hapimage_read_section_header(inputBuffer, inputBufferBytes, &section_header_length, &section_length, &section_type);
        if (result != HapImageResult_No_Error)
        {
            return result;
        }
        if (section_type == kHapSectionDimensions)
        {
            dimensions = ((uint8_t *)inputBuffer) + section_header_length;
        }
        else if (hapimage_is_top_level_section(section_type))
        {
            *frame = inputBuffer;
            *frameBytes = section_header_length + section_length;
        }
        inputBuffer = ((uint8_t *)inputBuffer) + section_header_length + section_length;
        inputBufferBytes -= section_header_length + section_length;
    } while ((*frame == NULL || dimensions == NULL) && inputBufferBytes > 0);
    if (!dimensions || !*frame)
    {
        return HapImageResult_Bad_Image;
    }
    *width = hapimage_read_4_byte_uint(dimensions);
    *height = hapimage_read_4_byte_uint(((uint8_t *)dimensions) + 4);
    return HapImageResult_No_Error;
}

unsigned int HapImageWrite(unsigned int width, unsigned int height,
                           void *outputBuffer, unsigned long outputBufferBytes,
                           unsigned long *outputBufferBytesUsed)
{
    if (outputBuffer == NULL || outputBufferBytesUsed == NULL)
    {
        return HapImageResult_Bad_Arguments;
    }
    if (outputBufferBytes < 16)
    {
        return HapImageResult_Buffer_Too_Small;
    }
    {
        // Start with Hap Image signature
        uint8_t *signature = outputBuffer;
        signature[0] = 0x88;
        signature[1] = 0x48;
        signature[2] = 0x61;
        signature[3] = 0x70;
        outputBuffer = signature + 4;
    }
    hapimage_write_section_header(outputBuffer, 8, kHapSectionDimensions);
    hapimage_write_4_byte_uint(((uint8_t *)outputBuffer) + 4, width);
    hapimage_write_4_byte_uint(((uint8_t *)outputBuffer) + 8, height);
    *outputBufferBytesUsed = 16;
    return HapImageResult_No_Error;
}
