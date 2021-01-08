/*
 * Copyright (C) 2017-2018 bouncyball
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <getopt.h>
#include <inttypes.h>
#include <string.h>
#include <strings.h>

#define MSG_INFO     0
#define MSG_ERROR    1
#define MLV_VIDEO_CLASS_FLAG_LJ92    0x20

#if defined(__WIN32)
#define SLASH   '\\'
#else
#define SLASH   '/'
#endif

#define MIN(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })

char * fpmutil_version = "1.0";

int quiet_mode = 0;
int no_header = 0;
int unified_mode = 0;
int one_pass_pbm = 0;

char * vid_mode = NULL;
char * cam_name = NULL;

enum ext_type { EXT_FPM, EXT_PBM };
enum ext_type output_ext = EXT_FPM;

enum get_mode { GET_NONE, GET_CLI, GET_MLV };
enum pattern { PATTERN_NONE = 0,
               PATTERN_EOSM = 331, // Pattern A
               PATTERN_650D = 301, // Pattern A
               PATTERN_700D = 326, // Pattern A
               PATTERN_100D = 346  // Pattern B
             };
enum video_mode { MV_NONE, MV_720,   MV_1080,   MV_1080CROP,   MV_ZOOM,   MV_CROPREC, 
                           MV_720_U, MV_1080_U, MV_1080CROP_U, MV_ZOOM_U, MV_CROPREC_U };

typedef struct {
    uint8_t     blockType[4];
    uint32_t    blockSize;
    uint64_t    timestamp;
} mlv_hdr_t;

typedef struct {
    uint8_t     fileMagic[4];
    uint32_t    blockSize;
    uint8_t     versionString[8];
    uint64_t    fileGuid;
    uint16_t    fileNum;
    uint16_t    fileCount;
    uint32_t    fileFlags;
    uint16_t    videoClass;
    uint16_t    audioClass;
    uint32_t    videoFrameCount;
    uint32_t    audioFrameCount;
    uint32_t    sourceFpsNom;
    uint32_t    sourceFpsDenom;
}  mlv_file_hdr_t;

typedef struct {
    uint8_t     blockType[4];
    uint32_t    blockSize;
    uint64_t    timestamp;
    uint16_t    xRes;
    uint16_t    yRes;
    uint32_t    dummy;
    uint32_t    crop;
    uint32_t    height;
    uint32_t    width;
    uint32_t    pitch;
    uint32_t    frame_size;
    uint32_t    bits_per_pixel;
    uint32_t    black_level;
    uint32_t    white_level;  
}  mlv_rawi_hdr_t;

typedef struct {
    uint8_t     blockType[4];
    uint32_t    blockSize;
    uint64_t    timestamp;
    uint16_t    sensor_res_x;
    uint16_t    sensor_res_y;
    uint16_t    sensor_crop;
    uint16_t    reserved;
    uint8_t     binning_x;
    uint8_t     skipping_x;
    uint8_t     binning_y;
    uint8_t     skipping_y;
    int16_t     offset_x;
    int16_t     offset_y;
}  mlv_rawc_hdr_t;

typedef struct {
    uint8_t     blockType[4];
    uint32_t    blockSize;
    uint64_t    timestamp;
    uint8_t     cameraName[32];
    uint32_t    cameraModel;
    uint8_t     cameraSerial[32];
}  mlv_idnt_hdr_t;

mlv_hdr_t mlv_hdr = { 0 };
mlv_file_hdr_t file_hdr = { 0 };
mlv_rawi_hdr_t rawi_hdr = { 0 };
mlv_rawc_hdr_t rawc_hdr = { 0 };
mlv_idnt_hdr_t idnt_hdr = { 0 };

struct pass_info
{
    int count;
    int range[10];
};

struct pixel_xy
{
    int x;
    int y;
};

struct pixel_map
{
    int count;
    int capacity;
    struct pass_info pass;
    struct pixel_xy * pixels;
};

char *strdup(const char *src)
{
    size_t len = strlen(src) + 1;
    char *s = malloc(len);
    if (s == NULL)
        return NULL;
    return (char *)memcpy(s, src, len);
}

static uint32_t file_set_pos(FILE *stream, uint64_t offset, int whence)
{
#if defined(__WIN32)
    return fseeko64(stream, offset, whence);
#else
    return fseek(stream, offset, whence);
#endif
}

static uint32_t atoh(char * string)
{
    register char *p;
    register uint32_t n;
    register int digit,lcase;

    p = string;
    n = 0;
    while(*p == ' ')
        p++;

    if(*p == '0' && ((*(p+1) == 'x') || (*(p+1) == 'X')))
        p+=2;

    while ((digit = (*p >= '0' && *p <= '9')) ||
        (lcase = (*p >= 'a' && *p <= 'f')) ||
        (*p >= 'A' && *p <= 'F')) {
        n *= 16;
        if (digit)  n += *p++ - '0';
        else if (lcase) n += 10 + (*p++ - 'a');
        else        n += 10 + (*p++ - 'A');
    }
    return(n);
}

static void print_msg(uint32_t type, const char* format, ... )
{
    if(quiet_mode) return;

    va_list args;
    va_start( args, format );
    char *fmt_str = malloc(strlen(format) + 32);

    switch(type)
    {
        case MSG_INFO:
                vfprintf(stdout, format, args);
            break;

        case MSG_ERROR:
                strcpy(fmt_str, "Error: ");
                strcat(fmt_str, format);
                vfprintf(stderr, fmt_str, args);
            break;
    }

    free(fmt_str);
    va_end( args );
}

/* get all needed data from MLV info blocks */
static int mlv_parse_file(char *mlv_name)
{
    FILE* mlvf = fopen(mlv_name, "rb");
    if(!mlvf)
    {
        print_msg(MSG_ERROR, "file '%s' not found\n", mlv_name);
        return -1;
    }
    print_msg(MSG_INFO, "Parsing file '%s'\n", mlv_name);

    if(fread(&file_hdr, sizeof(mlv_file_hdr_t), 1, mlvf) != 1)
    {
        print_msg(MSG_ERROR, "could not read from '%s'\n", mlv_name);
        return -1;   
    }
    if(memcmp(file_hdr.fileMagic, "MLVI", 4) != 0 || file_hdr.blockSize != 52)
    {
        print_msg(MSG_ERROR, "'%s' is not a valid MLV\n", mlv_name);
        return -1;
    }

    /* For safety analyze 32 blocks and search for RAWI blockname, then get values from the 
       first matched, if all blocks matched return 1 otherwise 0, on file error return -1
    */
    int i = 0, rawif = 0, rawcf = 0, idntf = 0;
    file_set_pos(mlvf, file_hdr.blockSize - sizeof(mlv_file_hdr_t), SEEK_CUR);
    for (i = 0; i < 32; ++i)
    {
        if(fread(&mlv_hdr, sizeof(mlv_hdr_t), 1, mlvf) != 1)
        {
            print_msg(MSG_ERROR, "could not read from '%s'\n", mlv_name);
            return -1;
        }

        if(!memcmp(mlv_hdr.blockType, "RAWI", 4))
        {
            if(!rawif)
            {
                file_set_pos(mlvf, -sizeof(mlv_hdr_t), SEEK_CUR);
                if(fread(&rawi_hdr, sizeof(mlv_rawi_hdr_t), 1, mlvf) != 1)
                {
                    print_msg(MSG_ERROR, "could not read from '%s'\n", mlv_name);
                    return -1;
                }
                file_set_pos(mlvf, mlv_hdr.blockSize - sizeof(mlv_rawi_hdr_t), SEEK_CUR);
                rawif = 1;
            }
            else
            { 
                file_set_pos(mlvf, mlv_hdr.blockSize - sizeof(mlv_hdr_t), SEEK_CUR);
            }
        }
        else if(!memcmp(mlv_hdr.blockType, "RAWC", 4))
        {
            if(!rawcf)
            {
                file_set_pos(mlvf, -sizeof(mlv_hdr_t), SEEK_CUR);
                if(fread(&rawc_hdr, sizeof(mlv_rawc_hdr_t), 1, mlvf) != 1)
                {
                    print_msg(MSG_ERROR, "could not read from '%s'\n", mlv_name);
                    return -1;
                }
                file_set_pos(mlvf, mlv_hdr.blockSize - sizeof(mlv_rawc_hdr_t), SEEK_CUR);
                rawcf = 1;
            }
            else
            {
                file_set_pos(mlvf, mlv_hdr.blockSize - sizeof(mlv_hdr_t), SEEK_CUR);
            }
        }
        else if(!memcmp(mlv_hdr.blockType, "IDNT", 4))
        {
            if(!idntf)
            {
                file_set_pos(mlvf, -sizeof(mlv_hdr_t), SEEK_CUR);
                if(fread(&idnt_hdr, sizeof(mlv_idnt_hdr_t), 1, mlvf) != 1)
                {
                    print_msg(MSG_ERROR, "could not read from '%s'\n", mlv_name);
                    return -1;
                }
                file_set_pos(mlvf, mlv_hdr.blockSize - sizeof(mlv_idnt_hdr_t), SEEK_CUR);
                idntf = 1;
            }
            else
            {
                file_set_pos(mlvf, mlv_hdr.blockSize - sizeof(mlv_hdr_t), SEEK_CUR);
            }
        }
        else
        {
            file_set_pos(mlvf, mlv_hdr.blockSize - sizeof(mlv_hdr_t), SEEK_CUR);
        }
        
        //print_msg(MSG_INFO, "%c%c%c%c\n", mlv_hdr.blockType[0], mlv_hdr.blockType[1], mlv_hdr.blockType[2], mlv_hdr.blockType[3]);

        if(rawif & idntf)
        {
            fclose(mlvf);
            return 1;
        }
    }

    fclose(mlvf);
    return 0;
}

/* detect crop rec */
static int get_croprec()
{
    if(rawc_hdr.blockType[0])
    {
        int sampling_x = rawc_hdr.binning_x + rawc_hdr.skipping_x;
        int sampling_y = rawc_hdr.binning_y + rawc_hdr.skipping_y;
        
        if( !(sampling_y == 5 && sampling_x == 3) )
        {
            return 1;
        }
    }

    return 0;
}

/* returns pixel pattern A, B or NONE in case of unsupported camera */
static int get_pattern(int get_mode, char *cam_name)
{
    switch(get_mode)
    {
        case GET_CLI:
            if(!strcasecmp(cam_name, "EOSM"))
            {
                memcpy(idnt_hdr.cameraName, "Canon EOS M", 12);
                idnt_hdr.cameraModel = 0x80000331;
                return PATTERN_EOSM;
            }
            else if(!strcasecmp(cam_name, "100D"))
            {
                memcpy(idnt_hdr.cameraName, "Canon EOS 100D", 15);
                idnt_hdr.cameraModel = 0x80000346;
                return PATTERN_100D;
            }
            else if(!strcasecmp(cam_name, "650D"))
            {
                memcpy(idnt_hdr.cameraName, "Canon EOS 650D", 15);
                idnt_hdr.cameraModel = 0x80000301;
                return PATTERN_650D;
            }
            else if(!strcasecmp(cam_name, "700D"))
            {
                memcpy(idnt_hdr.cameraName, "Canon EOS 700D", 15);
                idnt_hdr.cameraModel = 0x80000326;
                return PATTERN_700D;
            }
            else
            {
                return PATTERN_NONE;
            }
    
        case GET_MLV:
            switch(idnt_hdr.cameraModel)
            {
                case 0x80000331:
                    return PATTERN_EOSM;

                case 0x80000346:
                    return PATTERN_100D;

                case 0x80000301:
                    return PATTERN_650D;

                case 0x80000326:
                    return PATTERN_700D;

                default: // unsupported camera
                    return PATTERN_NONE;
            }

        default:
            return PATTERN_NONE;
    }
}

/* returns video mode value, special case when vid_mode == "croprec" */
static int get_video_mode(int get_mode, char * vid_mode)
{
    switch(get_mode)
    {
        case GET_CLI:
            if(!strcasecmp(vid_mode, "mv720"))
            {
                rawi_hdr.crop = 0;
                rawi_hdr.width = 1808;
                rawi_hdr.height = 727;
                return MV_720 + unified_mode;
            }   
            else if(!strcasecmp(vid_mode, "mv1080"))
            {
                rawi_hdr.crop = 0;
                rawi_hdr.width = 1808;
                rawi_hdr.height = 1190;
                return MV_1080 + unified_mode;
            }
            else if(!strcasecmp(vid_mode, "mv1080crop"))
            {
                rawi_hdr.crop = 0;
                rawi_hdr.width = 1872;
                rawi_hdr.height = 1060;
                return MV_1080CROP + unified_mode;
            }
            else if(!strcasecmp(vid_mode, "zoom"))
            {
                rawi_hdr.crop = 0;
                rawi_hdr.width = 2592;
                rawi_hdr.height = 1332;
                return MV_ZOOM + unified_mode;
            }
            else if(!strcasecmp(vid_mode, "croprec"))
            {
                rawi_hdr.crop = 1;
                rawi_hdr.width = 1808;
                rawi_hdr.height = 727;
                return MV_CROPREC + unified_mode;
            }
            else
            {
                rawi_hdr.crop = 0;
                return MV_NONE;
            }
        
        case GET_MLV:
            switch(rawi_hdr.width)
            {
                case 1808:
                    if(rawi_hdr.height < 900)
                    {
                        int is_crop_rec = get_croprec();
                        if((vid_mode != NULL && !strcasecmp(vid_mode, "croprec")) || is_crop_rec)
                        {
                            rawi_hdr.crop = (!is_crop_rec) ? 1 : is_crop_rec;
                            return MV_CROPREC + unified_mode;
                        }
                        else
                        {
                            rawi_hdr.crop = 0;
                            return MV_720 + unified_mode;
                        }
                    }
                    else
                    {
                        rawi_hdr.crop = 0;
                        return MV_1080 + unified_mode;
                    }

                case 1872:
                    rawi_hdr.crop = 0;
                    return MV_1080CROP + unified_mode;

                case 2592:
                    rawi_hdr.crop = 0;
                    return MV_ZOOM + unified_mode;

                default:
                    rawi_hdr.crop = 0;
                    return MV_NONE + unified_mode;
            }

        default:
            return MV_NONE;
    }
}

static char *get_output_filename(char *output_filename)
{
    if(!(output_filename))
    {
        char file_name[32];
        switch(output_ext)
        {
            case EXT_FPM:
                sprintf(file_name, "%x_%ix%i.fpm", idnt_hdr.cameraModel, rawi_hdr.width, rawi_hdr.height);
                break;
        
            case EXT_PBM:
                sprintf(file_name, "%x_%ix%i.pbm", idnt_hdr.cameraModel, rawi_hdr.width, rawi_hdr.height);
                break;
        }
        return strdup(file_name);
    }
    else
    {
        return output_filename;
    }
}

/* add focus pixel coordinats to the map */
static int add_pixel_to_map(struct pixel_map * map, int x, int y)
{
    if(!map->capacity)
    {
        map->capacity = 32;
        map->pixels = malloc(sizeof(struct pixel_xy) * map->capacity);
        if(!map->pixels) goto malloc_error;
    }
    else if(map->count >= map->capacity)
    {
        map->capacity *= 2;
        map->pixels = realloc(map->pixels, sizeof(struct pixel_xy) * map->capacity);
        if(!map->pixels) goto malloc_error;
    }
    
    map->pixels[map->count].x = x;
    map->pixels[map->count].y = y;
    map->count++;
    return 1;

malloc_error:

    print_msg(MSG_ERROR, "could not allocate memory\n");
    map->count = 0;
    return 0;
}

/* scan file name for ID and resolution */
static int scan_filename(char * input_filename, uint32_t * cameraModel, uint32_t * width, uint32_t * height)
{
    char file_path[1024];
    strcpy(file_path, input_filename);
    char *file_name = strrchr(file_path, SLASH);
    if(file_name) ++file_name;
    else file_name = file_path;
    
    enum pattern pattern = PATTERN_NONE;
    enum video_mode video_mode = MV_NONE;
    
    if(cam_name) pattern = get_pattern(GET_CLI, cam_name);
    if(vid_mode) video_mode = get_video_mode(GET_CLI, vid_mode);        

    char * n_cameraModel = strtok(file_name, "_");
    char * n_width = strtok(NULL, "x");
    char * n_height = strtok(NULL, ".");

    if( ( (n_cameraModel[0] != '8' && n_cameraModel[1] != '0') && !pattern) || ( (!n_width || !n_height) && !video_mode ) )
    {
        return 0;
    }

    if(!pattern && n_cameraModel)
    {
        *cameraModel = atoh(n_cameraModel);
    }
    
    if(!video_mode && n_width && n_height)
    {    
        *width = atoi(n_width);
        *height = atoi(n_height);
    }

    return 1;
}

/* load .fpm file */
static int fpm_load(struct pixel_map * map, char * file_name)
{
    FILE* f = fopen(file_name, "r");
    if(!f)
    {
        print_msg(MSG_ERROR, "could not read from '%s'\n", file_name);
        return 0;
    }
    
    if(fscanf(f, "#FPM%*[ ]%X%*[ ]%u%*[ ]%u%*[ ]%u%*[^\n]", &idnt_hdr.cameraModel, &rawi_hdr.width, &rawi_hdr.height, &rawi_hdr.crop) != 4)
    {
        if(!scan_filename(file_name, &idnt_hdr.cameraModel, &rawi_hdr.width, &rawi_hdr.height))
        {
            print_msg(MSG_ERROR, "'%s' map can not be converted!\nCould not acquire sufficient information from header, file name or command line\n", file_name);
            return 0;
        }
    }

    int x, y, prev_y = 0;
    while (fscanf(f, "%u%*[ \t]%u%*[^\n]", &x, &y) != EOF)
    {
        if(y < prev_y) // detect next pass start
        {
            map->pass.range[MIN(++map->pass.count, 9)] = map->count;
        }
        prev_y = y;

        add_pixel_to_map(map, x, y);
    }
    map->pass.range[MIN(++map->pass.count, 9)] = map->count;
    
    fclose(f);
    output_ext = EXT_PBM;
    return 1;    
}

/* save .fpm file */
static int fpm_save(struct pixel_map * map, char * file_name)
{
    FILE* f = fopen(file_name, "w");
    if(!f)
    {
        print_msg(MSG_ERROR, "could not open '%s'\n", file_name);
        return 0;
    }
    
    if(!no_header)
    {
        if(fprintf(f, "#FPM %X %u %u %u %u -- fpmutil v%s\n", idnt_hdr.cameraModel, rawi_hdr.width, rawi_hdr.height, rawi_hdr.crop, map->pass.count, fpmutil_version) < 0)
        {
            print_msg(MSG_ERROR, "could not write to '%s'\n", file_name);
            return 0;
        }
    }

    for (size_t i = 0; i < map->count; ++i)
    {
        if(fprintf(f, "%u \t %u\n", map->pixels[i].x, map->pixels[i].y) < 0)
        {
            print_msg(MSG_ERROR, "could not write to '%s'\n", file_name);
            return 0;
        }
    }
    
    print_msg(MSG_INFO, "%d pixels saved as %u pass focus pixel map '%s'\n", map->count, map->pass.count, file_name);

    fclose(f);
    return 1;;
}

/* 1 bit image pixel toggle for PBM map */
static void pbm_toggle_pixel(char * img_buf, int index)
{
    img_buf[index / 8] ^= 1 << (7 - index % 8);
}

/* 1 bit image get pixel value for PBM map */
static char pbm_get_pixel(char * img_buf, int index)
{
    return 1 & img_buf[index / 8] >> (7 - index % 8);
}

/* load .pbm file */
static int pbm_load(struct pixel_map * map, char * file_name)
{
    FILE* f = fopen(file_name, "rb");
    if(!f)
    {
        print_msg(MSG_ERROR, "could not open '%s'\n", file_name);
        return 0;
    }
    
    char pbm_header[256];
    if(fread(pbm_header, 256, 1, f) != 1)
    {
        print_msg(MSG_ERROR, "could not read from '%s'\n", file_name);
        return 0;
    }

    if(memcmp(pbm_header, "P4", 2) != 0 ) // Check P4 signature
    {
        print_msg(MSG_ERROR, "invalid PBM file '%s'\n", file_name);
        return 0;
    }

    if(strchr(pbm_header, '#')) // Check if there is a comment line
    {
        if(sscanf(pbm_header, "P4\n#%*[ ]%X%*[ ]%u%*[^\n]%u%*[ ]%u%*[^\n]", &idnt_hdr.cameraModel, &rawi_hdr.crop, &rawi_hdr.width, &rawi_hdr.height) != 4) // All matched
        {
            if(sscanf(pbm_header, "P4\n#%*[^\n]%u%*[ ]%u%*[^\n]", &rawi_hdr.width, &rawi_hdr.height) != 2) // Comment line did not match read resolution only
            {
                if(!scan_filename(file_name, &idnt_hdr.cameraModel, &rawi_hdr.width, &rawi_hdr.height)) // Get info from file name
                {
                    print_msg(MSG_ERROR, "'%s' map can not be converted!\nCould not acquire sufficient information from header, file name or command line\n", file_name);
                    return 0;
                }
            }
            else
            {
                uint32_t w, h;
                idnt_hdr.cameraModel = 0x0;
                scan_filename(file_name, &idnt_hdr.cameraModel, &w, &h); // The chance to override camera ID and croprec values from cli and file name
            }
        }
        else // All matched (accidental comment line match case)
        {
            if((idnt_hdr.cameraModel & 0xFFFF0000) != 0x80000000) // Check comment line data validity
            {
                uint32_t w, h;
                scan_filename(file_name, &idnt_hdr.cameraModel, &w, &h); // The chance to override camera ID and croprec values from cli and file name
            }
        }
    }
    else // If there is no comment line
    {
        if(sscanf(pbm_header, "P4\n%d%*[ ]%d%*[^\n]", &rawi_hdr.width, &rawi_hdr.height) != 2) // Try without comment line, get only resolution
        {
            if(!scan_filename(file_name, &idnt_hdr.cameraModel, &rawi_hdr.width, &rawi_hdr.height)) // Get info from file name
            {
                print_msg(MSG_ERROR, "'%s' map can not be converted!\nCould not acquire sufficient information from header, file name or command line\n", file_name);
                return 0;
            }
        }
        else
        {
            uint32_t w, h;
            scan_filename(file_name, &idnt_hdr.cameraModel, &w, &h); // The chance to override camera ID and croprec values from cli and file name
        }
    }

    size_t pbm_row_bytes = rawi_hdr.width / 8 + (!!(rawi_hdr.width % 8));
    size_t pbm_image_buf_size = pbm_row_bytes * rawi_hdr.height;
    char * pbm_image_buf = calloc(pbm_image_buf_size, 1);

    /* calculate header size */
    int offset = strrchr(pbm_header, '\n') - pbm_header + 1;
    file_set_pos(f, offset, SEEK_SET);
    if(fread(pbm_image_buf, pbm_image_buf_size, 1, f) != 1)
    {
        print_msg(MSG_ERROR, "could not read from '%s'\n", file_name);
        return 0;
    }
    
    size_t pbm_row_width = pbm_row_bytes * 8;
    for(size_t y = 0; y < rawi_hdr.height; ++y)
    {
        for(size_t x = 0; x < rawi_hdr.width; ++x)
        {
            if(pbm_get_pixel(pbm_image_buf, y * pbm_row_width + x))
            {
                add_pixel_to_map(map, x, y);
            }
        }
    }

    free(pbm_image_buf);
    fclose(f);
    output_ext = EXT_FPM;
    return 1;
}

/* save .pbm file */
static int pbm_save(struct pixel_map * map, char * file_name, int pass_No)
{
    FILE* f = fopen(file_name, "wb");
    if(!f)
    {
        print_msg(MSG_ERROR, "could not open '%s'\n", file_name);
        return 0;
    }

    /* save .pbm header */
    char pbm_header[64];
    sprintf(pbm_header, "P4\n# %X %u -- fpmutil v%s\n%u %u\n", idnt_hdr.cameraModel, rawi_hdr.crop, fpmutil_version, rawi_hdr.width, rawi_hdr.height);
    int pbm_header_size = strlen(pbm_header);
    if(fwrite(pbm_header, sizeof(char), pbm_header_size, f) != pbm_header_size)
    {
        print_msg(MSG_ERROR, "could not write to '%s'\n", file_name);
        return 0;
    }

    /* save .pbm image data */
    size_t pbm_row_bytes = rawi_hdr.width / 8 + (!!(rawi_hdr.width % 8));
    size_t pbm_image_buf_size = pbm_row_bytes * rawi_hdr.height;
    char * pbm_image_buf = calloc(pbm_image_buf_size, 1);
    size_t pbm_row_width = pbm_row_bytes * 8;
    
    int pass_start = 0;
    int pass_end = map->count;
    if(pass_No) // if pass_No is zero then write one file with all passes
    {
        pass_start = map->pass.range[pass_No - 1];
        pass_end = map->pass.range[pass_No];
    }

    for (size_t i = pass_start; i < pass_end; ++i)
    {
        pbm_toggle_pixel(pbm_image_buf, map->pixels[i].y * pbm_row_width + map->pixels[i].x);
    }

    if(fwrite(pbm_image_buf, pbm_image_buf_size, 1, f) != 1)
    {
        print_msg(MSG_ERROR, "could not write to '%s'\n", file_name);
        return 0;
    }
    
    if(one_pass_pbm)
    {
        print_msg(MSG_INFO, "%d pixels saved as 1 pass focus pixel map '%s'\n", map->count, file_name);
    }
    else
    {
        print_msg(MSG_INFO, "%d pixels saved as pass %u focus pixel map '%s'\n", pass_end - pass_start, pass_No, file_name);
    }
    
    free(pbm_image_buf);
    fclose(f);
    return 1;
}

/* load .fpm or .pbm pixel map */
static int load_pixel_map(struct pixel_map * map, char ** input_filename, int input_filecount)
{
    char file_name[1024] = { 0 };
    strcpy(file_name, input_filename[0]);
    char *ext = strrchr(file_name, '.');
    
    if(!ext)
    {
        print_msg(MSG_ERROR, "wrong input file name '%s'\n", file_name);
        return 0;
    }
    else if(!strcasecmp(ext, ".fpm"))
    {
        return fpm_load(map, file_name);
    }
    else if(!strcasecmp(ext, ".pbm"))
    {
        map->pass.count = input_filecount;

        int ret = 0;
        for(int i = 0; i < input_filecount; i++)
        {
            ret = pbm_load(map, input_filename[i]);
            if(!ret) break;
        }

        return ret;

    }

    print_msg(MSG_ERROR, "'%s' is not a valid focus map file extension\n", ext);
    return 0;
}

/* load .fpm or .pbm pixel map */
static int save_pixel_map(struct pixel_map * map, char * output_filename)
{
    char file_name[1024] = { 0 };
    strcpy(file_name, output_filename);
    char *ext = strrchr(file_name, '.');

    if(!ext)
    {
        print_msg(MSG_ERROR, "wrong output file name '%s'\n", file_name);
        return 0;
    }
    else if(!strcasecmp(ext, ".fpm"))
    {
        return fpm_save(map, file_name);
    }
    else if(!strcasecmp(ext, ".pbm"))
    {
        if(map->pass.count == 1 || one_pass_pbm)
        {
            return pbm_save(map, file_name, 0);
        }

        int ret = 0;
        char passNo[11] = { 0 };
        for(int i = 1; i <= MIN(map->pass.count, 9); i++)
        {
            sprintf(passNo, ".pass%u.pbm", i);
            memcpy(ext, passNo, 10);
            ret += pbm_save(map, file_name, i);
        }

        return ret;
    }

    print_msg(MSG_ERROR, "'%s' is not a valid focus map file extension\n", ext);
    return 0;
}

/* standard generators ************************************************************************************************/

/*
  mv720() function
  Draw the focus pixel pattern for mv720 video mode.
*/
static void mv720(struct pixel_map * map, int pattern)
{
    int shift = 0;
    int raw_width = rawi_hdr.width;

    int fp_start = 290; 
    int fp_end = 465;
    int x_rep = 8;
    int y_rep = 12;
    
    if(pattern == PATTERN_100D)
    {
        fp_start = 86;
        fp_end = 669;
    }

    for(int y = fp_start; y <= fp_end; y++)
    {
        if(((y + 3) % y_rep) == 0) shift = 7;
        else if(((y + 4) % y_rep) == 0) shift = 6;
        else if(((y + 9) % y_rep) == 0) shift = 3;
        else if(((y + 10) % y_rep) == 0) shift = 2;
        else continue;
    
        for(int x = 72; x < raw_width; x++)
        {
            if(((x + shift) % x_rep) == 0)
            {
                add_pixel_to_map(map, x, y);
            }
        }
    }
    map->pass.range[MIN(++map->pass.count, 9)] = map->count;
}

/*
  mv1080() function
  Draw the focus pixel pattern for mv1080 video mode.
*/
static void mv1080(struct pixel_map * map, int pattern)
{
    int shift = 0;
    int raw_width = rawi_hdr.width;

    int fp_start = 459;
    int fp_end = 755;
    int x_rep = 8;
    int y_rep = 10;
    
    if(pattern == PATTERN_100D)
    {
        fp_start = 119;
        fp_end = 1095;
    }

    for(int y = fp_start; y <= fp_end; y++)
    {
        if(((y + 0) % y_rep) == 0) shift=0;
        else if(((y + 1) % y_rep) == 0) shift = 1;
        else if(((y + 5) % y_rep) == 0) shift = 5;
        else if(((y + 6) % y_rep) == 0) shift = 4;
        else continue;
    
        for(int x = 72; x < raw_width; x++)
        {
            if(((x + shift) % x_rep) == 0)
            {
                add_pixel_to_map(map, x, y);
            }
        }
    }
    map->pass.range[MIN(++map->pass.count, 9)] = map->count;
}

/*
  mv1080crop() function
  Draw the focus pixel pattern for mv1080crop video mode.
*/
static void mv1080crop(struct pixel_map * map, int pattern)
{
    int shift = 0;
    int raw_width = rawi_hdr.width;

    int fp_start = 121;
    int fp_end = 1013;
    int x_rep = 24;
    int y_rep = 60;
    
    if(pattern == PATTERN_100D)
    {
        fp_start = 29;
        fp_end = 1057;
        x_rep = 12;
        y_rep = 6;
    }

    for(int y = fp_start; y <= fp_end; y++)
    {
        switch(pattern)
        {
            case PATTERN_EOSM:
            case PATTERN_650D:
            case PATTERN_700D:
                if(((y + 7) % y_rep) == 0 ) shift = 19;
                else if(((y + 11) % y_rep) == 0 ) shift = 13;
                else if(((y + 12) % y_rep) == 0 ) shift = 18;
                else if(((y + 14) % y_rep) == 0 ) shift = 12;
                else if(((y + 26) % y_rep) == 0 ) shift = 0;
                else if(((y + 29) % y_rep) == 0 ) shift = 1;
                else if(((y + 37) % y_rep) == 0 ) shift = 7;
                else if(((y + 41) % y_rep) == 0 ) shift = 13;
                else if(((y + 42) % y_rep) == 0 ) shift = 6;
                else if(((y + 44) % y_rep) == 0 ) shift = 12;
                else if(((y + 56) % y_rep) == 0 ) shift = 0;
                else if(((y + 59) % y_rep) == 0 ) shift = 1;
                else continue;
                break;

            case PATTERN_100D:
                if(((y + 2) % y_rep) == 0 ) shift = 0;
                else if(((y + 5) % y_rep) == 0 ) shift = 1;
                else if(((y + 6) % y_rep) == 0 ) shift = 6;
                else if(((y + 7) % y_rep) == 0 ) shift = 7;
                else continue;
                break;
        }
      
        for(int x = 72; x < raw_width; x++)
        {
            if(((x + shift) % x_rep) == 0)
            {
                add_pixel_to_map(map, x, y);
            }
        }
    }
    map->pass.range[MIN(++map->pass.count, 9)] = map->count;
}

/*
  zoom() function
  Draw the focus pixel pattern for zoom video mode.
*/
static void zoom(struct pixel_map * map, int pattern)
{
    int shift = 0;
    int raw_width = rawi_hdr.width;

    int fp_start = 31;
    int fp_end = rawi_hdr.height - 1;
    int x_rep = 24;
    int y_rep = 60;
    
    if(pattern == PATTERN_100D)
    {
        fp_start = 28;
        x_rep = 12;
        y_rep = 6;
    }

    for(int y = fp_start; y <= fp_end; y++)
    {
        switch(pattern)
        {
            case PATTERN_EOSM:
            case PATTERN_650D:
            case PATTERN_700D:
                if(((y + 7) % y_rep) == 0) shift = 19;
                else if(((y + 11) % y_rep) == 0) shift = 13;
                else if(((y + 12) % y_rep) == 0) shift = 18;
                else if(((y + 14) % y_rep) == 0) shift = 12;
                else if(((y + 26) % y_rep) == 0) shift = 0;
                else if(((y + 29) % y_rep) == 0) shift = 1;
                else if(((y + 37) % y_rep) == 0) shift = 7;
                else if(((y + 41) % y_rep) == 0) shift = 13;
                else if(((y + 42) % y_rep) == 0) shift = 6;
                else if(((y + 44) % y_rep) == 0) shift = 12;
                else if(((y + 56) % y_rep) == 0) shift = 0;
                else if(((y + 59) % y_rep) == 0) shift = 1;
                else continue;
                break;

            case PATTERN_100D:
                if(((y + 2) % y_rep) == 0) shift = 0;
                else if(((y + 5) % y_rep) == 0) shift = 1;
                else if(((y + 6) % y_rep) == 0) shift = 6;
                else if(((y + 7) % y_rep) == 0) shift = 7;
                else continue;
                break;
        }
        
        for(int x = 72; x < raw_width; x++)
        {
            if(((x + shift) % x_rep) == 0)
            {
                add_pixel_to_map(map, x, y);
            }
        }
    }
    map->pass.range[MIN(++map->pass.count, 9)] = map->count;
}

/*
  crop_rec() function
  Draw the focus pixel pattern for crop_rec video mode.
  Requires the crop_rec module.
*/
static void crop_rec(struct pixel_map * map, int pattern)
{
    int shift = 0;
    int raw_width = rawi_hdr.width;

    int fp_start = 219;
    int fp_end = 515;
    int x_rep = 8;
    int y_rep = 10;
    
    switch(pattern)
    {
        case PATTERN_EOSM:
        case PATTERN_650D:
        {
            // first pass is like fpm_mv720
            mv720(map, pattern);
            break;
        }

        case PATTERN_700D:
        {
            // no first pass needed
            break;
        }

        case PATTERN_100D:
        {
            // first pass is like fpm_mv720
            mv720(map, pattern);
            // second pass is like fpm_mv1080 with corrected fp_start/fp_end
            fp_start = 28;
            fp_end = 724;
            x_rep = 8;
            y_rep = 10;
            break;
        }

        default: // unsupported camera
            return;
    }

    for(int y = fp_start; y <= fp_end; y++)
    {
        if(((y + 0) % y_rep) == 0) shift=0;
        else if(((y + 1) % y_rep) == 0) shift = 1;
        else if(((y + 5) % y_rep) == 0) shift = 5;
        else if(((y + 6) % y_rep) == 0) shift = 4;
        else continue;

        for(int x = 72; x < raw_width; x++)
        {
            if(((x + shift) % x_rep) == 0)
            {
                add_pixel_to_map(map, x, y);
            }
        }
    }
    map->pass.range[MIN(++map->pass.count, 9)] = map->count;
}

/* unified generators ************************************************************************************************/

/*
  mv720_u() function
  Draw unified focus pixel pattern for mv720 video mode.
*/
static void mv720_u(struct pixel_map * map, int pattern)
{
    int shift = 0;
    int raw_width = rawi_hdr.width;

    int fp_start = 28; 
    int fp_end = 726;
    int x_rep = 8;
    int y_rep = 12;
    
    for(int y = fp_start; y <= fp_end; y++)
    {
        if(((y + 3) % y_rep) == 0) shift = 7;
        else if(((y + 4) % y_rep) == 0) shift = 6;
        else if(((y + 9) % y_rep) == 0) shift = 3;
        else if(((y + 10) % y_rep) == 0) shift = 2;
        else continue;
    
        for(int x = 72; x < raw_width; x++)
        {
            if(((x + shift) % x_rep) == 0)
            {
                add_pixel_to_map(map, x, y);
            }
        }
    
    }
    map->pass.range[MIN(++map->pass.count, 9)] = map->count;
}

/*
  mv1080_u() function
  Draw unified focus pixel pattern for mv1080 video mode.
*/
static void mv1080_u(struct pixel_map * map, int pattern)
{
    int shift = 0;
    int raw_width = rawi_hdr.width;

    int fp_start = 28;
    int fp_end = 1189;
    int x_rep = 8;
    int y_rep = 10;
    
    for(int y = fp_start; y <= fp_end; y++)
    {
        if(((y + 0) % y_rep) == 0) shift=0;
        else if(((y + 1) % y_rep) == 0) shift = 1;
        else if(((y + 5) % y_rep) == 0) shift = 5;
        else if(((y + 6) % y_rep) == 0) shift = 4;
        else continue;
    
        for(int x = 72; x < raw_width; x++)
        {
            if(((x + shift) % x_rep) == 0)
            {
                add_pixel_to_map(map, x, y);
            }
        }
    }
    map->pass.range[MIN(++map->pass.count, 9)] = map->count;
}

/*
  mv1080crop_u() functions (shifted and normal)
  Draw unified focus pixel pattern for mv1080crop video mode.
*/
/* shifted */
static void mv1080crop_u_shifted(struct pixel_map * map, int pattern)
{
    int shift = 0;
    int raw_width = rawi_hdr.width;

    int fp_start = 28;
    int fp_end = 1058;
    int x_rep = 8;
    int y_rep = 60;
    
    if(pattern == PATTERN_100D)
    {
        x_rep = 12;
        y_rep = 6;
    }

    for(int y = fp_start; y <= fp_end; y++)
    {
        switch(pattern)
        {
            case PATTERN_EOSM:
            case PATTERN_650D:
            case PATTERN_700D:
                if(((y + 7) % y_rep) == 0 ) shift = 2;
                else if(((y + 11) % y_rep) == 0 ) shift = 4;
                else if(((y + 12) % y_rep) == 0 ) shift = 1;
                else if(((y + 14) % y_rep) == 0 ) shift = 3;
                else if(((y + 26) % y_rep) == 0 ) shift = 7;
                else if(((y + 29) % y_rep) == 0 ) shift = 0;
                else if(((y + 37) % y_rep) == 0 ) shift = 6;
                else if(((y + 41) % y_rep) == 0 ) shift = 4;
                else if(((y + 42) % y_rep) == 0 ) shift = 5;
                else if(((y + 44) % y_rep) == 0 ) shift = 3;
                else if(((y + 56) % y_rep) == 0 ) shift = 7;
                else if(((y + 59) % y_rep) == 0 ) shift = 0;
                else continue;
                break;

            case PATTERN_100D:
                if(((y + 2) % y_rep) == 0 ) shift = 11;
                else if(((y + 5) % y_rep) == 0 ) shift = 0;
                else if(((y + 6) % y_rep) == 0 ) shift = 5;
                else if(((y + 7) % y_rep) == 0 ) shift = 6;
                else continue;
                break;
        }
      
        for(int x = 72; x < raw_width; x++)
        {
            if(((x + shift) % x_rep) == 0)
            {
                add_pixel_to_map(map, x, y);
            }
        }
    }
    map->pass.range[MIN(++map->pass.count, 9)] = map->count;
}
/* normal */
static void mv1080crop_u(struct pixel_map * map, int pattern)
{
    int shift = 0;
    int raw_width = rawi_hdr.width;

    int fp_start = 28;
    int fp_end = 1058;
    int x_rep = 8;
    int y_rep = 60;
    
    if(pattern == PATTERN_100D)
    {
        x_rep = 12;
        y_rep = 6;
    }

    for(int y = fp_start; y <= fp_end; y++)
    {
        switch(pattern)
        {
            case PATTERN_EOSM:
            case PATTERN_650D:
            case PATTERN_700D:
                if(((y + 7) % y_rep) == 0 ) shift = 3;
                else if(((y + 11) % y_rep) == 0 ) shift = 5;
                else if(((y + 12) % y_rep) == 0 ) shift = 2;
                else if(((y + 14) % y_rep) == 0 ) shift = 4;
                else if(((y + 26) % y_rep) == 0 ) shift = 0;
                else if(((y + 29) % y_rep) == 0 ) shift = 1;
                else if(((y + 37) % y_rep) == 0 ) shift = 7;
                else if(((y + 41) % y_rep) == 0 ) shift = 5;
                else if(((y + 42) % y_rep) == 0 ) shift = 6;
                else if(((y + 44) % y_rep) == 0 ) shift = 4;
                else if(((y + 56) % y_rep) == 0 ) shift = 0;
                else if(((y + 59) % y_rep) == 0 ) shift = 1;
                else continue;
                break;

            case PATTERN_100D:
                if(((y + 2) % y_rep) == 0 ) shift = 0;
                else if(((y + 5) % y_rep) == 0 ) shift = 1;
                else if(((y + 6) % y_rep) == 0 ) shift = 6;
                else if(((y + 7) % y_rep) == 0 ) shift = 7;
                else continue;
                break;
        }
      
        for(int x = 72; x < raw_width; x++)
        {
            if(((x + shift) % x_rep) == 0)
            {
                add_pixel_to_map(map, x, y);
            }
        }
    }
    map->pass.range[MIN(++map->pass.count, 9)] = map->count;

    /* Second pass shifted */
    mv1080crop_u_shifted(map, pattern);
}

/*
  zoom_u() function
  Draw unified focus pixel pattern for zoom video mode.
*/
static void zoom_u(struct pixel_map * map, int pattern)
{
    int shift = 0;
    int raw_width = rawi_hdr.width;

    int fp_start = 28;
    int fp_end = rawi_hdr.height - 1;
    int x_rep = 8;
    int y_rep = 60;
    
    if(pattern == PATTERN_100D)
    {
        x_rep = 12;
        y_rep = 6;
    }

    for(int y = fp_start; y <= fp_end; y++)
    {
        switch(pattern)
        {
            case PATTERN_EOSM:
            case PATTERN_650D:
            case PATTERN_700D:
                if(((y + 7) % y_rep) == 0) shift = 3;
                else if(((y + 11) % y_rep) == 0) shift = 5;
                else if(((y + 12) % y_rep) == 0) shift = 2;
                else if(((y + 14) % y_rep) == 0) shift = 4;
                else if(((y + 26) % y_rep) == 0) shift = 0;
                else if(((y + 29) % y_rep) == 0) shift = 1;
                else if(((y + 37) % y_rep) == 0) shift = 7;
                else if(((y + 41) % y_rep) == 0) shift = 5;
                else if(((y + 42) % y_rep) == 0) shift = 6;
                else if(((y + 44) % y_rep) == 0) shift = 4;
                else if(((y + 56) % y_rep) == 0) shift = 0;
                else if(((y + 59) % y_rep) == 0) shift = 1;
                else continue;
                break;

            case PATTERN_100D:
                if(((y + 2) % y_rep) == 0) shift = 0;
                else if(((y + 5) % y_rep) == 0) shift = 1;
                else if(((y + 6) % y_rep) == 0) shift = 6;
                else if(((y + 7) % y_rep) == 0) shift = 7;
                else continue;
                break;
        }
        
        for(int x = 72; x < raw_width; x++)
        {
            if(((x + shift) % x_rep) == 0)
            {
                add_pixel_to_map(map, x, y);
            }
        }
    }

    for(int y = fp_start; y <= fp_end; y++)
    {
       switch(pattern)
        {
            case PATTERN_EOSM:
            case PATTERN_650D:
            case PATTERN_700D:
                if(((y + 14) % y_rep) == 0) shift = 4;
                else continue;
                break;

            case PATTERN_100D:
                if(((y + 2) % y_rep) == 0) shift = 4;
                else if(((y + 5) % y_rep) == 0) shift = 5;
                else if(((y + 6) % y_rep) == 0) shift = 10;
                else if(((y + 7) % y_rep) == 0) shift = 11;
                else continue;
                break;
        }

        for(int x = 72; x < raw_width; x++)
        {
            if(((x + shift) % x_rep) == 0)
            {
                add_pixel_to_map(map, x, y);
            }
        } 
    }
    map->pass.range[MIN(++map->pass.count, 9)] = map->count;
}

/*
  crop_rec_u() function
  Draw unified focus pixel pattern for crop_rec video mode.
  Requires the crop_rec module.
*/
static void crop_rec_u(struct pixel_map * map, int pattern)
{
    // first pass is like mv720
    mv720_u(map, pattern);

    int shift = 0;
    int raw_width = rawi_hdr.width;

    int fp_start = 28;
    int fp_end = 726;
    int x_rep = 8;
    int y_rep = 10;

    for(int y = fp_start; y <= fp_end; y++)
    {
        if(((y + 0) % y_rep) == 0) shift=0;
        else if(((y + 1) % y_rep) == 0) shift = 1;
        else if(((y + 5) % y_rep) == 0) shift = 5;
        else if(((y + 6) % y_rep) == 0) shift = 4;
        else continue;

        for(int x = 72; x < raw_width; x++)
        {
            if(((x + shift) % x_rep) == 0)
            {
                add_pixel_to_map(map, x, y);
            }
        }
    }
    map->pass.range[MIN(++map->pass.count, 9)] = map->count;
}

/* end of generators **************************************************************************************************/

static void show_usage(char *executable)
{
    print_msg(MSG_INFO, "\nUsage: %s [options] [<inputfile1> <inputfile2> ...] [-o <outputfile>]\n", executable);
    print_msg(MSG_INFO, "  -o <outputfile>           output filename with '.fpm' or '.pbm' extension\n");
    print_msg(MSG_INFO, "                            if omitted then name will be auto generated\n");
    print_msg(MSG_INFO, "Options:\n");
    print_msg(MSG_INFO, "  -c|--camera-name <name>   name: EOSM, 100D, 650D, 700D\n");
    print_msg(MSG_INFO, "  -m|--video-mode <mode>    mode: mv720      (1808x72* )\n");
    print_msg(MSG_INFO, "                                  mv1080     (1808x11**)\n");
    print_msg(MSG_INFO, "                                  mv1080crop (1872x10**)\n");
    print_msg(MSG_INFO, "                                  zoom       (2592x1***)\n");
    print_msg(MSG_INFO, "                                  croprec    (1808x72* ) \n");
    print_msg(MSG_INFO, "\n");
    print_msg(MSG_INFO, "  -u|--unified              switch to different, unified map generation mode\n");
    print_msg(MSG_INFO, "  -n|--no-header            do not include header into '.fpm' file\n");
    print_msg(MSG_INFO, "  -1|--one-pass-pbm         export multi pass '.fpm' as one pass '.pbm'\n");
    print_msg(MSG_INFO, "  -q|--quiet                supress console output\n");
    print_msg(MSG_INFO, "  -h|--help                 show long help\n");
    print_msg(MSG_INFO, "\n");
}

static void show_help(char *executable)
{
    print_msg(MSG_INFO, "\n");
    print_msg(MSG_INFO, "Focus Pixel Map Utility v%s\n", fpmutil_version);
    print_msg(MSG_INFO, "****************************\n");

    show_usage(executable);

    print_msg(MSG_INFO, "Notes:\n");
    print_msg(MSG_INFO, "  * auto generated name format is like used by MLVFS: 'cameraID_width_height.fpm'\n");
    print_msg(MSG_INFO, "  * multiple input files can be specified only for '.pbm' map images to save one combined multipass '.fpm'\n");
    print_msg(MSG_INFO, "  * to build crop_rec compliant maps using '.mlv' input, '-m croprec' should be used in conjunction with input file\n");
    print_msg(MSG_INFO, "  * if input file extension is '.fpm' or '.pbm' then conversion between input and output formats will be done\n");
    print_msg(MSG_INFO, "  * output map format will be chosen according to the file extension and if extension is wrong program will abort\n");
    print_msg(MSG_INFO, "  * if input '.mlv' from unsupportred camera the warning will be shown and program will abort\n");
    print_msg(MSG_INFO, "  * if '-u' switch specified, will export unified, aggresive pixel map to fix restricted to 8-12bit lossless raw\n");
    print_msg(MSG_INFO, "  * if '-n' switch specified, will export '.fpm' without header\n");
    print_msg(MSG_INFO, "  * if '-1' switch specified, will export all passes in one .pbm, by default separate file created for each pass\n");
    print_msg(MSG_INFO, "\n");
    print_msg(MSG_INFO, "Examples:\n");
    print_msg(MSG_INFO, "  fpmutil -c EOSM -m mv1080                     will save '.fpm' 1808x1190 map with auto generated name\n");
    print_msg(MSG_INFO, "  fpmutil -n -c EOSM -m mv1080                  will save '.fpm' 1808x1190 map with auto generated name and no header\n");
    print_msg(MSG_INFO, "  fpmutil -c EOSM -m mv1080 -o focusmap.pbm     will save '.pbm' 1808x1190 graphical image map\n");
    print_msg(MSG_INFO, "  fpmutil input.mlv                             will save '.fpm' with the resolution taken from '.mlv'\n");
    print_msg(MSG_INFO, "  fpmutil -n input.mlv                          will save '.fpm' with the resolution taken from '.mlv' and no header\n");
    print_msg(MSG_INFO, "  fpmutil -m croprec input.mlv                  will save '.fpm' with crop_rec modes compliant pattern\n");
    print_msg(MSG_INFO, "  fpmutil -m croprec input.mlv -o output,pbm    will save '.pbm' files for each croprec pass\n");
    print_msg(MSG_INFO, "  fpmutil input.[fpm|pbm] -o output.[pbm|fpm]   will save '.pbm/fpm' converted from input to output format\n");
    print_msg(MSG_INFO, "  fpmutil input.[fpm|pbm]                       will save '.pbm/fpm' converted to opposite format of the input format\n");
    print_msg(MSG_INFO, "  fpmutil -c 100D -m croprec input.fpm          will save '.pbm' with overriden camera ID and video mode\n");
    print_msg(MSG_INFO, "  fpmutil input1.pbm input2.pbm                 will save '.fpm' with combined pixels from all input files as multipass map\n");
    print_msg(MSG_INFO, "  fpmutil -n input.pbm                          will save '.fpm' without header\n");
    print_msg(MSG_INFO, "\n");
}

int main(int argc, char *argv[])
{
    char *input_filename[10] = { NULL };
    char *output_filename = NULL;
    int opt = ' ';

    enum pattern pattern = PATTERN_NONE;
    enum video_mode video_mode = MV_NONE;

    static struct pixel_map focus_pixel_map = { 0, 0, { 0, { 0 } }, NULL };

    /* disable stdout buffering */
    setvbuf(stderr, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    /*  
        {"help",  no_argument, &help,  1 },
        {"switch1",  required_argument, NULL,  'S' },
        {"switch2",  optional_argument, NULL,  'T' },
    */
    struct option long_options[] = {
        { "video-mode", required_argument, NULL, 'm' },
        { "camera-name", required_argument, NULL, 'c' },
        { "unified",  no_argument, &unified_mode,  5 },
        { "no-header",  no_argument, &no_header,  1 },
        { "one-pass-pbm",  no_argument, &one_pass_pbm,  1 },
        { "quiet",  no_argument, &quiet_mode,  1 },
        { "help",  optional_argument, NULL, 'h'},
        { NULL, 0, NULL, 0 }
    };

    int index = 0;
    while ((opt = getopt_long(argc, argv, "c:m:o:un1qh", long_options, &index)) != -1)
    {
        switch (opt)
        {
            case 'c':
                cam_name = strdup(optarg);
                break;

            case 'm':
                vid_mode = strdup(optarg);
                break;

            case 'o':
                output_filename = strdup(optarg);
                break;

            case 'u':
                unified_mode = 5;
                break;

            case 'n':
                no_header = 1;
                break;

            case '1':
                one_pass_pbm = 1;
                break;

            case 'q':
                quiet_mode = 1;
                break;

            case 'h':
                show_help(argv[0]);
                return 1;

            case 0:
                break;

            default:
                show_usage(argv[0]);
                return 1;
        }
    }

    print_msg(MSG_INFO, "\n");
    print_msg(MSG_INFO, "Focus Pixel Map Utility v%s\n", fpmutil_version);
    print_msg(MSG_INFO, "****************************\n");
    
    if(argc == 1)
    {
        print_msg(MSG_ERROR, "missing command line options\n");
        show_usage(argv[0]);
        return 1;
    }

    /* if input file name is missing, use command line options */
    if(optind >= argc)
    {
        if(!cam_name || !vid_mode)
        {
            print_msg(MSG_ERROR, "missing required command line options\n");
            show_usage(argv[0]);
            return 1;
        }
        
        print_msg(MSG_INFO, "MLV file not specified, using command line option values\n");

        pattern = get_pattern(GET_CLI, cam_name);
        if(!pattern)
        {
            print_msg(MSG_ERROR, "unsupported camera '%s'\n", cam_name);
            show_usage(argv[0]);
            goto bailout;
        }
        
        video_mode = get_video_mode(GET_CLI, vid_mode);        
        if(!video_mode)
        {
            print_msg(MSG_ERROR, "unsupported video mode '%s'\n", vid_mode);
            show_usage(argv[0]);
            goto bailout;
        }

        print_msg(MSG_ERROR, "Using command line option values\n\nCamera     : %s (0x%X)\nVideo mode : %dx%d\n\n", idnt_hdr.cameraName, idnt_hdr.cameraModel, rawi_hdr.width, rawi_hdr.height);
    }
    else
    {
        /* if input file name specified make sure whether it's MLV or FPM/PBM
           if its MLV get all requred values from info blocks, ignore other command line options exept "-m croprec"
           if its FPM/PBM convert pixel map to output format specified by '-o' switch
           if its PBM look for more than one input files
        */
        int arg_idx = optind;
        while(argv[arg_idx])
        {
            input_filename[arg_idx - optind] = argv[arg_idx];
            arg_idx++;
        }

        char *ext = strrchr(input_filename[0], '.');
        if(!ext) // if input file has no file extension bail out
        {
            print_msg(MSG_ERROR, "wrong input file name\n");
            goto bailout;
        }
        else if(!strcasecmp(ext, ".mlv")) // if input file extension is .mlv
        {
            int ret = mlv_parse_file(input_filename[0]);
            if(ret == 1) // all needed info block found
            {
                pattern = get_pattern(GET_MLV, cam_name);
                if(!pattern)
                {
                    print_msg(MSG_ERROR, "wrong MLV, unsupported camera '%s'\n", idnt_hdr.cameraName);
                    goto bailout;
                }

                /* if restricted to 8-12bit lossless mode detected, unified, all in one, focus pixel map generation mode is activated */
                if( (file_hdr.videoClass & MLV_VIDEO_CLASS_FLAG_LJ92) && (rawi_hdr.white_level < 15000) ) unified_mode = 5;

                video_mode = get_video_mode(GET_MLV, vid_mode);
                if(video_mode == MV_CROPREC)
                {
                    print_msg(MSG_INFO, "Using command line option '-m croprec'\n");
                }
                else if(cam_name || vid_mode)
                {
                    print_msg(MSG_INFO, "Command line options ignored\n");
                }
                
                print_msg(MSG_INFO, "Using MLV info block values\n\nCamera     : %s (0x%X)\nVideo mode : %dx%d\n\n", idnt_hdr.cameraName, idnt_hdr.cameraModel, rawi_hdr.width, rawi_hdr.height);
            }
            else if(ret == -1) // file IO error
            {
                goto bailout;
            }
            else if(ret == 0) // no sufficient info blocks found
            {
                print_msg(MSG_ERROR, "MLV file does not have all needed info blocks\n");
                goto bailout;
            }
        }
        else // if input file extension is .fpm or .bpm convert between formats, on any other extension bail out
        {
            int input_filecount = arg_idx - optind; // each input .pbm image corresponds to a separate pass
            if(!load_pixel_map(&focus_pixel_map, input_filename, input_filecount))
            {
                goto bailout;
            }
            else
            {
                print_msg(MSG_INFO, "Converting '%s'\n\nVideo mode : %dx%d\n\n", input_filename[0], rawi_hdr.width, rawi_hdr.height);
                goto savemap;
            }
        }
    }

    print_msg(MSG_INFO, "Generating focus pixel map for ");
    switch(video_mode)
    {
        case MV_720:
            print_msg(MSG_INFO, "'mv720' mode\n\n");
            mv720(&focus_pixel_map, pattern);
            break;

        case MV_1080:
            print_msg(MSG_INFO, "'mv1080' mode\n\n");
            mv1080(&focus_pixel_map, pattern);
            break;

        case MV_1080CROP:
            print_msg(MSG_INFO, "'mv1080crop' mode\n\n");
            mv1080crop(&focus_pixel_map, pattern);
            break;

        case MV_ZOOM:
            print_msg(MSG_INFO, "'zoom' mode\n\n");
            zoom(&focus_pixel_map, pattern);
            break;

        case MV_CROPREC:
            print_msg(MSG_INFO, "'croprec' mode\n\n");
            crop_rec(&focus_pixel_map, pattern);
            break;

        case MV_720_U:
            print_msg(MSG_INFO, "'mv720' lossless mode\n\n");
            mv720_u(&focus_pixel_map, pattern);
            break;

        case MV_1080_U:
            print_msg(MSG_INFO, "'mv1080' lossless mode\n\n");
            mv1080_u(&focus_pixel_map, pattern);
            break;

        case MV_1080CROP_U:
            print_msg(MSG_INFO, "'mv1080crop' lossless mode\n\n");
            mv1080crop_u(&focus_pixel_map, pattern);
            break;

        case MV_ZOOM_U:
            print_msg(MSG_INFO, "'zoom' lossless mode\n\n");
            zoom_u(&focus_pixel_map, pattern);
            break;

        case MV_CROPREC_U:
            print_msg(MSG_INFO, "'croprec' lossless mode\n\n");
            crop_rec_u(&focus_pixel_map, pattern);
            break;

        default:
            break;
    }

savemap:

    /* auto generate output file name if '-o <outputfile>' switch omitted */
    output_filename = get_output_filename(output_filename);
    
    if(!save_pixel_map(&focus_pixel_map, output_filename))
    {
        print_msg(MSG_INFO, "Focus pixel map not saved\n");
    }
    
    free(output_filename);
    free(cam_name);
    free(vid_mode);
    free(focus_pixel_map.pixels);
    return 0;

bailout:

    free(cam_name);
    free(vid_mode);
    return 1;
}
