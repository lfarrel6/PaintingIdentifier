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
void rationalise(Mat&, vector<vector<Point>>&,int);
void initKB();
void p2fToPoint(Point2f&,Point&);
void allPerms(vector<Point>&, vector<RotatedRect>&);
void getPoints(Rect&, vector<Point>&);
void removeColFromImg(Mat&, Scalar&);
bool entersRectangle(Rect&, LineIterator&);
void joinLines(Vec4i, Vec4i, Vec4i&);
void mergeLines(vector<Vec4i>&, Vec4i&);

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
map<String,Mat> kb;
Scalar wallCol;

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

		//cvtColor(img, grey, CV_RGB2GRAY);
		formGroundTruths(groundTruth, currentImg);

		Mat hlsimg;
		cvtColor(img, hlsimg, CV_RGB2HLS);

		Mat meanshifted;
		Mat roi = Mat::zeros(img.rows, img.cols, CV_8UC1);

		meanShiftSegmentation(img, meanshifted, roi);
		cvtColor(img, grey, CV_RGB2GRAY);
		//removeColFromImg(meanshifted, wallCol);

		bitwise_not(roi, roi);

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
					imshow("regionToInvestigate", regionToInvestigate);
					

					vector<Point> corners;
					int maxCornerCount = 8;
					imshow("greyd", grey);

					goodFeaturesToTrack(grey, corners, maxCornerCount, 0.05, grey.rows*0.1, regionToInvestigate);

					cout << "corners found " << corners.size() << endl;

					Mat empty = Mat::zeros(img.size(), CV_8UC1);

					for (int co = 0; co < corners.size(); co++) {
						circle(img, corners[co], 2, (255,255,255), 2);
					}

					Mat img2;
					img.copyTo(img2);

					Rect paintingArea = boundingRect(corners);
					if (paintingArea.area() > 1000) {

						imshow("bounded corners", img2);
						//imshow("good features", img2);

						Mat subImage = img(paintingArea);
						Mat greyRegion, binaryRegion;
						cvtColor(test, greyRegion, CV_RGB2GRAY);

						adaptiveThreshold(greyRegion, binaryRegion, 255, ADAPTIVE_THRESH_GAUSSIAN_C, THRESH_BINARY_INV, 7, 2);
						morphologyEx(binaryRegion, binaryRegion, MORPH_OPEN, element);

						vector<Vec4i> lines;
						HoughLinesP(binaryRegion, lines, 1, CV_PI / 180, 40, 40, 30);
						
						imshow("binary region", binaryRegion);
						Size rSize = Size(binaryRegion.cols / 3, binaryRegion.rows / 3);
						//define a rectangle in the centre of the region to prevent noise on the image
						Rect noGoZone = Rect(Point((binaryRegion.cols / 2) - rSize.width/2, (binaryRegion.rows / 2) - rSize.height/2),rSize);

						Mat blank = Mat::zeros(test.size(), CV_8UC1);
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
									//line(blank, p1, p2, 255, 1);
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
						for (int o = 0; o < 10; o++) {
							for (int n = 0; n < lines.size(); n++) {
								mergeLines(lines, lines.at(n));
							}
						}

						for (vector<Vec4i>::iterator it = lines.begin(); it != lines.end(); ++it) {
							Vec4i thisvec = *it;
							Point p1(thisvec[0], thisvec[1]);
							Point p2(thisvec[2], thisvec[3]);

							line(blank, p1, p2, 255, 1);
						}

						//rectangle(test, noGoZone, CV_RGB(255,255,0), 2);
						imshow("painting area", subImage);
						imshow("hough lines", blank);

						imwrite("../data/hl/" + to_string(currentImg) + "-Region-" + to_string(i) + ".jpg", blank);

						vector<vector<Point>> contours;

						/*
						NEXT,PREV,CHILD,PARENT
						*/
						int next = 0;
						int prev = 1;
						int child = 2;
						int parent = 3;

						vector<Vec4i> hierarchy;
						findContours(blank, contours, hierarchy, CV_RETR_TREE, CHAIN_APPROX_NONE);

						blank = Mat::zeros(blank.size(), CV_8UC1);
						for (int cV = 0; cV < contours.size(); cV++) {
							cout << hierarchy[cV] << endl;
							drawContours(blank, contours, cV, 255, 1);
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

						double largestMatch = 0;
						Mat match;
						Point matchLocation;
						String matchName;
						Size newSize;
						//brute force template match
						/*for (map<String, Mat>::iterator it = kb.begin(); it != kb.end(); ++it) {
							Mat currentPainting = it->second;
							
							int r = currentPainting.rows;
							int c = currentPainting.cols;

							newSize = Size(max(r, c), max(r, c));

							Mat resizedROI;
							resize(subImage, resizedROI, newSize);


							Mat resized;
							resize(currentPainting, resized, newSize);

							imshow("resized1", resizedROI);
							imshow("resized2", resized);
							waitKey();

							Mat res;
							matchTemplate(resizedROI, resized, res, CV_TM_CCORR_NORMED);

							double max;
							minMaxLoc(res, NULL, &max, NULL, &matchLocation);
							cout << "match val: " << max << endl;
							if (max > largestMatch) {
								matchName = it->first;
								largestMatch = max;
								match = currentPainting;
							}

						}
						float roundedMatch = roundf(largestMatch * 100) / 100;
						cout << "rounded " << roundedMatch << endl;
						if (roundedMatch >= 0.7) {
							cout << "max match val: " << largestMatch << endl;
							rectangle(untouchedRegion, paintingArea, CV_RGB(255, 0, 0), 3);
							imshow("match w/ area", untouchedRegion);
							imshow("match2", match);
							waitKey();
						}
						else {
							cout << "no suitable match found" << endl;
						}*/
					}
				}
				else {
					cout << "rejected. angle: " << rRect.angle << endl;
					imshow("rejected region", img(boundary));
					
				}
			}



		imwrite("../data/ConnectedComponents/" + to_string(currentImg) + "-components.jpg", img);
	}
	//waitKey();
    return 0;
}

void removeColFromImg(Mat& img, Scalar& col) {
	int r, g, b;
	r = col[0]; g = col[1]; b = col[2];
	int low_r = r - 25; int low_g = g - 25; int low_b = b - 25;
	Scalar lowT = (low_b, low_g, low_r);
	
	int high_r = r + 25; int high_g = g + 25; int high_b = b + 25;
	Scalar highT = (high_b, high_g, high_r);

	Mat detected;
	inRange(img, lowT, highT, detected);
	rectangle(img, Rect(Point(0, 0), Point(100, 100)), col,10);
	imshow("to find in", img);
	rectangle(img, Rect(Point(0, 0), Point(100, 100)), highT, 10);
	imshow("col2", img);
	imshow("color found", detected);
	waitKey();
}

void getPoints(Rect& r, vector<Point>& corners) {
	corners.push_back(r.tl());                     //top left
	corners.push_back(r.tl() + Point(r.width, 0)); //top right
	corners.push_back(r.tl() + Point(0,r.height)); //bottom left
	corners.push_back(r.br());                     //bottom right
}

void p2fToPoint(Point2f& vertice, Point& pVert) {
	pVert = Point(vertice.x, vertice.y);
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

void mergeLines(vector<Vec4i>& lines, Vec4i& vec1) {
	int c = 0;
	bool hardExit = false;

	Point line1p1 = Point(vec1[0], vec1[1]);
	Point line1p2 = Point(vec1[2], vec1[3]);

	Point line1Ctr = (line1p1 + line1p2) / 2;
	for (vector<Vec4i>::iterator it2 = lines.begin(); it2 != lines.end();) {
		cout << "2: looping"<< endl;

		cout << "inside it1" << endl;
		if (*it2 != vec1) {
			Vec4i vec2 = *it2;
			Point line2p1 = Point(vec2[0], vec2[1]);
			Point line2p2 = Point(vec2[2], vec2[3]);
			Point line2Ctr = (line2p1 + line2p2) / 2;
			vector<double> dist;
			double avgDist = norm(line1p1 - line2p1) + norm(line1p2 - line2p2);
			avgDist /= 2;

			if (avgDist < 40) {
				Vec4i res;
				joinLines(vec1, vec2, res);
				vec1 = res;
				it2 = lines.erase(it2);
				cout << "deleted value , new size: " << lines.size() << endl;
			}
			else {
				cout << "too far" << endl;
				++it2;
			}
			cout << "syn" << endl;
		}
		else {
			cout << "it1==it2" << endl;
			++it2;
		}
	}
}

void joinLines(Vec4i l1, Vec4i l2, Vec4i& result) {
	Point l1p1 = Point(l1[0], l1[1]);
	Point l1p2 = Point(l1[2], l1[3]);

	Point l2p1 = Point(l2[0], l2[1]);
	Point l2p2 = Point(l2[2], l2[3]);

	double rise1 = l1p2.y - l1p1.y;
	double run1  = l1p2.x - l1p1.x;
	double slope1;
	bool vertical1 = false;

	if (run1 != 0) {
		slope1 = rise1 / run1;
	}
	else {
		vertical1 = true;
	}
	double rise2 = l2p2.y - l2p1.y;
	double run2 = l2p2.x - l2p1.x;
	double slope2;
	bool vertical2 = false;

	if (run2 != 0) {
		slope2 = rise2 / run2;
	}
	else {
		vertical2 = true;
	}

	if (vertical1 && vertical2) {
		int maxY = max(max(l1p1.y, l1p2.y), max(l2p1.y, l2p2.y));
		int minY = min(min(l1p1.y,l1p2.y), min(l2p1.y,l2p2.y));
		int meanX = (l1p1.x + l2p1.x) / 2;

		result = Vec4i(meanX, minY, meanX, maxY);
	}
	else if (!vertical1 && !vertical2) {
		int meanX1 = (l1p1.x + l2p1.x) / 2;
		int meanX2 = (l1p2.x + l2p2.x) / 2;
		int meanY1 = (l1p1.y + l2p1.y) / 2;
		int meanY2 = (l1p2.y + l2p2.y) / 2;

		result = Vec4i(meanX1,meanY1,meanX2,meanY2);
	}
	else {
		cout << "one vertical, one not" << endl;
		result = l1;
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
					
					wallCol = newVal;

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