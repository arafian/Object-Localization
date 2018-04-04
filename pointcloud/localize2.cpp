// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015-2017 Intel Corporation. All Rights Reserved.

#include <librealsense2/rs.hpp> // Include RealSense Cross Platform API
#include "example.hpp"          // Include short list of convenience functions for rendering

#include <algorithm>            // std::min, std::max
#include <iomanip>				// std::setprecision

#include <iostream>
#include <map>

struct Data {
	unsigned char* vertices;
	int width;
	int height;
};

struct RGB {
	int triple[3];
};



// Helper functions
void register_glfw_callbacks(window& app, glfw_state& app_state);
Data readBMP(char* filename);
int **create2D(int row, int col);

int main(int argc, char * argv[]) try
{
	
	// Create a simple OpenGL window for rendering:
	window app(1280, 720, "RealSense Pointcloud Example");
	// Construct an object to manage view state
	glfw_state app_state;
	// register callbacks to allow manipulation of the pointcloud
	register_glfw_callbacks(app, app_state);

	// Declare pointcloud object, for calculating pointclouds and texture mappings
	rs2::pointcloud pc;

	// We want the points object to be persistent so we can display the last cloud when a frame drops
	rs2::points points;

	// Declare RealSense pipeline, encapsulating the actual device and sensors
	rs2::pipeline pipe;

	rs2::config cfg;
	cfg.enable_stream(RS2_STREAM_DEPTH); // Enable default depth
										 // For the color stream, set format to RGBA
										 // To allow blending of the color frame on top of the depth frame
	cfg.enable_stream(RS2_STREAM_COLOR, RS2_FORMAT_RGBA8);
	// rs2::config cfg;
	// cfg.enable_stream(RS2_STREAM_DEPTH, 640, 480, RS2_FORMAT_Z16, 30); // Enable default depth
										 // For the color stream, set format to RGBA
										 // To allow blending of the color frame on top of the depth frame
	// cfg.enable_stream(RS2_STREAM_COLOR, 640, 480, RS2_FORMAT_RGB8, 30);
	
	int first = 1;

	// Start streaming with default recommended configuration
    pipe.start(cfg);
	
	while (app) // Application still alive?
	{
		// Wait for the next set of frames from the camera
		auto frames = pipe.wait_for_frames();
		
		auto depth = frames.get_depth_frame();

		auto depth_width = depth.get_width();

		auto depth_height = depth.get_height();

		// Generate the pointcloud and texture mappings
		points = pc.calculate(depth);

		auto color = frames.get_color_frame();

		/* this segment actually prints the pointcloud */
		auto vertices = points.get_vertices();

		// create classes for segmentation
		std::map<std::string, RGB> classes;
		classes["bike"] = { 0, 128, 0 }; // green
		classes["cyclist"] = { 255,192,203 }; // pink

		Data data = readBMP("testOutput.bmp");

		auto tex_coords = points.get_texture_coordinates(); // and texture coordinates

		double width_ratio = depth_width / (double) data.width;
		double height_ratio = depth_height / (double) data.height;

		double x_total = 0.0;
		double y_total = 0.0;
		double z_total = 0.0;
		int total_vertices = 0;

		// what we are trying to localize
		RGB target = classes["cyclist"];

		// go through image
		for (int x = 0; x < data.width; x++)
		{
			for (int y = 0; y < data.height; y++)
			{
				// if color of this pixel is same as target
				if (data.vertices[3 * (x * data.width + y)] == target.triple[0] &&
					data.vertices[3 * (x * data.width + y) + 1] == target.triple[1] &&
					data.vertices[3 * (x * data.width + y) + 2] == target.triple[2])
				{
					// update totals
					total_vertices += 1;
					int depth_x = (int) (x * width_ratio);
					int depth_y = (int) (y * height_ratio);
					x_total += vertices[depth_x + depth_y * data.width].x;
					y_total += vertices[depth_x + depth_y * data.width].y;
					z_total += vertices[depth_x + depth_y * data.width].z;
					color.get_data();
				}
			}
		}

		double avg_x = x_total / total_vertices;
		double avg_y = y_total / total_vertices;
		double avg_z = z_total / total_vertices;

		std::cout << std::setprecision(5) << "\nx average is: " << avg_x;
		std::cout << std::setprecision(5) << "\ny average is: " << avg_y;
		std::cout << std::setprecision(5) << "\nz average is: " << avg_z;

		// Tell pointcloud object to map to this color frame
		pc.map_to(color);

		// Upload the color frame to OpenGL
		app_state.tex.upload(color);

		// Draw the pointcloud
		draw_pointcloud(app, app_state, points);
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

// from https://stackoverflow.com/questions/9296059/read-pixel-value-in-bmp-file
Data readBMP(char* filename)
{
	int i;
	FILE* f = fopen(filename, "rb");
	unsigned char info[54];
	fread(info, sizeof(unsigned char), 54, f); // read the 54-byte header

	int numBits = *(int*)&info[28];
	if (numBits == 24) {
		///throw "24 bit bmp not supported";
	}

	// extract image height and width from header
	int width = *(int*)&info[18];
	int height = *(int*)&info[22];

	int size = 3 * width * height;
	unsigned char* vertices = new unsigned char[size]; // allocate 3 bytes per pixel
	fread(vertices, sizeof(unsigned char), size, f); // read the rest of the data at once
	fclose(f);

	for (i = 0; i < size; i += 3)
	{
		// Convert (B, G, R) to (R, G, B)
		unsigned char tmp = vertices[i];
		vertices[i] = vertices[i + 2];
		vertices[i + 2] = tmp;
	}

	// store relevant data in struct
	Data data;
	data.vertices = vertices;
	data.width = width;
	data.height = height;

	return data;
}

int **create2D(int row, int col) {
	// create the array
	int **arr = new int*[row];
	for (int i = 0; i < row; i++) {
		arr[i] = new int[col];
		// fill array
		for (int j = 0; j < col; j++) {
			arr[i][j] = 1;
		}
	}
	return arr;
}

int equalsRGB(RGB r1, RGB r2) {
	for (int i = 0; i < 3; i++) {
		// make sure each part is equal
		if (r1.triple[i] != r2.triple[i]) {
			return false;
		}
	}
	return true;
}