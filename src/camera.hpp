
#pragma once

#include <iostream>
#include <functional>
#include <vector>

#include <opencv2/opencv.hpp>

namespace vision {

typedef int FilterGroup;
typedef std::function<void(cv::Mat&)> FilterFunc;

struct Camera {
    int width, height;
    
    std::vector<std::vector<FilterFunc>> filters;
	cv::VideoCapture videoCap;

	cv::Mat rawFrame;
	cv::Mat filteredFrame;
	
	Camera(int id = 0);
	Camera(std::string url);

	// Captures and processes an image frame
    cv::Mat& capture(std::vector<FilterGroup> groups = {}, bool clearPreviousFilters = true, cv::Mat* image = nullptr);
    cv::Mat& captureCropped(std::vector<FilterGroup> groups, cv::Rect cropArea, bool clearPreviousFilters = true, cv::Mat* image = nullptr);

    // Adds a filter to a specified filter group
    void addFilter(FilterGroup group, FilterFunc filter);

private:
    void prepareVideoCap();

};

}
