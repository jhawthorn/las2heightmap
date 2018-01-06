#include <liblas/liblas.hpp>
#include <png++/png.hpp>
#include <iostream>
#include <stdint.h>
using namespace std;

#define WIDTH 2048
#define HEIGHT 2048

struct Point {
	double x, y, z;
	uint8_t classification;
};

class LasToHeightmap {
	liblas::Reader las;

	double offsetX;
	double offsetY;
	double scaleX;
	double scaleY;

	void addPoint(liblas::Point lasPoint) {
		double x = (lasPoint.GetX() - offsetX) * scaleX;
		double y = (lasPoint.GetY() - offsetY) * scaleY;
		double z = (lasPoint.GetZ());
		uint8_t classification = lasPoint.GetClassification().GetClass();

		if (classification == 2 || classification == 3)
			return;

		if ((int)x >= WIDTH)
			x = WIDTH-1;
		if ((int)y >= HEIGHT)
			y = HEIGHT-1;

		Point point = {x,y,z,classification};

		pointMatrix[(int)y][(int)x].push_back(point);
	}

	std::vector<Point> emptyVector;

	public:

	std::vector<Point> (*pointMatrix)[WIDTH];

	LasToHeightmap(std::ifstream &input_file) : las(input_file) {
		pointMatrix = new std::vector<Point>[HEIGHT][WIDTH]();
	}

	void perform() {
		auto lasHeader = las.GetHeader();
		offsetX = lasHeader.GetMinX();
		offsetY = lasHeader.GetMaxY();

		// FIXME: calculate this value
		scaleX = WIDTH/1000.0;
		scaleY = -HEIGHT/1000.0;

		for (int i = 0; i < 500; i++)
			las.ReadNextPoint();

		while (las.ReadNextPoint()) {
			liblas::Point lasPoint = las.GetPoint();

			addPoint(lasPoint);
		}
	}

	std::vector<Point> *pointsAt(int x, int y) {
		if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) {
			return &emptyVector;
		} else {
			return &pointMatrix[y][x];
		}
	}

	double heightAt(int x, int y) {
		std::vector<Point> neighbourPoints;

		auto addPoints = [&](std::vector<Point> *v) {
			std::copy(v->begin(), v->end(), std::back_inserter(neighbourPoints));
		};

		addPoints(pointsAt(x, y));

		if(neighbourPoints.empty()) {
			return 0;
		} else {
			std::sort(neighbourPoints.begin(), neighbourPoints.end(), [](Point p1, Point p2) { return p1.z < p2.z; });

			return neighbourPoints[neighbourPoints.size() / 2].z;
		}
	}

};

int main(int argc, char *argv[]) {
	if (argc != 3) {
		cerr << "Usage: " << argv[0] << " INPUT OUTPUT" << endl;
		return 1;
	}

	const char *input_filename = argv[1];
	const char *output_filename = argv[2];

	png::image<png::rgb_pixel> output_image(WIDTH, HEIGHT);

	std::ifstream input_file(input_filename, ios::in | ios::binary);

	liblas::Reader input_reader(input_file);

	LasToHeightmap lasToHeightmap(input_file);
	lasToHeightmap.perform();

	for (int y = 0; y < HEIGHT; y++) {
		for (int x = 0; x < WIDTH; x++) {
			double z = lasToHeightmap.heightAt(x, y);
			if (z < 0)
				z = 0;
			unsigned short iz = z * 256;
			output_image[y][x] = png::rgb_pixel(0, iz >> 8, iz & 0xff);
			//output_image[y][x] = png::rgb_pixel(z, z, z);
		}
	}

	output_image.write(output_filename);

	return 0;
}
