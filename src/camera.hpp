
#include <iostream>
#include <functional>
#include <vector>

#include <opencv2/opencv.hpp>

typedef int FilterGroup;
typedef std::function<void(cv::Mat&)> FilterFunc;

struct Camera {
    std::vector<std::vector<FilterFunc>> filters;
	cv::VideoCapture videoCap;

	cv::Mat rawFrame;
	cv::Mat filteredFrame;
	
	Camera(int id = 0);

	// Captures and processes an image frame
	void capture(FilterGroup group);

    // Adds a filter to a specified filter group
    void addFilter(FilterGroup group, FilterFunc filter);
};
