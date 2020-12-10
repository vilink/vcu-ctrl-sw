/******************************************************************************
*
* Copyright (C) 2008-2020 Allegro DVT2.  All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* Use of the Software is limited solely to applications:
* (a) running on a Xilinx device, or
* (b) that interact with a Xilinx device through a bus or interconnect.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* XILINX OR ALLEGRO DVT2 BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
* WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
* OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*
* Except as contained in this notice, the name of  Xilinx shall not be used
* in advertising or otherwise to promote the sale, use or other dealings in
* this Software without prior written authorization from Xilinx.
*
*
* Except as contained in this notice, the name of Allegro DVT2 shall not be used
* in advertising or otherwise to promote the sale, use or other dealings in
* this Software without prior written authorization from Allegro DVT2.
*
******************************************************************************/

#include "lib_common/StreamBuffer.h"
#include "lib_common/StreamBufferPrivate.h"
#include "lib_common/Utils.h"
#include "lib_common/AvcLevelsLimit.h"
#include "lib_common/HevcLevelsLimit.h"

static const uint8_t STREAM_ALLOC_LOG2_MAXCUSIZE = 6;

/****************************************************************************/
static int GetOneLCUPCMSize(AL_EChromaMode eChromaMode, uint8_t uLog2MaxCuSize, uint8_t uBitDepth)
{
  static const uint16_t AL_PCM_SIZE[4][3] =
  {
    { 256, 1024, 4096 }, { 384, 1536, 6144 }, { 512, 2048, 8192 }, { 768, 3072, 12288 }
  };
  return AL_PCM_SIZE[eChromaMode][uLog2MaxCuSize - 4] * uBitDepth / 8;
}

/****************************************************************************/
int GetPCMSize(uint32_t uNumLCU, uint8_t uLog2MaxCuSize, AL_EChromaMode eChromaMode, uint8_t uBitDepth, bool bIntermediateBuffer)
{
  (void)bIntermediateBuffer;

  // Sample cost
  int iLCUSize = GetOneLCUPCMSize(eChromaMode, uLog2MaxCuSize, uBitDepth);

  // Rounding because of potential tiling
  iLCUSize = RoundUp(iLCUSize, HW_IP_BURST_ALIGNMENT);

  return uNumLCU * iLCUSize;
}

/****************************************************************************/
int GetPcmVclNalSize(AL_TDimension tDim, AL_EChromaMode eMode, int iBitDepth)
{
  uint32_t uNumLCU = GetBlkNumber(tDim, STREAM_ALLOC_LOG2_MAXCUSIZE);
  return GetPCMSize(uNumLCU, STREAM_ALLOC_LOG2_MAXCUSIZE, eMode, iBitDepth, false);
}

/****************************************************************************/
int Hevc_GetMaxVclNalSize(AL_TDimension tDim, AL_EChromaMode eMode, int iBitDepth)
{
  int iLCUSize = GetOneLCUPCMSize(eMode, STREAM_ALLOC_LOG2_MAXCUSIZE, iBitDepth);
  /* Spec. A.3.2, A.3.3: Number of bits in the macroblock is at most: 5 * RawCtuBits / 3. */
  iLCUSize = (iLCUSize * 5 + 2) / 3;
  // Round at LCU?
  int iSize = GetBlkNumber(tDim, STREAM_ALLOC_LOG2_MAXCUSIZE) * iLCUSize;
  return RoundUp(iSize, HW_IP_BURST_ALIGNMENT);
}

/****************************************************************************/
int AL_GetMaxNalSize(AL_ECodec eCodec, AL_TDimension tDim, AL_EChromaMode eMode, int iBitDepth, int iLevel, int iProfileIdc)
{
  (void)iLevel, (void)iProfileIdc;

  int iMaxPCM = (eCodec == AL_CODEC_HEVC) ? Hevc_GetMaxVclNalSize(tDim, eMode, iBitDepth) : GetPcmVclNalSize(tDim, eMode, iBitDepth);

  int iNumSlices = 1;

  if(eCodec == AL_CODEC_AVC)
    iNumSlices = AL_AVC_GetMaxNumberOfSlices(iProfileIdc, iLevel, 1, 60, INT32_MAX);

  if(eCodec == AL_CODEC_HEVC)
    iNumSlices = AL_HEVC_GetMaxNumberOfSlices(iLevel);

  int iMaxNalSize = iMaxPCM + AL_ENC_MAX_HEADER_SIZE + (iNumSlices * AL_MAX_SLICE_HEADER_SIZE);

  return RoundUp(iMaxNalSize, 32);
}

/****************************************************************************/
int AL_GetMitigatedMaxNalSize(AL_TDimension tDim, AL_EChromaMode eMode, int iBitDepth)
{
  /* Mitigated worst case: PCM + one slice per row. */
  int iMaxPCM = GetPcmVclNalSize(tDim, eMode, iBitDepth);
  int iNumSlices = ((tDim.iHeight + 15) / 16);
  iMaxPCM += AL_ENC_MAX_HEADER_SIZE + (iNumSlices * AL_MAX_SLICE_HEADER_SIZE);

  return RoundUp(iMaxPCM, 32);
}

