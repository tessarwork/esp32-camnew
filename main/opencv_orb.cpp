#include <stdio.h>
#include <stdlib.h>
#include <opencv2/core/core.hpp>
#include <opencv2/features2d/features2d.hpp>
#include <opencv2/core/ocl.hpp>
#include <opencv2/core/cuda.hpp>
#include <opencv2/imgcodecs/legacy/constants_c.h>
#include <opencv2/core.hpp>
#include <opencv2/objdetect.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

#include <opencv2/core/utility.hpp>
#include "opencv2/opencv_modules.hpp"

#include <opencv2/core/cvstd.hpp>


#include <opencv2/core/softfloat.hpp>
#include <opencv2/imgproc/types_c.h>

#include <iostream>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

using namespace std;
using namespace cv;

extern "C" long int sysconf(int);
long int sysconf(int wtf){ 
    return 1;
}

extern "C" void init_opencv_orb();
extern "C" void orb_features2d(uint8_t* data, int width, int height, int* descriptor, int* desc_size);
// extern "C" void orb_resize(uint8_t* data, int width, int height);

cv::Ptr<cv::ORB> orb_detector;
std::vector<cv::KeyPoint> mvkeys;
cv::Mat feature_extract;
// cv::Mat dst; //destination image

void init_opencv_orb(){ 
    orb_detector = cv::ORB::create();
    orb_detector->setFastThreshold(29);
    orb_detector->setNLevels(1);
    orb_detector->setMaxFeatures(10);
    orb_detector->setScaleFactor(1.2);
    orb_detector->setEdgeThreshold(35);
    orb_detector->setScoreType(cv::ORB::ScoreType::FAST_SCORE);
    orb_detector->setFirstLevel(0);
}



void orb_features2d(uint8_t* data, int width, int height, int* descriptor, int* desc_size){ 
    cv::Mat rawdata( height, width, CV_8UC1, (void*) data);

    // orb_detector -> detect(rawdata, mvkeys);
    mvkeys.push_back(cv::KeyPoint(50, 50, 20));
    orb_detector -> compute(rawdata, mvkeys, feature_extract);

    int num_elements = feature_extract.total();
    *desc_size = num_elements;

    for(int i = 0; i < num_elements; ++i ){ 
        descriptor[i] = static_cast<int>(feature_extract.at<uint8_t>(i));

    }



    printf("For picture: mvKeys.size = %i\n", mvkeys.size());
    std::cout << "Feature Extraction: \n" << feature_extract << std::endl;

    std::cout << "Feature Extraction (as int array): \n";

    for (int i = 0; i < num_elements; i++) {
        std::cout << descriptor[i] << " ";
    }
    std::cout << std::endl;
    
    // return feature_extract;

}