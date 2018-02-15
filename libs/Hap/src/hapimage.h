/*
 hapimage.h
 
 Copyright (c) 2018, Tom Butterworth. All rights reserved.
 
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

#ifndef hapimage_h
#define hapimage_h

#ifdef __cplusplus
extern "C" {
#endif

enum HapImageResult {
    HapImageResult_No_Error = 0,
    HapImageResult_Bad_Arguments,
    HapImageResult_Buffer_Too_Small,
    HapImageResult_Bad_Image,
    HapImageResult_Internal_Error
};

/*
 These functions deal only with those parts of Hap specific to Hap Images. To perform complete encoding or decoding, the functions
 in this file can be used with those in the Hap reference source, available at https://github.com/Vidvox/hap
 */

/*
 Parses a Hap Image header and on success sets width, height, frame and frameBytes and returns HapResult_No_Error.
 The values returned in frame and frameBytes may subsequently be passed to the HapGet...() and HapDecode() functions from hap.h.
 */
unsigned int HapImageRead(const void *inputBuffer, unsigned long inputBufferBytes,
                          unsigned int *width, unsigned int *height,
                          const void **frame, unsigned long *frameBytes);
/*
 Generates a Hap Image header in outputBuffer and returns HapResult_No_Error on success.
 When saving a Hap Image file the generated header must be immediately followed by a frame created with HapEncode() from hap.h.
 For this encoder, outputBuffer must be at least 16 bytes long (decoders must not rely on headers being of any fixed length).
 outputBufferBytesUsed will be set to the length of the generated header in bytes.
 */
unsigned int HapImageWrite(unsigned int width, unsigned int height,
                           void *outputBuffer, unsigned long outputBufferBytes,
                           unsigned long *outputBufferBytesUsed);
#ifdef __cplusplus
}
#endif

#endif
