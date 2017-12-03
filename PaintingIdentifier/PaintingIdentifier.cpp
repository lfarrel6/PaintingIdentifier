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
void meanShiftSegmentation(Mat&, Mat&, Rect&, Mat&);
void floodFillPostprocess(Mat&, Rect&, Mat&, const Scalar&);
bool isSubset(Rect&, Rect&);
bool hasOverlap(Rect&, Rect&);
Rect mergeRects(Rect&, Rect&);
void compress(vector<Rect>&, int, vector<Rect>&);

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

Mat groundTruth, img;
int spatialRad, colorRad, maxPyrLevel;


int main()
{

	//init meanshift params
	spatialRad  = 7;
	colorRad    = 20;
	maxPyrLevel = 1;

	namedWindow("meanshift", CV_WINDOW_AUTOSIZE);
	for (int i = 0; i < GROUND_TRUTHS.size(); i++) {
		
		img = imread("../data/Gallery"+to_string(i+1)+".jpg", IMREAD_COLOR);
		img.copyTo(groundTruth);
		formGroundTruths(groundTruth, i);

		Mat meanshifted;
		Rect largestSeg(0, 0, 0, 0);
		Mat test;

		meanShiftSegmentation(img, meanshifted, largestSeg, test);
		imshow("meanshift", meanshifted);
		waitKey(0);

		Mat gray(img.size(), CV_8UC1);
		Mat edgeImage(img.size(), CV_8UC1);
		cvtColor(img, gray, CV_BGR2GRAY);

		Canny(meanshifted, edgeImage, 50, 200, 3);
		imshow("edge image", edgeImage);
		Mat colourEdge(edgeImage.rows,edgeImage.cols,CV_8UC1,Scalar(0));
		//cvtColor(edgeImage, colourEdge, CV_GRAY2BGR);

		vector<Vec4i> lines;
		HoughLinesP(edgeImage, lines, 1, CV_PI / 180, 50, 50, 10);
		for (size_t i = 0; i < lines.size(); i++)
		{
			Vec4i l = lines[i];
			line(colourEdge, Point(l[0], l[1]), Point(l[2], l[3]), Scalar(255, 255, 255), 3, CV_AA);
		}
		imshow("detected lines", colourEdge);
		waitKey(0);
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

void meanShiftSegmentation(Mat& target, Mat& output, Rect& segment, Mat& roi)
{
	pyrMeanShiftFiltering(target, output, spatialRad, colorRad, maxPyrLevel);

	floodFillPostprocess(output, segment, roi, Scalar::all(2));
}

//wrapper for floodFill, returns bounding rectangles for segments within range in segments vector
void floodFillPostprocess(Mat& target, Rect& segment, Mat& roi, const Scalar& colorDiff = Scalar::all(2))
{
	CV_Assert(!target.empty());
	Mat mask(target.rows + 2, target.cols + 2, CV_8UC1, Scalar::all(0));

	mask.copyTo(roi);

	vector<Rect> nestedRegions;
	bool first_iter = true;
	for (int y = 0; y < target.rows; y++)
	{
		for (int x = 0; x < target.cols; x++)
		{
			if (x != 0 && y != 0) {
				first_iter = false;
			}
			if (mask.at<uchar>(y + 1, x + 1) == 0)
			{
				//get rgb values for this point
				Point3_<uchar>* p = target.ptr<Point3_<uchar> >(y, x);
				Scalar newVal(p->x, p->y, p->z);

				//segment boundary passed to floodfill, returns boundingRect of segment
				Rect segmentBoundary;
				int connectivity = 8;
				floodFill(target, mask, Point(x, y), newVal, &segmentBoundary, colorDiff, colorDiff, connectivity);

				if (!first_iter) {
					if (segmentBoundary.area() > 500) {

						nestedRegions.push_back(segmentBoundary);
						if (isSubset(segmentBoundary, segment)) {
							cout << "nested1" << endl;
						}
						else if (isSubset(segment, segmentBoundary)) {
							cout << "nested2" << endl;
						}
						/*else {
							imshow("all-regions", Mat(target, segmentBoundary));
							cout << "showing @ " << x << ", " << y << endl;
						}*/

						//waitKey(0);
					}

					if (segmentBoundary.area() > segment.area()) {
						segment = segmentBoundary;
					}
				}
			}

		}
	}
	/*cout << "size: " << nestedRegions.size() << endl;
	for (int i = 0; i < nestedRegions.size(); i++) {
		cout << "compression starting" << endl;
		compress(nestedRegions, i, nestedRegions);
		cout << "compression " << i << " complete" << endl;
	}
	cout << "size after: " << nestedRegions.size() << endl;
	for (int i = 0; i < nestedRegions.size(); i++) {
		Mat mrg = Mat(img, nestedRegions.at(i));
		imshow("merging output", mrg);
		waitKey(0);
	}*/
	roi = Mat(target, segment);
}

//true if r1 is subset of r2
bool isSubset(Rect& r1, Rect& r2) {
	return (r1 | r2).area() == r2.area();
}

bool hasOverlap(Rect& r1, Rect& r2) {
	return (r1 & r2).area() > 0;
}

Rect mergeRects(Rect& r1, Rect& r2) {
	return (r1 | r2);
}

void compress(vector<Rect>& rList, int i, vector<Rect>& result) {
	Rect focal = rList.at(i);
	vector<Rect> compressed;
	cout << "compression @ " << i << endl;
	for (int j = 0; j < rList.size(); j++) {
		if (j != i) {
			cout << "j : " << j << endl;
			if (hasOverlap(focal, rList.at(j))) {
				focal = mergeRects(focal, rList.at(j));
			}
			else {
				/*imshow("no overlap", Mat(img, rList.at(j)));
				waitKey(0);*/
				compressed.push_back(rList.at(j));
			}
		}
	}
	compressed.push_back(focal);
	result = compressed;
}