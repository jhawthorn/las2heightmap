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
	uint8_t intensity;

	double distance2(const Point &p) const {
		double dx = p.x - x;
		double dy = p.y - y;
		double dz = p.z - z;
		return dx*dx + dy*dy + dz*dz;
	}

	double distance(const Point &p) const {
		return sqrt(distance2(p));
	}
};

class LasToHeightmap {
	liblas::Reader las;

	double offsetX;
	double offsetY;
	double offsetZ;
	double scaleX;
	double scaleY;

	void addPoint(liblas::Point lasPoint) {
		double x = (lasPoint.GetX() - offsetX) * scaleX;
		double y = (lasPoint.GetY() - offsetY) * scaleY;
		double z = (lasPoint.GetZ() - offsetZ);
		uint8_t classification = lasPoint.GetClassification().GetClass();
		uint16_t intensity = lasPoint.GetIntensity();
		intensity /= 256;
		if (classification == 3 || classification == 5)
			return;

		if ((int)x >= WIDTH)
			x = WIDTH-1;
		if ((int)y >= HEIGHT)
			y = HEIGHT-1;

		if (intensity > 255)
			intensity = 255;

		Point point = {x,y,z,classification,(uint8_t)intensity};

		pointMatrix[(int)y][(int)x].push_back(point);
	}

	public:

	std::vector<Point> (*pointMatrix)[WIDTH];

	LasToHeightmap(std::ifstream &input_file) : las(input_file) {
		pointMatrix = new std::vector<Point>[HEIGHT][WIDTH]();
	}

	void perform() {
		auto lasHeader = las.GetHeader();
		offsetX = lasHeader.GetMinX();
		offsetY = lasHeader.GetMaxY();

		offsetZ = -16;

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
			return NULL;
		} else {
			return &pointMatrix[y][x];
		}
	}

	/* Makes a "fake" point with some heuristics on the points around it */
	Point pointAt(int x, int y, int range=3) {
		Point point = {(double)x + 0.5, (double)y + 0.5, 0, 0, 0};
		std::vector<Point> neighbourPoints;
		neighbourPoints.reserve(2048);

		auto addPoints = [&](std::vector<Point> *v) {
			if (v) {
				std::copy(v->begin(), v->end(), std::back_inserter(neighbourPoints));
			}
		};

		for (int dy = -range; dy <= range; dy++) {
			for (int dx = -range; dx <= range; dx++) {
				addPoints(pointsAt(x+dx, y+dy));
			}
		}

		if(neighbourPoints.empty()) {
			return point;
		} else {
			std::sort(neighbourPoints.begin(), neighbourPoints.end(), [](const Point &p1, const Point &p2) { return p1.z < p2.z; });

			Point medianPoint = neighbourPoints[neighbourPoints.size() / 2];
			point.z = medianPoint.z;

			auto representativePoint = std::min_element(
					neighbourPoints.begin(), neighbourPoints.end(),
					[&](const Point &p1, const Point &p2) { return point.distance2(p1) < point.distance2(p2); }
					);
			if (representativePoint != neighbourPoints.end()) {
				/* If we can find a point near out median z point, use its intensity */
				//point.intensity = representativePoint->intensity;
				point.intensity = representativePoint->intensity;
			} else {
				/* Otherwise, take the average */
				cerr << "this should never happen" << endl;
				point.intensity = 255; /*DEBUG*/
			}

			//point.intensity = medianPoint.intensity;

			return point;
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

	liblas::ReaderFactory f;
	liblas::Reader input_reader(input_file);

	cout << "Collecting all points..." << endl;
	LasToHeightmap lasToHeightmap(input_file);
	lasToHeightmap.perform();

	cout << "Creating heightmap..." << endl;
	for (int y = 0; y < HEIGHT; y++) {
		for (int x = 0; x < WIDTH; x++) {
			Point p = lasToHeightmap.pointAt(x, y);
			double z = p.z;
			if (z < 0)
				z = 0;
			unsigned short iz = z * 256;
			output_image[y][x] = png::rgb_pixel(p.intensity, iz >> 8, iz & 0xff);
			//output_image[y][x] = png::rgb_pixel(z, z, z);
		}
	}

	output_image.write(output_filename);

	return 0;
}
