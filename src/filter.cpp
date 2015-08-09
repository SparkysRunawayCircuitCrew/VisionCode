#include "filter.hpp"
#include "Timer.h"

#include <iostream>
#include <iomanip>

#include <fstream>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define ENABLE_MAIN 1

using namespace cv;
using namespace vision;
using namespace std;
// ---------------------------------------------------------------------
// ---------------------------------------------------------------------

Filter::Filter() :
    _colorTransformed(),
    _colorReduced(),
    _grayScale(),
    _bw(),
    _dilated(),
    _dilationElem(),
    _eroded(),
    _erosionElem(),
    _polyEpsilon(8),
    _redEnabled(true),
    _yellowEnabled(true)
{
    memset(&_fileData, 0, sizeof(_fileData));
    loadConfig();

    // Initialize a erosion block for eroding black and with image
    int eDim = 5;
    Point ePoint(eDim / 2, eDim / 2);
    Size eSize(eDim, eDim);
    _erosionElem = getStructuringElement(MORPH_RECT, eSize, ePoint);

    // Initialize a slightly larger to "glue" holes created in object back
    // together
    int dDim = eDim + 2;
    Point dPoint(dDim / 2, dDim / 2);
    Size dSize(dDim, dDim);
    _dilationElem = getStructuringElement(MORPH_RECT, dSize, dPoint);
}

// ---------------------------------------------------------------------
// ---------------------------------------------------------------------

Filter::~Filter() {
}

// ---------------------------------------------------------------------
// ---------------------------------------------------------------------

namespace {
    istream& readColorRanges(istream& in, int* ranges) {
        for (int i = 0; i < 6; i++) {
            in >> ranges[i];
        }
        return in;
    }
}

// ---------------------------------------------------------------------
// ---------------------------------------------------------------------

void Filter::loadConfig() {
    std::ifstream values("/etc/avc.conf.d/values.txt");
    readColorRanges(values, _redRanges);
    readColorRanges(values, _yelRanges);
    values.close();
}

// ---------------------------------------------------------------------
// ---------------------------------------------------------------------

Found Filter::filter(const Mat& src) {
    _fileData.frameCount++;
    _fileData.found = Found::None;
    _fileData.boxWidth = _fileData.boxHeight = 0;
    _fileData.xMid = _fileData.yBot = 0;

    // Crop the image (need to adjust this if we move/tilt camera)
    _cropped = src(cv::Rect(50, 10, src.cols - 50, src.rows - 50));

    // This could be a command line option
    bool enableBlur = false;

    if (enableBlur) {
	// Apply blur to smear colors together better (this adds a
	// HUGE! impact to FPS)
	blur(_cropped, _blurred, Size(3, 3));

	// Convert to HSV color space
	cvtColor(_blurred, _colorTransformed, cv::COLOR_BGR2HSV);
    } else {
	// Convert to HSV color space
	cvtColor(_cropped, _colorTransformed, cv::COLOR_BGR2HSV);
    }

    // Try looking for yellow stanchion first
    Found found = _yellowEnabled ? filterColorRange(_yelRanges, Found::Yellow) : Found::None;

    if ((found == Found::None) && _redEnabled) {
        // If yellow not found, then try red
        found = filterColorRange(_redRanges, Found::Red);
    }

    // Transfer final values and set safety frame count to match to signal done
    _fileData.found = found;
    _fileData.safetyFrameCount = _fileData.frameCount;

    return found;
}

// ---------------------------------------------------------------------
// ---------------------------------------------------------------------

void Filter::writeImages(const string& baseName, const Mat& orig, bool writeOrig) const {
    Mat contoursImg, possibleImg, polygonImg, foundImg;
    _cropped.copyTo(contoursImg);
    _cropped.copyTo(possibleImg);
    _cropped.copyTo(polygonImg);
    _cropped.copyTo(foundImg);

    Scalar goodColor(255, 255, 0);
    Scalar badColor(100, 200, 255);
    Scalar labelColor(255, 128, 200);

    int n = _contours.size();
    for (int i = 0; i < n; i++) {
	vector<Point> polygon;
	Rect br;
        approxPolyDP(Mat(_contours[i]), polygon, _polyEpsilon, true);
	const Scalar* shapeColor = &badColor;

	if (isPossibleStanchion(contoursImg, polygon, br)) {
	    shapeColor = &goodColor;
	    drawContours(possibleImg, _contours, i, *shapeColor, 1);
	}

	drawContours(contoursImg, _contours, i, *shapeColor, 1);

	const cv::Point* pts = (const cv::Point*) Mat(polygon).data;
	int npts = Mat(polygon).rows;
	polylines(polygonImg, &pts, &npts, 1, true, *shapeColor, 3);

	char buf[100];
	snprintf(buf, sizeof(buf), "%d", polygon.size());
	Point textPt(br.x + br.width / 2 - 8, br.y + (br.height / 2) - 5);
	putText(polygonImg, buf, textPt, FONT_HERSHEY_PLAIN,
		0.75, labelColor);
    }

    string foundExt;
    bool found = false;

    if (_fileData.found == Found::Red) {
	found = true;
	foundExt = "-red.png";
    } else if (_fileData.found == Found::Yellow) {
	found = true;
	foundExt = "-yellow.png";
    }

    if (found) {
	int x = _fileData.getX();
	int y = _fileData.getY();
	int w = _fileData.getWidth();
	int h = _fileData.getHeight();
	int cx = _fileData.xMid;
	int cy = y + h / 2;
	int rx = x + w;
	int by = _fileData.yBot;
	int iw = foundImg.cols;
	int ih = foundImg.rows;

	Rect br(x, y, w, h);
	rectangle(foundImg, br, goodColor, 3);
	
	int fontSpacing = 15;
	int topText = y - 4;
	int botText = by + fontSpacing;
	int topGap = topText - fontSpacing;
	int botGap = ih - botText;

	int textY = (botGap > topGap) ? botText : topText;
	textY = min(max(fontSpacing, textY), ih - fontSpacing);

	char buf[1024];
	snprintf(buf, sizeof(buf), "%s sz(%dx%d) tl(%d,%d), cp(%d,%d), br(%d,%d)",
		 (_fileData.found == Found::Yellow ? "Yel" : "Red"),
		 w, h, x, y, cx, cy, rx, by);

	putText(foundImg, buf, Point(4, textY), FONT_HERSHEY_PLAIN,
		0.75, labelColor);
    }

    // NOTE: Keep two arrays in sync and ordered by steps done
    const string names[] = {
	(writeOrig ? "-orig.png" : ""),
	"-cropped.png",
	"-blurred.png",
	"-hsv.png",
	"-hsv-reduced.png",
	"-bw.png",
	"-eroded.png",
	"-dilated.png",
	"-contours.png",
	"-contours-possible.png",
	"-polygons.png",
	foundExt
    };
    const Mat* images[] = {
	&orig,
	&_cropped,
	&_blurred,
	&_colorTransformed,
	&_colorReduced,
	&_bw,
	&_eroded,
	&_dilated,
	&contoursImg,
	&possibleImg,
	&polygonImg,
	&foundImg
    };

    n = sizeof(names) / sizeof(names[0]);

    for (int i = 0; i < n; i++) {
	if ((names[i].size() != 0) && (images[i] != 0) && (images[i]->rows > 0)) {
	    ostringstream fn;
	    fn << baseName << "-step" << setw(2) << setfill('0') << i << names[i];
	    imwrite(fn.str(), *images[i]);
	}
    }

}

// ---------------------------------------------------------------------
// ---------------------------------------------------------------------

bool Filter::isPossibleStanchion(const cv::Mat& img,
				 const std::vector<cv::Point>& polygon,
				 cv::Rect& br) {
    br = cv::boundingRect(polygon);
    int w = br.width;
    int h = br.height;
    int hw = ((h * 100) / w);
    int pts = polygon.size();
    // Distance of top of bounding rectangle from top of screen must
    // be more than distance of bottom of bounding rectange from middle
    // (or bottom must be below middle)
    int imgMid = img.rows / 2;
    int distFromTop = br.y; // smallest can be is 0
    int distFromMid = (img.rows / 2) - (br.y + h);  // goes negative if below mid

    // Also, top can not be below middle of image

	// Let's check to make sure the stanchion is in the center
	int distFromCenter = abs((img.cols / 2) - (br.x + w));

    // We don't like short fat stanchions, but allow them to be fairly
    // skinny (for the case when it is just showin up on the edge)
    bool couldBeStanchion = (w > 15) && (h > 40) && (hw > 50) && (hw < 800)
	&& (pts >= 4) && (pts < 20) 
	&& (distFromTop > distFromMid) && (distFromTop < imgMid)
	&& (distFromCenter < 50);

    // Set to true to get temporary diagnostic output
    if (false) {
	cerr << "isPossibleStanchion() results:\n"
	     << "found: " << couldBeStanchion
	     << "  w: " << w
	     << "  h: " << h
	     << "  hw: " << hw
	     << "  pts: " << pts
	     << "  distFromTop: " << distFromTop
	     << "  distFromMid: " << distFromMid
	     << "\n";
    }

    return couldBeStanchion;
}

// ---------------------------------------------------------------------
// ---------------------------------------------------------------------

Found Filter::filterColorRange(const int* ranges, Found colorToFind) {
    Scalar lower(ranges[0], ranges[2], ranges[4]);
    Scalar upper(ranges[1], ranges[3], ranges[5]);

    // Filter each color channel for specified ranges
    inRange(_colorTransformed, lower, upper, _colorReduced);

    // NASTY HACK! Red Hue values wrap around 255 (we want Hue values from 0-10
    // and from something like 160-255).
    if (colorToFind == Found::Red) {
	Scalar lower(0, ranges[2], ranges[4]);
	Scalar upper(10, ranges[3], ranges[5]);
	inRange(_colorTransformed, lower, upper, _redLower);
	bitwise_or(_redLower, _colorReduced, _colorReduced);
    }

    // Take it down to black and white
    threshold(_colorReduced, _bw, 32, 255, THRESH_BINARY);

    // Erode the image to clean up little bits of noise
    erode(_bw, _eroded, _erosionElem);

    // Dilate the image to try and fuse small holes
    dilate(_eroded, _dilated, _dilationElem);

    // Now go look for stanchion in black and white image
    vector<Vec4i> hierarchy;

    cv::Mat bwClone = _dilated.clone();
    findContours(bwClone, _contours, hierarchy, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE);

    int n = _contours.size();
    int maxH = 0;

    for (int i = 0; i < n; i++) {
        vector<Point> polygon;
        Rect br;

        approxPolyDP(Mat(_contours[i]), polygon, _polyEpsilon, true);

        if (isPossibleStanchion(_cropped, polygon, br) && (br.height > maxH)) {
	    int h = br.height;
	    int w = br.width;
            maxH = h;
            _fileData.boxWidth = w;
            _fileData.boxHeight = h;
            _fileData.xMid = br.x + (w / 2);
            _fileData.yBot = br.y + h;
            _fileData.found = colorToFind;
        }
    }

    return _fileData.found;
}

// ---------------------------------------------------------------------
// ---------------------------------------------------------------------

ostream& Filter::print(ostream& out) const {

    out << "Filter frames processed: "
      << _fileData.frameCount
      << "  Found: ";
    if (_fileData.found == Found::Red) {
        out << " Red Stanchion\n";
    } else if (_fileData.found == Found::Yellow) {
        out << " Yellow Stanchion\n";
    } else {
        out << " Nothing";
        return out;
    }

    out << "  Width: " << _fileData.boxWidth
        << "  Height: " << _fileData.boxHeight
        << "  X-Mid: " << _fileData.xMid
        << "  Y-Bot: " << _fileData.yBot;

    return out;
}

// ---------------------------------------------------------------------
// ---------------------------------------------------------------------

ostream& Filter::printFrameRate(ostream& out, float secs) const {
    float frames = getFileData().frameCount;
    float fps = (secs > 0) ? (frames / secs) : 0;

    out << "Frame: " << frames << "  total time: "
	<< secs << " secs (" << fps << " FPS), results:\n"
	<< *this << "\n";

    return out;
}

// ---------------------------------------------------------------------
// ---------------------------------------------------------------------

namespace {
    bool isInterrupted = false;

    void interrupted(int sig) {
        isInterrupted = true; 
    }

    /**
     * Helper class to deal with command line argument options.
     */

    class Options {
    public:
	Options(int argc, char** argv) : 
	    ok(true),
	    verboseOut(false),
	    enableRed(true),
	    enableYellow(true),
	    changeDirEnabled(false),
	    readFromFile(false),
	    inputFile(""),
	    outputDir("/dev/shm"),
	    stanchionsFile("/dev/shm/stanchions"),
	    changeDir(""),
	    periodicWrite(0)
	{

	    int opt;
	    while ((opt = getopt(argc, argv, "c:f:ho:p:rvy")) != -1) {
		switch (opt) {

		case 'c':
		    changeDir = optarg;
		    changeDirEnabled = true;
		    break;

		case 'f':
		    readFromFile = true;
		    inputFile = optarg;
		    break;

		case 'o':
		    outputDir = optarg;
		    break;

		case 'p':
		    periodicWrite = atoi(optarg);
		    if (periodicWrite < 1) {
			cerr << "Periodic count must be more than 0";
			ok = false;
		    }
		    break;

		case 'r':
		    enableRed = true;
		    enableYellow = false;
		    break;

		case 'v':
		    verboseOut = true;
		    break;

		case 'y':
		    enableRed = false;
		    enableYellow = true;
		    break;

		case 'h':
		default:
		    ok = false;
		    cerr << "\n"
"Usage:\n"
"\n"
"  avc-vision [-h] [-v] [-r|-y] [-f FILE_TO_PROCESS] [-o OUTPUT_DIR]\n"
"             [-c CHANGE_DIR]\n"
"\n"
"Where:\n"
"\n"
"  -h\n"
"    Displays this help information\n"
"\n"
"  -r\n"
"    Only enable search for red stanchion (disable yellow)\n"
"\n"
"  -y\n"
"    Only enable search for yellow stanchion (disable red)\n"
"\n"
"  -v\n"
"    Display more verbose output to the console\n"
"\n"
"  -f FILE_TO_PROCESS\n"
"    Reads image from FILE_TO_PROCESS, processes image, writes out\n"
"    multiple image files for each step of processing and displays\n"
"    summary results to the console.\n"
"\n"
"  -o OUTPUT_DIR\n"
"    This option indicates that the program should dump information\n"
"    about the last image processed prior to program termination (only\n"
"    used in streaming mode).\n"
"\n"
"  -p PERIODIC_CNT\n"
"    If you want to force an image to be logged, you can use this option.\n"
"    For example, \"-p 100\" would force every 100th image to be saved to\n"
"    the CHANGE_DIR regardless if there was a change in object detection or\n"
"    not. NOTE: You MUST also specify the -c CHANGE_DIR option to enable!\n"
"\n"
"  -c CHANGE_DIR\n"
"    This option enables the video streaming mode to save a copy of\n"
"    the original image just processed each time it finds something\n"
"    different. Files names will be: avc-vision-FRAME.png. NOTE: If\n"
"    you include the -v option, then ALL frames are written.\n"
"\n";
		}
	    }
	}

	bool verbose() const { return verboseOut; }
	bool isOk() const { return ok; }
	bool isFileMode() const { return readFromFile; }
	const string& getImageFile() const { return inputFile; }

	const string& getOutputDir() const { return outputDir; }

	void writeToChangeDir(const Mat& img, int frame) const {
	    if (changeDirEnabled) {
		ostringstream buf;
		buf << changeDir << "/avc-vision-" << setw(6)
		    << setfill('0') << frame << ".png";
		imwrite(buf.str(), img);
	    }
	}

	void writePeriodic(const Mat& img, int frame) const {
	    if (changeDirEnabled &&
		(periodicWrite > 0) && ((frame % periodicWrite) == 0)) {
		ostringstream buf;
		buf << changeDir << "/avc-vision-" << setw(6)
		    << setfill('0') << frame << ".png";
		imwrite(buf.str(), img);
	    }
	}

	/** Name of stanchions output file (typically: "/dev/shm/stanchions"). */
	const string& getStanchionsFile() const { return stanchionsFile; }

	const bool isRedEnabled() const { return enableRed; }
	const bool isYellowEnabled() const { return enableYellow; }

    private:
	bool ok;

	bool verboseOut;

	bool enableRed;
	bool enableYellow;

	bool changeDirEnabled;

	// Used to indicate that we should process single image from file
	// (-f FILE)
	bool readFromFile;
	string inputFile;

	// Output directory
	string outputDir;

	// Output for copies of frame images when -c CHANGE_DIR specified
	string changeDir;

	// How often to write out a image file to the CHANGE_DIR regardless
	// of whether there was an actual change.
	int periodicWrite;

	// Stanchions file
	string stanchionsFile;
    };
}

// ---------------------------------------------------------------------
// ---------------------------------------------------------------------

#if ENABLE_MAIN

int main(int argc, char* argv[]) {
    signal(SIGINT, interrupted);
    signal(SIGTERM, interrupted);

    // Evalutate/check command line arguments
    Options opts(argc, argv);
    if (opts.isOk() != true) {
	return 1;
    }

    Filter filter;
    filter.setRedEnabled(opts.isRedEnabled());
    filter.setYellowEnabled(opts.isYellowEnabled());

    // If processing a single file (-f FILE)
    if (opts.isFileMode()) {
        Mat orig = imread(opts.getImageFile());

        // Create base name for output files
        string baseName(opts.getImageFile());
        int pos = baseName.rfind('.');
        if (pos != string::npos) {
            baseName.erase(pos);
        }

        avc::Timer timer;
        Found found = filter.filter(orig);

	filter.printFrameRate(cout, timer.secsElapsed());

        // Write out individual image files
        filter.writeImages(baseName, orig, false);

        // Return 0 to parent process if we found image (for scripting)
        return (found == Found::None ? 1 : 0);
    }

    // Video processing
    VideoCapture videoFeed(0);
    {
	int attempts = 0;
	while (!videoFeed.isOpened()) {
	    float waitSecs = 3;
	    attempts++;
	    cerr << "Failed to open camera on attempt " << attempts
		 << ", trying again in "
		 << waitSecs << " seconds.\n";
	    avc::Timer::sleep(waitSecs);
	    if (isInterrupted) {
			return 1;
	    }
	    videoFeed.release();
	    videoFeed.open(0);
	}
    }

    videoFeed.set(CV_CAP_PROP_FRAME_WIDTH, 320);
    videoFeed.set(CV_CAP_PROP_FRAME_HEIGHT, 240);

    Mat origFrame;

    // Where to write out information about what we see
    ofstream stanchionsFile(opts.getStanchionsFile());

    // Get initial frame and toss (incase first one is bad)
    videoFeed >> origFrame;

    avc::Timer timer;

    int foundLast = -1;

    while (!isInterrupted) {
        videoFeed >> origFrame;

        int found = filter.filter(origFrame);
	if ((found != foundLast) || opts.verbose()) {
	    opts.writeToChangeDir(origFrame, filter.getFileData().frameCount);
	    filter.printFrameRate(cout, timer.secsElapsed());
	    foundLast = found;
	} else {
	    // No change in detection state, however, go write out image
	    // if user enabled the periodic feature (-p PERIODIC) and we've
	    // reached the periodic count
	    opts.writePeriodic(origFrame, filter.getFileData().frameCount);
	}

	stanchionsFile.seekp(0);
	stanchionsFile.write((const char*) &filter.getFileData(), sizeof(FileData));
	stanchionsFile.flush();
    }

    if (filter.getFileData().frameCount > 0) {
	filter.printFrameRate(cout, timer.secsElapsed());

	filter.writeImages(opts.getOutputDir() + "/avc-vision", origFrame, true);
    } else {
	cout << "***ERROR*** Failed to read/process any video frames from camera\n";
    }

    return 0;
}

#endif
