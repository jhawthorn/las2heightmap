#include <memory>
#include <iostream>
#include <stdint.h>

#include <pdal/PointTable.hpp>
#include <pdal/PointView.hpp>
#include <pdal/io/LasReader.hpp>
#include <pdal/io/LasHeader.hpp>
#include <pdal/Options.hpp>

#include <png++/png.hpp>
using namespace std;

#define WIDTH 2048
#define HEIGHT 2048

struct Point {
	double x, y, z;
	int classification;
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
	pdal::LasReader las_reader;

	double offsetX;
	double offsetY;
	double offsetZ;
	double scaleX;
	double scaleY;

	void addPoint(double x, double y, double z, int classification, int intensity) {
		x = (x - offsetX) * scaleX;
		y = (y - offsetY) * scaleY;
		z = (z - offsetZ);
		intensity /= 256;

		/* Skip vegetation, "other", and noise */
		if (classification == 3 || classification == 4 || classification == 7 || classification == 8)
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

	LasToHeightmap(pdal::Options &las_opts) {
		las_reader.setOptions(las_opts);

		pointMatrix = new std::vector<Point>[HEIGHT][WIDTH]();
	}

	void perform() {
		pdal::PointTable table;
		las_reader.prepare(table);
		pdal::PointViewSet point_view_set = las_reader.execute(table);
		pdal::PointViewPtr point_view = *point_view_set.begin();
		pdal::Dimension::IdList dims = point_view->dims();
		pdal::LasHeader las_header = las_reader.header();

		offsetX = las_header.minX();
		offsetY = las_header.maxY();

		offsetZ = -16;

		// FIXME: calculate this value
		scaleX = WIDTH/1000.0;
		scaleY = -HEIGHT/1000.0;

		for (pdal::PointId idx = 0; idx < point_view->size(); ++idx) {
			using namespace pdal::Dimension;
			double x = point_view->getFieldAs<double>(Id::X, idx);
			double y = point_view->getFieldAs<double>(Id::Y, idx);
			double z = point_view->getFieldAs<double>(Id::Z, idx);

			int classification = point_view->getFieldAs<int>(Id::Classification, idx);
			int intensity = point_view->getFieldAs<int>(Id::Intensity, idx);

			addPoint(x, y, z, classification, intensity);
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
			std::nth_element(
					neighbourPoints.begin(),
					neighbourPoints.begin() + (neighbourPoints.size() / 2),
					neighbourPoints.end(),
					[](const Point &p1, const Point &p2) { return p1.z < p2.z; }
					);

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

	cout << "Collecting all points..." << endl;
	pdal::Option las_opt("filename", input_filename);
	pdal::Options las_opts;
	las_opts.add(las_opt);
	LasToHeightmap lasToHeightmap(las_opts);
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
