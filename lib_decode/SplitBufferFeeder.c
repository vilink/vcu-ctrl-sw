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

#include "SplitBufferFeeder.h"
#include "InternalError.h"
#include "lib_decode/lib_decode.h"
#include "lib_common/Fifo.h"
#include "lib_common/BufferSeiMeta.h"
#include "lib_common/StreamSection.h"
#include "lib_common/BufferStreamMeta.h"
#include "lib_assert/al_assert.h"
#include "lib_decode/WorkPool.h"

UNIT_ERROR AL_Decoder_TryDecodeOneUnit(AL_HDecoder hDec, AL_TBuffer* pBuf);
void AL_Decoder_InternalFlush(AL_HDecoder hDec);

typedef struct al_t_SplitBufferFeeder
{
  const AL_TFeederVtable* vtable;
  AL_HANDLE hDec;
  AL_TFifo inputFifo;
  WorkPool workPool;

  AL_EVENT incomingWorkEvent;
  AL_THREAD process;
  int32_t numInputBuf;
  int32_t keepGoing;
  AL_MUTEX lock;
  bool eos;
  bool stopped;
  AL_TBuffer* pEOSBuffer;
}AL_TSplitBufferFeeder;

static bool enqueueBuffer(AL_TSplitBufferFeeder* this, AL_TBuffer* pBuf)
{
  AL_Buffer_Ref(pBuf);

  if(!AL_Fifo_Queue(&this->inputFifo, pBuf, AL_WAIT_FOREVER))
  {
    AL_Buffer_Unref(pBuf);
    return false;
  }
  Rtos_AtomicIncrement(&this->numInputBuf);
  return true;
}

static void freeBuf(AL_TFeeder* hFeeder, AL_TBuffer* pBuf)
{
  AL_Assert(pBuf);
  AL_TSplitBufferFeeder* this = (AL_TSplitBufferFeeder*)hFeeder;
  AL_WorkPool_Remove(&this->workPool, pBuf);
  AL_Buffer_Unref(pBuf);
}

static bool HasInputBuffer(AL_TSplitBufferFeeder* this)
{
  int32_t numInputBuf = Rtos_AtomicDecrement(&this->numInputBuf);
  Rtos_AtomicIncrement(&this->numInputBuf);
  return numInputBuf >= 0;
}

static bool shouldKeepGoing(AL_TSplitBufferFeeder* this)
{
  int32_t keepGoing = Rtos_AtomicDecrement(&this->keepGoing);
  Rtos_AtomicIncrement(&this->keepGoing);
  return keepGoing >= 0;
}

static void signal(AL_TFeeder* hFeeder)
{
  AL_TSplitBufferFeeder* this = (AL_TSplitBufferFeeder*)hFeeder;
  Rtos_SetEvent(this->incomingWorkEvent);
}

static bool IsSuccess(UNIT_ERROR err)
{
  return (err == SUCCESS_ACCESS_UNIT) || (err == SUCCESS_NAL_UNIT);
}

static void Process(AL_TSplitBufferFeeder* this)
{
  AL_TBuffer* workBuf = AL_Fifo_Dequeue(&this->inputFifo, AL_NO_WAIT);

  if(!workBuf)
    return;

  Rtos_AtomicDecrement(&this->numInputBuf);

  AL_HANDLE hDec = this->hDec;

  AL_WorkPool_PushBack(&this->workPool, workBuf);

  UNIT_ERROR err = AL_Decoder_TryDecodeOneUnit(hDec, workBuf);
  signal((AL_TFeeder*)this);

  AL_TCircMetaData* pMeta = (AL_TCircMetaData*)AL_Buffer_GetMetaData(workBuf, AL_META_TYPE_CIRCULAR);

  if(pMeta && pMeta->bLastBuffer)
  {
    // There won't be a callback for the eos buffer as it doesn't contain a frame / slice
    // so we free it here.
    freeBuf((AL_TFeeder*)this, workBuf);
    this->stopped = true;
    AL_Decoder_InternalFlush(this->hDec);
  }
  else
  {
    if(this->stopped)
    {
      this->stopped = false;
      Rtos_GetMutex(this->lock);
      this->eos = false;
      Rtos_ReleaseMutex(this->lock);
    }

    if(!IsSuccess(err))
    {
      freeBuf((AL_TFeeder*)this, workBuf);
      return;
    }
  }
}

static void* Process_EntryPoint(void* userParam)
{
  Rtos_SetCurrentThreadName("DecFeeder");
  AL_TSplitBufferFeeder* this = (AL_TSplitBufferFeeder*)userParam;

  while(1)
  {
    Rtos_WaitEvent(this->incomingWorkEvent, AL_WAIT_FOREVER);

    // if we still have input buffers, we want to handle them before exiting
    if(!shouldKeepGoing(this) && !HasInputBuffer(this))
    {
      if(!this->stopped)
        AL_Decoder_InternalFlush(this->hDec);
      break;
    }

    Process(this);
  }

  return NULL;
}

static void reset(AL_TFeeder* pFeeder)
{
  (void)pFeeder;
}

static bool pushBuffer(AL_TFeeder* hFeeder, AL_TBuffer* pBuf, size_t uSize, bool bLastBuffer)
{
  AL_TSplitBufferFeeder* this = (AL_TSplitBufferFeeder*)hFeeder;
  AL_TCircMetaData* pMetaCirc = (AL_TCircMetaData*)AL_Buffer_GetMetaData(pBuf, AL_META_TYPE_CIRCULAR);

  if(!pMetaCirc)
  {
    AL_TMetaData* pMeta = (AL_TMetaData*)AL_CircMetaData_Create(0, uSize, bLastBuffer);

    if(!pMeta)
      return false;

    if(!AL_Buffer_AddMetaData(pBuf, pMeta))
    {
      Rtos_Free(pMeta);
      return false;
    }
  }
  else
  {
    pMetaCirc->iOffset = 0;
    pMetaCirc->iAvailSize = uSize;
  }

  AL_TSeiMetaData* pSei = (AL_TSeiMetaData*)AL_Buffer_GetMetaData(pBuf, AL_META_TYPE_SEI);

  if(pSei)
    AL_SeiMetaData_Reset(pSei);

  if(!enqueueBuffer(this, pBuf))
    return false;

  signal(hFeeder);
  return true;
}

static void pushEOS(AL_TFeeder* hFeeder)
{
  AL_TSplitBufferFeeder* this = (AL_TSplitBufferFeeder*)hFeeder;

  /* Ensure in subframe latency that even if the user doesn't send a full frame at the end of the stream, we can finish sent slices */
  if(this->pEOSBuffer)
    pushBuffer(hFeeder, this->pEOSBuffer, AL_Buffer_GetSize(this->pEOSBuffer), true);
  signal(hFeeder);
}

static void flush(AL_TFeeder* hFeeder)
{
  AL_TSplitBufferFeeder* this = (AL_TSplitBufferFeeder*)hFeeder;
  Rtos_GetMutex(this->lock);
  this->eos = true;
  Rtos_ReleaseMutex(this->lock);
  pushEOS(hFeeder);
}

static void destroy_process(AL_TFeeder* hFeeder)
{
  AL_TSplitBufferFeeder* this = (AL_TSplitBufferFeeder*)hFeeder;

  Rtos_GetMutex(this->lock);
  // if you flushed and didn't send another buffer after that, you asked for eos
  // and we will process all the previous buffers.
  bool eos = this->eos;
  Rtos_ReleaseMutex(this->lock);

  // we weren't asked to flush, just drop all the remaining buffers.
  if(!eos)
  {
    AL_TBuffer* workBuf = AL_Fifo_Dequeue(&this->inputFifo, AL_NO_WAIT);

    while(workBuf)
    {
      Rtos_AtomicDecrement(&this->numInputBuf);

      AL_Buffer_Unref(workBuf);
      workBuf = AL_Fifo_Dequeue(&this->inputFifo, AL_NO_WAIT);
    }

    pushEOS(hFeeder);
  }

  Rtos_AtomicDecrement(&this->keepGoing);
  Rtos_SetEvent(this->incomingWorkEvent);

  Rtos_JoinThread(this->process);
  Rtos_DeleteThread(this->process);
}

static void destroy(AL_TFeeder* hFeeder)
{
  AL_TSplitBufferFeeder* this = (AL_TSplitBufferFeeder*)hFeeder;

  if(this->process)
    destroy_process(hFeeder);

  AL_Assert(this->workPool.size == 0);

  AL_WorkPool_Deinit(&this->workPool);
  AL_Fifo_Deinit(&this->inputFifo);

  Rtos_DeleteMutex(this->lock);
  Rtos_DeleteEvent(this->incomingWorkEvent);

  Rtos_Free(this);
}

static bool CreateProcess(AL_TSplitBufferFeeder* this)
{
  this->process = Rtos_CreateThread(Process_EntryPoint, this);

  if(!this->process)
    return false;
  return true;
}

static const AL_TFeederVtable SplitBufferFeederVtable =
{
  &destroy,
  &pushBuffer,
  &signal,
  &flush,
  &reset,
  &freeBuf,
};

static bool addEOSMeta(AL_TBuffer* pEOSBuffer)
{
  AL_TStreamMetaData* pStreamMeta = NULL;

  if(NULL == AL_Buffer_GetMetaData(pEOSBuffer, AL_META_TYPE_STREAM))
  {
    pStreamMeta = AL_StreamMetaData_Create(1);

    if(!AL_Buffer_AddMetaData(pEOSBuffer, (AL_TMetaData*)pStreamMeta))
    {
      AL_MetaData_Destroy((AL_TMetaData*)pStreamMeta);
      return false;
    }
    AL_StreamMetaData_ClearAllSections(pStreamMeta);
    AL_StreamMetaData_AddSection(pStreamMeta, 0, 0, AL_SECTION_END_FRAME_FLAG);
  }

  if(NULL == AL_Buffer_GetMetaData(pEOSBuffer, AL_META_TYPE_CIRCULAR))
  {
    AL_TCircMetaData* pCircMeta = AL_CircMetaData_Create(0, AL_Buffer_GetSize(pEOSBuffer), true);

    if(!pCircMeta)
    {
      AL_Buffer_RemoveMetaData(pEOSBuffer, (AL_TMetaData*)pStreamMeta);
      AL_MetaData_Destroy((AL_TMetaData*)pStreamMeta);
      return false;
    }

    if(!AL_Buffer_AddMetaData(pEOSBuffer, (AL_TMetaData*)pCircMeta))
    {
      AL_Buffer_RemoveMetaData(pEOSBuffer, (AL_TMetaData*)pStreamMeta);
      AL_MetaData_Destroy((AL_TMetaData*)pStreamMeta);
      AL_MetaData_Destroy((AL_TMetaData*)pCircMeta);
      return false;
    }
  }
  return true;
}

AL_TFeeder* AL_SplitBufferFeeder_Create(AL_HANDLE hDec, int iMaxBufNum, AL_TBuffer* pEOSBuffer)
{
  AL_Assert(pEOSBuffer);

  if(!addEOSMeta(pEOSBuffer))
    return NULL;

  AL_TSplitBufferFeeder* this = Rtos_Malloc(sizeof(*this));

  if(!this)
    return NULL;

  this->vtable = &SplitBufferFeederVtable;
  this->hDec = hDec;
  this->eos = false;
  this->pEOSBuffer = pEOSBuffer;
  this->numInputBuf = 0;
  this->keepGoing = 1;
  this->stopped = true;

  if(iMaxBufNum <= 0 || !AL_Fifo_Init(&this->inputFifo, iMaxBufNum))
    goto fail_queue_allocation;

  if(iMaxBufNum <= 0 || !AL_WorkPool_Init(&this->workPool, iMaxBufNum))
    goto fail_work_queue_allocation;

  this->incomingWorkEvent = Rtos_CreateEvent(false);

  if(!this->incomingWorkEvent)
    goto fail_event;

  this->lock = Rtos_CreateMutex();

  if(!this->lock)
    goto fail_mutex;

  if(!CreateProcess(this))
    goto cleanup;

  return (AL_TFeeder*)this;

  cleanup:
  Rtos_DeleteMutex(this->lock);
  fail_mutex:
  Rtos_DeleteEvent(this->incomingWorkEvent);
  fail_event:
  AL_WorkPool_Deinit(&this->workPool);
  fail_work_queue_allocation:
  AL_Fifo_Deinit(&this->inputFifo);
  fail_queue_allocation:
  Rtos_Free(this);
  return NULL;
}

