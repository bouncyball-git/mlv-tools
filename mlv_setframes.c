/*
 * Copyright (C) 2016 Magic Lantern Team
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

#include "stdint.h"
#include "stdio.h"
#include <time.h>
#include <utime.h>
#include <sys/stat.h>
#include "string.h"

#define uint8_t unsigned char
#define uint16_t unsigned short
#define uint32_t unsigned long
#define uint64_t unsigned long long

enum block_type { BT_NONE, BT_VIDF, BT_AUDF, BT_NULL, BT_RTCI, BT_XREF, BT_RAWI, BT_WAVI, BT_EXPO, BT_LENS, BT_IDNT, BT_INFO, BT_WBAL, BT_STYL, BT_MARK, BT_ELVL, BT_DEBG, BT_BKUP, BT_MLVI };

typedef struct {
    uint8_t     blockType[4];
    uint32_t    blockSize;
    uint64_t    timestamp;
} mlv_hdr_t;

mlv_hdr_t mlv_hdr;

uint32_t file_set_pos(FILE *stream, uint64_t offset, int whence)
{
#if defined(__WIN32)
    return fseeko64(stream, offset, whence);
#else
    return fseek(stream, offset, whence);
#endif
}

unsigned char check_block_type()
{
    /* chech every block type to make sure mlv not corrupted */
    if(!memcmp(mlv_hdr.blockType, "VIDF", 4)) return BT_VIDF;
    if(!memcmp(mlv_hdr.blockType, "AUDF", 4)) return BT_AUDF;
    if(!memcmp(mlv_hdr.blockType, "NULL", 4)) return BT_NULL;
    if(!memcmp(mlv_hdr.blockType, "RTCI", 4)) return BT_RTCI;
    if(!memcmp(mlv_hdr.blockType, "XREF", 4)) return BT_XREF;
    if(!memcmp(mlv_hdr.blockType, "RAWI", 4)) return BT_RAWI;
    if(!memcmp(mlv_hdr.blockType, "WAVI", 4)) return BT_WAVI;
    if(!memcmp(mlv_hdr.blockType, "EXPO", 4)) return BT_EXPO;
    if(!memcmp(mlv_hdr.blockType, "LENS", 4)) return BT_LENS;
    if(!memcmp(mlv_hdr.blockType, "IDNT", 4)) return BT_IDNT;
    if(!memcmp(mlv_hdr.blockType, "INFO", 4)) return BT_INFO;
    if(!memcmp(mlv_hdr.blockType, "WBAL", 4)) return BT_WBAL;
    if(!memcmp(mlv_hdr.blockType, "STYL", 4)) return BT_STYL;
    if(!memcmp(mlv_hdr.blockType, "MARK", 4)) return BT_MARK;
    if(!memcmp(mlv_hdr.blockType, "ELVL", 4)) return BT_ELVL;
    if(!memcmp(mlv_hdr.blockType, "DEBG", 4)) return BT_DEBG;
    if(!memcmp(mlv_hdr.blockType, "BKUP", 4)) return BT_BKUP;
    if(!memcmp(mlv_hdr.blockType, "MLVI", 4)) return BT_MLVI;
    return BT_NONE;
}

void file_get_raw_times(struct utimbuf *rawtimes, char *filename)
{
    struct stat attr;
    stat(filename, &attr);
    rawtimes->actime = attr.st_atime;
    rawtimes->modtime = attr.st_mtime;
}

int file_set_raw_times(struct utimbuf *rawtimes, char *filename)
{
    return utime(filename, rawtimes);
}


int main(int argc, char** argv)
{

    short setf = 0;
    if(argc < 2)
    {
        printf(
            "\n"
            "usage:\n"
            "\n"
            " %s file.mlv [--set]\n"
            "\n   --set    if specified actually writes frameCount to file"
            "\n            otherwise just outputs the information\n"
            "\n   Extra testing option:"
            "\n   --set0x00000000    sets zero frameCount to any mlv file\n",
            argv[0]
        );
        return 1;
    }
    else if((argc > 2) && (strcmp(argv[2], "--set") == 0))
    {
        setf = 1;
    }
    else if((argc > 2) && (strcmp(argv[2], "--set0x00000000") == 0))
    {
        setf = 2;
    }

    struct utimbuf file_raw_times;
    uint32_t frame_count = 0, frame_number = 0;
    static unsigned short frame_count_offset = 0x24;
    static unsigned short mlv_hdr_t_size = sizeof(mlv_hdr_t);

    /* Zero mlv hdr struct */
    memset(&mlv_hdr, 0x00, sizeof(mlv_hdr_t));
    
    /* Open file */    
    char *in_file_name = argv[1];
    FILE* in_file = fopen(in_file_name, "r+b");
    if(!in_file)
    {
        printf("%s: Error: could not open file\n", in_file_name);
        return 1;
    }

    /* Check if file is a valid MLV */
    if(fread(&mlv_hdr, mlv_hdr_t_size, 1, in_file) != 1)
    {
        printf("%s: Error: could not read from file\n", in_file_name);
        goto bailout;
    }
    if(memcmp(mlv_hdr.blockType, "MLVI", 4) != 0 || mlv_hdr.blockSize != 52)
    {
        printf("%s: Error: not a valid MLV file\n", in_file_name);
        goto bailout;
    }
    
    /* Check if frameCount != 0 */
    file_set_pos(in_file, frame_count_offset - mlv_hdr_t_size, SEEK_CUR);
    if(fread(&frame_count, 4, 1, in_file) != 1)
    {
        printf("%s: Error: could not read from file\n", in_file_name);
        goto bailout;
    }
    if( (frame_count && setf != 2) || (!frame_count && setf == 2) )
    {
        printf("%s: Already has frameCount set to %lu\n", in_file_name, frame_count);
        goto bailout;
    }
    file_set_pos(in_file, mlv_hdr.blockSize - frame_count_offset - 4, SEEK_CUR);

    /* Start counting frames */
    frame_count = 0;
    while(fread(&mlv_hdr, mlv_hdr_t_size, 1, in_file) == 1)
    {   
        switch(check_block_type())
        {
            case BT_VIDF:
                frame_count++;
                if(fread(&frame_number, 4, 1, in_file) !=1)
                {
                    printf("%s: Error: could not read from file\n", in_file_name);
                    goto bailout;
                }
                printf("\r%s: Processing... frameCount = %lu, frameNumber = %lu", in_file_name, frame_count, frame_number);
                file_set_pos(in_file, mlv_hdr.blockSize - mlv_hdr_t_size - 4, SEEK_CUR);
                break;
            case BT_XREF:
                printf("%s: Looks like XREF file. Skipping...\n", in_file_name);
                goto bailout;
            case BT_AUDF:
            case BT_NULL:
            case BT_RTCI:
            case BT_RAWI:
            case BT_WAVI:
            case BT_EXPO:
            case BT_LENS:
            case BT_IDNT:
            case BT_INFO:
            case BT_WBAL:
            case BT_STYL:
            case BT_MARK:
            case BT_ELVL:
            case BT_DEBG:
            case BT_BKUP:
            case BT_MLVI:
                file_set_pos(in_file, mlv_hdr.blockSize - mlv_hdr_t_size, SEEK_CUR);
                break;
            case BT_NONE:
            default:
                printf("\n%s: Looks like mlv file corrupted\n", in_file_name);
                goto bailout;
        }
        //printf("\n%c%c%c%c FrameNumber = %lu FrameCount = %lu BlockSize = %lu", mlv_hdr.blockType[0], mlv_hdr.blockType[1],mlv_hdr.blockType[2],mlv_hdr.blockType[3], frame_number, frame_count, mlv_hdr.blockSize);
    }

    if(!frame_count) 
    {
        printf("\n%s: Hmmm... strange mlv file w/o VIDF blocks ;)\n", in_file_name);
    }
    else
    {
        
        uint32_t fcnt = 0;
        if(setf == 2) fcnt = frame_count;
        printf("\n%s: Looks like a valid MLV file w/frameCount set to %lu\n", in_file_name, fcnt);
        if(setf > 0)
        {
            if(setf == 2) frame_count = 0;
            file_get_raw_times(&file_raw_times, in_file_name);
            
            file_set_pos(in_file, frame_count_offset, SEEK_SET);
            if(fwrite(&frame_count, 4, 1, in_file) != 1)
            {
                printf("%s: Error: failed writing to file\n", in_file_name);
                goto bailout;
            }
            printf("%s: Changed frameCount value to %lu\n", in_file_name, frame_count);            
            
            fclose(in_file);
            if(file_set_raw_times(&file_raw_times, in_file_name) == -1)
            {
                printf("%s: Failed updating file time. No big deal :)\n", in_file_name);
            }
            return 0;
        }
    }
    
    fclose(in_file);
    return 0;

bailout:

    fclose(in_file);
    return 1;
}
