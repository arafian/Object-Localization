#include <librealsense2/rs.hpp> // Include RealSense Cross Platform API
#include <librealsense2/hpp/rs_internal.hpp>
#include <stdio.h>
#include <Windows.h>
#include <iostream>
#include <cmath>
#include "example.hpp"

const int W = 640;
const int H = 480;
const int TARGET_RED = 0x22;
const int TARGET_GREEN = 0xc7;
const int TARGET_BLUE = 0xd6;
const int TARGET_DIST = 100;

GLvoid *mask_pixels = malloc(sizeof(UINT8) * W * H * 3);
GLuint gl_handle;

int filter_rgb(UINT8 r, UINT8 g, UINT8 b);
void upload_mask();
void show_mask(const rect& r);

int main(int argc, char * argv[]) try
{
	const int stream_volume = W * H;
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
		rs2::depth_frame *mask_frame_ptr = (rs2::depth_frame *)malloc(sizeof(rs2::depth_frame));
		auto color = frames.get_color_frame();


		colorFrame = (UINT8*)color.get_data();

		// Build pointcloud
		points = pc.calculate(depth);
		auto vertices = points.get_vertices();

		float totalX = 0.0, totalY = 0.0, totalZ = 0.0;
		int count = 0;


		for (int x = 0; x < W; x++) {
			for (int y = 0; y < H; y++) {
				UINT8 R = colorFrame[3 * (x + y * W)];
				UINT8 G = colorFrame[3 * (x + y * W) + 1];
				UINT8 B = colorFrame[3 * (x + y * W) + 2];
				if (filter_rgb(R, G, B)) {
					((UINT8 *)mask_pixels)[3 * (x + y * W)] = TARGET_RED;
					((UINT8 *)mask_pixels)[3 * (x + y * W) + 1] = TARGET_GREEN;
					((UINT8 *)mask_pixels)[3 * (x + y * W) + 2] = TARGET_BLUE;
					totalX += vertices[x + y * W].x;
					totalY += vertices[x + y * W].y;
					totalZ += vertices[x + y * W].z;
					count++;
				}
				else {
					((UINT8 *)mask_pixels)[3 * (x + y * W)] = 0x00;
					((UINT8 *)mask_pixels)[3 * (x + y * W) + 1] = 0x00;
					((UINT8 *)mask_pixels)[3 * (x + y * W) + 2] = 0x00;
				}
			}
		}

		float avgX = totalX / count;
		float avgY = totalY / count;
		float avgZ = totalZ / count;

		printf("Average Of (%d) Green Stuff: %f, %f, %f\n", count, avgX, avgY, avgZ);

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
