#pragma once

#include "filedata.hpp"

#include <opencv2/opencv.hpp>

#include <string>
#include <iostream>

namespace vision {

    /**
     * Filter which attempts to find a yellow or red stanchion in an image
     * (assumes only one will be found and prefers yellow over red).
     */

    class Filter {
    public:
        /** Construct and initialize a new instance. */
        Filter();

        /** Destructor cleans up any allocated memory. */
        ~Filter();

        /**
         * Apply filter rules to image stored in matrix.
         *
         * @param mat Matrix containing original image data in BGR form.
         *
         * @return Found::None, Found::Red or Found::Yellow indicating
         * whether we found a stanchion (tons of get methods can be used
         * after this as well).
         */
        Found filter(const cv::Mat& mat);

	/**
	 * Checks a polygon bounding box to see if it could be a stanchion image.
	 *
	 * @param w Width in pixels of the bounding box.
	 * @param h Height in pixels of the bounding box.
	 *
	 * @return true If it passes minimum requirements in height and aspect ratio.
	 */
	static bool isPossibleStanchion(const std::vector<cv::Point>& polygon,
					cv::Rect& br) {
	    br = cv::boundingRect(polygon);
	    int w = br.width;
	    int h = br.height;
	    int hw = ((h * 100) / w);
	    int pts = polygon.size();

	    return (w > 5) && (h > 15) && (hw > 50) && (hw < 300)
		&& (pts > 3) && (pts < 10);
	}

        /**
         * Method allows you to disable the fall back to red filter
         * (only useful when debugging yellow search algorithms).
         */
        void disableRed() { _redEnabled = false; }

        /**
         * Writes out all image files (from each step of the process).
         *
         * @param baseName The base name to use for each file name (if
         * you don't include path information, files will be created
         * in the current directory).
	 *
	 * @param orig Reference to original image that was processed
	 * by the filter.
	 *
	 * @param Pass true if you want to write out the original as
	 * well as all the parts.
         */
        void writeImages(const std::string& baseName, const cv::Mat& orig,
			 bool writeOrig = true) const;

        /** Get color transformed (XYZ, HSV, or whatever color space we switch to). */
        const cv::Mat& getColorTransformed() const { return _colorTransformed; }

        /** Get color reduced matrix. */
        const cv::Mat& getColorReduced() const { return _colorReduced; }

        /** Get gray scale matrix. */
        const cv::Mat& getGrayScale() const { return _grayScale; }

        /** Get black and white matrix. */
        const cv::Mat& getBW() const { return _bw; }
  
        /** Get file data information (results of last filter). */
        const FileData& getFileData() const { return _fileData; }

        /** Dump information about results of last image processed. */
        std::ostream& print(std::ostream& out) const;

	/** Dump information about last image processes and total FPS.
	 *
	 * @param out Where to write the information.
	 * @param secs How many total seconds have elapsed.
	 */
	std::ostream& printFrameRate(std::ostream& out, float secs) const;

    private:
        void loadConfig();
        Found filterColorRange(const int* ranges, Found colorToFind);

	cv::Mat _cropped;
        cv::Mat _colorTransformed;
        cv::Mat _colorReduced;
        cv::Mat _grayScale;
        cv::Mat _bw;

	// Contours found (if any)
	std::vector<std::vector<cv::Point>> _contours;

        // Color reduction levels (min0, max0, min1, max1, min2, max2)
        int _yelRanges[6];
        int _redRanges[6];

        FileData _fileData;

        // Whether or not the red filter is enabled (normally is
        // unless debugging yellow)
        bool _redEnabled;
    };

    // Helper method to dump information about Filter to output stream
    inline std::ostream& operator <<(std::ostream& out, const Filter& f) {
        return f.print(out);
    }
}
