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

#pragma once

#include <lib_rtos/types.h> // bool
#include <lib_common/Utils.h> // Min
#include <lib_assert/al_assert.h>

#define MAX_ELEMENTS 32

typedef struct _IntVector
{
  int count;
  int elements[MAX_ELEMENTS];
}IntVector;

void IntVector_Init(IntVector* self);
void IntVector_Add(IntVector* self, int element);
void IntVector_MoveBack(IntVector* self, int element);
void IntVector_Remove(IntVector* self, int element);
bool IntVector_IsIn(IntVector* self, int element);
int IntVector_Count(IntVector* self);
void IntVector_Revert(IntVector* self);
void IntVector_Copy(IntVector* from, IntVector* to);

#define VECTOR_FOREACH(iterator, v) \
  AL_Assert((v).count <= MAX_ELEMENTS); \
  for(int i = 0, iterator = (v).elements[0]; i < (v).count; i++, iterator = (v).elements[Min(i, MAX_ELEMENTS - 1)])
