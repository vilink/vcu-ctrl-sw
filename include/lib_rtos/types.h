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

/**************************************************************************//*!
   \addtogroup lib_rtos
   @{
   \file
******************************************************************************/
#pragma once
#include "config.h"  // add by justchen
#include <stddef.h> // for NULL and size_t
#include <stdint.h>
#include <stdbool.h>

#define AL_INTROSPECT(...)

#ifdef __GNUC__

#define _CRT_SECURE_NO_WARNINGS
#define __AL_ALIGNED__(x) __attribute__((aligned(x)))
#define AL_INLINE inline
#define AL_API extern
#define AL_DEPRECATED(msg) __attribute__((deprecated(msg)))

#ifndef __cplusplus
#define static_assert _Static_assert
#endif

#else // _MSC_VER

#define __AL_ALIGNED__(x)
#define __attribute__(x)
#define AL_INLINE __inline
#define AL_API extern
#define AL_DEPRECATED(msg) __declspec(deprecated(msg))

#ifndef __cplusplus
#define static_assert(assertion, ...) _STATIC_ASSERT(assertion)
#endif

#endif

#define AL_DEPRECATED_ENUM_VALUE(eType, name, val, msg) AL_DEPRECATED(msg) static const eType name = val

typedef uint64_t AL_64U __AL_ALIGNED__ (8); // Ensure that 64bits has same alignment on all platforms
typedef int64_t AL_64S;
typedef uint8_t* AL_VADDR; /*!< Virtual address. byte pointer */
typedef uint32_t AL_PADDR; /*!< Physical address, 32-bit address registers */
typedef AL_64U AL_PTR64;
typedef uint32_t AL_ERR;
typedef void* AL_HANDLE;

#define AL_MAX_NUM_REF 16
#define AL_MAX_NUM_B_PICT 15

/*@}*/

