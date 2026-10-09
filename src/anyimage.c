#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <tiffio.h>	// TIFF reading

#define NANOSVG_ALL_COLOR_KEYWORDS	// Include full list of color keywords.
#define NANOSVG_IMPLEMENTATION
#include "nanosvg.h"	// SVG parsing

#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvgrast.h"	// SVG rasterization

#define STB_IMAGE_IMPLEMENTATION
#define STBI_SUPPORT_ZLIB
#include "stb_image.h"	// PNG/JPG/GIF/TGA/BMP/PIC/PNM/PSD/HDR reading

#include <webp/decode.h>
#include <avif/avif.h>

// Structure to hold streamed data
typedef struct {
	unsigned char *data;
	size_t size;
	size_t offset;
} MemoryStream;

unsigned char *read_stbi(const char *filename, int *x, int *y) {
	int channels_in_file;
	stbi_uc *data = NULL;

	if ((data = stbi_load(filename, x, y, &channels_in_file, 4))) {
		return data;
	}

	return NULL;
}

unsigned char *memory_stbi(const unsigned char *buffer, size_t len, int *x, int *y) {
	int channels;
	stbi_uc *data = NULL;

	if (stbi_info_from_memory((const stbi_uc *)buffer, (int)len, x, y, &channels)
		&& (data = stbi_load_from_memory(buffer, len, x, y, &channels, 4))) {
		return data;
	}

	return NULL;
}

unsigned char *read_nsvg(const char *filename, int *x, int *y) {
	NSVGimage *shapes = NULL;
	NSVGrasterizer *rast = NULL;
	unsigned char *data = NULL;

	if ((shapes = nsvgParseFromFile(filename, "px", 96.0f))) {
		if (!shapes->width && !shapes->height) {
			nsvgDelete(shapes);
			return NULL;
		}

		*x = (int)shapes->width;
		*y = (int)shapes->height;

		rast = nsvgCreateRasterizer();
		if (!rast) {
			fprintf(stderr, "Raster allocation failed\n");
			nsvgDelete(shapes);
			return NULL;
		}

		data = (unsigned char *)malloc(shapes->width * shapes->height * 4);
		if (!data) {
			fprintf(stderr, "Memory allocation failed\n");
			nsvgDeleteRasterizer(rast);
			nsvgDelete(shapes);
			return NULL;
		}

		nsvgRasterize(rast, shapes, 0, 0, 1.0f, data,
			shapes->width, shapes->height, shapes->width * 4);
		nsvgDeleteRasterizer(rast);
		nsvgDelete(shapes);
		return data;
	}

	return NULL;
}

unsigned char *memory_nsvg(const unsigned char *buffer, size_t len, int *x, int *y) {
	NSVGimage *shapes = NULL;
	NSVGrasterizer *rast = NULL;
	unsigned char *data = NULL;

	if ((shapes = nsvgParse((char *)buffer, "px", 96.0f))) {
		if (!shapes->width && !shapes->height) {
			nsvgDelete(shapes);
			return NULL;
		}

		rast = nsvgCreateRasterizer();
		if (!rast) {
			fprintf(stderr, "Raster allocation failed\n");
			nsvgDelete(shapes);
			return NULL;
		}

		data = malloc(shapes->width * shapes->height * 4);
		if (!data) {
			fprintf(stderr, "Memory allocation failed\n");
			nsvgDeleteRasterizer(rast);
			nsvgDelete(shapes);
			return NULL;
		}

		nsvgRasterize(rast, shapes, 0, 0, 1.0f, data, shapes->width, shapes->height, shapes->width * 4);
		nsvgDeleteRasterizer(rast);
		nsvgDelete(shapes);
		*x = (int)shapes->width;
		*y = (int)shapes->height;
		return data;
	}

	return NULL;
}

/* Custom read function for TIFFClientOpen */
static tsize_t mem_read(thandle_t handle, tdata_t buf, tsize_t size) {
	MemoryStream *image = (MemoryStream *)handle;
	if (image->offset + size > image->size)
		size = image->size - image->offset;
	memcpy(buf, image->data + image->offset, size);
	image->offset += size;
	return size;
}

/* Dummy write function (read-only) */
static tsize_t mem_write(thandle_t handle, tdata_t buf, tsize_t size) {
	return 0; // not supported
}

/* Seek function */
static toff_t mem_seek(thandle_t handle, toff_t offset, int whence) {
	MemoryStream *image = (MemoryStream *)handle;
	size_t new_offset;
	switch (whence) {
		case SEEK_SET: new_offset = offset; break;
		case SEEK_CUR: new_offset = image->offset + offset; break;
		case SEEK_END: new_offset = image->size + offset; break;
		default: return (toff_t)-1;
	}
	if (new_offset > image->size) return (toff_t)-1;
	image->offset = new_offset;
	return image->offset;
}

/* Close function */
static int mem_close(thandle_t handle) {
	return 0; // nothing to free here
}

/* Size function */
static toff_t mem_size(thandle_t handle) {
	MemoryStream *image = (MemoryStream *)handle;
	return image->size;
}

/* Map/unmap functions (optional, not used here) */
static int mem_map(thandle_t handle, tdata_t *data, toff_t *size) { return 0; }
static void mem_unmap(thandle_t handle, tdata_t data, toff_t size) {}

// Function to read TIFF into RGBA buffer
static unsigned char *read_tiff_rgba(const char *filename, uint32_t *width, uint32_t *height) {
	TIFF *tif = TIFFOpen(filename, "r");
	if (!tif) {
		return NULL;
	}

	TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, width);
	TIFFGetField(tif, TIFFTAG_IMAGELENGTH, height);

	size_t i, npixels = (*width) * (*height);
	uint32_t *raster = (uint32_t *)_TIFFmalloc(npixels * sizeof(uint32_t));
	if (!raster) {
		fprintf(stderr, "Memory allocation failed\n");
		TIFFClose(tif);
		return NULL;
	}

	if (!TIFFReadRGBAImage(tif, *width, *height, raster, 0)) {
		_TIFFfree(raster);
		TIFFClose(tif);
		return NULL;
	}

	// Convert from uint32_t RGBA to unsigned char RGBA
	unsigned char *img_data = (unsigned char *)malloc(npixels * 4);
	if (!img_data) {
		fprintf(stderr, "Memory allocation failed\n");
		_TIFFfree(raster);
		TIFFClose(tif);
		return NULL;
	}

	for (i = 0; i < npixels; i++) {
		uint32_t pixel = raster[i];
		img_data[i * 4 + 0] = TIFFGetR(pixel);
		img_data[i * 4 + 1] = TIFFGetG(pixel);
		img_data[i * 4 + 2] = TIFFGetB(pixel);
		img_data[i * 4 + 3] = TIFFGetA(pixel);
	}

	_TIFFfree(raster);
	TIFFClose(tif);
	return img_data;
}

unsigned char *read_tiff(const char *filename, int *x, int *y) {
	unsigned char *data = NULL;

	if ((data = read_tiff_rgba(filename, x, y))) {
		return data;
	}

	return NULL;
}

unsigned char *memory_tiff(const unsigned char *buffer, size_t len, int *x, int *y) {
	TIFF *tif = NULL;
	unsigned char *data = NULL;
	MemoryStream stb;

	stb.data = (unsigned char *)buffer;
	stb.size = len;
	if ((tif = TIFFClientOpen("MemTIFF", "r", (thandle_t)&stb, mem_read,
		mem_write, mem_seek, mem_close, mem_size, mem_map, mem_unmap))) {
		uint32_t width, height;
		TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width);
		TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &height);

		data = (stbi_uc *)_TIFFmalloc(width * height * sizeof(uint32_t));
		if (!data) {
			fprintf(stderr, "Raster allocation failed\n");
			TIFFClose(tif);
			return 0;
		}

		/* Read RGBA web_image */
		if (!TIFFReadRGBAImageOriented(tif, width, height, (uint32_t *)data, ORIENTATION_TOPLEFT, 0)) {
			fprintf(stderr, "TIFFReadRGBAImage failed\n");
			_TIFFfree(data);
			TIFFClose(tif);
			return 0;
		}
		*x = width;
		*y = height;
		return data;
	}

	return NULL;
}

unsigned char *memory_webp(const unsigned char *buffer, size_t len, int *x, int *y) {
	unsigned char *data = WebPDecodeRGBA(buffer, len, x, y);
	if (!data) {
		fprintf(stderr, "Failed to decode WebP image.\n");
		return NULL;
	}

	return data;
}

static void avif_alpha_blend(unsigned char *dst, const unsigned char *src, int width, int height,
	unsigned char bg_r, unsigned char bg_g, unsigned char bg_b) {
	int y, x;
	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			int idx = (y * width + x) * 4;
			unsigned char r = src[idx];
			unsigned char g = src[idx + 1];
			unsigned char b = src[idx + 2];
			unsigned char a = src[idx + 3];

			// Blend against background
			dst[idx] = (r * a + bg_r * (255 - a)) / 255;
			dst[idx + 1] = (g * a + bg_g * (255 - a)) / 255;
			dst[idx + 2] = (b * a + bg_b * (255 - a)) / 255;
			dst[idx + 3] = 255; // Fully opaque for XImage
		}
	}
}

unsigned char *read_avif(const char *filename, int *x, int *y) {
	avifDecoder *decoder = avifDecoderCreate();
	if (!decoder) {
		fprintf(stderr, "Failed to create AVIF decoder\n");
		return NULL;
	}

	avifResult res = avifDecoderSetIOFile(decoder, filename);
	if (res != AVIF_RESULT_OK) {
		fprintf(stderr, "AVIF read error: %s\n", avifResultToString(res));
		avifDecoderDestroy(decoder);
		return NULL;
	}

	res = avifDecoderParse(decoder);
	if (res != AVIF_RESULT_OK) {
		avifDecoderDestroy(decoder);
		return NULL;
	}

	res = avifDecoderNextImage(decoder);
	if (res != AVIF_RESULT_OK) {
		fprintf(stderr, "No image in AVIF file\n");
		avifDecoderDestroy(decoder);
		return NULL;
	}

	*x = decoder->image->width;
	*y = decoder->image->height;

	avifRGBImage rgb;
	avifRGBImageSetDefaults(&rgb, decoder->image);
	rgb.depth = 8;
	rgb.format = AVIF_RGB_FORMAT_RGBA;
	avifRGBImageAllocatePixels(&rgb);

	if (avifImageYUVToRGB(decoder->image, &rgb) != AVIF_RESULT_OK) {
		fprintf(stderr, "Failed to convert AVIF to RGBA\n");
		avifRGBImageFreePixels(&rgb);
		avifDecoderDestroy(decoder);
		return NULL;
	}

	// Allocate blended buffer
	unsigned char *blended = malloc(rgb.rowBytes * rgb.height);
	if (!blended) {
		fprintf(stderr, "Memory allocation failed\n");
		avifRGBImageFreePixels(&rgb);
		avifDecoderDestroy(decoder);
		return NULL;
	}

	avif_alpha_blend(blended, rgb.pixels, *x, *y, 255, 255, 255); // white background

	avifRGBImageFreePixels(&rgb);
	avifDecoderDestroy(decoder);
	return blended;
}

unsigned char *memory_avif(const unsigned char *buffer, size_t len, int *x, int *y) {
	avifDecoder *decoder = avifDecoderCreate();
	if (!decoder) {
		trace;
		fprintf(stderr, "Failed to create AVIF decoder\n");
		return NULL;
	}

	if (avifDecoderSetIOMemory(decoder, buffer, len) != AVIF_RESULT_OK) {
		fprintf(stderr, "Failed to read AVIF from memory\n");
		avifDecoderDestroy(decoder);
		return NULL;
	}

	avifResult res = avifDecoderParse(decoder);
	if (res != AVIF_RESULT_OK) {
		avifDecoderDestroy(decoder);
		return NULL;
	}

	res = avifDecoderNextImage(decoder);
	if (res != AVIF_RESULT_OK) {
		fprintf(stderr, "No image in AVIF file\n");
		avifDecoderDestroy(decoder);
		return NULL;
	}

	*x = decoder->image->width;
	*y = decoder->image->height;

	avifRGBImage rgb;
	avifRGBImageSetDefaults(&rgb, decoder->image);
	rgb.depth = 8;
	rgb.format = AVIF_RGB_FORMAT_RGBA;
	avifRGBImageAllocatePixels(&rgb);

	if (avifImageYUVToRGB(decoder->image, &rgb) != AVIF_RESULT_OK) {
		fprintf(stderr, "Failed to convert AVIF to RGBA\n");
		avifRGBImageFreePixels(&rgb);
		avifDecoderDestroy(decoder);
		return NULL;
	}

	// Allocate blended buffer
	unsigned char *blended = malloc(rgb.rowBytes * rgb.height);
	if (!blended) {
		fprintf(stderr, "Memory allocation failed\n");
		avifRGBImageFreePixels(&rgb);
		avifDecoderDestroy(decoder);
		return NULL;
	}

	avif_alpha_blend(blended, rgb.pixels, *x, *y, 255, 255, 255); // white background

	avifRGBImageFreePixels(&rgb);
	avifDecoderDestroy(decoder);
	return blended;
}

unsigned char *read_file(const char *filename, int *len) {
	*len = 0;
	FILE *fp = fopen(filename, "rb");
	if (!fp) {
		perror("fopen");
		return NULL;
	}

	fseek(fp, 0, SEEK_END);
	long file_size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	unsigned char *file_data = malloc(file_size);
	if (!file_data) {
		fprintf(stderr, "Memory allocation failed.\n");
		fclose(fp);
		return NULL;
	}

	if (fread(file_data, 1, file_size, fp) != (size_t)file_size) {
		fprintf(stderr, "Error: Failed to read file.\n");
		free(file_data);
		fclose(fp);
		return NULL;
	}

	fclose(fp);
	*len = (int)file_size;
	return file_data;
}
