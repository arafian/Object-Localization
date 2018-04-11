#include <librealsense2/rs.hpp> // Include RealSense Cross Platform API
#include <librealsense2/hpp/rs_internal.hpp>
#include <stdio.h>
#include <Windows.h>
#include <iostream>
#include <cmath>
#include "example.hpp"

const int W = 640;
const int H = 480;
const int TARGET_RED = 0xC0;
const int TARGET_GREEN = 0x10;
const int TARGET_BLUE = 0x10;
const int TARGET_DIST = 90;

GLvoid *mask_pixels = malloc(sizeof(UINT8) * W * H * 3);
GLuint gl_handle;

typedef struct Pixel{
	int x, y;
	Pixel *nextPixel;
} Pixel;

typedef struct blob{
	Pixel *pixels;
	int size;
	blob *nextBlob;
} blob;

int filter_rgb(UINT8 r, UINT8 g, UINT8 b);
blob *createBlob(int x, int y);
Pixel *createPixels(int x, int y, int *size);
void upload_mask();
void show_mask(const rect& r);

int main(int argc, char * argv[]) try
{
	UINT8 *colorFrame = NULL;
	UINT16 *depthFrame = NULL;

	window app(W * 2, H, "RealSense Capture Example");

	texture color_image;

	// Declare pointcloud object, for calculating pointclouds and texture mappings
	rs2::pointcloud pc;
	// We want the points object to be persistent so we can display the last cloud when a frame drops
	rs2::points points;
	// Declare RealSense pipeline, encapsulating the actual device and sensors
	rs2::pipeline pipe;
	// declare Frameset object
	rs2::frameset frames;
	// Start streaming with default recommended configuration
	rs2::config cfg;
	// Use a configuration object to request only depth from the pipeline
	cfg.enable_stream(RS2_STREAM_DEPTH, W, H, RS2_FORMAT_Z16, 30);
	cfg.enable_stream(RS2_STREAM_COLOR, W, H, RS2_FORMAT_RGB8, 30);
	pipe.start(cfg);

	while (app)
	{
		printf("getting frame:\n");
		frames = pipe.wait_for_frames();

		auto depth = frames.get_depth_frame();
		//rs2::depth_frame *mask_frame_ptr = (rs2::depth_frame *)malloc(sizeof(rs2::depth_frame));
		auto color = frames.get_color_frame();


		colorFrame = (UINT8*)color.get_data();

		// Build pointcloud
		points = pc.calculate(depth);
		auto vertices = points.get_vertices();

		// Create mask by filtering RGB values
		for (int x = 0; x < W; x++) {
			for (int y = 0; y < H; y++) {
				UINT8 R = colorFrame[3 * (x + y * W)];
				UINT8 G = colorFrame[3 * (x + y * W) + 1];
				UINT8 B = colorFrame[3 * (x + y * W) + 2];
				if (filter_rgb(R, G, B)) {
					((UINT8 *)mask_pixels)[3 * (x + y * W)] = TARGET_RED;
					((UINT8 *)mask_pixels)[3 * (x + y * W) + 1] = TARGET_GREEN;
					((UINT8 *)mask_pixels)[3 * (x + y * W) + 2] = TARGET_BLUE;
				}
				else {
					((UINT8 *)mask_pixels)[3 * (x + y * W)] = 0x00;
					((UINT8 *)mask_pixels)[3 * (x + y * W) + 1] = 0x00;
					((UINT8 *)mask_pixels)[3 * (x + y * W) + 2] = 0x00;
				}
			}
		}

		// Separate into blobs, and determine the largest blob
		blob *blobsHead = NULL, *blobsTail = NULL, *blobsTemp = NULL, *largestBlob = NULL;
		for (int x = 0; x < W; x++) {
			for (int y = 0; y < H; y++) {
				if (((UINT8 *)mask_pixels)[3 * (x + y * W)]) {
					blobsTemp = createBlob(x, y);
					if (!blobsTemp) {
						printf("ERROR! Out of Memory!");
						return EXIT_FAILURE;
					}
					if (!blobsHead) {
						blobsHead = blobsTemp;
					}
					else {
						blobsTail->nextBlob = blobsTemp;
					}
					blobsTail = blobsTemp;

					if (!largestBlob || largestBlob->size < blobsTemp->size) {
						largestBlob = blobsTemp;
					}
				}
			}
		}

		float totalX = 0.0, totalY = 0.0, totalZ = 0.0;
		int count = 0;
		// Only fil back the largest Blob (and average it's vertices from the pointcloud)
		Pixel *pixelsPtr = NULL;

		if (largestBlob) {
			pixelsPtr = largestBlob->pixels;
			while (pixelsPtr) {
				((UINT8 *)mask_pixels)[3 * (pixelsPtr->x + pixelsPtr->y * W)] = TARGET_RED;
				((UINT8 *)mask_pixels)[3 * (pixelsPtr->x + pixelsPtr->y * W) + 1] = TARGET_GREEN;
				((UINT8 *)mask_pixels)[3 * (pixelsPtr->x + pixelsPtr->y * W) + 2] = TARGET_BLUE;

				totalX += vertices[pixelsPtr->x + pixelsPtr->y * W].x;
				totalY += vertices[pixelsPtr->x + pixelsPtr->y * W].y;
				totalZ += vertices[pixelsPtr->x + pixelsPtr->y * W].z;
				pixelsPtr = pixelsPtr->nextPixel;
			}
		}

		// Free all of the blobs and pixel lists
		Pixel *pixelFree = NULL;
		while (blobsHead) {
			pixelsPtr = blobsHead->pixels;
			while (pixelsPtr) {
				pixelFree = pixelsPtr;
				pixelsPtr = pixelsPtr->nextPixel;
				free(pixelFree);
			}
			blobsTemp = blobsHead;
			blobsHead = blobsHead->nextBlob;
			free(blobsTemp);
		}

		float avgX = count == 0 ? 0 : totalX / count;
		float avgY = count == 0 ? 0 : totalY / count;
		float avgZ = count == 0 ? 0 : totalZ / count;

		printf("Average Of (%d) Stuff: %f, %f, %f\n", count, avgX, avgY, avgZ);

		color_image.render(color, { 0, 0, app.width() / 2, app.height() });
		upload_mask();
		rect r = { app.width() / 2, 0, app.width() / 2, app.height() };
		show_mask(r.adjust_ratio({ float(W), float(H) }));


	}
	return EXIT_SUCCESS;
}
catch (const rs2::error & e)
{
	std::cerr << "RealSense error calling " << e.get_failed_function() << "(" << e.get_failed_args() << "):\n    " << e.what() << std::endl;
	return EXIT_FAILURE;
}
catch (const std::exception & e)
{
	std::cerr << e.what() << std::endl;
	return EXIT_FAILURE;
}

int filter_rgb(UINT8 r, UINT8 g, UINT8 b) {
	return (r - TARGET_RED) * (r - TARGET_RED)
		+ (g - TARGET_GREEN) * (g - TARGET_GREEN)
		+ (b - TARGET_BLUE) * (b - TARGET_BLUE)
		< TARGET_DIST * TARGET_DIST;
}

void upload_mask()
{
	if (!gl_handle)
		glGenTextures(1, &gl_handle);
	GLenum err = glGetError();

	glBindTexture(GL_TEXTURE_2D, gl_handle);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, W, H, 0, GL_RGB, GL_UNSIGNED_BYTE, mask_pixels);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
}

void show_mask(const rect& r)
{
	if (!gl_handle) return;

	glBindTexture(GL_TEXTURE_2D, gl_handle);
	glEnable(GL_TEXTURE_2D);
	glBegin(GL_QUAD_STRIP);
	glTexCoord2f(0.f, 1.f); glVertex2f(r.x, r.y + r.h);
	glTexCoord2f(0.f, 0.f); glVertex2f(r.x, r.y);
	glTexCoord2f(1.f, 1.f); glVertex2f(r.x + r.w, r.y + r.h);
	glTexCoord2f(1.f, 0.f); glVertex2f(r.x + r.w, r.y);
	glEnd();
	glDisable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, 0);
}

// Returns a new pointer to a blob or NULL if error
blob *createBlob(int x, int y) {
	blob *returnBlob = (blob *)malloc(sizeof(blob));
	if (!returnBlob) {
		return NULL;
	}
	returnBlob->nextBlob = NULL;
	returnBlob->size = 0;
	returnBlob->pixels = createPixels(x, y, &(returnBlob->size));
	return returnBlob;
}

// Returns a new pointer to a pixel or NULL is error
Pixel *createPixels(int x, int y, int *size) {
	UINT8 *pixels = (UINT8 *)mask_pixels;
	Pixel *headPixel = NULL, *tailPixel = NULL;
	pixels[3 * (x + y * W)] = 0;
	*size++;

	headPixel = (Pixel *)malloc(sizeof(Pixel));
	if (!headPixel)
		return NULL;
	headPixel->x = x;
	headPixel->y = y;
	headPixel->nextPixel = NULL;
	tailPixel = headPixel;

	if (y + 1 < H && pixels[3 * (x + (y + 1) * W)]) {
		tailPixel->nextPixel = createPixels(x, y + 1, size);
		if (!(tailPixel->nextPixel))
			return NULL;
		while (tailPixel->nextPixel) {
			tailPixel = tailPixel->nextPixel;
		}
	}
	if (x - 1 >= 0 && pixels[3 * ((x - 1) + y * W)]) {
		tailPixel->nextPixel = createPixels(x - 1, y, size);
		if (!(tailPixel->nextPixel))
			return NULL;
		while (tailPixel->nextPixel) {
			tailPixel = tailPixel->nextPixel;
		}
	}
	if (y - 1 >= 0 && pixels[3 * (x + (y - 1) * W)]) {
		tailPixel->nextPixel = createPixels(x, y - 1, size);
		if (!(tailPixel->nextPixel))
			return NULL;
		while (tailPixel->nextPixel) {
			tailPixel = tailPixel->nextPixel;
		}
	}
	if (x + 1 < W && pixels[3 * ((x + 1) + y * W)]) {
		tailPixel->nextPixel = createPixels(x + 1, y, size);
		if (!(tailPixel->nextPixel))
			return NULL;
		while (tailPixel->nextPixel) {
			tailPixel = tailPixel->nextPixel;
		}
	}
	
	return headPixel;
}
