
#include <iostream>
#include <functional>
#include <vector>

#include <opencv2/core.hpp>

typedef std::function<cv::Mat(cv::Mat)> FilterFunc;

struct Camera {
	std::vector<FilterFunc> filters;
	cv::VideoCapture videoCap;

	cv::Mat rawFrame;
	cv::Mat filteredFrame;
	
	Camera(int id = 0);
	~Camera();

	// Captures and processes an image frame
	void capture();
};
