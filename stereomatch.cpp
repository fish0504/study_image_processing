#include "stereomatch.hpp"



static void saveXYZ(const char* filename, const Mat& mat)
{
    const double max_z = 1.0e4;
    FILE* fp = fopen(filename, "wt");
    for(int y = 0; y < mat.rows; y++)
    {
        for(int x = 0; x < mat.cols; x++)
        {
            Vec3f point = mat.at<Vec3f>(y, x);
            if(fabs(point[2] - max_z) < FLT_EPSILON || fabs(point[2]) > max_z) continue;
            fprintf(fp, "%f %f %f\n", point[0], point[1], point[2]);
        }
    }
    fclose(fp);
}


bool init_stereomatch()
{
std::string intrinsic_filename_left="l2_intrinsic.xml";
std::string intrinsic_filename_right="r2_intrinsic.xml";
std::string extrinsic_filename="extrinsic2.xml";
std::string img1_filename="./calib_img2/left/matching_sample_left.png";
std::string img2_filename="./calib_img2/right/matching_sample_right.png";
bm = StereoBM::create(16,9);
sgbm = StereoSGBM::create(0,16,3);
    if( !intrinsic_filename_left.empty() )
    {
        // reading intrinsic parameters
        FileStorage fs(intrinsic_filename_left, FileStorage::READ);
        if(!fs.isOpened())
        {
            printf("Failed to open file %s\n", intrinsic_filename.c_str());
            return -1;
        }

        
        fs["M1"] >> M1;
        fs["D1"] >> D1;
        fs.open(intrinsic_filename_right, FileStorage::READ);
        fs["M2"] >> M2;
        fs["D2"] >> D2;

        M1 *= scale;
        M2 *= scale;

        fs.open(extrinsic_filename, FileStorage::READ);
        if(!fs.isOpened())
        {
            printf("Failed to open file %s\n", extrinsic_filename.c_str());
            return -1;
        }

        
        fs["R"] >> R;
        fs["T"] >> T;
        std::ostringstream oss;
        std::string save_name;
        oss << "disparity  " << (alg==STEREO_BM ? "bm" :
                                 alg==STEREO_SGBM ? "sgbm" :
                                 alg==STEREO_HH ? "hh" :
                                 alg==STEREO_VAR ? "var" :
                                 alg==STEREO_HH4 ? "hh4" :
                                 alg==STEREO_3WAY ? "sgbm3way" : "");
        save_name=oss.str();
        //oss << "  blocksize:" << (alg==STEREO_BM ? SADWindowSize : sgbmWinSize);
        oss << "  max-disparity:" << numberOfDisparities;
        disp_name = oss.str();
        namedWindow(disp_name, cv::WINDOW_AUTOSIZE);
    }
    else{
        return false;
    }
    return true;
}

cv::Mat getDepthImage(cv::Mat img1,cv::Mat img2){
   
    int color_mode = alg == STEREO_BM ? 0 : -1;
    
    //Mat img1 = imread(img1_filename, color_mode);
    //Mat img2 = imread(img2_filename, color_mode);
    if (img1.empty())
    {
        printf("Command-line parameter error: could not load the first input image file\n");
        //return -1;
    }
    if (img2.empty())
    {
        printf("Command-line parameter error: could not load the second input image file\n");
        //return -1;
    }
    cv::Rect roi(cv::Point2i(w/2-roi_width/2,h/2-roi_height/2),cv::Size(roi_width,roi_height));
    //printf("now1\n");
    
    if (scale != 1.f)
    {
        Mat temp1, temp2;
        int method = scale < 1 ? INTER_AREA : INTER_CUBIC;
        cv::resize(img1, temp1, Size(), scale, scale, method);
        img1 = temp1;
        cv::resize(img2, temp2, Size(), scale, scale, method);
        img2 = temp2;
    }
    //img1=img1(roi);
    //img2=img2(roi);
    
    Size img_size = img1.size();
    Rect roi1,roi2;
    Mat Q;
    
    //printf("now2\n");
    cv::stereoRectify( M1, D1, M2, D2,img_size , R, T, R1, R2, P1, P2, Q, CALIB_ZERO_DISPARITY, -1,img_size, &roi1, &roi2 );
    //printf("now3\n");
    Mat map11, map12, map21, map22;
    cv::initUndistortRectifyMap(M1, D1, R1, P1, img_size, CV_16SC2, map11, map12);
    cv::initUndistortRectifyMap(M2, D2, R2, P2, img_size, CV_16SC2, map21, map22);
    
    Mat img1r, img2r;
    cv::remap(img1, img1r, map11, map12, INTER_LINEAR);
    cv::remap(img2, img2r, map21, map22, INTER_LINEAR);
    
    img1 = img1r;
    img2 = img2r;
    numberOfDisparities = numberOfDisparities > 0 ? numberOfDisparities : ((img_size.width/8) + 15) & -16;

    bm->setROI1(roi1);
    bm->setROI2(roi2);
    bm->setPreFilterCap(31);
    bm->setBlockSize(SADWindowSize > 0 ? SADWindowSize : 9);
    bm->setMinDisparity(0);
    bm->setNumDisparities(numberOfDisparities);
    bm->setTextureThreshold(10);
    bm->setUniquenessRatio(15);
    bm->setSpeckleWindowSize(100);
    bm->setSpeckleRange(32);
    bm->setDisp12MaxDiff(1);

    sgbm->setPreFilterCap(63);
    int sgbmWinSize = SADWindowSize > 0 ? SADWindowSize : 3;
    sgbm->setBlockSize(sgbmWinSize);

    int cn = img1.channels();

    sgbm->setP1(8*cn*sgbmWinSize*sgbmWinSize);
    sgbm->setP2(32*cn*sgbmWinSize*sgbmWinSize);
    sgbm->setMinDisparity(0);
    sgbm->setNumDisparities(numberOfDisparities);
    sgbm->setUniquenessRatio(10);
    sgbm->setSpeckleWindowSize(100);
    sgbm->setSpeckleRange(32);
    sgbm->setDisp12MaxDiff(1);
    if(alg==STEREO_HH)
        sgbm->setMode(StereoSGBM::MODE_HH);
    else if(alg==STEREO_SGBM)
        sgbm->setMode(StereoSGBM::MODE_SGBM);
    else if(alg==STEREO_HH4)
        sgbm->setMode(StereoSGBM::MODE_HH4);
    else if(alg==STEREO_3WAY)
        sgbm->setMode(StereoSGBM::MODE_SGBM_3WAY);

    Mat disp, disp8;
    //Mat img1p, img2p, dispp;
    //copyMakeBorder(img1, img1p, 0, 0, numberOfDisparities, 0, IPL_BORDER_REPLICATE);
    //copyMakeBorder(img2, img2p, 0, 0, numberOfDisparities, 0, IPL_BORDER_REPLICATE);
    
    int64 t = getTickCount();
    float disparity_multiplier = 1.0f;
    if( alg == STEREO_BM )
    {
        bm->compute(img1, img2, disp);
        if (disp.type() == CV_16S)
            disparity_multiplier = 16.0f;
    }
    else if( alg == STEREO_SGBM || alg == STEREO_HH || alg == STEREO_HH4 || alg == STEREO_3WAY )
    {
        sgbm->compute(img1, img2, disp);
        if (disp.type() == CV_16S)
            disparity_multiplier = 16.0f;
    }
    t = getTickCount() - t;
    //printf("Time elapsed: %fms\n", t*1000/getTickFrequency());

    //disp = dispp.colRange(numberOfDisparities, img1p.cols);
    if( alg != STEREO_VAR )
        disp.convertTo(disp8, CV_8U, 255/(numberOfDisparities*16.));
    else
        disp.convertTo(disp8, CV_8U);

    Mat disp8_3c;
    //printf("now4\n");
    // if (color_display)
    //     cv::applyColorMap(disp8, disp8_3c, COLORMAP_TURBO);

    // if(!disparity_filename.empty())
    //     //imwrite(disparity_filename, color_display ? disp8_3c : disp8);

    // if(!point_cloud_filename.empty())
    // {
    //     printf("storing the point cloud...");
    //     fflush(stdout);
    //     Mat xyz;
    //     Mat floatDisp;
    //     disp.convertTo(floatDisp, CV_32F, 1.0f / disparity_multiplier);
    //     reprojectImageTo3D(floatDisp, xyz, Q, true);
    //     saveXYZ(point_cloud_filename.c_str(), xyz);
    //     printf("\n");
    // }

    if( !no_display )
    {
        

        // namedWindow("left", cv::WINDOW_NORMAL);
        // imshow("left", img1);
        // namedWindow("right", cv::WINDOW_NORMAL);
        // imshow("right", img2);
        
        imshow(disp_name, color_display ? disp8_3c : disp8);
        convertToPython(disp8);
        //imwrite("./disp_results/"+save_name+".png",disp8);
        //printf("depth:\nwidth %d height %d",disp8.cols,disp8.rows);
        //printf("press ESC key or CTRL+C to close...");
        //fflush(stdout);
        //printf("\n");
        // while(1)
        // {
        //     if(waitKey() == 27) //ESC (prevents closing on actions like taking screenshots)
        //         break;
        // }
    }

    return disp8;
}