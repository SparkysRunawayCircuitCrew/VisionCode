
#include <iostream>
#include <functional>
#include <vector>

#include <opencv2/opencv.hpp>

typedef int FilterGroup;
typedef std::function<void(cv::Mat&)> FilterFunc;

struct Camera {
    int width, height;
    
    std::vector<std::vector<FilterFunc>> filters;
	cv::VideoCapture videoCap;

	cv::Mat rawFrame;
	cv::Mat filteredFrame;
	
	Camera(int id = 0);

	// Captures and processes an image frame
	void capture();
	void capture(std::vector<FilterGroup> groups);
	void captureCropped(std::vector<FilterGroup> groups, cv::Rect cropArea);

    // Adds a filter to a specified filter group
    void addFilter(FilterGroup group, FilterFunc filter);
};
