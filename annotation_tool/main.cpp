#include <direct.h>
#include <iostream>
#include <opencv\highgui.h>
#include <opencv2\imgproc\imgproc.hpp>


using namespace std;
using namespace cv;

static int posX, posY;
static int click_posX, click_posY;

Mat img_display;
bool add_ready(false);
vector<Point> polygon;
vector<vector<Point>> polygon_groups;

void DrawPolygon(Mat & src, vector<Point> polygon, Scalar line_color, bool draw_circle){
	for (int i = 0; i < (int)polygon.size() - 1; i++){
		Point pt1, pt2;
		pt1 = polygon[i];
		pt2 = polygon[i + 1];		
		line(src, pt1, pt2, line_color);
	}

	if (draw_circle){
		for (int i = 0; i < (int)polygon.size(); i++){
			circle(src, polygon[i], 2, CV_RGB(255, 215, 0), -1);
		}
	}
}

void DrawPolygonGroup(Mat & src, vector<vector<Point>> polygon_groups){
	for (int i = 0; i < (int)polygon_groups.size(); i++){
		DrawPolygon(src, polygon_groups[i], CV_RGB(255, 165, 0), false);
	}
}

void GeneratePolyMask(Mat src, Mat & dst, vector<Point> poly){
	Mat img_bin = Mat::zeros(src.size(), CV_8UC1);

	vector<vector<Point>> pPoly;
	pPoly.push_back(poly);
	fillPoly(img_bin, pPoly, Scalar(255, 255, 255));
	dst = img_bin.clone();
}

void redraw(){
	Mat img = img_display.clone();
	if (img.data != NULL){
		DrawPolygonGroup(img, polygon_groups);
		if (add_ready){
			DrawPolygon(img, polygon, CV_RGB(30, 144, 255), false);
		}
		else{
			DrawPolygon(img, polygon, CV_RGB(100, 149, 237), true);
		}
		imshow("sample", img);
	}
}

void CallBackFunc(int event, int x, int y, int flags, void* userdata)
{	
	if (event == EVENT_LBUTTONDOWN){
		polygon.push_back(Point(x, y));		
		add_ready = false;		
	}
	else if (event == EVENT_RBUTTONDOWN){
		if (!polygon.empty()) polygon.pop_back();
		add_ready = false;		
	}
	if (event == EVENT_LBUTTONDBLCLK){
		polygon.push_back(polygon[0]);
		add_ready = true;		
	}
	redraw();
}

Mat CropImage(Mat &src){
	Mat img;
	img = src(Rect(13, 20, 210, 440)).clone();
	transpose(img, img);
	flip(img, img, 0);
	resize(img, img, Size(320, 180));
	Mat dst = img(Rect(0, 10, 320, 160));

	return dst.clone();
}

void export_annotation(FileStorage & f, string name){
	f << name.c_str() << "[";
	for (int i = 0; i < (int)polygon_groups.size();i++)
	{
		f << "{:" << "x" << "[:";
		for (int j = 0; j < (int)polygon_groups[i].size(); j++){
			f << polygon_groups[i][j].x;
		}
		f << "]";

		f << "y" << "[:";		
		for (int j = 0; j < (int)polygon_groups[i].size(); j++){
			f << polygon_groups[i][j].y;
		}
		f << "]" << "}";
	}
	f << "]";
}

void import_annotation(FileStorage & f, string name){
	FileNode fn = f[name];
	FileNodeIterator it = fn.begin(), it_end = fn.end();

	polygon_groups.clear();
	for (int idx = 0; it != it_end; ++it, idx++){
		vector<Point> p;
		vector<int> x_val, y_val;
		(*it)["x"] >> x_val;
		(*it)["y"] >> y_val;
		for (int i = 0; i<(int)x_val.size(); i++){
			p.push_back(Point(x_val[i], y_val[i]));
		}
		polygon_groups.push_back(p);
	}
}

void save_current_frame(string root, int i){
	string file_log = root + "/log.yml";
	FileStorage fl(file_log, FileStorage::WRITE);
	fl << "end frame" << i;
	fl.release();
}

int restore_current_frame(string root){
	string file_log = root + "/log.yml";
	FileStorage fl;
	
	if (fl.open(file_log, FileStorage::READ) == true){
		int n;
		fl["end frame"] >> n;
		return n;
	}
	else{
		return 1;
	}
}

int main(int argc, char *argv[], char *envp[])
{
	string root, attribute;
	string image_dir, annotation_dir, attribute_dir;

	if (argc < 2) return 0;
	root = argv[1];
	attribute = argv[2];

	image_dir = root + "/images";
	annotation_dir = root + "/annotations";
	attribute_dir = annotation_dir + '/' + attribute;
	
	_mkdir(annotation_dir.c_str());
	_mkdir(attribute_dir.c_str());

	vector<int> params;
	params.push_back(CV_IMWRITE_JPEG_QUALITY);
	params.push_back(100);

	int frame = restore_current_frame(annotation_dir);

	for (;;){
		// load image
		char image_path[100];
		sprintf_s(image_path, 100, "%s/%08d.jpg", image_dir.c_str(), frame);
		Mat img = imread(image_path);
		if (img.data == NULL) continue;

		namedWindow("sample", CV_WINDOW_NORMAL);
		moveWindow("sample", 10, 10);
		setMouseCallback("sample", CallBackFunc, NULL);
		putText(img, image_path, Point(2, 7), FONT_HERSHEY_SIMPLEX, 0.25, CV_RGB(0, 0, 255));

		img_display = img.clone();

		char file_name[100];
		sprintf_s(file_name, 100, "%s/%08d.yml", attribute_dir.c_str(), frame);
		FileStorage fr;
		if (fr.open(file_name, FileStorage::READ) == true){
			import_annotation(fr, attribute);
		}
		redraw();

		while (true){
			int cKey = waitKey();				
			if (cKey == 'x' || cKey == 2555904 || cKey == 32){ // next image
				polygon_groups.clear();
				polygon.clear();
				redraw();
				frame++;
				break;
			}
			else if (cKey == 'z' || cKey == 2424832){
				polygon_groups.clear();
				polygon.clear();
				redraw();
				frame--;
				if (frame == 0) frame = 1;
				break;
			}
			else if (cKey == 'a'){ // add
				if (add_ready) {
					polygon_groups.push_back(polygon);
					polygon.clear();
					redraw();

					// save
					FileStorage f(file_name, FileStorage::WRITE);
					export_annotation(f, attribute);
					f.release();

					save_current_frame(annotation_dir, frame);
				}
			}
			else if (cKey == 'e'){ // erase
				if (!polygon_groups.empty()) {
					polygon_groups.pop_back();
					redraw();

					FileStorage f(file_name, FileStorage::WRITE);
					export_annotation(f, attribute);
					f.release();

					save_current_frame(annotation_dir, frame);
				}
			}
			else if (cKey == 'c'){ // clear polygon
				polygon.clear();
				redraw();
			}
		}
	}

	return 0;
}
