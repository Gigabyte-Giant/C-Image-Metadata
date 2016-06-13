//
// JPEGMetaExtractor
// Include/ExifTypes.h
//
#ifndef __EXIFEXTRACTC_EXIFTYPES_H__
#define __EXIFEXTRACTC_EXIFTYPES_H__

#include <stdint.h>

#define EXIF_BYTE      1
#define EXIF_ASCII     2
#define EXIF_SHORT     3
#define EXIF_LONG      4
#define EXIF_RATIONAL  5
#define EXIF_UNDEFINED 7
#define EXIF_SLONG     9
#define EXIF_SRATIONAL 10

typedef uint16_t ExifShort;
typedef uint32_t ExifLong;

typedef struct exifRational
{
    ExifLong numerator;
    ExifLong denominator;
} ExifRational;

#endif