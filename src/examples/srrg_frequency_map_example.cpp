#include <fstream>
#include <opencv2/highgui/highgui.hpp>
#include "srrg_system_utils/system_utils.h"
#include "srrg_txt_io/message_reader.h"
#include "srrg_txt_io/message_writer.h"
#include "srrg_txt_io/laser_message.h"
#include "srrg_frequency_map/frequency_map.h"

#include <Eigen/Core>
#include <Eigen/Geometry>


using namespace std;
using namespace srrg_core;

// Help objects to force linking
LaserMessage l;

const char* banner[] = {
    "srrg_depth2laser_app: example on how to convert depth images into laser scans",
    "",
    "usage: srrg_depth2laser_app [options] <dump_file>",
    0
};


int main(int argc, char ** argv) {
    if (argc < 2 || !strcmp(argv[1], "-h")) {
        printBanner(banner);
        return 0;
    }

    Eigen::Matrix2d boundingBox = Eigen::Matrix2d::Zero();
    float rows, cols, gain=5, square_size=0.05f;
    float resolution = 0.05f, max_range = 5.0f, usable_range=5.0f, angle, threshold = -1.0f;
    string mapFilename = "output";
  
    float xmin=std::numeric_limits<float>::max();
    float xmax=std::numeric_limits<float>::min();
    float ymin=std::numeric_limits<float>::max();
    float ymax=std::numeric_limits<float>::min();

    vector<LaserMessage*> laser_scans;
    
    MessageReader reader;
    reader.open(argv[1]);

    BaseMessage* msg = 0;
    while ((msg = reader.readMessage())) {
      msg->untaint();
      LaserMessage* laser_msg = dynamic_cast<LaserMessage*>(msg);
      if (laser_msg) {
	float x = laser_msg->odometry().translation().x();
	float y = laser_msg->odometry().translation().y();

	xmax = xmax > x+usable_range ? xmax : x+usable_range;
	ymax = ymax > y+usable_range ? ymax : y+usable_range;
	xmin = xmin < x-usable_range ? xmin : x-usable_range;
	ymin = ymin < y-usable_range ? ymin : y-usable_range;

	laser_scans.push_back(laser_msg);
      }
    }

    boundingBox(0,0)=xmin;
    boundingBox(0,1)=xmax;
    boundingBox(1,0)=ymin;
    boundingBox(1,1)=ymax;

    cerr << "Read " << laser_scans.size() << " laser scans"<< endl;
    cerr << "Bounding box: " << endl << boundingBox << endl;
    if(laser_scans.size() == 0)  {
      cerr << "No laser scans found ... quitting!" << endl;
      return 0;
    }

    Eigen::Vector2i size;
    if(rows != 0 && cols != 0) {
      size = Eigen::Vector2i(rows, cols);
    } else {
      size = Eigen::Vector2i((boundingBox(0, 1) - boundingBox(0, 0))/ resolution,
			   (boundingBox(1, 1) - boundingBox(1, 0))/ resolution);
    }
    cerr << "Map size: " << size.transpose() << endl;
    if(size.x() == 0 || size.y() == 0) {
      cerr << "Zero map size ... quitting!" << endl;
      return 0;
    }

    Eigen::Vector2f offset(boundingBox(0, 0),boundingBox(1, 0));
    FrequencyMapCell unknownCell;
    FrequencyMap map = FrequencyMap(resolution, offset, size, unknownCell);

    for(size_t i = 0; i < laser_scans.size(); ++i) {
      LaserMessage* msg = laser_scans[i];
      map.integrateScan(msg, msg->odometry(), max_range, usable_range, gain, square_size);
    }

    cv::Mat mapImage(map.rows(), map.cols(), CV_8UC1);
    mapImage.setTo(cv::Scalar(0));
    for(int c = 0; c < map.cols(); c++) {
      for(int r = 0; r < map.rows(); r++) {
	if(map(r, c).misses() == 0 && map(r, c).hits() == 0) {
	  mapImage.at<unsigned char>(r, c) = 127;
	} else {
	  float fraction = (float)map(r, c).hits()/(float)(map(r, c).hits()+map(r, c).misses());
	  if (threshold > 0 && fraction > threshold)
	    mapImage.at<unsigned char>(r, c) = 0;
	  else if (threshold > 0 && fraction <= threshold)
	    mapImage.at<unsigned char>(r, c) = 255;
	  else {
	    float val = 255*(1-fraction);
	    mapImage.at<unsigned char>(r, c) = (unsigned char)val;
	  }
	}
      }
    }
    cv::imwrite(mapFilename + ".png", mapImage);
    
    cerr << "done" << endl;
}
