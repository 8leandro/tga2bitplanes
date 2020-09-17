///////////////////////// TGA2SMS /////////////////////////
///////////////////////////////////////////////////////////
// A tool by LEANDRO C. DUARTE  * leandro_calil@hotmail.com
// This  will  parse 24 or 32-bit TGA  files and make  them
// SEGA Master System 4bpp planar  graphics format-friendly
///////////////////////////////////////////////////////////
// A more "C-ish" flavor of C++ is adopted if convenient :)
#include <cstdint>
#include <cstdio>
#include <string>

typedef uint8_t  BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;

#define MAX_COLORS              16
#define MIN_RASTER_WIDTH        128
#define MIN_RASTER_HEIGHT       16
#define TILE_WIDTH              8
#define TILE_HEIGHT             8

#define HEADER_SIZE             18
// #defines for the TGA 18-byte header
// Adapted from https://www.fileformat.info/format/tga/egff.htm
#define ID_LENGTH_8             0x00 // 00h  Size of Image ID field
#define COL_MAP_TYPE_8          0x01 // 01h  Color map type
#define IMAGE_TYPE_8            0x02 // 02h  Image type code
#define COL_M_START_16          0x03 // 03h  Color map origin
#define COL_M_LENGTH_16         0x05 // 05h  Color map length
#define COL_M_DEPTH_8           0x07 // 07h  Depth of color map entries
#define X_OFFSET_16	            0x08 // 08h  X origin of image
#define Y_OFFSET_16             0x0A // 0Ah  Y origin of image
#define WIDTH_16                0x0C // 0Ch  Width of image
#define HEIGHT_16               0x0E // 0Eh  Height of image
#define PIXEL_DEPTH_8           0x10 // 10h  Image pixel size
#define IMG_DESCR_8             0x11 // 11h  Image descriptor byte

using namespace std;

// Helper function that ORes LSB as is and MSB 8 bits shifted leftwards
WORD read_word(const BYTE *a) { return *a | ((*(a+1))<<8); }
// Shifting bits 6 times to the right is the same as dividing by 64
// Once r, g and b are shifted and made 2 bits, they're put together
// into a single byte, with the following format: 00BBGGRR
BYTE toBGR8(const BYTE r, const BYTE g, const BYTE b) {
    return (r>>6) | ((g>>6) << 2) | ((b>>6) << 4); }
// Changes the extension of a given file or appends if there is none
/*string changeExtension(const string f, const string ext) {
    if(f.empty()) return "." + ext;
}*/

class TGA_File {
    #define COLOR_PALETTE(c) (color_palette[(c)] & 0xff) // helper macro that gets color and ignores the 9th bit
	private:
        string lastErrorStr = "";
        string file_name = "";
		BYTE header[HEADER_SIZE]={0};
        WORD width=0;
        WORD height=0, actual_height=0; // actual height = height minus upper palette row, if present
        BYTE *raster = nullptr; // this array holds the image "as is"
        BYTE *raster_6b[4] = {nullptr}; // these ones hold as follows: original, x-mirrored, y-mirrored, both axes mirrored
        DWORD sizeOfRaster=0;
        bool skipAttributes=true; // when image has 32 bits per pixel, this is used to skip attributes/alpha byte
        bool hasPalRow=false; // the image is, by default, treated as its first row of tiles not being color palette indexes
        WORD color_palette[MAX_COLORS]={0}; // the least significant 8 bits mean color, the 9th bit serves as a bool "isTaken"
	public:
        ~TGA_File(void) {
            delete[] raster;
            delete[] raster_6b[0]; delete[] raster_6b[1];
            delete[] raster_6b[2]; delete[] raster_6b[3];
        }
        ////////////////////////////////////////
        char *getLastError(void) { return (char *)lastErrorStr.c_str(); }
        ////////////////////////////////////////
        bool isColorIndexed(const BYTE c) {
            for(int i=0;i<MAX_COLORS;i++) {
                if(color_palette[i])
                    // We make use of the COLOR_PALETTE macro here, since we're only interested in 8 bits of data
                    if((COLOR_PALETTE(i)) == c) return true;
            }
            return false;
        }
        ////////////////////////////////////////
        bool assignIndex(const BYTE c) {
            if(isColorIndexed(c)) return false;
            for(int i=0;i<MAX_COLORS;i++) {
                if(!color_palette[i]) {
                    color_palette[i] = 0x100 | c; // sets 9th bit and assigns color
                    return true;
                }
            }
            return false;
        }
        ////////////////////////////////////////
        int sumUniqueColors(void) {
            bool present[0x100]={false};
            int sum=0;
            for(DWORD i=0;i<(width*height);i++) {
                if(!present[raster_6b[0][i]]) {
                    present[raster_6b[0][i]] = true;
                    sum++;
                }
            }
            return sum;
        }
        ////////////////////////////////////////
        bool to8bits(void) {
            BYTE *raster_offset = nullptr;
            BYTE *raster6b_offset = nullptr;
            if(sizeOfRaster==0) return false;
            raster_offset = raster;
            for(DWORD i=0;i<(width*height);i++) { // builds the color palette
                BYTE b,g,r;
                b = *(raster_offset++);
                g = *(raster_offset++);
                r = *(raster_offset++);
                raster_offset += skipAttributes;
                assignIndex(toBGR8(r,g,b));
            }
            try {
                raster_6b[0] = new BYTE[width*actual_height];
                raster_6b[1] = new BYTE[width*actual_height];
                raster_6b[2] = new BYTE[width*actual_height];
                raster_6b[3] = new BYTE[width*actual_height];
            } catch(...) {
                lastErrorStr = "ERROR: memory allocation for raster failed!";
                return false;
            }
            raster_offset = raster + (hasPalRow ? (width * TILE_HEIGHT * (header[PIXEL_DEPTH_8]/8)) : 0);
            raster6b_offset = raster_6b[0];
            for(DWORD i=0;i<(width*actual_height);i++) {
                BYTE b,g,r;
                b = *(raster_offset++);
                g = *(raster_offset++);
                r = *(raster_offset++);
                raster_offset += skipAttributes; // skipAttributes will be 1 if the file holds 32 bits per pixel, 0 otherwise
                *(raster6b_offset++) = toBGR8(r,g,b);
            }
            for(DWORD y=0;y<actual_height;y++) { // stores flipped images
                for(DWORD x=0;x<width;x++) {
                    BYTE pixel = raster_6b[0][y*width+x];
                    raster_6b[1][y*width+(width-1-x)] = pixel;                      // horizontal flipping
                    raster_6b[2][(actual_height-1-y)*width+x] = pixel;              // vertical flipping
                    raster_6b[3][(actual_height-1-y)*width+(width-1-x)] = pixel;    // both
                }
            }
            return true;
        }
        ////////////////////////////////////////
        // The image is treated as not carrying palette index tiles in row zero by default!
	    bool loadFromFile(const string f, const bool hasPalRow = false) {
            FILE *fp;
            DWORD bytesRead=0;
            file_name = f;
            this->hasPalRow = hasPalRow;
            if((fp = fopen(f.c_str(), "rb")) == NULL) {
                lastErrorStr = "ERROR: unable to open file " + f + "!";
                return false;
            }
            if((bytesRead=fread(header, 1, HEADER_SIZE, fp)) < HEADER_SIZE) {
                lastErrorStr = "ERROR: premature end of file at header fetch!\nOFFSET: " + to_string(bytesRead);
                fclose(fp);
                return false;
            }
            if((header[COL_MAP_TYPE_8] != 0) || (header[IMAGE_TYPE_8] != 2)) {
                // COLOR MAP TYPE 8 means the image currently being processed does not have indexed colors
                // IMAGE TYPE 2 means no RLE compression. Both #defined constants are header offsets
                lastErrorStr = "ERROR: image has either indexed colors or RLE compression!";
                fclose(fp);
                return false;
            }
            width = read_word(&header[WIDTH_16]);
            height = read_word(&header[HEIGHT_16]);
            actual_height = height - (hasPalRow ? TILE_HEIGHT : 0);
            if(((width % TILE_WIDTH) != 0) || ((height % TILE_HEIGHT) != 0)) {
                lastErrorStr = "ERROR: image has either width or height non multiple of 8!";  
                fclose(fp);
                return false;
            }
            if((width < MIN_RASTER_WIDTH) || (height < MIN_RASTER_HEIGHT)) {
                // the minimum height of 16 was set originally because of the first row (palette indexes)
                lastErrorStr = "ERROR: image is too small to extract tiles from!";
                fclose(fp);
                return false;
            }
            fseek(fp, header[ID_LENGTH_8]+read_word(&header[COL_M_LENGTH_16]), SEEK_CUR); // skips ID and color map, if any
            sizeOfRaster = header[PIXEL_DEPTH_8] / 8 * width * height;
            try {
                raster = new BYTE[sizeOfRaster];
            } catch(...) {
                lastErrorStr = "ERROR: memory allocation for raster failed!";
                fclose(fp);
                return false;
            }
            // The tool relies on true being one and false being zero
            skipAttributes = ((header[PIXEL_DEPTH_8] == 32) ? true : false); // should we skip color attributes (aka ALPHA)?
            if((bytesRead=(DWORD)fread(raster, 1, sizeOfRaster, fp)) < sizeOfRaster) {
                lastErrorStr = "ERROR: premature end of file at raster fetch!\nOFFSET: " + to_string(bytesRead);
                fclose(fp);
                return false;
            }
            fclose(fp);
            to8bits();
            return true;
        }
} tga_file;

int main(int argc, char *argv[]) {
    bool cmdPalRow=false;

    printf( "▄▄▄█████▓  ▄████  ▄▄▄        ██████ ███▄ ▄███▓  ██████ \n"
            "▓  ██▒ ▓▒ ██▒ ▀█▒▒████▄  2 ▒██    ▒▓██▒▀█▀ ██▒▒██    ▒ \n"
            "▒ ▓██░ ▒░▒██░▄▄▄░▒██  ▀█▄  ░ ▓██▄  ▓██    ▓██░░ ▓██▄   \n"
            "░ ▓██▓ ░ ░▓█  ██▓░██▄▄▄▄██   ▒   ██▒██    ▒██   ▒   ██▒\n"
            "  ▒██▒ ░ ░▒▓███▀▒ ▓█   ▓██▒▒██████▒▒██▒   ░██▒▒██████▒▒\n"
            "  ▒ ░░    ░▒   ▒  ▒▒   ▓▒█░▒ ▒▓▒ ▒ ░ ▒░   ░  ░▒ ▒▓▒ ▒ ░\n"
            "    ░      ░   ░   ▒   ▒▒ ░░ ░▒  ░ ░  ░      ░░ ░▒  ░ ░\n"
            "  ░      ░ ░   ░   ░   ▒   ░  ░  ░ ░      ░   ░  ░  ░  \n"
            "               ░       ░  ░      ░        ░         ░  \n"
	        "       A helper tool written by LEANDRO C. DUARTE      \n"
            "          >>>> leandro_calil@hotmail.com <<<<          \n"
            "This program will parse TGA files into SMS planar tiles\n\n");
    if(argc < 2) {
        printf("Usage: %s [SOURCE TGA FILE] [OPTIONS]\n", argv[0]);
        return -1;
    }
    for(int i=2;i<argc;i++) {
        if(argv[i][0] != '-') continue;
        switch(argv[i][1]) {
            case 'p':
                cmdPalRow=true;
                break;
            default:
                printf("WARNING: ignoring unrecognized option \"-%c\"!\n", argv[i][1]);
        }
    }
    if(!tga_file.loadFromFile(argv[1], cmdPalRow)) {
        printf("%s\n", tga_file.getLastError());
        return -1;
    }
    if(tga_file.sumUniqueColors() > MAX_COLORS) {
        printf("ERROR: image has too many unique colors!\nSUM OF COLORS: %02d / %02d\n", tga_file.sumUniqueColors(), MAX_COLORS);
    }
    ////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////// TEMPORARY TESTING CODE FROM THIS POINT ON ///////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////
    return 0;
}
