/*
    # TGA2Bitplanes
    A tool written by LEANDRO CALIL DUARTE
    This will parse 24 or 32-bit  TGA files and make  them
    SEGA Master System 4-bits-per-pixel bitplanes-friendly
*/
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef uint8_t  BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;

// Slightly adapted from https://www.fileformat.info/format/tga/egff.htm
// WARNING: this program reads this full header in one go and the use of
// packed  structs  (to  prevent  padding)  is  implementation-specific!
// Although my tackle cannot  be considered  as compliant to major  best
// practices and affects code portability, I have  chosen to go the  GCC
// way   with  the   use  of  the   __attribute__((packed))   extension.
struct __attribute__((packed)) tgaHeader_t {
  BYTE idLength;        /* 00h  Size of Image ID field */
  BYTE colorMapType;    /* 01h  Color map type */
  BYTE imageType;       /* 02h  Image type code */
  WORD cMapStart;       /* 03h  Color map origin */
  WORD cMapLength;      /* 05h  Color map length */
  BYTE cMapDepth;       /* 07h  Depth of color map entries */
  WORD xOffset;         /* 08h  X origin of image */
  WORD yOffset;         /* 0Ah  Y origin of image */
  WORD width;           /* 0Ch  Width of image */
  WORD height;          /* 0Eh  Height of image */
  BYTE pixelDepth;      /* 10h  Image pixel size */
  BYTE imageDescriptor; /* 11h  Image descriptor byte */
} tgaHeader;

struct __attribute__((packed)) pixel_t {
    BYTE b;
    BYTE g;
    BYTE r;
    // BYTE a; //////////////////////////////////////////
    // The above line is  commented out fully on  purpose
    // TGA encoding for the picture is going to be either
    // BGRA (32 bits) or BGR (24 bits). Color data  (RGB)
    // is stored low-order byte first (little-endian) and
    // attribute byte (A) is a separate, individual thing
};

struct pixel_t *raster=NULL;
struct pixel_t palette[16]={0};

int main(int argc, char *argv[]) {
    FILE *fp=NULL;
    DWORD bytesToRead=0, bytesActuallyRead=0;
    int attrByteSkip=0;
    BYTE *offset=NULL; // general purpose pointer for iterations
    printf("# TGA2Bitplanes\n");
    printf("A tool written by LEANDRO C. DUARTE\n");
    printf("             >>>> leandro_calil@hotmail.com\n");
    printf("This will parse 24 or 32-bit TGA files and make them\n");
    printf("SEGA Master System 4-bits-per-pixel bitplanes-friendly\n\n");
    if(argc != 2) {
        printf("Usage: %s [SOURCE TGA FILE]\n", argv[0]);
        return -1;
    }
    if((fp = fopen(argv[1], "rb")) == NULL) {
        printf("Unable to open file \"%s\"!\n", argv[1]);
        return -1;
    }
    fread(&tgaHeader, sizeof(struct tgaHeader_t), 1, fp); // fetches the header
    if((tgaHeader.colorMapType != 0) || (tgaHeader.imageType != 2)) {
        printf("Program aborted! Image has either indexed colors or RLE compression.\n");
        fclose(fp);
        return -1;
    }
    if(((tgaHeader.width % 8) != 0) || ((tgaHeader.height % 8) != 0)) {
        printf("Program aborted! Image has either width or height non multiple of 8.\n");
        fclose(fp);
        return -1;
    }
    if((tgaHeader.width < 128) || (tgaHeader.height < 16)) {
        printf("Program aborted! Image is too small to build tiles from.\n");
        fclose(fp);
        return -1;
    }
    fseek(fp, tgaHeader.idLength+tgaHeader.cMapLength, SEEK_CUR); // skips id and color map, if any
    bytesToRead = tgaHeader.pixelDepth / 8 * tgaHeader.width * tgaHeader.height;
    attrByteSkip = ((tgaHeader.pixelDepth == 32) ? 1 : 0); // should we skip color attributes?
    if((raster = malloc(bytesToRead)) == NULL) {
        printf("Program aborted! Memory allocation for image raster failed.\n");
        fclose(fp);
        return -1;
    }
    if((bytesActuallyRead=(DWORD)fread(raster, sizeof(BYTE), bytesToRead, fp)) < bytesToRead) {
        printf("Program aborted! End of file prematurely reached at raster offset %d.\n", bytesActuallyRead);
        fclose(fp);
        return -1;
    }
    fclose(fp);
    offset = (BYTE *)raster; // we'll begin fetching palette bytes from the beginning
    for(int i=0;i<16;i++) {
        palette[i].b = *(offset++);
        palette[i].g = *(offset++);
        palette[i].r = *(offset++);
        offset += attrByteSkip; // skips attributes, in case that byte is present
        offset += (7 * tgaHeader.pixelDepth/8); // moves 7 columns forwards
    }
    offset = (BYTE *)raster; // resets offset to the beginning of the raster
    ////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////// TEMPORARY TESTING CODE FROM THIS POINT ON ///////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////
    printf("Pointer is %p\n", offset);
    offset += (8*8*16*(tgaHeader.pixelDepth/8)); // moves forwards to the first actual pixel
    printf("Pointer is %p\n", offset);
    FILE *dest = fopen("raw.img", "wb");
    BYTE r, g, b;
    for(DWORD i=0;i<(tgaHeader.width*(tgaHeader.height-8));i++) {
        b = *(offset++);
        g = *(offset++);
        r = *(offset++);
        offset+=attrByteSkip;
        fputc(r, dest);
        fputc(g, dest);
        fputc(b, dest);
    }
    fclose(dest);
    return 0;
}