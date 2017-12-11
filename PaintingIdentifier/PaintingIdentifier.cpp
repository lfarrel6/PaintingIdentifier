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
bool isSubset(Rect&, Rect&);
bool hasOverlap(Rect&, Rect&);
Rect mergeRects(Rect&, Rect&);
void compress(vector<Rect>&, int, vector<Rect>&);
void findEucDistance(Vec3b&, float&);
void labelToRegion(Mat&, vector<vector<Point2i>>&);
Scalar randomColour(RNG&);
void processRegionHoles(Mat&, Mat&, Scalar&, Mat&);
void findAgreement(Mat&, Mat&, Mat&);
void colourRegions(vector<vector<Point2i>>&, Mat&);
void rationalise(Mat&, vector<vector<Point>>&);
void initKB();
void p2fToPoint(Point2f&,Point&);
void allPerms(vector<Point>&, vector<RotatedRect>&);


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

Mat groundTruth, img, grey, preserve;
int spatialRad, colorRad, maxPyrLevel;
int currentImg;
vector<Mat> kb;

int main()
{

	//init meanshift params
	spatialRad  = 7;
	colorRad    = 20;
	maxPyrLevel = 1;

	initKB();

	for (currentImg = 0; currentImg < GROUND_TRUTHS.size(); currentImg++) {

		img = imread("../data/Gallery" + to_string(currentImg + 1) + ".jpg", IMREAD_COLOR);
		img.copyTo(groundTruth);
		img.copyTo(preserve);

		cvtColor(img, grey, CV_RGB2GRAY);
		formGroundTruths(groundTruth, currentImg);

		Mat hlsimg;
		cvtColor(img, hlsimg, CV_RGB2HLS);

		Mat meanshifted;
		Mat roi = Mat::zeros(img.rows, img.cols, CV_8UC1);

		meanShiftSegmentation(img, meanshifted, roi);
		bitwise_not(roi, roi);

		threshold(roi, roi, 0, 255, CV_THRESH_BINARY);

		int morph_elem = 1;
		int morph_size = 1;
		Mat element = getStructuringElement(morph_elem, Size(2 * morph_size + 1, 2 * morph_size + 1), Point(morph_size, morph_size));

		morphologyEx(roi, roi, MORPH_OPEN, element);

		Rect rectRoi = boundingRect(roi);
		Mat something(img, rectRoi);

		vector<vector<Point>> contours;
		findContours(roi, contours, CV_RETR_TREE, CHAIN_APPROX_NONE);

		rationalise(roi,contours);

		RNG rng;
		for (int i = 0; i < contours.size(); i++) {
			Scalar randColour = randomColour(rng);
			vector<vector<Point>> render;
			vector<Point> contour = contours[i];

			render.push_back(contour);

			Rect boundary = boundingRect(contour);
			Mat untouchedRegion = preserve(boundary);

			drawContours(img, render, -1, randColour, CV_FILLED);

			RotatedRect rRect = minAreaRect(contour);

			if (rRect.angle < -80 || rRect.angle > -10) {

				cout << boundary.br() << " " << boundary.tl() << endl;
				cout << img.rows << " " << img.cols << endl;

				Mat test = meanshifted(boundary);

				//imwrite("../data/IdentifiedRegions/Image" + to_string(currentImg) + "-Region" + to_string(i) + ".jpg", test



				Mat toBin;
				cvtColor(test, toBin, CV_RGB2GRAY);

				adaptiveThreshold(toBin, toBin, 255, ADAPTIVE_THRESH_GAUSSIAN_C, THRESH_BINARY, 7, 2);
				morphologyEx(toBin, toBin, MORPH_OPEN, element);

				bitwise_not(toBin, toBin);

				vector<Point> corners;
				int maxCornerCount = 8;
				Mat maskRegion = Mat::zeros(toBin.size(), CV_8UC1);
				/*Laplacian(grey, grey, CV_8UC1);
				imshow("lap", grey);
				morphologyEx(grey, grey, MORPH_CLOSE, element);
				imshow("opened lap ;)", grey);
				waitKey();*/
				Mat greySub;
				cvtColor(meanshifted(boundary), greySub, CV_BGR2GRAY);

				imshow("greyd", greySub);

				/*cv::SiftFeatureDetector detector;
				vector<KeyPoint> keypoints;
				detector.detect(greySub, keypoints);
				*/
				goodFeaturesToTrack(greySub, corners, maxCornerCount, 0.05, greySub.rows*0.2);

				cout << "corners found " << corners.size() << endl;

				adaptiveThreshold(greySub, greySub, 255, ADAPTIVE_THRESH_GAUSSIAN_C, THRESH_BINARY, 7, 2);
				morphologyEx(greySub, greySub, MORPH_CLOSE, element);
				Mat canny;
				Canny(greySub, canny, 100, 300);
				imshow("canny", canny);
				waitKey();


				for (int c = 0; c < corners.size(); c++) {
					Point thisCorner = corners[c];
					circle(test, thisCorner, 2, CV_RGB(255, 255, 255), 2);
					imshow("good features", test);
					waitKey();
				}
				RotatedRect featuresArea = minAreaRect(corners);
				Point2f vertices[4];
				featuresArea.points(vertices);

				vector<Point> pVerts;
				for (int vp = 0; vp < 4; vp++) {
					Point p;
					p2fToPoint(vertices[vp], p);
					pVerts.push_back(p);
				}

				fillConvexPoly(maskRegion, pVerts, 255);
				
				imshow("mask", maskRegion);
				imshow("bin", toBin);
				Mat vis, dif, ided;

				ided = meanshifted(boundary);

				cvtColor(maskRegion, vis, CV_GRAY2BGR);
				subtract(vis, ided, dif);
				subtract(vis, dif, dif);

				//addWeighted(maskRegion, 0.3, greySub, 0.7, 0, vis);
				imshow("visualisation", dif);


				vector<Vec4i> lines;

				HoughLinesP(toBin, lines, 1, CV_PI / 180, 60, 150, 10);

				double horizAngle = atan(0) * 180 / CV_PI;

				Mat lineCanvas = Mat::zeros(toBin.size(), CV_8UC1);

				vector<pair<Point, Point>> reducedLines;
				cout << "pre reduction: " << lines.size() << endl;
				for (int i = 0; i < lines.size(); i++) {
					Vec4i points = lines[i];
					bool vertical = false;

					double rise = points[3] - points[1];
					double run = points[2] - points[0];
					double slope;
					if (run == 0) {
						slope = -0.5;
						vertical = true;
					}
					else {
						slope = abs(rise / run);
					}
					Point p1(points[0], points[1]);
					Point p2(points[2], points[3]);
					//if slope is within range of 0 +/- 0.1

					if ((slope >= -0.15 && slope <= 0.15) || slope > 25 || vertical) {

						line(lineCanvas, p1, p2, 255, 1);

						reducedLines.push_back(pair<Point, Point>(p1, p2));
					}


				}

				cout << "post reduction: " << reducedLines.size() << endl;

				double largestMatch = 0;
				int indexoi = 0;
				//brute force template match
				/*for (int kbInd = 0; kbInd < kb.size(); kbInd++) {
					int r = kb.at(kbInd).rows;
					int c = kb.at(kbInd).cols;

					Mat resized;

					Size newSize(max(r, c), max(r, c));
					resize(kb.at(kbInd), resized, newSize);
					Mat resizedROI;


					Mat res;
					matchTemplate(resizedROI, resized, res,CV_TM_CCORR_NORMED);

					double max;
					minMaxLoc(res, NULL, &max);
					if (max > largestMatch) {
						largestMatch = max;
						indexoi = kbInd;
					}

				}*/
			}
			else {
				cout << "rejected. angle: " << rRect.angle << endl;
				imshow("rejected region", img(boundary));
				waitKey();
			}
		}



		imwrite("../data/ConnectedComponents/" + to_string(currentImg) + "-components.jpg", img);
	}
	//waitKey();
    return 0;
}

void p2fToPoint(Point2f& vertice, Point& pVert) {
	pVert = Point(vertice.x, vertice.y);
}

void initKB() {
	for (int i = 0; i < 6; i++) {
		kb.push_back(imread("../data/KB/Painting" + to_string(i + 1) + ".jpg"));
	}
}

void rationalise(Mat& binary, vector<vector<Point>>& contours) {
	for (vector<vector<Point>>::iterator it = contours.begin(); it != contours.end();) {
		Rect boundary = boundingRect(*it);
		if (boundary.area() < 500) {
			it = contours.erase(it);
		}
		else if (boundary.height<=100 || boundary.width<=100) {
			it = contours.erase(it);
		}
		else {
			cout << "height: " << boundary.height << " , width: " << boundary.width << endl;

			Mat subImg = Mat(binary, boundary);
			vector<Point> nonZeros;
			findNonZero(subImg, nonZeros);

			int subPixels = subImg.rows*subImg.cols;

			if (nonZeros.size() < subPixels * 0.5) {
				it = contours.erase(it);
			}
			else {

				vector<Point> contour = *it;
				/*Point offset = Point(1, 1);
				for (int i = 0; i < contour.size(); i++) {
					if (contour[i].x == 0 && contour[i].y != 0) {
						contour[i].y -= 1;
					}
					else if (contour[i].y == 0 && contour[i].x != 0) {
						contour[i].x -= 1;
					}
					else if (contour[i].x != 0 && contour[i].y != 0) {
						contour[i] -= offset;
					}
				}

				*it = contour;*/
				it++;
			}
		}
	}
}

void colourRegions(vector<vector<Point2i>>& regions, Mat& dst) {
	RNG rng;
	for (int i = 0; i < regions.size(); i++) {
		Scalar col = randomColour(rng);
		for (int j = 0; j < regions[i].size(); j++) {
			int x = regions[i][j].x;
			int y = regions[i][j].y;

			Vec3b colourVec(col[0], col[1], col[2]);

			dst.at<Vec3b>(y, x) = colourVec;

		}
		
	}
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
					
					//Mat imgRegion = Mat(mask, imgSpace);
					//Mat(imgRegion, segmentBoundary).copyTo(roi);
					
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
					
					//mask.copyTo(roi);
					/*Mat testing1,testing2;
					bitwise_not(roi, testing1);

					cout << currentImg << ": " << img.size() << " , " << mask.size() << endl;

					addWeighted(grey, 0.5, testing1, 0.5, 0, testing2);
					imshow("test", testing2);
					waitKey();*/
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

	vector<vector<Point>> components;
	findContours(binaryDiff, components, CV_RETR_TREE, CV_CHAIN_APPROX_NONE);

	drawContours(img, components, -1, (255, 0, 0), 3);

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

	Mat labelImg;
	binImg.copyTo(labelImg);

	cout << labelImg.size() << endl;

	for (int j = 0; j < labelImg.rows; j++) {
		uchar* row = (uchar*)labelImg.ptr(j);
		for (int i = 0; i < labelImg.cols; i++) {
			if (*row != 1) {
				cout << "skip" << endl;
				//ignore bg & labelled regions
				continue;
			}

			Rect regionBoundary;
			Mat mask = Mat::zeros(labelImg.size(), CV_8UC1);
			floodFill(labelImg, Point(i, j), labelC, &regionBoundary, 0, 0, 4);

			vector<Point2i> thisComp;
			for (int y = regionBoundary.y; y < (regionBoundary.y + regionBoundary.height) && y < labelImg.rows; y++) {
				uchar* row2 = (uchar*)labelImg.ptr(y);
				for (int x = regionBoundary.x; x < (regionBoundary.x + regionBoundary.width) && x < labelImg.cols; x++) {
					if (*row2 != labelC) {
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