#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <malloc.h>

// compile time options

#define INPUT_FILE_PATH "global_grav_1min/grav.img.22.1"
#define OUTPUT_FILE_PATH "grav.bmp"

// uncomment only one of the following to set DPI output
#define DPI_72
//#define DPI_96
//#define DPI_150
//#define DPI_200
//#define DPI_300

// Inverse scale is how much to shrink the output. 
// This is halved.
// So, for example, input size of 21600x17280 = 373,248,000 raw 
// byte output. Set inverse scale to 16, then raw output is 
// (21600 / 8) x (17280 / 8) = 5,832,000 bytes.
// Set to 2 if not used.
#define INVERSE_SCALE 2

// comment the following line to disable debug output
#define DEBUG

////////////////////////////////////////////////////////////////////////////////
// geo source definitions

// from ftp://topex.ucsd.edu/pub/global_grav_1min/README.21.1
//
//
// Version 16.1 gravity is similar to V15.1 except that the
// gravity field runs to a higher latitude.
//
// param   V15.1     V16.1
// ___________________________
// nlon     21600    21600
// nlat     12672    17280
// rlt0   -72.006  -80.738
// rltf    72.006   80.738
// ___________________________
// The projection is the same spherical Mercator used in all of our
// other gravity grids.

#define NLON 21600
#define NLAT 17280

#define MIN_LAT -80.738
#define MAX_LAT  80.738

#define OUTPUT_IMAGE_WIDTH (NLON / (INVERSE_SCALE / 2))
#define OUTPUT_IMAGE_HEIGHT (NLAT / (INVERSE_SCALE / 2))

#define OUTPUT_RAW_SINGLE_UNITS (OUTPUT_IMAGE_WIDTH * OUTPUT_IMAGE_HEIGHT)

////////////////////////////////////////////////////////////////////////////////
// bitmap defintions

// lots of useful information from
// http://stackoverflow.com/questions/11004868/creating-a-bmp-file-bitmap-in-c

#define BITS_PER_PIXEL 24
#define BITMAP_PLANES 1
#define BITMAP_COMPRESSION 0
#define BITMAP_COLORS_IN_PALLETTE 0
#define BITMAP_IMPORTANT_COLORS 0

#pragma pack(push,1)
typedef struct bitmap_file_header {
    uint8_t signature[2];
    uint32_t file_size;
    uint32_t reserved;
    uint32_t fileoffset_to_pixelarray;
} bitmap_file_header;
typedef struct dib_header{
    uint32_t header_size;
    uint32_t width_in_pixels;
    uint32_t height_in_pixels;
    uint16_t color_planes;
    uint16_t bits_per_pixel;
    uint32_t compression;
    uint32_t image_size;
    uint32_t x_pixels_per_meter;
    uint32_t y_pixels_per_meter;
    uint32_t colors_in_pallette;
    uint32_t important_colors;
} dib_header;
typedef struct bitmap {
    bitmap_file_header bitmap_file_header;
    dib_header dib_header;
} bitmap;
#pragma pack(pop)

#pragma pack(push,1)
typedef struct bitmap_pixel {
    uint8_t B;
    uint8_t G;
    uint8_t R;
    //uint8_t A;
    //uint8_t unused;
} bitmap_pixel;
#pragma pack(pop)

#define PIXEL_BYTES 3

////////////////////////////////////////////////////////////////////////////////
// other stuff

// 1 pixel/inch = 39.37007874016 dot/meter
// 1 dot/meter = 0.0254 pixel/inch

#ifdef DPI_72
	#define X_PIXEL_PER_METER 2835
	#define Y_PIXEL_PER_METER 2835
#endif
#ifdef DPI_96
	#define X_PIXEL_PER_METER 3780
	#define Y_PIXEL_PER_METER 3780
#endif
#ifdef DPI_150
	#define X_PIXEL_PER_METER 5906
	#define Y_PIXEL_PER_METER 5906
#endif
#ifdef DPI_200
	#define X_PIXEL_PER_METER 7874
	#define Y_PIXEL_PER_METER 7874
#endif
#ifdef DPI_300
	#define X_PIXEL_PER_METER 11811
	#define Y_PIXEL_PER_METER 11811
#endif

// The bits representing the bitmap pixels are packed in rows. The size of each 
// row is rounded up to a multiple of 4 bytes (a 32-bit DWORD) by padding.
#define OUTPUT_ROW_BYTES ((((BITS_PER_PIXEL * OUTPUT_IMAGE_WIDTH) + 31)/32)*4)
#define OUTPUT_ROW_BYTES_WITHOUT_PADDING ((BITS_PER_PIXEL * OUTPUT_IMAGE_WIDTH)/8)

// This is the number of bytes the pixel information uses.
// note that bitmap format supports negative height, but ...
// that's not tested here.
#define OUTPUT_PIXEL_BYTE_COUNT (OUTPUT_ROW_BYTES * OUTPUT_IMAGE_HEIGHT)

// total output size is size of the bitmap header plus
// size of the pixel data
#define OUTPUT_FILE_SIZE (OUTPUT_PIXEL_BYTE_COUNT + sizeof(bitmap))

#define BYTE_SWAP(x) ( (((x) >> 8) & 0x00FF) | (((x) << 8) & 0xFF00) )

////////////////////////////////////////////////////////////////////////////////
// error checks

/* definition to expand macro then apply to pragma message */
#define VALUE_TO_STRING(x) #x
#define VALUE(x) VALUE_TO_STRING(x)
#define VAR_NAME_VALUE(var) #var "="  VALUE(var)

#ifndef X_PIXEL_PER_METER
	#error DPI is not set (check source code; compile time constant)
#endif

#if BITS_PER_PIXEL!=24
	#error only 24 bits per pixel is currently supported.
#endif

#if ((OUTPUT_ROW_BYTES - OUTPUT_ROW_BYTES_WITHOUT_PADDING) > 4) || ((OUTPUT_ROW_BYTES - OUTPUT_ROW_BYTES_WITHOUT_PADDING) < 0)
	#pragma message(VAR_NAME_VALUE(OUTPUT_ROW_BYTES_WITHOUT_PADDING))
	#pragma message(VAR_NAME_VALUE(OUTPUT_ROW_BYTES))
	#error the number of padding bytes is either too large (>4) or negative.
#endif

// allocates a new bitmap and sets to values as defined above
bitmap* new_bitmap()
{
	bitmap *b = (bitmap *)calloc(1, sizeof(bitmap));
	if (b == NULL)
	{
		printf("Could not create new bitmap: out of memory?\n");
		exit(-1);
	}
	
	b->bitmap_file_header.signature[0] = 'B';
	b->bitmap_file_header.signature[1] = 'M';
	b->bitmap_file_header.file_size = OUTPUT_FILE_SIZE;
	b->bitmap_file_header.reserved = 0;
	b->bitmap_file_header.fileoffset_to_pixelarray = sizeof(bitmap);

	b->dib_header.header_size = sizeof(dib_header);
	b->dib_header.width_in_pixels = OUTPUT_IMAGE_WIDTH;
	b->dib_header.height_in_pixels = OUTPUT_IMAGE_HEIGHT;
	b->dib_header.color_planes = BITMAP_PLANES;
	b->dib_header.bits_per_pixel = BITS_PER_PIXEL;
	b->dib_header.compression = BITMAP_COMPRESSION;
	b->dib_header.image_size = OUTPUT_RAW_SINGLE_UNITS;
	b->dib_header.x_pixels_per_meter = X_PIXEL_PER_METER;
	b->dib_header.y_pixels_per_meter = Y_PIXEL_PER_METER;
	b->dib_header.colors_in_pallette = BITMAP_COLORS_IN_PALLETTE;
	b->dib_header.important_colors = BITMAP_IMPORTANT_COLORS;
	
	return b;
}

// prints bitmap header contents
void print_bitmap_info(bitmap *b)
{
	if (b == NULL)
	{
		return;
	}
	
	printf("bitmap_file_header.signature: %c%c\n", b->bitmap_file_header.signature[0], b->bitmap_file_header.signature[1]);
	printf("bitmap_file_header.file_size: %u\n", b->bitmap_file_header.file_size);
	printf("bitmap_file_header.reserved: %u\n", b->bitmap_file_header.reserved);
	printf("bitmap_file_header.fileoffset_to_pixelarray: %u\n", b->bitmap_file_header.fileoffset_to_pixelarray);
	
	printf("dib_header.header_size: %u\n", b->dib_header.header_size);
	printf("dib_header.width_in_pixels: %u\n", b->dib_header.width_in_pixels);
	printf("dib_header.height_in_pixels: %u\n", b->dib_header.height_in_pixels);
	printf("dib_header.color_planes: %u\n", b->dib_header.color_planes);
	printf("dib_header.bits_per_pixel: %u\n", b->dib_header.bits_per_pixel);
	printf("dib_header.compression: %u\n", b->dib_header.compression);
	printf("dib_header.image_size: %u\n", b->dib_header.image_size);
	printf("dib_header.x_pixels_per_meter: %u\n", b->dib_header.x_pixels_per_meter);
	printf("dib_header.y_pixels_per_meter: %u\n", b->dib_header.y_pixels_per_meter);
	printf("dib_header.colors_in_pallette: %u\n", b->dib_header.colors_in_pallette);
	printf("dib_header.important_colors: %u\n", b->dib_header.important_colors);
}

// Writes pixel information to the current position in the bitmap file.
void bitmap_write_pixel(FILE *fp, bitmap_pixel bp)
{
	if (fp == NULL)
	{
		printf("bitmap_write_pixel: file pointer is not valid (file is not open?)\n");
		exit(-1);
	}
	
	// bitmap order is blue green red
	fputc(bp.B, fp);
	fputc(bp.G, fp);
	fputc(bp.R, fp);
}

// helper macro to look closer to triple hex values...
// note there's no final semicolon
#define BP_RGB(bp,r,g,b) bp->R=r;bp->G=g;bp->B=b

// converts an altimetry reading to RGB pixel information
void z_to_pixel(int16_t z, bitmap_pixel* bp)
{
	// from ftp://topex.ucsd.edu/pub/global_grav_1min/README.21.1
	//
	// The gravity anomaly units are 0.1 milligal. An even
        // value signifies the cell does not have an altimeter
	// measurement while an odd value signifies that it does.
	if (z & 0x1)
	{
		if (z < -300)
		{
			BP_RGB(bp, 0x00, 0x00, 0x66);
		}
		else if (z < -200 && z >= -300)
		{
			BP_RGB(bp, 0x00, 0x00, 0xcc);
		}
		else if (z < -100 && z >= -200)
		{
			BP_RGB(bp, 0x00, 0x66, 0xcc);
		}
		else if (z < 0 && z >= -100)
		{
			BP_RGB(bp, 0x00, 0x99, 0xff);
		}
		else if (z < 100 && z >= 0)
		{
			BP_RGB(bp, 0x00, 0xdd, 0xaa);
		}
		else if (z < 200 && z >= 100)
		{
			BP_RGB(bp, 0x00, 0xff, 0x00);
		}
		else if (z < 300 && z >= 200)
		{
			BP_RGB(bp, 0x99, 0xff, 0x66);
		}
		else if (z < 400 && z >= 300)
		{
			BP_RGB(bp, 0xcc, 0xff, 0x66);
		}
		else
		{
			BP_RGB(bp, 0xff, 0xff, 0x66);
		}
		
		//bp->R = 0x80;
		//bp->G = (int)(((double)(z*-1))*128.0/200.0) & 0xff;
		//bp->B = 0x80;
	}
	else
	{
		// no value. Set to white.
		BP_RGB(bp, 0xff, 0xff, 0xff);
	}
}

// writes required number of padding bytes to end of the current
// bitmap row
void write_bitmap_row_padding(FILE *fp)
{
	uint8_t rem = OUTPUT_ROW_BYTES - OUTPUT_ROW_BYTES_WITHOUT_PADDING;
	for (int j=0; j<rem; j++)
	{
		fputc(0, fp);
	}
}

int main(int argc , char *argv[])
{
	// bitmap file and header information
	bitmap* bmp = new_bitmap();
	
	// RGB pixel data
	bitmap_pixel pixel;
	
	// current column out of all possible columns in output
	uint32_t current_output_column = 0;
	
	// current row out of all possible rows in output
	uint32_t current_output_row = 0;
	
	// current read position of the source line; should
	// only vary from 0 to number of columns in row.
	uint32_t source_row_x = 0;
	
	// starting output position of the bitmap pixel data
	uint32_t file_offset = bmp->bitmap_file_header.fileoffset_to_pixelarray;
	
	// value read from input source
	int16_t read_value;
	
	#ifdef DEBUG
	int16_t min_read = 0;
	int16_t max_read = 0;
	print_bitmap_info(bmp);
	printf("OUTPUT_ROW_BYTES: %u\n", OUTPUT_ROW_BYTES);
	printf("OUTPUT_ROW_BYTES_WITHOUT_PADDING: %u\n", OUTPUT_ROW_BYTES_WITHOUT_PADDING);
	printf("OUTPUT_RAW_SINGLE_UNITS: %u\n", OUTPUT_RAW_SINGLE_UNITS);
	#endif
	
	FILE* output_file = fopen(OUTPUT_FILE_PATH,"wb+");
	FILE* input_file = fopen(INPUT_FILE_PATH, "r");
	
	// Done with the bitmap information, write that to output. All remaining
	// pixel data comes afterwards which doesn't depend on any of the header
	// information.
	fwrite(bmp, 1, sizeof(bitmap), output_file);
	free(bmp);
	
	// for each output that needs to be written ...
	int i=0;
	while(i < OUTPUT_RAW_SINGLE_UNITS)
	{
		// If this is the end of the row, write the bitmap padding,
		// advance the row, and reset the column position. Additionally,
		// since no data was read from the source, don't increment counter.
		if (current_output_column + PIXEL_BYTES > OUTPUT_ROW_BYTES_WITHOUT_PADDING)
		{
			write_bitmap_row_padding(output_file);

			current_output_column = 0;
			current_output_row += INVERSE_SCALE;
			source_row_x = 0;

			continue;
		}
		
		// seek to next altimetry reading
		fseek(input_file, (current_output_row * NLON) + source_row_x, 0);
		
		// read value
		fread(&read_value, 1, 2, input_file);
		
		// fix endieness
		read_value = BYTE_SWAP(read_value);
		
		// convert to RGB value
		z_to_pixel(read_value, &pixel);

		// write to output
		bitmap_write_pixel(output_file, pixel);

		// increment counters
		source_row_x += INVERSE_SCALE;		
		current_output_column += PIXEL_BYTES;
		i++;
		
		#ifdef DEBUG
		if (read_value < min_read)
		{
			min_read = read_value;
		}
		if (read_value > max_read)
		{
			max_read = read_value;
		}
		#endif
	}
	
	// write last padding pixels (if necessary)
	if (current_output_column > OUTPUT_ROW_BYTES_WITHOUT_PADDING)
	{
		write_bitmap_row_padding(output_file);
	}
	
	fclose(output_file);
	fclose(input_file);
	
	#ifdef DEBUG
	printf("min value read: %d\n", min_read);
	printf("max value read: %d\n", max_read);
	#endif
	
	return 0;
}