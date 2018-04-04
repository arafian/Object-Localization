
#include <iostream>
#include <stdlib.h>
#include <librealsense2/rs.hpp>

#include "assert.h"

using namespace rs2;

static void rs2_deproject_pixel_to_point(float point[3], const struct rs2_intrinsics * intrin, const float pixel[2], float depth);
/*
void main()
{
	// Create a Pipeline, which serves as a top-level API for streaming and processing frames
	pipeline pipe;

	// Configure and start the pipeline
	pipe.start();

	while (true)
	{
		// Block program until frames arrive
		frameset frames = pipe.wait_for_frames();

		// Try to get a frame of a depth image
		depth_frame depth = frames.get_depth_frame();
		// The frameset might not contain a depth frame, if so continue until it does
		if (!depth) continue;

		// Get the depth frame's dimensions
		float width = depth.get_width();
		float height = depth.get_height();

		// Query the distance from the camera to the object in the center of the image
		float dist_to_center = depth.get_distance(width / 2, height / 2);
		float test_dist = depth.get_distance(0, 0);

		// Print the distance 
		std::cout << "The camera is facing an object " << dist_to_center << " meters away \r";

		//std::cout << depth[0];
		std::cout << "\nframe is: " << depth[0 + 2 * 640];
		
		rs2::config cfg;
		cfg.enable_stream(RS2_STREAM_DEPTH); // Enable default depth
		auto profile = pipe.start(cfg);

		auto stream = profile.get_stream(RS2_STREAM_DEPTH).as<rs2::video_stream_profile>();
		auto intrinsics = stream.get_intrinsics(); // Calibration data
		// rs2_intrinsics instrinsicts = frames.get_intrinsics();
		// point which will be point in returned 3D space
		double point[3];
		// rs2_deproject_pixel_to_point(point[], &intrinsics, );
		std::cout << point;
		
	}
}
*/
/* Given pixel coordinates and depth in an image with no distortion or inverse distortion coefficients, compute the corresponding point in 3D space relative to the same camera */
static void rs2_deproject_pixel_to_point(float point[3], const struct rs2_intrinsics * intrin, const float pixel[2], float depth)
{
	assert(intrin->model != RS2_DISTORTION_MODIFIED_BROWN_CONRADY); // Cannot deproject from a forward-distorted image
	assert(intrin->model != RS2_DISTORTION_FTHETA); // Cannot deproject to an ftheta image
													//assert(intrin->model != RS2_DISTORTION_BROWN_CONRADY); // Cannot deproject to an brown conrady model

	float x = (pixel[0] - intrin->ppx) / intrin->fx;
	float y = (pixel[1] - intrin->ppy) / intrin->fy;
	if (intrin->model == RS2_DISTORTION_INVERSE_BROWN_CONRADY)
	{
		float r2 = x * x + y * y;
		float f = 1 + intrin->coeffs[0] * r2 + intrin->coeffs[1] * r2*r2 + intrin->coeffs[4] * r2*r2*r2;
		float ux = x * f + 2 * intrin->coeffs[2] * x*y + intrin->coeffs[3] * (r2 + 2 * x*x);
		float uy = y * f + 2 * intrin->coeffs[3] * x*y + intrin->coeffs[2] * (r2 + 2 * y*y);
		x = ux;
		y = uy;
	}
	point[0] = depth * x;
	point[1] = depth * y;
	point[2] = depth;
}

/* Transform 3D coordinates relative to one sensor to 3D coordinates relative to another viewpoint */
