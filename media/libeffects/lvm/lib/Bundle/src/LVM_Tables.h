/*
 * Copyright (C) 2004-2010 NXP Software
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __LVM_TABLES_H__
#define __LVM_TABLES_H__


/************************************************************************************/
/*                                                                                  */
/*    Includes                                                                      */
/*                                                                                  */
/************************************************************************************/

#include "BIQUAD.h"
#include "LVM_Types.h"

/************************************************************************************/
/*                                                                                  */
/*    Treble Boost Filter Coefficients                                              */
/*                                                                                  */
/************************************************************************************/

#ifdef BUILD_FLOAT
extern FO_FLOAT_LShx_Coefs_t     LVM_TrebleBoostCoefs[];
#else
extern FO_C16_LShx_Coefs_t     LVM_TrebleBoostCoefs[];
#endif

/************************************************************************************/
/*                                                                                  */
/*    Volume control gain and time constant tables                                  */
/*                                                                                  */
/************************************************************************************/
#ifdef BUILD_FLOAT
extern const LVM_FLOAT LVM_VolumeTable[];
#else
extern const LVM_INT16 LVM_VolumeTable[];
#endif

extern const LVM_INT16 LVM_MixerTCTable[];



#endif /* __LVM_TABLES_H__ */


