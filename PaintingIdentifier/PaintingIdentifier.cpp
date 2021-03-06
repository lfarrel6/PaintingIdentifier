// PaintingIdentifier.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "opencv2/imgproc.hpp"
#include "opencv2/imgcodecs.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/features2d.hpp"
#include <iostream>
#include <map>

using namespace cv;
using namespace std;

void formGroundTruths(Mat&, int);
void meanShiftSegmentation(Mat&, Mat&, Mat&);
void floodFillPostprocess(Mat&, Mat&, Mat&, const Scalar&);
Scalar randomColour(RNG&);
void rationalise(Mat&, vector<vector<Point>>&,int);
void initKB();
void allPerms(vector<Point>&, vector<RotatedRect>&);
bool entersRectangle(Rect&, LineIterator&);
Rect reduceRegion(Mat&, Mat&);
double getDiceCoefficient(vector<Point>&,vector<Point>&);

const map<String, vector<Point>> IMAGE1_GT = { {"Painting 2",{ Point(212,261), Point(445,225), Point(428,725), Point(198,673)   }},
											   {"Painting 1",{ Point(686,377), Point(1050,361), Point(1048,705), Point(686,652) }} 
	                                         };

const map<String, vector<Point>> IMAGE2_GT = { {"Painting 3",{ Point(252,279), Point(691,336), Point(695,662), Point(258,758)     }},
											   {"Painting 2",{ Point(897,173), Point(1063,234), Point(1079,672), Point(917,739)   }},
											   {"Painting 1",{ Point(1174,388), Point(1221,395), Point(1216,544), Point(1168,555) }}
	                                         };

const map<String, vector<Point>> IMAGE3_GT = { {"Painting 4",{ Point(68,329), Point(350,337), Point(351,545), Point(75,558)       }},
											   {"Painting 5",{ Point(629,346), Point(877,350), Point(873,517), Point(627,530)     }},
											   {"Painting 6",{ Point(1057,370), Point(1187,374), Point(1182,487), Point(1053,493) }}
	                                         };

const map<String, vector<Point>> IMAGE4_GT = { {"Painting 4",{ Point(176,348), Point(298,347), Point(307,481), Point(184,475)   }},
											   {"Painting 5",{ Point(469,343), Point(690,338), Point(692,495), Point(472,487)   }},
											   {"Painting 6",{ Point(924,349), Point(1161,344), Point(1156,495), Point(924,488) }}
	                                         };

const vector<map<String,vector<Point>>> GROUND_TRUTHS = { IMAGE1_GT, IMAGE2_GT, IMAGE3_GT, IMAGE4_GT };

Mat groundTruth, img, grey, preserve, result;
int spatialRad, colorRad, maxPyrLevel;
int currentImg, counter;
map<String,Mat> kb;

int main()
{

	//init meanshift params
	spatialRad  = 7;
	colorRad    = 20;
	maxPyrLevel = 1;
	counter = 0;
	initKB();

	for (currentImg = 0; currentImg < GROUND_TRUTHS.size(); currentImg++) {

		img = imread("../data/Gallery" + to_string(currentImg + 1) + ".jpg", IMREAD_COLOR);
		img.copyTo(groundTruth);
		img.copyTo(preserve);
		img.copyTo(result);

		formGroundTruths(groundTruth, currentImg);

		Mat hlsimg;
		cvtColor(img, hlsimg, CV_RGB2HLS);

		Mat meanshifted;
		Mat roi = Mat::zeros(img.rows, img.cols, CV_8UC1);

		meanShiftSegmentation(img, meanshifted, roi);

		cvtColor(img, grey, CV_RGB2GRAY);

		bitwise_not(roi, roi);
		imwrite("../data/WallMasks/" + to_string(currentImg) + ".jpg", roi);

		threshold(roi, roi, 0, 255, CV_THRESH_BINARY);

		int morph_elem = 1;
		int morph_size = 1;
		Mat element = getStructuringElement(morph_elem, Size(2 * morph_size + 1, 2 * morph_size + 1), Point(morph_size, morph_size));

		morphologyEx(roi, roi, MORPH_OPEN, element);

		Rect rectRoi = boundingRect(roi);
		Mat something(meanshifted, rectRoi);

		vector<vector<Point>> contours;
		findContours(roi, contours, CV_RETR_TREE, CHAIN_APPROX_NONE);

		int minimumArea = 20000;
		rationalise(roi,contours,minimumArea);
		RNG rng;
		
		for (int i = 0; i < contours.size(); i++) {
				Scalar randColour = randomColour(rng);

				Rect boundary = boundingRect(contours[i]);
				Mat untouchedRegion = preserve(boundary);

				RotatedRect rRect = minAreaRect(contours[i]);

				if (rRect.angle < -80 || rRect.angle > -10) {

					cout << boundary.br() << " " << boundary.tl() << endl;
					cout << img.rows << " " << img.cols << endl;

					Mat test = meanshifted(boundary);
					Mat regionToInvestigate = Mat::zeros(img.size(),CV_8UC1);
					
					drawContours(regionToInvestigate, contours, i, 255, CV_FILLED);

					vector<Point> corners;
					int maxCornerCount = 8;

					goodFeaturesToTrack(grey, corners, maxCornerCount, 0.05, grey.rows*0.1, regionToInvestigate);

					Mat canvas;
					img.copyTo(canvas);
					for (int c = 0; c < corners.size(); c++) {
						circle(canvas, corners[c], 5, CV_RGB(0, 0, 255), 3);
					}
					imwrite("../data/Good Features/PreProcess/Pre" + to_string(currentImg) + "-" + to_string(counter) + ".jpg", canvas);


					Mat img2;
					img.copyTo(img2);

					Rect paintingArea = boundingRect(corners);
					if (paintingArea.area() > 1000) {


						Mat subImage = img(paintingArea);
						Mat greyRegion, binaryRegion;
						cvtColor(test, greyRegion, CV_RGB2GRAY);

						adaptiveThreshold(greyRegion, binaryRegion, 255, ADAPTIVE_THRESH_GAUSSIAN_C, THRESH_BINARY_INV, 7, 2);
						morphologyEx(binaryRegion, binaryRegion, MORPH_OPEN, element);

						Rect frame = reduceRegion(test, binaryRegion);
						Mat frameArea = untouchedRegion(frame);

						double largestMatch = 0, largestHLSMatch = 0;
						Mat match;
						Point matchLocation;
						String matchName;
						Size newSize;
						//brute force template match
						for (map<String, Mat>::iterator it = kb.begin(); it != kb.end(); ++it) {
							Mat currentPainting = it->second;
							
							int r = currentPainting.rows;
							int c = currentPainting.cols;

							newSize = Size(max(r, c), max(r, c));

							Mat hls;
							cvtColor(frameArea, hls, CV_RGB2HLS);

							Mat resizedROI,resizedHLSROI;
							resize(frameArea, resizedROI, newSize);
							resize(hls, resizedHLSROI, newSize);

							Mat hlsKB;
							cvtColor(currentPainting, hlsKB, CV_RGB2HLS);

							Mat resized,resizedHLS;
							resize(currentPainting, resized, newSize);
							resize(hlsKB, resizedHLS, newSize);

							Mat res,resHLS;
							matchTemplate(resizedROI, resized, res, CV_TM_CCORR_NORMED);
							matchTemplate(resizedHLSROI,resizedHLS,resHLS,CV_TM_CCORR_NORMED);

							double max,maxHLS,matchVal;
							minMaxLoc(res, NULL, &max, NULL, &matchLocation);
							minMaxLoc(resHLS, NULL, &maxHLS, NULL);
							cout << "match val: " << max << " hls mactch: " << maxHLS << endl;
							matchVal = max + maxHLS;

							if (matchVal > largestMatch) {
								matchName = it->first;
								largestMatch = matchVal;
								match = currentPainting;
							}
						}
						float roundedMatch = roundf(largestMatch * 100) / 100;
						cout << "rounded " << roundedMatch << endl;

						double rScale = untouchedRegion.rows / newSize.height;
						double cScale = untouchedRegion.cols / newSize.width;

						matchLocation.x = round(matchLocation.x * cScale);
						matchLocation.y = round(matchLocation.y * rScale);

						Rect matchedArea(frame.tl()+boundary.tl(),frame.size());

						cout << "max match val: " << largestMatch << endl;
						rectangle(result, matchedArea, CV_RGB(255, 0, 0), 2);
						putText(result, matchName, matchedArea.tl()+Point(matchedArea.width/4,matchedArea.height/4),FONT_HERSHEY_PLAIN, 2, CV_RGB(155, 155, 155),3);

						
					}
					counter++;
				}
			}


		imwrite("../data/Result/" + to_string(currentImg) + "-output.jpg", result);
	}
    return 0;
}

double getDiceCoefficient(vector<Point>& detected, vector<Point>& trueRegion) {
	RotatedRect rDetected = minAreaRect(detected);
	RotatedRect tRect = minAreaRect(trueRegion);
	
	vector<Point> vertices;
	rotatedRectangleIntersection(rDetected, tRect, vertices);

	if (vertices.size() > 0) {
		Rect intersectionBox = boundingRect(vertices);
		Rect detectedBox = rDetected.boundingRect();
		Rect trueBox = tRect.boundingRect();

		return (2 * intersectionBox.area()) / (trueBox.area() + detectedBox.area());
	}
	return 0;
}

Rect reduceRegion(Mat& region, Mat& binaryRegion) {
	vector<Vec4i> lines;
	HoughLinesP(binaryRegion, lines, 1, CV_PI / 180, 40, 40, 30);

	Mat zeros = Mat::zeros(binaryRegion.size(),CV_8UC1);
	for (int i = 0; i < lines.size(); i++) {
		Point p1(lines[i][0], lines[i][1]);
		Point p2(lines[i][2], lines[i][3]);

		line(zeros, p1, p2, 255, 1);
	}
	imwrite("../data/Hough Lines/PreProcess/Pre" + to_string(currentImg) + "-"+to_string(counter)+".jpg", zeros);

	Size rSize = Size(binaryRegion.cols / 3, binaryRegion.rows / 3);
	//define a rectangle in the centre of the region to prevent noise on the image
	Rect noGoZone = Rect(Point((binaryRegion.cols / 2) - rSize.width / 2, (binaryRegion.rows / 2) - rSize.height / 2), rSize);

	Mat blank = Mat::zeros(region.size(), CV_8UC1);
	for (vector<Vec4i>::iterator hl = lines.begin(); hl != lines.end();) {
		Vec4i thisLine = *hl;
		Point p1 = Point(thisLine[0], thisLine[1]);
		Point p2 = Point(thisLine[2], thisLine[3]);

		LineIterator lineIt(binaryRegion, p1, p2, 8, false);
		if (!entersRectangle(noGoZone, lineIt)) {

			double rise = p2.y - p1.y;
			double run = p2.x - p1.x;
			double slope;
			bool vertical = false;
			if (run == 0) {
				vertical = true;
			}
			else {
				slope = rise / run;
			}

			if (vertical || (slope >= -0.2 && slope <= 0.2) || (slope < -25 || slope > 25)) {
				++hl;
			}
			else {
				hl = lines.erase(hl);
			}
		}
		else {
			cout << "line entered no go" << endl;
			hl = lines.erase(hl);
		}
	}
	zeros = Mat::zeros(binaryRegion.size(), CV_8UC1);
	for (vector<Vec4i>::iterator it = lines.begin(); it != lines.end(); ++it) {
		Vec4i thisvec = *it;
		Point p1(thisvec[0], thisvec[1]);
		Point p2(thisvec[2], thisvec[3]);

		line(blank, p1, p2, 255, 8);
		line(zeros, p1, p2, 255, 1);
	}
	imwrite("../data/HL Binary/BinaryRegion" + to_string(currentImg) + "-" + to_string(counter) + ".jpg", blank);
	imwrite("../data/Hough Lines/PostProcess/Post" + to_string(currentImg) + "-"+to_string(counter)+".jpg", zeros);

	Mat greyRegion;

	vector<Point> corners;
	int maxCornerCount = 5;
	cvtColor(region, greyRegion, CV_RGB2GRAY);

	Mat canvas;
	region.copyTo(canvas);

	goodFeaturesToTrack(greyRegion, corners, maxCornerCount, 0.05, greyRegion.rows*0.2, blank);
	for (int i = 0; i < corners.size(); i++) {
		circle(canvas, corners[i], 5, CV_RGB(0, 0, 255), 3);
	}
	imwrite("../data/Good Features/PostProcess/Post" + to_string(currentImg) + "-"+to_string(counter)+".jpg", canvas);

	return boundingRect(corners);
}

void initKB() {
	for (int i = 0; i < 6; i++) {
		String paintingName = "Painting" + to_string(i + 1);
		kb.insert({ paintingName, imread("../data/KB/" + paintingName + ".jpg") });
	}
}

void rationalise(Mat& binary, vector<vector<Point>>& contours, int minArea) {
	for (vector<vector<Point>>::iterator it = contours.begin(); it != contours.end();) {
		Rect boundary = boundingRect(*it);
		if (boundary.area() < minArea) {
			it = contours.erase(it);
		}
		else if (boundary.height<=100 || boundary.width<=100) {
			it = contours.erase(it);
		}
		else {
			cout << boundary.area() << endl;
			cout << "height: " << boundary.height << " , width: " << boundary.width << endl;

			Mat subImg = Mat(binary, boundary);
			vector<Point> nonZeros;
			findNonZero(subImg, nonZeros);

			double subPixels = subImg.rows*subImg.cols;

			if (nonZeros.size() < subPixels * 0.75) {
				double perc = nonZeros.size() / subPixels;
				cout << "rejected " << perc << endl;
				it = contours.erase(it);
			}
			else {
				it++;
			}
		}
	}
}

bool entersRectangle(Rect& r, LineIterator& li) {
	for (int i = 0; i < li.count; i++, ++li) {
		Point x = li.pos();
		//cout << "point: " << x << endl;
		if (r.contains(x)) {
			return true;
		}
	} return false;
}

void formGroundTruths(Mat& target, int ind) {
	
	map<String, vector<Point>> gts = GROUND_TRUTHS.at(ind);
	for (map<String, vector<Point>>::iterator it = gts.begin(); it != gts.end(); ++it) {
		vector<vector<Point>> contour = { it->second };
		drawContours(target, contour, -1, CV_RGB(255, 0, 0), 2);
		Point ctr = (0, 0);
		for (vector<Point>::iterator ctrGet = it->second.begin(); ctrGet != it->second.end(); ++ctrGet) {
			ctr += *ctrGet;
		}
		putText(target, it->first, ctr / 4, FONT_HERSHEY_COMPLEX_SMALL, 0.8, CV_RGB(120, 255, 160));
		imwrite("../data/GroundTruths/" + to_string(currentImg) + ".jpg", target);
	}
	
}

void meanShiftSegmentation(Mat& target, Mat& output, Mat& roi)
{
	Mat meanshift;
	pyrMeanShiftFiltering(target, meanshift, spatialRad, colorRad, maxPyrLevel);
	meanshift.copyTo(output);

	Scalar colorDiff = Scalar::all(2);

	floodFillPostprocess(meanshift, output, roi, colorDiff);
}

//wrapper for floodFill, returns bounding rectangles for segments within range in segments vector
void floodFillPostprocess(Mat& input, Mat& output, Mat& roi, const Scalar& colorDiff = Scalar::all(2))
{
	CV_Assert(!input.empty());

	Mat mask(input.rows + 2, input.cols + 2, CV_8UC1, Scalar::all(0));

	Rect segment(0, 0, 0, 0);

	bool first_iter = true;
	for (int y = 0; y < input.rows; y++)
	{
		for (int x = 0; x < input.cols; x++)
		{
			if (x != 0 && y != 0) {
				first_iter = false;
			}
			if (mask.at<uchar>(y + 1, x + 1) == 0)
			{
				//get rgb values for this point
				Point3_<uchar>* p = input.ptr<Point3_<uchar> >(y, x);
				Scalar newVal(p->x, p->y, p->z);

				//segment boundary passed to floodfill, returns boundingRect of segment
				Rect segmentBoundary;
				int connectivity = 8;
				//8 | (255 << 8) | FLOODFILL_MASK_ONLY
				//8 connectivity, fill mask with 255, dont change image
				floodFill(output, mask, Point(x, y), newVal, &segmentBoundary, colorDiff, colorDiff, 8 | (255 << 8) | FLOODFILL_MASK_ONLY);

				if (segmentBoundary.area() > segment.area()) {
					roi = Mat::zeros(roi.size(), CV_8UC1);
					cout << "new roi" << endl;
					segment = segmentBoundary;
					
					vector<Point> pointsOfInterest;
					findNonZero(mask, pointsOfInterest);
					Rect roirect(Point(0, 0), Point(roi.cols, roi.rows));
					for (int i = 0; i < pointsOfInterest.size(); i++) {
						Point w = pointsOfInterest[i];
						Point wPrime(w.x - 1, w.y - 1);
					
						if (wPrime.inside(roirect)) {
							roi.at<uchar>(wPrime) = 255;
						}
					}
					
				}
			}

		}
	}
	cout << segment.area() << "||" << output.rows*output.cols << endl;
}

Scalar randomColour(RNG& rng)
{
	int icolor = (unsigned)rng;
	return Scalar(icolor & 255, (icolor >> 8) & 255, (icolor >> 16) & 255);
}