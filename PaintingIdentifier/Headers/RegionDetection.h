#ifndef RegionDetection_h
#define RegionDetection_h

//True if r1 is a subset of r2
bool isSubset(Rect r1, Rect r2){
	return (r1 | r2).area() == r2.area();
}

bool hasOverlap(Rect r1, Rect r2){
	return (r1 & r2).area() > 0;
}

Rect mergeRects(Rect r1, Rect r2){
	return (r1 | r2);
}

void colourRegions(vector<vector<Point2i>>& regions, Mat& dst){
	RNG rng;
	for(int i = 0; i < regions.size(); i++){
		Scalar col = randomColour(rng);
		for(int j = 0; j < regions[i].size(); j++){
			int x = regions[i][j].x;
			int y = regions[i][j].y;

			Vec3b colourVec(col[0],col[1],col[2]);

			dst.at<Vec3b>(y,x) = colourVec;
		}
	}
}

Scalar randomColour(RNG& rng){
	int icolor = (unsigned)rng;
	return Scalar(icolor&255, (icolor>>8)&255, (icolor>>16)&255);
}

#endif