// PaintingIdentifier.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "opencv2/imgproc.hpp"
#include "opencv2/imgcodecs.hpp"
#include "opencv2/highgui.hpp"
#include <iostream>
#include <map>

using namespace cv;
using namespace std;

void formGroundTruths(Mat&, int);
Mat meanShiftSegmentation(Mat&, Mat&, Rect&, Mat&);
void floodFillPostprocess(Mat&, Rect&, Mat&, const Scalar&);

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

int spatialRad, colorRad, maxPyrLevel;


int main()
{

	//init meanshift params
	spatialRad  = 7;
	colorRad    = 20;
	maxPyrLevel = 1;

	namedWindow("meanshift", CV_WINDOW_AUTOSIZE);
	Mat groundTruth;
	for (int i = 0; i < GROUND_TRUTHS.size(); i++) {
		
		groundTruth = imread("../data/Gallery"+to_string(i+1)+".jpg", IMREAD_COLOR);
		formGroundTruths(groundTruth, i);

		/*Mat meanshifted;
		Rect largestSeg(0, 0, 0, 0);
		Mat test;

		Mat x = meanShiftSegmentation(groundTruth, meanshifted, largestSeg, test);
		imshow("meanshift", meanshifted);
		waitKey(0);*/
	}

	waitKey();
    return 0;
}

void formGroundTruths(Mat& target, int ind) {
	namedWindow("gt", CV_WINDOW_AUTOSIZE);
	imshow("gt", target);
	waitKey(0);
	map<String, vector<Point>> gts = GROUND_TRUTHS.at(ind);
	for (map<String, vector<Point>>::iterator it = gts.begin(); it != gts.end(); ++it) {
		vector<vector<Point>> contour = { it->second };
		drawContours(target, contour, -1, CV_RGB(255, 0, 0), 2);
		Point ctr = (0, 0);
		for (vector<Point>::iterator ctrGet = it->second.begin(); ctrGet != it->second.end(); ++ctrGet) {
			ctr += *ctrGet;
		}
		putText(target, it->first, ctr / 4, FONT_HERSHEY_COMPLEX_SMALL, 0.8, CV_RGB(120, 255, 160));
	}
	imshow("gt", target);
	waitKey(0);
}

Mat meanShiftSegmentation(Mat& target, Mat& output, Rect& segment, Mat& roi)
{
	pyrMeanShiftFiltering(target, output, spatialRad, colorRad, maxPyrLevel);

	floodFillPostprocess(output, segment, roi, Scalar::all(2));
	cvtColor(roi, roi, CV_BGR2GRAY);


	Mat binarized;
	threshold(roi, binarized, 0, 255, THRESH_BINARY + THRESH_OTSU);

	vector<vector<Point>> c;
	findContours(binarized, c, CV_RETR_TREE, CHAIN_APPROX_NONE);
	Mat contours;
	target.copyTo(contours);
	drawContours(contours, c, -1, (0, 0, 255));

	imshow("cs", contours);

	imshow("grey", binarized);
	waitKey(0);

	Mat conComp;
	connectedComponents(binarized,conComp);

	return conComp;
}

//wrapper for floodFill, returns bounding rectangles for segments within range in segments vector
void floodFillPostprocess(Mat& target, Rect& segment, Mat& roi, const Scalar& colorDiff = Scalar::all(2))
{
	CV_Assert(!target.empty());
	Mat mask(target.rows + 2, target.cols + 2, CV_8UC1, Scalar::all(0));

	Scalar roiColor;
	mask.copyTo(roi);
	Point poi;

	for (int y = 0; y < target.rows; y++)
	{
		for (int x = 0; x < target.cols; x++)
		{
			if (mask.at<uchar>(y + 1, x + 1) == 0)
			{
				//get rgb values for this point
				Point3_<uchar>* p = target.ptr<Point3_<uchar> >(y, x);
				Scalar newVal(p->x, p->y, p->z);

				//segment boundary passed to floodfill, returns boundingRect of segment
				Rect segmentBoundary;
				int connectivity = 8;
				floodFill(target, mask, Point(x, y), newVal, &segmentBoundary, colorDiff, colorDiff, connectivity);

				if (segmentBoundary.area() > segment.area()) {
					segment = segmentBoundary;
					roiColor = newVal;
					poi = Point(x, y);
				}
			}

		}
	}
	roi = Mat(target, segment);
}
