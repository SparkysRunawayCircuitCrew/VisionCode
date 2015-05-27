
#include <iostream>

#include "camera.hpp"
#include <opencv2/opencv.hpp>

Camera::Camera(int id): videoCap(id) { }

void Camera::capture(std::vector<FilterGroup> groups) {
    this->videoCap >> this->rawFrame;
    this->filteredFrame = this->rawFrame;

    for (FilterGroup& group : groups) {
        for (auto& filter : this->filters[group]) {
            filter(this->filteredFrame);
        }
    }
}

void Camera::addFilter(FilterGroup group, FilterFunc filter) {
    // Prevents segfault by ensuring vector is large enough
    if (group >= this->filters.size()) {
        this->filters.resize(group + 1);
    }

    this->filters[group].push_back(filter);
}
