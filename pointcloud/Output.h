#pragma once

#include <fstream>

using namespace std;
typedef unsigned char uchar;

class Image {
public:
	Image(const char* fileName);
	~Image();
	void writeTo(const char* fileName);
	void smoothFilter();
private:
	ifstream * in;
	ofstream* out;
	uchar headerData[1078]; // not sure about this size
	uchar** imageData;
	uchar filter[3][3];
	uchar** filteredData;
};