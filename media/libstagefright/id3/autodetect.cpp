/*
 * Copyright (C) 2008 The Android Open Source Project
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

//#define LOG_NDEBUG 0
#define LOG_TAG "AutoDetect"
#include <utils/Log.h>

#include <stdio.h>
#include <stdlib.h>
#include "../include/autodetect.h"
#include <cutils/properties.h>
#include <utils/String8.h>
#include "unicode/ucnv.h"
#include "unicode/ustring.h"

namespace android {

struct CharRange {
    uint16_t first;
    uint16_t last;
};

#define ARRAY_SIZE(x)   (sizeof(x) / sizeof(*x))

// generated from http://unicode.org/Public/MAPPINGS/VENDORS/MICSFT/WINDOWS/CP932.TXT
static const CharRange kShiftJISRanges[] = {
    { 0x8140, 0x817E },
    { 0x8180, 0x81AC },
    { 0x81B8, 0x81BF },
    { 0x81C8, 0x81CE },
    { 0x81DA, 0x81E8 },
    { 0x81F0, 0x81F7 },
    { 0x81FC, 0x81FC },
    { 0x824F, 0x8258 },
    { 0x8260, 0x8279 },
    { 0x8281, 0x829A },
    { 0x829F, 0x82F1 },
    { 0x8340, 0x837E },
    { 0x8380, 0x8396 },
    { 0x839F, 0x83B6 },
    { 0x83BF, 0x83D6 },
    { 0x8440, 0x8460 },
    { 0x8470, 0x847E },
    { 0x8480, 0x8491 },
    { 0x849F, 0x84BE },
    { 0x8740, 0x875D },
    { 0x875F, 0x8775 },
    { 0x877E, 0x877E },
    { 0x8780, 0x879C },
    { 0x889F, 0x88FC },
    { 0x8940, 0x897E },
    { 0x8980, 0x89FC },
    { 0x8A40, 0x8A7E },
    { 0x8A80, 0x8AFC },
    { 0x8B40, 0x8B7E },
    { 0x8B80, 0x8BFC },
    { 0x8C40, 0x8C7E },
    { 0x8C80, 0x8CFC },
    { 0x8D40, 0x8D7E },
    { 0x8D80, 0x8DFC },
    { 0x8E40, 0x8E7E },
    { 0x8E80, 0x8EFC },
    { 0x8F40, 0x8F7E },
    { 0x8F80, 0x8FFC },
    { 0x9040, 0x907E },
    { 0x9080, 0x90FC },
    { 0x9140, 0x917E },
    { 0x9180, 0x91FC },
    { 0x9240, 0x927E },
    { 0x9280, 0x92FC },
    { 0x9340, 0x937E },
    { 0x9380, 0x93FC },
    { 0x9440, 0x947E },
    { 0x9480, 0x94FC },
    { 0x9540, 0x957E },
    { 0x9580, 0x95FC },
    { 0x9640, 0x967E },
    { 0x9680, 0x96FC },
    { 0x9740, 0x977E },
    { 0x9780, 0x97FC },
    { 0x9840, 0x9872 },
    { 0x989F, 0x98FC },
    { 0x9940, 0x997E },
    { 0x9980, 0x99FC },
    { 0x9A40, 0x9A7E },
    { 0x9A80, 0x9AFC },
    { 0x9B40, 0x9B7E },
    { 0x9B80, 0x9BFC },
    { 0x9C40, 0x9C7E },
    { 0x9C80, 0x9CFC },
    { 0x9D40, 0x9D7E },
    { 0x9D80, 0x9DFC },
    { 0x9E40, 0x9E7E },
    { 0x9E80, 0x9EFC },
    { 0x9F40, 0x9F7E },
    { 0x9F80, 0x9FFC },
    { 0xE040, 0xE07E },
    { 0xE080, 0xE0FC },
    { 0xE140, 0xE17E },
    { 0xE180, 0xE1FC },
    { 0xE240, 0xE27E },
    { 0xE280, 0xE2FC },
    { 0xE340, 0xE37E },
    { 0xE380, 0xE3FC },
    { 0xE440, 0xE47E },
    { 0xE480, 0xE4FC },
    { 0xE540, 0xE57E },
    { 0xE580, 0xE5FC },
    { 0xE640, 0xE67E },
    { 0xE680, 0xE6FC },
    { 0xE740, 0xE77E },
    { 0xE780, 0xE7FC },
    { 0xE840, 0xE87E },
    { 0xE880, 0xE8FC },
    { 0xE940, 0xE97E },
    { 0xE980, 0xE9FC },
    { 0xEA40, 0xEA7E },
    { 0xEA80, 0xEAA4 },
    { 0xED40, 0xED7E },
    { 0xED80, 0xEDFC },
    { 0xEE40, 0xEE7E },
    { 0xEE80, 0xEEEC },
    { 0xEEEF, 0xEEFC },
    { 0xFA40, 0xFA7E },
    { 0xFA80, 0xFAFC },
    { 0xFB40, 0xFB7E },
    { 0xFB80, 0xFBFC },
    { 0xFC40, 0xFC4B },
};

// generated from http://unicode.org/Public/MAPPINGS/VENDORS/MICSFT/WINDOWS/CP936.TXT
static const CharRange kGBKRanges[] = {
    { 0x8140, 0x817E },
    { 0x8180, 0x81FE },
    { 0x8240, 0x827E },
    { 0x8280, 0x82FE },
    { 0x8340, 0x837E },
    { 0x8380, 0x83FE },
    { 0x8440, 0x847E },
    { 0x8480, 0x84FE },
    { 0x8540, 0x857E },
    { 0x8580, 0x85FE },
    { 0x8640, 0x867E },
    { 0x8680, 0x86FE },
    { 0x8740, 0x877E },
    { 0x8780, 0x87FE },
    { 0x8840, 0x887E },
    { 0x8880, 0x88FE },
    { 0x8940, 0x897E },
    { 0x8980, 0x89FE },
    { 0x8A40, 0x8A7E },
    { 0x8A80, 0x8AFE },
    { 0x8B40, 0x8B7E },
    { 0x8B80, 0x8BFE },
    { 0x8C40, 0x8C7E },
    { 0x8C80, 0x8CFE },
    { 0x8D40, 0x8D7E },
    { 0x8D80, 0x8DFE },
    { 0x8E40, 0x8E7E },
    { 0x8E80, 0x8EFE },
    { 0x8F40, 0x8F7E },
    { 0x8F80, 0x8FFE },
    { 0x9040, 0x907E },
    { 0x9080, 0x90FE },
    { 0x9140, 0x917E },
    { 0x9180, 0x91FE },
    { 0x9240, 0x927E },
    { 0x9280, 0x92FE },
    { 0x9340, 0x937E },
    { 0x9380, 0x93FE },
    { 0x9440, 0x947E },
    { 0x9480, 0x94FE },
    { 0x9540, 0x957E },
    { 0x9580, 0x95FE },
    { 0x9640, 0x967E },
    { 0x9680, 0x96FE },
    { 0x9740, 0x977E },
    { 0x9780, 0x97FE },
    { 0x9840, 0x987E },
    { 0x9880, 0x98FE },
    { 0x9940, 0x997E },
    { 0x9980, 0x99FE },
    { 0x9A40, 0x9A7E },
    { 0x9A80, 0x9AFE },
    { 0x9B40, 0x9B7E },
    { 0x9B80, 0x9BFE },
    { 0x9C40, 0x9C7E },
    { 0x9C80, 0x9CFE },
    { 0x9D40, 0x9D7E },
    { 0x9D80, 0x9DFE },
    { 0x9E40, 0x9E7E },
    { 0x9E80, 0x9EFE },
    { 0x9F40, 0x9F7E },
    { 0x9F80, 0x9FFE },
    { 0xA040, 0xA07E },
    { 0xA080, 0xA0FE },
    { 0xA1A1, 0xA1FE },
    { 0xA2A1, 0xA2AA },
    { 0xA2B1, 0xA2E2 },
    { 0xA2E5, 0xA2EE },
    { 0xA2F1, 0xA2FC },
    { 0xA3A1, 0xA3FE },
    { 0xA4A1, 0xA4F3 },
    { 0xA5A1, 0xA5F6 },
    { 0xA6A1, 0xA6B8 },
    { 0xA6C1, 0xA6D8 },
    { 0xA6E0, 0xA6EB },
    { 0xA6EE, 0xA6F2 },
    { 0xA6F4, 0xA6F5 },
    { 0xA7A1, 0xA7C1 },
    { 0xA7D1, 0xA7F1 },
    { 0xA840, 0xA87E },
    { 0xA880, 0xA895 },
    { 0xA8A1, 0xA8BB },
    { 0xA8BD, 0xA8BE },
    { 0xA8C0, 0xA8C0 },
    { 0xA8C5, 0xA8E9 },
    { 0xA940, 0xA957 },
    { 0xA959, 0xA95A },
    { 0xA95C, 0xA95C },
    { 0xA960, 0xA97E },
    { 0xA980, 0xA988 },
    { 0xA996, 0xA996 },
    { 0xA9A4, 0xA9EF },
    { 0xAA40, 0xAA7E },
    { 0xAA80, 0xAAA0 },
    { 0xAB40, 0xAB7E },
    { 0xAB80, 0xABA0 },
    { 0xAC40, 0xAC7E },
    { 0xAC80, 0xACA0 },
    { 0xAD40, 0xAD7E },
    { 0xAD80, 0xADA0 },
    { 0xAE40, 0xAE7E },
    { 0xAE80, 0xAEA0 },
    { 0xAF40, 0xAF7E },
    { 0xAF80, 0xAFA0 },
    { 0xB040, 0xB07E },
    { 0xB080, 0xB0FE },
    { 0xB140, 0xB17E },
    { 0xB180, 0xB1FE },
    { 0xB240, 0xB27E },
    { 0xB280, 0xB2FE },
    { 0xB340, 0xB37E },
    { 0xB380, 0xB3FE },
    { 0xB440, 0xB47E },
    { 0xB480, 0xB4FE },
    { 0xB540, 0xB57E },
    { 0xB580, 0xB5FE },
    { 0xB640, 0xB67E },
    { 0xB680, 0xB6FE },
    { 0xB740, 0xB77E },
    { 0xB780, 0xB7FE },
    { 0xB840, 0xB87E },
    { 0xB880, 0xB8FE },
    { 0xB940, 0xB97E },
    { 0xB980, 0xB9FE },
    { 0xBA40, 0xBA7E },
    { 0xBA80, 0xBAFE },
    { 0xBB40, 0xBB7E },
    { 0xBB80, 0xBBFE },
    { 0xBC40, 0xBC7E },
    { 0xBC80, 0xBCFE },
    { 0xBD40, 0xBD7E },
    { 0xBD80, 0xBDFE },
    { 0xBE40, 0xBE7E },
    { 0xBE80, 0xBEFE },
    { 0xBF40, 0xBF7E },
    { 0xBF80, 0xBFFE },
    { 0xC040, 0xC07E },
    { 0xC080, 0xC0FE },
    { 0xC140, 0xC17E },
    { 0xC180, 0xC1FE },
    { 0xC240, 0xC27E },
    { 0xC280, 0xC2FE },
    { 0xC340, 0xC37E },
    { 0xC380, 0xC3FE },
    { 0xC440, 0xC47E },
    { 0xC480, 0xC4FE },
    { 0xC540, 0xC57E },
    { 0xC580, 0xC5FE },
    { 0xC640, 0xC67E },
    { 0xC680, 0xC6FE },
    { 0xC740, 0xC77E },
    { 0xC780, 0xC7FE },
    { 0xC840, 0xC87E },
    { 0xC880, 0xC8FE },
    { 0xC940, 0xC97E },
    { 0xC980, 0xC9FE },
    { 0xCA40, 0xCA7E },
    { 0xCA80, 0xCAFE },
    { 0xCB40, 0xCB7E },
    { 0xCB80, 0xCBFE },
    { 0xCC40, 0xCC7E },
    { 0xCC80, 0xCCFE },
    { 0xCD40, 0xCD7E },
    { 0xCD80, 0xCDFE },
    { 0xCE40, 0xCE7E },
    { 0xCE80, 0xCEFE },
    { 0xCF40, 0xCF7E },
    { 0xCF80, 0xCFFE },
    { 0xD040, 0xD07E },
    { 0xD080, 0xD0FE },
    { 0xD140, 0xD17E },
    { 0xD180, 0xD1FE },
    { 0xD240, 0xD27E },
    { 0xD280, 0xD2FE },
    { 0xD340, 0xD37E },
    { 0xD380, 0xD3FE },
    { 0xD440, 0xD47E },
    { 0xD480, 0xD4FE },
    { 0xD540, 0xD57E },
    { 0xD580, 0xD5FE },
    { 0xD640, 0xD67E },
    { 0xD680, 0xD6FE },
    { 0xD740, 0xD77E },
    { 0xD780, 0xD7F9 },
    { 0xD840, 0xD87E },
    { 0xD880, 0xD8FE },
    { 0xD940, 0xD97E },
    { 0xD980, 0xD9FE },
    { 0xDA40, 0xDA7E },
    { 0xDA80, 0xDAFE },
    { 0xDB40, 0xDB7E },
    { 0xDB80, 0xDBFE },
    { 0xDC40, 0xDC7E },
    { 0xDC80, 0xDCFE },
    { 0xDD40, 0xDD7E },
    { 0xDD80, 0xDDFE },
    { 0xDE40, 0xDE7E },
    { 0xDE80, 0xDEFE },
    { 0xDF40, 0xDF7E },
    { 0xDF80, 0xDFFE },
    { 0xE040, 0xE07E },
    { 0xE080, 0xE0FE },
    { 0xE140, 0xE17E },
    { 0xE180, 0xE1FE },
    { 0xE240, 0xE27E },
    { 0xE280, 0xE2FE },
    { 0xE340, 0xE37E },
    { 0xE380, 0xE3FE },
    { 0xE440, 0xE47E },
    { 0xE480, 0xE4FE },
    { 0xE540, 0xE57E },
    { 0xE580, 0xE5FE },
    { 0xE640, 0xE67E },
    { 0xE680, 0xE6FE },
    { 0xE740, 0xE77E },
    { 0xE780, 0xE7FE },
    { 0xE840, 0xE87E },
    { 0xE880, 0xE8FE },
    { 0xE940, 0xE97E },
    { 0xE980, 0xE9FE },
    { 0xEA40, 0xEA7E },
    { 0xEA80, 0xEAFE },
    { 0xEB40, 0xEB7E },
    { 0xEB80, 0xEBFE },
    { 0xEC40, 0xEC7E },
    { 0xEC80, 0xECFE },
    { 0xED40, 0xED7E },
    { 0xED80, 0xEDFE },
    { 0xEE40, 0xEE7E },
    { 0xEE80, 0xEEFE },
    { 0xEF40, 0xEF7E },
    { 0xEF80, 0xEFFE },
    { 0xF040, 0xF07E },
    { 0xF080, 0xF0FE },
    { 0xF140, 0xF17E },
    { 0xF180, 0xF1FE },
    { 0xF240, 0xF27E },
    { 0xF280, 0xF2FE },
    { 0xF340, 0xF37E },
    { 0xF380, 0xF3FE },
    { 0xF440, 0xF47E },
    { 0xF480, 0xF4FE },
    { 0xF540, 0xF57E },
    { 0xF580, 0xF5FE },
    { 0xF640, 0xF67E },
    { 0xF680, 0xF6FE },
    { 0xF740, 0xF77E },
    { 0xF780, 0xF7FE },
    { 0xF840, 0xF87E },
    { 0xF880, 0xF8A0 },
    { 0xF940, 0xF97E },
    { 0xF980, 0xF9A0 },
    { 0xFA40, 0xFA7E },
    { 0xFA80, 0xFAA0 },
    { 0xFB40, 0xFB7E },
    { 0xFB80, 0xFBA0 },
    { 0xFC40, 0xFC7E },
    { 0xFC80, 0xFCA0 },
    { 0xFD40, 0xFD7E },
    { 0xFD80, 0xFDA0 },
    { 0xFE40, 0xFE4F },
};

// generated from http://unicode.org/Public/MAPPINGS/VENDORS/MICSFT/WINDOWS/CP949.TXT
static const CharRange kEUCKRRanges[] = {
    { 0x8141, 0x815A },
    { 0x8161, 0x817A },
    { 0x8181, 0x81FE },
    { 0x8241, 0x825A },
    { 0x8261, 0x827A },
    { 0x8281, 0x82FE },
    { 0x8341, 0x835A },
    { 0x8361, 0x837A },
    { 0x8381, 0x83FE },
    { 0x8441, 0x845A },
    { 0x8461, 0x847A },
    { 0x8481, 0x84FE },
    { 0x8541, 0x855A },
    { 0x8561, 0x857A },
    { 0x8581, 0x85FE },
    { 0x8641, 0x865A },
    { 0x8661, 0x867A },
    { 0x8681, 0x86FE },
    { 0x8741, 0x875A },
    { 0x8761, 0x877A },
    { 0x8781, 0x87FE },
    { 0x8841, 0x885A },
    { 0x8861, 0x887A },
    { 0x8881, 0x88FE },
    { 0x8941, 0x895A },
    { 0x8961, 0x897A },
    { 0x8981, 0x89FE },
    { 0x8A41, 0x8A5A },
    { 0x8A61, 0x8A7A },
    { 0x8A81, 0x8AFE },
    { 0x8B41, 0x8B5A },
    { 0x8B61, 0x8B7A },
    { 0x8B81, 0x8BFE },
    { 0x8C41, 0x8C5A },
    { 0x8C61, 0x8C7A },
    { 0x8C81, 0x8CFE },
    { 0x8D41, 0x8D5A },
    { 0x8D61, 0x8D7A },
    { 0x8D81, 0x8DFE },
    { 0x8E41, 0x8E5A },
    { 0x8E61, 0x8E7A },
    { 0x8E81, 0x8EFE },
    { 0x8F41, 0x8F5A },
    { 0x8F61, 0x8F7A },
    { 0x8F81, 0x8FFE },
    { 0x9041, 0x905A },
    { 0x9061, 0x907A },
    { 0x9081, 0x90FE },
    { 0x9141, 0x915A },
    { 0x9161, 0x917A },
    { 0x9181, 0x91FE },
    { 0x9241, 0x925A },
    { 0x9261, 0x927A },
    { 0x9281, 0x92FE },
    { 0x9341, 0x935A },
    { 0x9361, 0x937A },
    { 0x9381, 0x93FE },
    { 0x9441, 0x945A },
    { 0x9461, 0x947A },
    { 0x9481, 0x94FE },
    { 0x9541, 0x955A },
    { 0x9561, 0x957A },
    { 0x9581, 0x95FE },
    { 0x9641, 0x965A },
    { 0x9661, 0x967A },
    { 0x9681, 0x96FE },
    { 0x9741, 0x975A },
    { 0x9761, 0x977A },
    { 0x9781, 0x97FE },
    { 0x9841, 0x985A },
    { 0x9861, 0x987A },
    { 0x9881, 0x98FE },
    { 0x9941, 0x995A },
    { 0x9961, 0x997A },
    { 0x9981, 0x99FE },
    { 0x9A41, 0x9A5A },
    { 0x9A61, 0x9A7A },
    { 0x9A81, 0x9AFE },
    { 0x9B41, 0x9B5A },
    { 0x9B61, 0x9B7A },
    { 0x9B81, 0x9BFE },
    { 0x9C41, 0x9C5A },
    { 0x9C61, 0x9C7A },
    { 0x9C81, 0x9CFE },
    { 0x9D41, 0x9D5A },
    { 0x9D61, 0x9D7A },
    { 0x9D81, 0x9DFE },
    { 0x9E41, 0x9E5A },
    { 0x9E61, 0x9E7A },
    { 0x9E81, 0x9EFE },
    { 0x9F41, 0x9F5A },
    { 0x9F61, 0x9F7A },
    { 0x9F81, 0x9FFE },
    { 0xA041, 0xA05A },
    { 0xA061, 0xA07A },
    { 0xA081, 0xA0FE },
    { 0xA141, 0xA15A },
    { 0xA161, 0xA17A },
    { 0xA181, 0xA1FE },
    { 0xA241, 0xA25A },
    { 0xA261, 0xA27A },
    { 0xA281, 0xA2E7 },
    { 0xA341, 0xA35A },
    { 0xA361, 0xA37A },
    { 0xA381, 0xA3FE },
    { 0xA441, 0xA45A },
    { 0xA461, 0xA47A },
    { 0xA481, 0xA4FE },
    { 0xA541, 0xA55A },
    { 0xA561, 0xA57A },
    { 0xA581, 0xA5AA },
    { 0xA5B0, 0xA5B9 },
    { 0xA5C1, 0xA5D8 },
    { 0xA5E1, 0xA5F8 },
    { 0xA641, 0xA65A },
    { 0xA661, 0xA67A },
    { 0xA681, 0xA6E4 },
    { 0xA741, 0xA75A },
    { 0xA761, 0xA77A },
    { 0xA781, 0xA7EF },
    { 0xA841, 0xA85A },
    { 0xA861, 0xA87A },
    { 0xA881, 0xA8A4 },
    { 0xA8A6, 0xA8A6 },
    { 0xA8A8, 0xA8AF },
    { 0xA8B1, 0xA8FE },
    { 0xA941, 0xA95A },
    { 0xA961, 0xA97A },
    { 0xA981, 0xA9FE },
    { 0xAA41, 0xAA5A },
    { 0xAA61, 0xAA7A },
    { 0xAA81, 0xAAF3 },
    { 0xAB41, 0xAB5A },
    { 0xAB61, 0xAB7A },
    { 0xAB81, 0xABF6 },
    { 0xAC41, 0xAC5A },
    { 0xAC61, 0xAC7A },
    { 0xAC81, 0xACC1 },
    { 0xACD1, 0xACF1 },
    { 0xAD41, 0xAD5A },
    { 0xAD61, 0xAD7A },
    { 0xAD81, 0xADA0 },
    { 0xAE41, 0xAE5A },
    { 0xAE61, 0xAE7A },
    { 0xAE81, 0xAEA0 },
    { 0xAF41, 0xAF5A },
    { 0xAF61, 0xAF7A },
    { 0xAF81, 0xAFA0 },
    { 0xB041, 0xB05A },
    { 0xB061, 0xB07A },
    { 0xB081, 0xB0FE },
    { 0xB141, 0xB15A },
    { 0xB161, 0xB17A },
    { 0xB181, 0xB1FE },
    { 0xB241, 0xB25A },
    { 0xB261, 0xB27A },
    { 0xB281, 0xB2FE },
    { 0xB341, 0xB35A },
    { 0xB361, 0xB37A },
    { 0xB381, 0xB3FE },
    { 0xB441, 0xB45A },
    { 0xB461, 0xB47A },
    { 0xB481, 0xB4FE },
    { 0xB541, 0xB55A },
    { 0xB561, 0xB57A },
    { 0xB581, 0xB5FE },
    { 0xB641, 0xB65A },
    { 0xB661, 0xB67A },
    { 0xB681, 0xB6FE },
    { 0xB741, 0xB75A },
    { 0xB761, 0xB77A },
    { 0xB781, 0xB7FE },
    { 0xB841, 0xB85A },
    { 0xB861, 0xB87A },
    { 0xB881, 0xB8FE },
    { 0xB941, 0xB95A },
    { 0xB961, 0xB97A },
    { 0xB981, 0xB9FE },
    { 0xBA41, 0xBA5A },
    { 0xBA61, 0xBA7A },
    { 0xBA81, 0xBAFE },
    { 0xBB41, 0xBB5A },
    { 0xBB61, 0xBB7A },
    { 0xBB81, 0xBBFE },
    { 0xBC41, 0xBC5A },
    { 0xBC61, 0xBC7A },
    { 0xBC81, 0xBCFE },
    { 0xBD41, 0xBD5A },
    { 0xBD61, 0xBD7A },
    { 0xBD81, 0xBDFE },
    { 0xBE41, 0xBE5A },
    { 0xBE61, 0xBE7A },
    { 0xBE81, 0xBEFE },
    { 0xBF41, 0xBF5A },
    { 0xBF61, 0xBF7A },
    { 0xBF81, 0xBFFE },
    { 0xC041, 0xC05A },
    { 0xC061, 0xC07A },
    { 0xC081, 0xC0FE },
    { 0xC141, 0xC15A },
    { 0xC161, 0xC17A },
    { 0xC181, 0xC1FE },
    { 0xC241, 0xC25A },
    { 0xC261, 0xC27A },
    { 0xC281, 0xC2FE },
    { 0xC341, 0xC35A },
    { 0xC361, 0xC37A },
    { 0xC381, 0xC3FE },
    { 0xC441, 0xC45A },
    { 0xC461, 0xC47A },
    { 0xC481, 0xC4FE },
    { 0xC541, 0xC55A },
    { 0xC561, 0xC57A },
    { 0xC581, 0xC5FE },
    { 0xC641, 0xC652 },
    { 0xC6A1, 0xC6FE },
    { 0xC7A1, 0xC7FE },
    { 0xC8A1, 0xC8FE },
    { 0xCAA1, 0xCAFE },
    { 0xCBA1, 0xCBFE },
    { 0xCCA1, 0xCCFE },
    { 0xCDA1, 0xCDFE },
    { 0xCEA1, 0xCEFE },
    { 0xCFA1, 0xCFFE },
    { 0xD0A1, 0xD0FE },
    { 0xD1A1, 0xD1FE },
    { 0xD2A1, 0xD2FE },
    { 0xD3A1, 0xD3FE },
    { 0xD4A1, 0xD4FE },
    { 0xD5A1, 0xD5FE },
    { 0xD6A1, 0xD6FE },
    { 0xD7A1, 0xD7FE },
    { 0xD8A1, 0xD8FE },
    { 0xD9A1, 0xD9FE },
    { 0xDAA1, 0xDAFE },
    { 0xDBA1, 0xDBFE },
    { 0xDCA1, 0xDCFE },
    { 0xDDA1, 0xDDFE },
    { 0xDEA1, 0xDEFE },
    { 0xDFA1, 0xDFFE },
    { 0xE0A1, 0xE0FE },
    { 0xE1A1, 0xE1FE },
    { 0xE2A1, 0xE2FE },
    { 0xE3A1, 0xE3FE },
    { 0xE4A1, 0xE4FE },
    { 0xE5A1, 0xE5FE },
    { 0xE6A1, 0xE6FE },
    { 0xE7A1, 0xE7FE },
    { 0xE8A1, 0xE8FE },
    { 0xE9A1, 0xE9FE },
    { 0xEAA1, 0xEAFE },
    { 0xEBA1, 0xEBFE },
    { 0xECA1, 0xECFE },
    { 0xEDA1, 0xEDFE },
    { 0xEEA1, 0xEEFE },
    { 0xEFA1, 0xEFFE },
    { 0xF0A1, 0xF0FE },
    { 0xF1A1, 0xF1FE },
    { 0xF2A1, 0xF2FE },
    { 0xF3A1, 0xF3FE },
    { 0xF4A1, 0xF4FE },
    { 0xF5A1, 0xF5FE },
    { 0xF6A1, 0xF6FE },
    { 0xF7A1, 0xF7FE },
    { 0xF8A1, 0xF8FE },
    { 0xF9A1, 0xF9FE },
    { 0xFAA1, 0xFAFE },
    { 0xFBA1, 0xFBFE },
    { 0xFCA1, 0xFCFE },
    { 0xFDA1, 0xFDFE },
};

// generated from http://unicode.org/Public/MAPPINGS/VENDORS/MICSFT/WINDOWS/CP950.TXT
static const CharRange kBig5Ranges[] = {
    { 0xA140, 0xA17E },
    { 0xA1A1, 0xA1FE },
    { 0xA240, 0xA27E },
    { 0xA2A1, 0xA2FE },
    { 0xA340, 0xA37E },
    { 0xA3A1, 0xA3BF },
    { 0xA3E1, 0xA3E1 },
    { 0xA440, 0xA47E },
    { 0xA4A1, 0xA4FE },
    { 0xA540, 0xA57E },
    { 0xA5A1, 0xA5FE },
    { 0xA640, 0xA67E },
    { 0xA6A1, 0xA6FE },
    { 0xA740, 0xA77E },
    { 0xA7A1, 0xA7FE },
    { 0xA840, 0xA87E },
    { 0xA8A1, 0xA8FE },
    { 0xA940, 0xA97E },
    { 0xA9A1, 0xA9FE },
    { 0xAA40, 0xAA7E },
    { 0xAAA1, 0xAAFE },
    { 0xAB40, 0xAB7E },
    { 0xABA1, 0xABFE },
    { 0xAC40, 0xAC7E },
    { 0xACA1, 0xACFE },
    { 0xAD40, 0xAD7E },
    { 0xADA1, 0xADFE },
    { 0xAE40, 0xAE7E },
    { 0xAEA1, 0xAEFE },
    { 0xAF40, 0xAF7E },
    { 0xAFA1, 0xAFFE },
    { 0xB040, 0xB07E },
    { 0xB0A1, 0xB0FE },
    { 0xB140, 0xB17E },
    { 0xB1A1, 0xB1FE },
    { 0xB240, 0xB27E },
    { 0xB2A1, 0xB2FE },
    { 0xB340, 0xB37E },
    { 0xB3A1, 0xB3FE },
    { 0xB440, 0xB47E },
    { 0xB4A1, 0xB4FE },
    { 0xB540, 0xB57E },
    { 0xB5A1, 0xB5FE },
    { 0xB640, 0xB67E },
    { 0xB6A1, 0xB6FE },
    { 0xB740, 0xB77E },
    { 0xB7A1, 0xB7FE },
    { 0xB840, 0xB87E },
    { 0xB8A1, 0xB8FE },
    { 0xB940, 0xB97E },
    { 0xB9A1, 0xB9FE },
    { 0xBA40, 0xBA7E },
    { 0xBAA1, 0xBAFE },
    { 0xBB40, 0xBB7E },
    { 0xBBA1, 0xBBFE },
    { 0xBC40, 0xBC7E },
    { 0xBCA1, 0xBCFE },
    { 0xBD40, 0xBD7E },
    { 0xBDA1, 0xBDFE },
    { 0xBE40, 0xBE7E },
    { 0xBEA1, 0xBEFE },
    { 0xBF40, 0xBF7E },
    { 0xBFA1, 0xBFFE },
    { 0xC040, 0xC07E },
    { 0xC0A1, 0xC0FE },
    { 0xC140, 0xC17E },
    { 0xC1A1, 0xC1FE },
    { 0xC240, 0xC27E },
    { 0xC2A1, 0xC2FE },
    { 0xC340, 0xC37E },
    { 0xC3A1, 0xC3FE },
    { 0xC440, 0xC47E },
    { 0xC4A1, 0xC4FE },
    { 0xC540, 0xC57E },
    { 0xC5A1, 0xC5FE },
    { 0xC640, 0xC67E },
    { 0xC940, 0xC97E },
    { 0xC9A1, 0xC9FE },
    { 0xCA40, 0xCA7E },
    { 0xCAA1, 0xCAFE },
    { 0xCB40, 0xCB7E },
    { 0xCBA1, 0xCBFE },
    { 0xCC40, 0xCC7E },
    { 0xCCA1, 0xCCFE },
    { 0xCD40, 0xCD7E },
    { 0xCDA1, 0xCDFE },
    { 0xCE40, 0xCE7E },
    { 0xCEA1, 0xCEFE },
    { 0xCF40, 0xCF7E },
    { 0xCFA1, 0xCFFE },
    { 0xD040, 0xD07E },
    { 0xD0A1, 0xD0FE },
    { 0xD140, 0xD17E },
    { 0xD1A1, 0xD1FE },
    { 0xD240, 0xD27E },
    { 0xD2A1, 0xD2FE },
    { 0xD340, 0xD37E },
    { 0xD3A1, 0xD3FE },
    { 0xD440, 0xD47E },
    { 0xD4A1, 0xD4FE },
    { 0xD540, 0xD57E },
    { 0xD5A1, 0xD5FE },
    { 0xD640, 0xD67E },
    { 0xD6A1, 0xD6FE },
    { 0xD740, 0xD77E },
    { 0xD7A1, 0xD7FE },
    { 0xD840, 0xD87E },
    { 0xD8A1, 0xD8FE },
    { 0xD940, 0xD97E },
    { 0xD9A1, 0xD9FE },
    { 0xDA40, 0xDA7E },
    { 0xDAA1, 0xDAFE },
    { 0xDB40, 0xDB7E },
    { 0xDBA1, 0xDBFE },
    { 0xDC40, 0xDC7E },
    { 0xDCA1, 0xDCFE },
    { 0xDD40, 0xDD7E },
    { 0xDDA1, 0xDDFE },
    { 0xDE40, 0xDE7E },
    { 0xDEA1, 0xDEFE },
    { 0xDF40, 0xDF7E },
    { 0xDFA1, 0xDFFE },
    { 0xE040, 0xE07E },
    { 0xE0A1, 0xE0FE },
    { 0xE140, 0xE17E },
    { 0xE1A1, 0xE1FE },
    { 0xE240, 0xE27E },
    { 0xE2A1, 0xE2FE },
    { 0xE340, 0xE37E },
    { 0xE3A1, 0xE3FE },
    { 0xE440, 0xE47E },
    { 0xE4A1, 0xE4FE },
    { 0xE540, 0xE57E },
    { 0xE5A1, 0xE5FE },
    { 0xE640, 0xE67E },
    { 0xE6A1, 0xE6FE },
    { 0xE740, 0xE77E },
    { 0xE7A1, 0xE7FE },
    { 0xE840, 0xE87E },
    { 0xE8A1, 0xE8FE },
    { 0xE940, 0xE97E },
    { 0xE9A1, 0xE9FE },
    { 0xEA40, 0xEA7E },
    { 0xEAA1, 0xEAFE },
    { 0xEB40, 0xEB7E },
    { 0xEBA1, 0xEBFE },
    { 0xEC40, 0xEC7E },
    { 0xECA1, 0xECFE },
    { 0xED40, 0xED7E },
    { 0xEDA1, 0xEDFE },
    { 0xEE40, 0xEE7E },
    { 0xEEA1, 0xEEFE },
    { 0xEF40, 0xEF7E },
    { 0xEFA1, 0xEFFE },
    { 0xF040, 0xF07E },
    { 0xF0A1, 0xF0FE },
    { 0xF140, 0xF17E },
    { 0xF1A1, 0xF1FE },
    { 0xF240, 0xF27E },
    { 0xF2A1, 0xF2FE },
    { 0xF340, 0xF37E },
    { 0xF3A1, 0xF3FE },
    { 0xF440, 0xF47E },
    { 0xF4A1, 0xF4FE },
    { 0xF540, 0xF57E },
    { 0xF5A1, 0xF5FE },
    { 0xF640, 0xF67E },
    { 0xF6A1, 0xF6FE },
    { 0xF740, 0xF77E },
    { 0xF7A1, 0xF7FE },
    { 0xF840, 0xF87E },
    { 0xF8A1, 0xF8FE },
    { 0xF940, 0xF97E },
    { 0xF9A1, 0xF9FE },
};

static bool charMatchesEncoding(int ch, const CharRange* encodingRanges, int rangeCount) {
    // Use binary search to see if the character is contained in the encoding
    int low = 0;
    int high = rangeCount;

    while (low < high) {
        int i = (low + high) / 2;
        const CharRange* range = &encodingRanges[i];
        if (ch >= range->first && ch <= range->last)
            return true;
        if (ch > range->last)
            low = i + 1;
        else
            high = i;
    }

    return false;
}

/**
 * Return true if ch can legally start a two byte sequence representing a
 * character encoded in encoding.
 *
 * It only supports checking a limited number of encodings. Other encodings
 * will return false regardless of the value of ch and regardless of if the
 * encoding can yield two bytes per character or not.
 */
static bool isTwoByteChar(char ch, Encoding encoding) {
    switch (encoding) {
        case kEncodingShiftJIS:
            if (ch >= 0x80 && !(ch >= 0xa1 && ch <= 0xdf)) {
                return true;
            }
            break;
        case kEncodingGBK:
            if (ch > 0x80) {
                return true;
            }
            break;
        case kEncodingBig5:
        case kEncodingEUCKR:
            if (ch > 0x7F) {
                return true;
            }
            break;
        default:
            break;
    }

    return false;
}

/**
 * Get the CharRange and its size for the selected encoding
 */
static void getCharRange(Encoding encoding, const CharRange** charRange, int* size) {
    *charRange = NULL;
    *size = -1;
    switch (encoding) {
        case kEncodingShiftJIS:
            *size = ARRAY_SIZE(kShiftJISRanges);
            *charRange = kShiftJISRanges;
            break;
        case kEncodingGBK:
            *size = ARRAY_SIZE(kGBKRanges);
            *charRange = kGBKRanges;
            break;
        case kEncodingBig5:
            *size = ARRAY_SIZE(kBig5Ranges);
            *charRange = kBig5Ranges;
            break;
        case kEncodingEUCKR:
            *size = ARRAY_SIZE(kEUCKRRanges);
            *charRange = kEUCKRRanges;
            break;
        default:
            break;
    }
}

/**
 * Convert the system locale string into desired encoding
 */
static Encoding localeToEncoding(const char *locale) {
    Encoding encoding = kEncodingNone;

    if (!strncmp(locale, "ja", 2)) {        // Japanese
        encoding = kEncodingShiftJIS;
    } else if (!strncmp(locale, "ko", 2)) { // Korean
        encoding = kEncodingEUCKR;
    } else if (!strncmp(locale, "th", 2)) { // Thai
        encoding = kEncodingCP874;
    } else if (!strncmp(locale, "ru", 2) || // Russian
               !strncmp(locale, "uk", 2) || // Ukrainan
               !strncmp(locale, "bg", 2) || // Bulgarian
               !strncmp(locale, "mk", 2)) { // Macedonian
        encoding = kEncodingCP1251;
    } else if (!strncmp(locale, "zh", 2)) { // Chinese
        if (!strncmp(locale, "zh_CN", 5)) {
            encoding = kEncodingGBK;        // Simplified chinese (mainland China)
        } else {
            // assume traditional for non-mainland Chinese locales (Taiwan, Hong Kong, Singapore)
            encoding = kEncodingBig5;
        }
    }
    return encoding;
}

/**
 * Get UConverters for [encoding]->[UTF-8] conversion.
 */
static bool getUConverters(UConverter** srcConv, UConverter** utf8Conv, Encoding encoding) {
    UErrorCode status = U_ZERO_ERROR;

    *utf8Conv = ucnv_open("UTF-8", &status);
    if (U_FAILURE(status)) {
        ALOGE("Could not open UConverter for encoding UTF-8");
        return false;
    }

    switch (encoding) {
        case kEncodingShiftJIS:
            *srcConv = ucnv_open("shift-jis", &status);
            break;
        case kEncodingGBK:
            *srcConv = ucnv_open("gbk", &status);
            break;
        case kEncodingBig5:
            *srcConv = ucnv_open("Big5", &status);
            break;
        case kEncodingEUCKR:
            *srcConv = ucnv_open("EUC-KR", &status);
            break;
        case kEncodingCP874:
            *srcConv = ucnv_open("windows-874-2000", &status);
            break;
        case kEncodingCP1251:
            *srcConv = ucnv_open("windows-1251", &status);
            break;
        default:
            ALOGE("could not find UConverter for encoding 0x%08x", encoding);
    }

    if (U_FAILURE(status) || *srcConv == NULL) {
        ucnv_close(*utf8Conv);
        *utf8Conv = NULL;
        return false;
    }

    return true;
}

AutoDetect::AutoDetect(int stringsEstimate, const char *locale)
    : mLocaleEncoding(kEncodingNone),
      mAddedStrings(NULL),
      mAddedStringSize(0),
      mAddedStringLen(0) {
    if (locale) {
        setLocale(locale);
    } else {
        // Read system locale setting from system property and format into proper
        // locale string, for example "ja_JP", "ko_KR","zh_HK", "zh_CN" or "zh_TW".
        char locale_value[PROPERTY_VALUE_MAX] = "";

        if (property_get("persist.sys.locale", locale_value, NULL) > 0) {
            const char* splitter_pos = strstr(locale_value, "-");
            if (splitter_pos != NULL) {
                const char* region_value = splitter_pos + 1;
                const size_t len = splitter_pos - locale_value;
                String8 locale(locale_value, len);
                locale.append("_");
                locale.append(region_value);
                setLocale(locale.string());
            }
        }
    }

    // No need to store strings unless we will attempt to detect a two-byte encoding.
    if ((mLocaleEncoding & ~kOneByteEncodings) && stringsEstimate > 0) {
        mAddedStrings = (char *)malloc(stringsEstimate);
        if (mAddedStrings) {
            mAddedStringSize = stringsEstimate;
        }
    }
}

AutoDetect::~AutoDetect() {
    if (mAddedStrings) {
        free(mAddedStrings);
    }
}

void AutoDetect::setLocale(const char *locale) {
    mLocaleEncoding = localeToEncoding(locale);
}

Encoding AutoDetect::getLocaleEncoding() const {
    return mLocaleEncoding;
}

bool AutoDetect::verifyEncoding(const char* str, int numBytes, Encoding encoding) const {
    const CharRange* charRange;
    int size;

    if (encoding & kOneByteEncodings) {
        return true;
    }

    getCharRange(encoding, &charRange, &size);
    if (charRange == NULL || size < 0) {
        return false;
    }

    uint8_t ch1 = 0, ch2 = 0;
    const char* endOfStr = str + numBytes;
    while (str < endOfStr) {
        ch1 = *str++;
        if (isTwoByteChar(ch1, encoding)) {
            ch2 = *str++;
            int ch = (int) ch1 << 8 | (int) ch2;
            if (!charMatchesEncoding(ch, charRange, size)) {
                return false;
            }
        }
    }

    return true;
}

Encoding AutoDetect::possibleEncodings(const char* str, int numBytes) const
{
    Encoding result = kEncodingAll;

    // Start with highest encoding bit, loop to lowest.
    Encoding enc = (Encoding)(kEncodingAll + 1);
    do {
        enc = (Encoding)(enc >> 1);
        if (!verifyEncoding(str, numBytes, enc)) {
            result = (Encoding)(result & ~enc);
        }
    } while (enc > 1);

    return result;
}

Encoding AutoDetect::suggestEncoding(const char* str, int numBytes) const {
    // Attempt to guess which encoding is the best match for the given string, in order:
    //
    // 1) If system locale matches a one-byte encoding, use it directly (since detection is not
    //    possible for one-byte encodings).
    // 2) If system locale matches a two-byte encoding and if the string is valid in that encoding,
    //    use it.
    // 3) Special case for Chinese encodings since we support both Big5 and GBK: if system locale
    //    if Big5 but doesn't match, but GBK does, suggest it (and vice versa).
    // 4) None of the supported encodings possible.
    //
    // Note: It's easy to assume that if only one two-byte encoding matches, then it could be a
    // reasonable guess. However, it turns out that e.g. a text encoded in TIS-620 also has a high
    // probablility of mapping against Shift-JIS.

    Encoding suggestion = kEncodingNone;

    if (str != NULL) {
        if (mLocaleEncoding & kOneByteEncodings) {
            suggestion = mLocaleEncoding;
        } else if (mLocaleEncoding & ~kOneByteEncodings) {
            if (verifyEncoding(str, numBytes, mLocaleEncoding)) {
                suggestion = mLocaleEncoding;
            } else if (mLocaleEncoding == kEncodingBig5 &&
                    verifyEncoding(str, numBytes, kEncodingGBK)) {
                suggestion = kEncodingGBK;
            } else if (mLocaleEncoding == kEncodingGBK &&
                    verifyEncoding(str, numBytes, kEncodingBig5)) {
                suggestion = kEncodingBig5;
            }
        }
    }

    return suggestion;
}

void AutoDetect::addString(const char* str, int numBytes) {
    // No need to store strings unless we will attempt to detect a two-byte encoding.
    if (str != NULL && mLocaleEncoding & ~kOneByteEncodings) {
        if (mAddedStringSize < mAddedStringLen + numBytes + 2) {
            // Grow space
            while (mAddedStringSize < mAddedStringLen + numBytes + 2) {
                mAddedStringSize += 128;
            }
            mAddedStrings = (char*)realloc(mAddedStrings, mAddedStringSize);
        }

        if (mAddedStrings) {
            if (mAddedStringLen > 0) {
                *(mAddedStrings + mAddedStringLen) = ' '; // Add a space bar between the strings
                mAddedStringLen++;
            }

            memcpy(mAddedStrings + mAddedStringLen, str, numBytes);
            mAddedStringLen += numBytes;

            *(mAddedStrings + mAddedStringLen) = 0; // Zero terminate for safety
        }
    }
}

Encoding AutoDetect::suggestEncoding() const {
    return suggestEncoding(mAddedStrings, mAddedStringLen);
}

bool AutoDetect::convertToUTF8(const char* str, int numBytes, String8 *s, Encoding encoding)
        const {
    bool result = false;
    if (str != NULL) {
        size_t bufsize = numBytes * 3 + 1;
        char* const buffer = new char[bufsize];

        if (buffer) {
            UConverter *utf8ConvClone = NULL;
            UConverter *srcConvClone = NULL;
            if (getUConverters(&srcConvClone, &utf8ConvClone, encoding)) {
                UErrorCode status = U_ZERO_ERROR;
                char* target = buffer;
                // keep space for terminating zero away from ucnv_convertEx()
                char* const targetLimit = target + bufsize -1;
                ucnv_convertEx(utf8ConvClone, srcConvClone,
                        &target, targetLimit,
                        &str, str + numBytes,
                        NULL, NULL, NULL, NULL,
                        TRUE, TRUE,
                        &status);

                if (U_FAILURE(status)) {
                    ALOGE("ucnv_convertEx failed: %d\n", status);
                    s->setTo("???", 3);
                } else {
                    // zero terminate
                    *target = 0;
                    // strip trailing spaces
                    while (--target > buffer && *target == ' ') {
                        *target = 0;
                    }
                    // skip leading spaces
                    char *start = buffer;
                    while (*start == ' ') {
                        start++;
                    }
                    s->setTo(start);
                    result = true;
                }
                ucnv_close(utf8ConvClone);
                ucnv_close(srcConvClone);
            } else {
                s->setTo("???", 3);
            }
            delete[] buffer;
        } else {
            ALOGE("cannot use ucnv_convertEx()");
            s->setTo("???", 3);
        }
    }

    return result;
}

}  // namespace android
