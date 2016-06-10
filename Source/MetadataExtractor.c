//
// JPEGMetaExtractor
// Source/MetadataExtractor.c
//
#include "MetadataExtractor.h"

ExifShort getShort
(
    void *pShort,
    int fileByteOrder
)
{
    uint16_t swappedValue = 0;
    
    unsigned char left  = *((unsigned char *) pShort + 0);
    unsigned char right = *((unsigned char *) pShort + 1);
    
    if (BYTE_ORDER_INTEL == fileByteOrder)
    {
        swappedValue |= right << 8;
        swappedValue |= left;
    }
    else if (BYTE_ORDER_MOTOROLA == fileByteOrder)
    {
        swappedValue |= left << 8;
        swappedValue |= right;
    }
    
    return swappedValue;
}

ExifLong getLong
(
    void *pLong,
    int fileByteOrder
)
{
    int32_t swappedValue = 0x0;
    
    if (BYTE_ORDER_INTEL == fileByteOrder)
    {
        swappedValue |= *((char *) pLong + 3) << 24;
        swappedValue |= *((unsigned char *) pLong + 2) << 16;
        swappedValue |= *((unsigned char *) pLong + 1) << 8;
        swappedValue |= *((unsigned char *) pLong + 0) << 0;
    }
    else if (BYTE_ORDER_MOTOROLA == fileByteOrder)
    {
        swappedValue |= *((char *) pLong + 0) << 24;
        swappedValue |= *((unsigned char *) pLong + 1) << 16;
        swappedValue |= *((unsigned char *) pLong + 2) << 8;
        swappedValue |= *((unsigned char *) pLong + 3) << 0;
    }
    
    return ((uint32_t) swappedValue) & 0xffffffff;
}

int getTypeBytes
(
    int typeId
)
{
    switch (typeId)
    {
        case EXIF_BYTE:
        case EXIF_ASCII:
        case EXIF_UNDEFINED:
            return 1;
        
        case EXIF_SHORT:
            return 2;
        
        case EXIF_LONG:
        case EXIF_SLONG:
            return 4;
        
        case EXIF_RATIONAL:
        case EXIF_SRATIONAL:
            return 8;
        
        default:
            return 0;
    }
}

MetadataAttributesContainer *extractMetadata
(
    const char *pImageFilePath
)
{
    MetadataAttributesContainer *pMetadataContainer = NULL;
    FILE *pImageFile = NULL;
    char *pAppSeg1 = NULL;
    
    // For those of you whom can't read hexadecimal, this lovely value, when
    //  stored in a 32-bit-signed-integer variable, is -1 in decimal.
    int32_t fileByteOrder = 0xFFFFFFFF;
    
    // This variable will be used to keep track of our Exif offset.
    // uint32_t exifOffset = 0x0;
    
    // Make sure we've been provided with a "valid" file path
    if (NULL == pImageFilePath)
        return NULL;
    
    // Make sure we can open the file without any problems
    if (NULL == (pImageFile = fopen(pImageFilePath, "rb")))
        return NULL;
    
    // Make sure the current file is of valid Exif format
    if (!isValidEXIFFile(pImageFile))
        return NULL;
    
    pMetadataContainer = allocateMetadataAttributesContainer();
    
    // The first thing that we're going to do, is attempt to determine the
    //  file's byte-order. If we don't know the byte-order of the file, all
    //  values are basically useless to us.
    //
    // We're also going to check to make sure that we have a valid byte-order.
    if (0xFFFFFFFF == (fileByteOrder = getFileByteOrder(pImageFile)))
        return NULL;
    
    // The next thing that we're going to do, is read the APP1 segment into
    //  memory.
    pAppSeg1 = getExifAttributeInfoSegment(pImageFile, fileByteOrder);
    
    // If the 'pAppSeg1' variable is NULL, that means we were unable to read
    //  the APP1 segment into memory, therefore we won't do anything else.
    if (NULL == pAppSeg1)
        return NULL;
    
    // We will now attempt to "parse" the APP1 segment. In doing so, we will
    //  be able to pull values from it.
    parseExifAttributeInfoSegment(pMetadataContainer, pAppSeg1, fileByteOrder);
    
    free(pAppSeg1);
    
    return pMetadataContainer;
}

bool isValidEXIFFile
(
    FILE *pImageFile
)
{
    bool bValidExifFile = false;
    
    // If the file-pointer that we get passed, is 'NULL', then we needn't go
    //  any further, otherwise things will break, and we'll get blamed for that.
    if (NULL == pImageFile)
        return false;
    
    // Preserve the current offset in the file, as we're going to jump to the
    //  start of the file, therefore overriding any existing offset.
    long userFileOffset = ftell(pImageFile);
    
    // Jump to the beginning of the file, overriding any existing position that
    //  may have been set by the user.
    fseek(pImageFile, 0x0, SEEK_SET);
    
    // Attempt to determine if the current file is a valid Exif file. According
    //  to the Exif 2.2 specification (heading 4.7), an Exif file begins with
    //  the 2-byte marker '0xFFD8'.
    bValidExifFile = (
        PAD_BYTE == fgetc(pImageFile) &&
        SOI_BYTE == fgetc(pImageFile)
    );
    
    // Move the file's position pointer back to where it was before we jumped
    //  to the beginning.
    fseek(pImageFile, userFileOffset, SEEK_SET);
    
    // Return a boolean value indicating whether or not this appears to be a
    //  valid Exif file ('false' if not, 'true' otherwise).
    return bValidExifFile;
}

int32_t getFileByteOrder
(
    FILE *pImageFile
)
{
    int32_t fileByteOrder  = 0xFFFFFFFF;
    bool bFoundApp1Segment = false;
    
    // If the file-pointer that we get passed, is 'NULL', then we needn't go
    //  any further, otherwise things will break, and we'll get blamed for that.
    if (NULL == pImageFile)
        return 0xFFFFFFFF;
    
    // Preserve the current offset in the file, as we're going to be jumping
    //  around in the file, therefore changing the current offset. We'll revert
    //  back to this offset, later on.
    long userFileOffset = ftell(pImageFile);
    
    // There's a chance that the offset position in the current file, has been
    //  changed already, therefore we're going to jump back to the very start
    //  of the file. This will let us iterate through every byte in the file,
    //  from the beginning.
    fseek(pImageFile, 0x0, SEEK_SET);
    
    // The byte-order of the file, is located at the start of what's called the
    //  TIFF header. The TIFF header is located 8 bytes from the start of the
    //  APP1 segment.
    //
    // What we're going to do, is loop until we've found the APP1 segment, and,
    //  if we find it, we'll move forward another 8 bytes, and try to compute
    //  the byte-order of the file.
    while (!bFoundApp1Segment)
    {
        int currentByte  = fgetc(pImageFile);
        bool bHasPadding = (PAD_BYTE == currentByte);
        
        currentByte = fgetc(pImageFile);
        
        // The APP1 segment marker is prefixed with a padding byte (0xFF), so if
        //  we saw one, and the current byte matches that of the APP1 marker,
        //  we've almost located the APP1 segment. The real APP1 segment should
        //  contain 5 bytes that make up the string 'Exif\0'. If we find that,
        //  then we've successfully located the APP1 segment.
        if (bHasPadding && APP1_TAG == currentByte)
        {
            // This is only important because if we don't do it, our offsets
            //  will get screwed over, which isn't hard to fix, but it makes
            //  more sense to just go ahead and do this.
            fseek(pImageFile, 0x2, SEEK_CUR);
            
            // Before we do any string comparison, we'll go ahead and read the
            //  next 5 bytes from the file, into memory. This will make it a
            //  bit easier for us to deal with.
            char *pExifString = (char *) malloc(sizeof(char) * 5);
            fread(pExifString, 0x1, 0x5, pImageFile);
            
            // Here, we're checking to make sure that each of the 5 bytes in
            //  the 'pExifString' variable, match that of the null-terminated
            //  'Exif' string.
            if (!memcmp(pExifString, "Exif", 0x5))
            {
                // According to the Exif 2.2 specification, a file's byte-order
                //  is signified using either the string 'II' or 'MM'. This
                //  variable will be used to temporarily store that code.
                char *pByteOrderCode = (char *) malloc(sizeof(char) * 2);
                
                // We're now going to jump 1 byte ahead of the current offset
                //  in the file. This will bring the offset in the file, to the
                //  point in the file that contains the byte-order code.
                fseek(pImageFile, 0x1, SEEK_CUR);
                
                // The next 2 bytes in the file are used to specify the
                //  byte-order code of the file. We will now read that code into
                //  our temporary string.
                fread(pByteOrderCode, 0x1, 0x2, pImageFile);
                
                // According to the Exif 2.2 specification, any one of the
                //  following codes can be used:
                //  - 'MM': Motorola Byte-Order (Big-Endian)
                //  - 'II': Intel Byte-Order (Little-Endian)
                if (!memcmp(pByteOrderCode, "MM", 0x2))
                    fileByteOrder = BYTE_ORDER_MOTOROLA;
                else if (!memcmp(pByteOrderCode, "II", 0x2))
                    fileByteOrder = BYTE_ORDER_INTEL;
                    
                free(pByteOrderCode);
                
                bFoundApp1Segment = true;
            }
            
            free(pExifString);
        }
    }
    
    // Move the file's position pointer back to where it was before we got a
    //  hold of it.
    fseek(pImageFile, userFileOffset, SEEK_SET);
    
    return fileByteOrder;
}

char *getExifAttributeInfoSegment
(
    FILE *pImageFile,
    int32_t fileByteOrder
)
{
    char *pExifAttributeInfoSegment = NULL;
    bool bFoundApp1Segment = false;
    
    // If the file-pointer that we get passed, is 'NULL', then we needn't go
    //  any further, otherwise things will break, and we'll get blamed for that.
    if (NULL == pImageFile)
        return NULL;
    
    // Preserve the current offset in the file, as we're going to be jumping
    //  around in the file, therefore changing the current offset. We'll revert
    //  back to this offset, later on.
    long userFileOffset = ftell(pImageFile);
    
    // There's a chance that the offset position in the current file, has been
    //  changed already, therefore we're going to jump back to the very start
    //  of the file. This will let us iterate through every byte in the file,
    //  from the beginning.
    fseek(pImageFile, 0x0, SEEK_SET);
    
    // Until we've found the APP1 segment, we're going to loop through every
    //  byte in the file, looking for it.
    //
    // Once we find something that resembles the APP1 segment, we'll attempt to
    //  verify that it is indeed the APP1 segment, and if it is, we'll allocate
    //  some memory for it, and copy it into memory.
    while (!bFoundApp1Segment)
    {
        long segmentStart = 0x0;
        int currentByte = fgetc(pImageFile);
        bool bHasPadding = (PAD_BYTE == currentByte);
        
        currentByte = fgetc(pImageFile);
        
        if (bHasPadding && APP1_TAG == currentByte)
        {
            // We'll keep track of the current offset in the file, because if
            //  this segment is indeed the real APP1 segment, we'll jump back
            //  to here, later on.
            segmentStart = ftell(pImageFile);
            
            // Here, we're allocating enough memory for 2 bytes of data. These
            //  2 bytes are used to specify the size of this segment. This
            //  information is extremely important, as it's what we'll base our
            //  memory allocation upon.
            unsigned char *pSegmentSize = (unsigned char *) malloc(
                sizeof(unsigned char) * 2);
            
            fread(pSegmentSize, 1, 2, pImageFile);
            
            // Before we do any string comparison, we'll go ahead and read the
            //  next 5 bytes from the file, into memory. This will make it a
            //  bit easier for us to deal with.
            char *pExifString = (char *) malloc(sizeof(char) * 5);
            fread(pExifString, 0x1, 0x5, pImageFile);
            
            // Here, we're checking to make sure that each of the 5 bytes in
            //  the 'pExifString' variable, match that of the null-terminated
            //  'Exif' string.
            if (!memcmp(pExifString, "Exif", 0x5))
            {
                uint16_t segmentSize = getShort(pSegmentSize, fileByteOrder);
                
                // Go back to the start of the segment, as we're going to want
                //  to copy the entire thing into memory.
                fseek(pImageFile, segmentStart, SEEK_SET);
                
                // Allocate the memory that will be used to hold this segment.
                pExifAttributeInfoSegment = (char *) malloc(segmentSize);
                
                // Here, we'll make sure that the memory was allocated
                //  successfully. If so, we'll read this entire segment into
                //  memory.
                if (NULL != pExifAttributeInfoSegment)
                    fread(pExifAttributeInfoSegment, 1, segmentSize, pImageFile);
                
                bFoundApp1Segment = true;
            }
            
            free(pExifString);
            free(pSegmentSize);
        }
    }
    
    fseek(pImageFile, userFileOffset, SEEK_SET);
    
    return pExifAttributeInfoSegment;
}

void parseExifAttributeInfoSegment
(
    MetadataAttributesContainer *pAttributeContainer,
    char *pAttributeInfoSegment,
    int32_t fileByteOrder
)
{
    // This variable, which is used to keep track of our Exif offset, is going
    //  to start with an offset of "0 from the TIFF header". The TIFF header,
    //  is located 6 bytes from the start of our APP1 segment. We'll specify
    //  that, here.
    ExifLong exifOffset = 0x6;
    
    // If the segment that we're passed, is NULL, then we're going to exit,
    //  because we can't do anything else.
    if (NULL == pAttributeInfoSegment)
        return;
    
    // The last 4 bytes of the TIFF header, contain a 4-byte long offset. This
    //  offset is what we'll use to access the 0th IFD. We'll get this offset,
    //  and add it to our existing offset, here.
    exifOffset += getLong((pAttributeInfoSegment + (exifOffset + 0x6)),
        fileByteOrder);
        
    // readIFD(pAttributeContainer, pAttributeInfoSegment, exifOffset, fileByteOrder);
    
    processImageFileDirectory(
        pAttributeContainer,
        (pAttributeInfoSegment + exifOffset),
        exifOffset,
        fileByteOrder
    );
}

// void readIFD
// (
//     MetadataAttributesContainer *pAttributeContainer,
//     char *pAPP1Segment,
//     ExifLong offset,
//     int32_t fileByteOrder
// )
// {
//     // If the segment that we're passed, is NULL, then we're going to exit,
//     //  because we can't do anything else.
//     if (NULL == pAPP1Segment)
//         return;
    
//     // The first two bytes of every IFD, are used to specify the number of
//     //  entries that said IFD contains. We'll fetch that value, here. We'll also
//     //  increase our offset by 2 bytes, to skip the number of entries value
//     //  later on.
//     ExifShort numEntries = getShort((pAPP1Segment + offset), fileByteOrder);
//     offset += 0x4;
    
//     printf("Number of Entries: %d\n", numEntries);
    
//     for (int i = 0; i < numEntries; ++i)
//     {
//         processAttribute(pAttributeContainer, pAPP1Segment,
//             (pAPP1Segment + offset), fileByteOrder);
        
//         // Every IFD entry uses a total of 12 bytes of space. To make sure our
//         //  offset is correct for every entry after this, we'll increase it by
//         //  12 bytes.
//         offset += 0xC;
//     }
    
//     // If this is the root IFD, there should be two more attributes, which
//     //  contain offsets to the EXIF IFD, and the GPS IFD.
//     ExifShort nextIFD = getShort(pAPP1Segment + offset, fileByteOrder);
        
//     if (0x0 != nextIFD)
//     {
//         printf("Next IFD Marker: %04x\n", nextIFD);
        
//         readIFD(
//             pAttributeContainer,
//             pAPP1Segment,
//             0x8 + getLong(pAPP1Segment + (offset + 8), fileByteOrder),
//             fileByteOrder
//         );
//     }
// }

uint32_t getImageFileDirectoryLength
(
    char *pDirectory,
    int32_t fileByteOrder
)
{
    if (NULL == pDirectory)
        return 0x0;
    
    return (0x2 + (getShort((pDirectory + 0x2), fileByteOrder) * 0xC));
}

bool processImageFileDirectory
(
    MetadataAttributesContainer *pAttributesContainer,
    char *pDirectory,
    uint32_t offsetFromTIFF,
    int32_t fileByteOrder
)
{
    uint32_t offset = 0x0;
    
    // It is required that both 'pAttributesContainer' and 'pDirectory' have
    //  valid values. If either of them are NULL, then we're going to
    //  immediately exit the function, returning 'false' to the caller.
    if (NULL == pAttributesContainer || NULL == pDirectory)
        return false;
    
    // The first 2 bytes of every IFD are used to specify the marker for said
    //  IFD. The marker is what we use to identify IFDs. We'll capture that
    //  marker, here. Once the marker has been captured, we'll increase our
    //  offset, by 2.
    // ExifShort marker = getShort((pDirectory + offset), fileByteOrder);
    offset += 0x2;
    
    // printf("IFD Marker: %04x\n", marker);
    
    // The next 2 bytes in the IFD, are used to specify the number of entries
    //  this IFD contains. We'll capture that count, and increase our offset
    //  by 2.
    ExifShort entryCount = getShort((pDirectory + offset), fileByteOrder);
    offset += 0x2;
    
    // The next 'n' (calculated using expression `entryCount * 12`) bytes of the
    //  IFD, contain the actual attributes.
    for (int attributeIndex = 0; attributeIndex < entryCount; ++attributeIndex)
    {
        // MetadataAttribute *pAttribute = NULL;
        
        // ExifShort attributeTag        = 0x0;
        // ExifShort attributeType       = 0x0;
        // ExifLong attributeCount       = 0x0;
        // ExifLong attributeValueBytes  = 0x0;
        // ExifLong attributeValueOffset = 0x0;
        
        // // The first 2 bytes of the attribute, are used to specify the
        // //  attribute's tag, which is what we'll use to identify the attribute.
        // attributeTag = getShort((pDirectory + (offset + 0x0)), fileByteOrder);
        
        // // The next 2 bytes of the attribute, are used to specify the
        // //  attribute's type, which is something we need to know.
        // attributeType = getShort((pDirectory + (offset + 0x2)), fileByteOrder);
        
        // // The next 4 bytes of the attribute, specify the "count of values" for
        // //  this attribute. The count of values, is not the number of bytes,
        // //  instead, it's the count of values that match this attribute's type.
        // attributeCount = getLong((pDirectory + (offset + 0x4)), fileByteOrder);
        
        // // We can compute the number of bytes that this attribute's value will
        // //  use, by multiplying the size of the attribute's type, by the number
        // //  of values.
        // attributeValueBytes = getTypeBytes(attributeType) * attributeCount;
        
        // // The final 4 bytes of the attribute, will hold either the attribute's
        // //  value, or an offset to the attribute's value.
        // //
        // // If the attribute's value will fit within 4 bytes, these 4 bytes will
        // //  contain the actual value. If the attribute's value won't fit in 4
        // //  bytes, these 4 bytes will contain an offset to the attribute's value.
        // if (4 >= attributeValueBytes)
        // {
        //     attributeValueOffset = (offset + 0x8);
        // }
        // else
        // {
        //     uint32_t fullOffset = getLong((pDirectory + (offset + 0x8)),
        //         fileByteOrder);
                
        //     attributeValueOffset = fullOffset - offsetFromTIFF;
        // }
        
        // // Now that we've captured all the values that describe this attribute,
        // //  we're going to (try to) get an attribute that matches this tag,
        // //  from the attributes container.
        // pAttribute = pAttributesContainer->getAttributeByTag(
        //     pAttributesContainer, attributeTag);
        
        // // The 'getAttributeByTag' function returns NULL if it wasn't able to
        // //  find an attribute that matches the specified tag. If it doesn't
        // //  return NULL, that means an attribute with this tag, was registered.
        // if (NULL != pAttribute)
        // {
        //     // TODO...
        // }
        
        // Now, we're going to process this attribute. When we process an
        //  attribute, we're basically doing everything that needs to be done
        //  to capture this attribute's value.
        processAttribute(
            pAttributesContainer,
            pDirectory,
            (pDirectory + offset),
            offsetFromTIFF,
            fileByteOrder
        );
        
        // Since every attribute uses 12 bytes, we need to increase our offset
        //  by 12 after each iteration. We'll do that here.
        offset += 0xC;
    }
    
    return true;
}

// bool processAttribute
// (
//     MetadataAttributesContainer *pAttributesContainer,
//     char *pAttributeInfoSegment,
//     void *pAttribute,
//     int32_t fileByteOrder
// )
// {
//     MetadataAttribute *pMetadataAttribute = NULL;
    
//     ExifShort attributeTag   = getShort(pAttribute, fileByteOrder);
//     ExifShort attributeType  = getShort(pAttribute + 2, fileByteOrder);
//     ExifLong attributeCount  = getLong(pAttribute + 4, fileByteOrder);
//     ExifShort attributeBytes = getTypeBytes(attributeType) * attributeCount;
    
//     printf("Tag: %04x\n", attributeTag);
//     printf("Type: %04x\n", attributeType);
    
//     // Here, we're going to try and get a pointer to the structure for the
//     //  attribute that matches the specified tag. We'll store the attribute's
//     //  value in this structure once we've captured it.
//     pMetadataAttribute = pAttributesContainer->getAttributeByTag(
//         pAttributesContainer,
//         attributeTag
//     );
    
//     // The 'getAttributeByTag' function will return NULL if it can't find an
//     //  attribute that is marked with the specified tag. This will happen
//     //  whenever we try and get an attribute that hasn't been registered with
//     //  the container.
//     //
//     // We'll exit the function, and return 'false' if this is the case.
//     if (NULL == pMetadataAttribute)
//         return false;
    
//     // If the value for this attribute fits within 4 bytes, the value will be
//     //  located 8 bytes from the start of this attribute.
//     //
//     // If the value for this attribute needs more than 4 bytes, the value will
//     //  be located at an offset from the start of the TIFF header. This offset
//     //  will be located 8 bytes from the start of the attribute.
//     if (4 >= attributeBytes)
//     {
//         pMetadataAttribute->pValue = getAttributeValue(
//             (pAttribute + 8),
//             attributeBytes,
//             attributeType,
//             fileByteOrder
//         );
//     }
//     else
//     {
//         pMetadataAttribute->pValue = getAttributeValue(
//             ((pAttributeInfoSegment + 0x8) + getLong(pAttribute + 0x8, fileByteOrder)),
//             attributeBytes,
//             attributeType,
//             fileByteOrder
//         );
//     }
    
//     return true;
// }

bool processAttribute
(
    MetadataAttributesContainer *pAttributesContainer,
    char *pIFD,
    void *pAttributeStart,
    uint32_t offsetFromTIFF,
    int32_t fileByteOrder
)
{
    MetadataAttribute *pMetadataAttribute = NULL;
    
    ExifShort attributeTag        = 0x0;
    ExifShort attributeType       = 0x0;
    ExifLong attributeCount       = 0x0;
    ExifLong attributeValueBytes  = 0x0;
    ExifLong attributeValueOffset = 0x0;
    
    if (NULL == pAttributesContainer || NULL == pIFD || NULL == pAttributeStart)
        return false;
        
    // The first 2 bytes of the attribute, are used to specify the
    //  attribute's tag, which is what we'll use to identify the attribute.
    attributeTag = getShort((pAttributeStart + 0x0), fileByteOrder);
    
    // The next 2 bytes of the attribute, are used to specify the
    //  attribute's type, which is something we need to know.
    attributeType = getShort((pAttributeStart + 0x2), fileByteOrder);
    
    // The next 4 bytes of the attribute, specify the "count of values" for
    //  this attribute. The count of values, is not the number of bytes,
    //  instead, it's the count of values that match this attribute's type.
    attributeCount = getLong((pAttributeStart + 0x4), fileByteOrder);
    
    // We can compute the number of bytes that this attribute's value will
    //  use, by multiplying the size of the attribute's type, by the number
    //  of values.
    attributeValueBytes = getTypeBytes(attributeType) * attributeCount;
    
    // The final 4 bytes of the attribute, will hold either the attribute's
    //  value, or an offset to the attribute's value.
    //
    // If the attribute's value will fit within 4 bytes, these 4 bytes will
    //  contain the actual value. If the attribute's value won't fit in 4
    //  bytes, these 4 bytes will contain an offset to the attribute's value.
    if (4 >= attributeValueBytes)
    {
        attributeValueOffset = 0x8;
    }
    else
    {
        uint32_t fullOffset = getLong((pAttributeStart + 0x8), fileByteOrder);
        
        attributeValueOffset = (fullOffset + 0x8) - offsetFromTIFF;
    }
    
    // Now that we've captured all the values that describe this attribute,
    //  we're going to (try to) get an attribute that matches this tag,
    //  from the attributes container.
    pMetadataAttribute = pAttributesContainer->getAttributeByTag(
        pAttributesContainer, attributeTag);
    
    // The 'getAttributeByTag' function returns NULL if it wasn't able to
    //  find an attribute that matches the specified tag. If it doesn't
    //  return NULL, that means an attribute with this tag, was registered.
    if (NULL != pMetadataAttribute)
    {
        printf("%s (%04x): ", pMetadataAttribute->pName, attributeTag);
        
        pMetadataAttribute->pValue = getAttributeValue(
            (pIFD + attributeValueOffset),
            attributeValueBytes,
            attributeType,
            fileByteOrder
        );
        
        // This is a test, only a test. All I want to do, is make sure I got the
        //  right values.
        switch (attributeType)
        {
            case EXIF_ASCII:
                printf("%s", (char *) pMetadataAttribute->pValue);
                break;
            
            case EXIF_RATIONAL:
                printf("%u/%u", ((ExifRational *) pMetadataAttribute->pValue)->numerator, ((ExifRational *) pMetadataAttribute->pValue)->denominator);
                break;
            
            default:
                printf("Unknown");
                break;
        }
        
        printf("\n");
    }
    
    return true;
}

void *getAttributeValue
(
    void *pValueStart,
    ExifShort valueBytes,
    ExifShort valueType,
    int32_t fileByteOrder
)
{
    void *pValue = NULL;
    
    // If the pointer to the start of the value is NULL, we won't be able to do
    //  anything else, therefore we'll exit the function without doing anything.
    if (NULL == pValueStart)
        return NULL;
    
    // To simplify this a bit, we're going to go ahead and allocate some memory
    //  that we'll 'memcpy' values to, later on.
    pValue = malloc(valueBytes);
    
    // Just a note, we're not using a switch/case here, because we might need
    //  to be able to declare variables, which we won't be able to do within
    //  a switch/case.
    if (EXIF_BYTE == valueType || EXIF_ASCII == valueType)
    {
        memcpy(pValue, pValueStart, valueBytes);
    }
    else if (EXIF_SHORT == valueType)
    {
        // TODO...
    }
    else if (EXIF_LONG == valueType)
    {
        // TODO...
    }
    else if (EXIF_RATIONAL == valueType)
    {
        ExifRational rational;
        
        rational.numerator   = getLong(pValueStart, fileByteOrder);
        rational.denominator = getLong(pValueStart + 4, fileByteOrder);
        
        memcpy(pValue, &rational, valueBytes);
    }
    
    return pValue;
}