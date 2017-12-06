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
void floodFillPostprocess(Mat&, Mat&, Rect&, Mat&, Mat&, Scalar&, const Scalar&);
bool isSubset(Rect&, Rect&);
bool hasOverlap(Rect&, Rect&);
Rect mergeRects(Rect&, Rect&);
void compress(vector<Rect>&, int, vector<Rect>&);
void findEucDistance(Vec3b&, float&);
void labelToRegion(Mat&, vector<vector<Point2i>>&);
Scalar randomColour(RNG&);
void processRegionHoles(Mat&, Mat&, Scalar&, Mat&);
void findAgreement(Mat&, Mat&, Mat&);

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
int currentImg;

int main()
{

	//init meanshift params
	spatialRad  = 7;
	colorRad    = 20;
	maxPyrLevel = 1;

	for (currentImg = 0; currentImg < GROUND_TRUTHS.size(); currentImg++) {
		
		img = imread("../data/Gallery"+to_string(currentImg+1)+".jpg", IMREAD_COLOR);
		img.copyTo(groundTruth);
		formGroundTruths(groundTruth, currentImg);

		Mat hlsimg;
		cvtColor(img, hlsimg, CV_RGB2HLS);

		Mat meanshifted;
		Rect largestSeg(0, 0, 0, 0);
		Mat test;

		meanShiftSegmentation(img, meanshifted, largestSeg, test);
		imshow("img", meanshifted);

		//Mat hlsMS;
		//Rect hlsSeg(0, 0, 0, 0);
		//Mat hlsROI;
		//meanShiftSegmentation(hlsimg, hlsMS, hlsSeg, hlsROI);
		//cout << "mean shifts finished" << endl;
		//Mat convertBack;
		//cvtColor(hlsMS, convertBack, CV_HLS2RGB);
		//cout << "hls to rgb complete" << endl;
		//Mat result;
		//findAgreement(meanshifted, convertBack, result);
		//cout << "agreement found" << endl;

		

		Mat gray(img.size(), CV_8UC1);
		Mat edgeImage(img.size(), CV_8UC1);
		cvtColor(img, gray, CV_BGR2GRAY);

		//Canny(meanshifted, edgeImage, 50, 200, 3);
		//imshow("edge image", edgeImage);
		Mat colourEdge(edgeImage.rows,edgeImage.cols,CV_8UC1,Scalar(0));
		//cvtColor(edgeImage, colourEdge, CV_GRAY2BGR);

		vector<Vec4i> lines;
		HoughLinesP(edgeImage, lines, 1, CV_PI / 180, 50, 50, 10);
		for (size_t i = 0; i < lines.size(); i++)
		{
			Vec4i l = lines[i];
			line(colourEdge, Point(l[0], l[1]), Point(l[2], l[3]), Scalar(255, 255, 255), 3, CV_AA);
		}
		//imshow("detected lines", colourEdge);
		//waitKey(0);
	}

	//waitKey();
    return 0;
}

void findAgreement(Mat& rgb, Mat& hls, Mat& res) {

	Mat maskRGB = Mat::zeros(rgb.size(), CV_8UC3);

	maskRGB.copyTo(res);

	Mat differenceImageRGB, differenceImageHLS;
	absdiff(rgb, maskRGB, differenceImageRGB);

	Mat maskHLS = Mat::zeros(hls.size(), CV_8UC3);
	absdiff(hls, maskHLS, differenceImageHLS);

	float distanceRGB, distanceHLS;

	int commonRegions = 0;

	for (int i = 0; i < rgb.cols && i < hls.cols; i++) {
		for (int j = 0; j < rgb.rows && j < hls.rows; j++) {

			Vec3b colourDiffRGB = differenceImageRGB.at<Vec3b>(j, i);
			Vec3b colourDiffHLS = differenceImageHLS.at<Vec3b>(j, i);
			findEucDistance(colourDiffRGB, distanceRGB);
			findEucDistance(colourDiffHLS, distanceHLS);

			if (distanceRGB > 0 && distanceHLS > 0) {
				res.at<Vec3b>(j, i) = rgb.at<Vec3b>(j, i);
			}

		}
	}
	imwrite("../data/ConnectedComponents/" + to_string(currentImg) + "-components.jpg", res);
}

void formGroundTruths(Mat& target, int ind) {
	//namedWindow("gt", CV_WINDOW_AUTOSIZE);
	//imshow("gt", target);
	//waitKey(0);
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
	//imshow("gt", target);
	//waitKey(0);
}

void meanShiftSegmentation(Mat& target, Mat& output, Rect& segment, Mat& roi)
{
	Mat meanshift;
	pyrMeanShiftFiltering(target, meanshift, spatialRad, colorRad, maxPyrLevel);
	meanshift.copyTo(output);

	Mat mask(target.rows + 2, target.cols + 2, CV_8UC1, Scalar::all(0));

	Scalar colorDiff = Scalar::all(2);
	Scalar wallColor;

	floodFillPostprocess(meanshift, output, segment, roi, mask, wallColor, colorDiff);

	Mat connectedComps;
	processRegionHoles(meanshift, roi, wallColor, connectedComps);

	output = connectedComps;
}

//wrapper for floodFill, returns bounding rectangles for segments within range in segments vector
void floodFillPostprocess(Mat& input, Mat& output, Rect& segment, Mat& roi, Mat& mask, Scalar& wallColor, const Scalar& colorDiff = Scalar::all(2))
{
	CV_Assert(!input.empty());
	

	mask.copyTo(roi);

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
				floodFill(output, mask, Point(x, y), newVal, &segmentBoundary, colorDiff, colorDiff, connectivity);

				if (segmentBoundary.area() > segment.area()) {
					segment = segmentBoundary;
					roi = output;
					wallColor = newVal;
				}
			}

		}
	}
	cout << segment.area() << "||" << output.rows*output.cols << endl;
}

void processRegionHoles(Mat& baseImg, Mat& changedImg, Scalar& colorOfInterest, Mat& connectedComps) {
	
	/*Mat result(changedImg.size(), CV_8UC1);
	findColour(changedImg, result, colorOfInterest);*/
	
	Mat differenceImage;
	cout << "process holes " << baseImg.channels() << " :: " << changedImg.channels() << endl;
	absdiff(baseImg, changedImg, differenceImage);

	Mat binaryDiff = Mat::zeros(baseImg.size(), CV_8UC1);
	float thresholdVal = 20;
	float distance;

	for (int i = 0; i < differenceImage.cols; i++) {
		for (int j = 0; j < differenceImage.rows; j++) {
			Vec3b colourDiff = differenceImage.at<Vec3b>(j, i);
			findEucDistance(colourDiff, distance);

			if (distance < thresholdVal) {
				binaryDiff.at<unsigned char>(j, i) = 255;
			}
		}
	}

	vector<vector<Point2i>> regions;

	threshold(binaryDiff, binaryDiff, 0, 255, THRESH_BINARY);

	cout << "thresholded" << endl;

	int morph_elem = 0;
	int morph_size = 1;
	Mat element = getStructuringElement(morph_elem, Size(2 * morph_size + 1, 2 * morph_size + 1), Point(morph_size, morph_size));
	morphologyEx(binaryDiff, binaryDiff, MORPH_OPEN, element);

	labelToRegion(binaryDiff, regions);

	connectedComps = Mat::zeros(binaryDiff.rows, binaryDiff.cols, CV_8UC3);

	RNG rng;
	for (int i = 0; i < regions.size(); i++) {
		Scalar col = randomColour(rng);
		for (int j = 0; j < regions[i].size(); j++) {
			int x = regions[i][j].x;
			int y = regions[i][j].y;

			Vec3b colourVec(col[0], col[1], col[2]);

			connectedComps.at<Vec3b>(y, x) = colourVec;

		}

	}
	cout << "regions labelled" << endl;
	//imshow("comps", connectedComps);
	//waitKey();
	//imwrite("../data/ConnectedComponents/" + to_string(currentImg) + "-components.jpg", connectedComps);
}

Scalar randomColour(RNG& rng)
{
	int icolor = (unsigned)rng;
	return Scalar(icolor & 255, (icolor >> 8) & 255, (icolor >> 16) & 255);
}

void labelToRegion(Mat& binImg, vector<vector<Point2i>>& labelledRegions) {
	labelledRegions.clear();

	//0 - background | 1 - unlabelled fg | 2+ - labelled

	int labelC = 2;

	Mat labelImg(binImg.size(), CV_8UC1);
	binImg.copyTo(labelImg);
	
	cout << labelImg.size() << endl;

	for (int j = 0; j < labelImg.rows; j++) {
		//int* row = (int*)labelImg.ptr(j);
		for (int i = 0; i < labelImg.cols; i++) {
			try {
				if (labelImg.at<uchar>(i,j) != 1) {
					//ignore bg & labelled regions
					continue;
				}
			}
			catch (...) {

			}

			Rect regionBoundary;
			Mat mask = Mat::zeros(labelImg.size(), CV_8UC1);
			floodFill(labelImg, Point(i, j), labelC, &regionBoundary, 0, 0, 4);

			vector<Point2i> thisComp;
			for (int y = regionBoundary.y; y < (regionBoundary.y + regionBoundary.height) && y < labelImg.rows; y++) {
				for (int x = regionBoundary.x; x < (regionBoundary.x + regionBoundary.width) && x < labelImg.cols; x++) {
					cout << x << " < " << labelImg.cols << endl;
					if (labelImg.at<unsigned char>(x,y) != labelC) {
						continue;
					}
					thisComp.push_back(Point2i(x, y));
				}
			}
			labelledRegions.push_back(thisComp);
			labelC++;
		}
	}

}

void findEucDistance(Vec3b& points, float& dist) {
	dist = (points[0] * points[0] + points[1] * points[1] + points[2] * points[2]);
	dist = sqrt(dist);
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