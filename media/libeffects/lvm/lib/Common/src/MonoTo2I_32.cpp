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

/**********************************************************************************
   INCLUDE FILES
***********************************************************************************/

#include "VectorArithmetic.h"

/**********************************************************************************
   FUNCTION MonoTo2I_32
***********************************************************************************/

void MonoTo2I_32( const LVM_INT32  *src,
                 LVM_INT32  *dst,
                 LVM_INT16 n)
{
    LVM_INT16 ii;
    src += (n-1);
    dst += ((n*2)-1);

    for (ii = n; ii != 0; ii--)
    {
        *dst = *src;
        dst--;

        *dst = *src;
        dst--;
        src--;
    }

    return;
}
void MonoTo2I_Float( const LVM_FLOAT  *src,
                     LVM_FLOAT  *dst,
                     LVM_INT16 n)
{
    LVM_INT16 ii;
    src += (n - 1);
    dst += ((n * 2) - 1);

    for (ii = n; ii != 0; ii--)
    {
        *dst = *src;
        dst--;

        *dst = *src;
        dst--;
        src--;
    }

    return;
}
/**********************************************************************************/
