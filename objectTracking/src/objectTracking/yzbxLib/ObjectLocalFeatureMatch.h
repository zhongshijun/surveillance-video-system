#ifndef OBJECTLOCALFEATUREMATCH_H
#define OBJECTLOCALFEATUREMATCH_H
//computer object local feature and return match for each object.
//that's to see, the pathWeights for GraphBasedTracker

#include "yzbx_config.h"
#include "yzbx_utility.h"
#include <opencv2/opencv.hpp>
#include <opencv2/features2d/features2d.hpp>
#include <QtCore>
using namespace std;
using namespace cv;

class ObjectLocalFeatureMatch
{
public:
    ObjectLocalFeatureMatch(int MaxFeaturePointNum=50);
    int MaxFeaturePointNum;
    void process(const cv::Mat &img1,const cv::Mat &mask1,const cv::Mat &img2,const cv::Mat &mask2){
        getGoodMatches(img1,mask1,img2,mask2);
    }
    void getFeaturePoint_Step1(const cv::Mat &img,const cv::Mat &mask, std::vector<cv::KeyPoint> &kps){
        if(!kps.empty()) kps.clear();

        Ptr<FeatureDetector> detector=new BRISK(10);
        assert(!detector.empty());

        vector<KeyPoint> keypoints;
        detector->detect(img, keypoints,mask);
        kps.swap(keypoints);
        // detect(img,keypoints,mask) failed, keypoints out of mask!!!
//        assert(mask.channels()==1);
//        for(uint i=0;i<keypoints.size();i++){
//            Point2f p=keypoints[i].pt;
////            assert(mask.at<uchar>(p.y,p.x)==255);
//            if(mask.at<uchar>(p.y,p.x)!=255){
//                keypoints.erase(keypoints.begin()+i);
//                i--;
//            }
//        }

        //use the best MaxFeaturePointNum feature point according to response.
//        if(keypoints.size()<MaxFeaturePointNum){
//            kps.swap(keypoints);
//        }
//        else{
//            //sort from big to small!!!
//            std::sort(keypoints.begin(), keypoints.end(),
//                      [](KeyPoint const & a, KeyPoint const & b) -> bool
//            { return a.response > b.response; } );

//            assert(keypoints[0].response>=keypoints[1].response);

//            const int n=keypoints.size();
//            for(uint i=0;i<MaxFeaturePointNum;i++){
//                kps.push_back(keypoints[i]);
//            }
//        }
    }

    void getDescriptorMat_Step2(const cv::Mat &img,vector<KeyPoint> &kps ,cv::Mat &des){
        Ptr<DescriptorExtractor> extractor= DescriptorExtractor::create("FREAK");
        assert(!extractor.empty());
        extractor->compute(img, kps, des);
    }

    static void getGoodMatches_Step3(Mat &descriptors_1, Mat &descriptors_2, vector<DMatch> &good_matches){
//        Ptr<DescriptorMatcher> matcher=new BFMatcher(NORM_HAMMING,true);
//        matcher->match(descriptors_1, descriptors_2, good_matches);
        Ptr<DescriptorMatcher> matcher=new BFMatcher(NORM_HAMMING);
        vector<vector<DMatch>> knn_matchesA,knn_matchesB;
        vector<DMatch> sym_matchesA,sym_matchesB;
        if(!good_matches.empty()) good_matches.clear();
        matcher->knnMatch(descriptors_1,descriptors_2,knn_matchesA,2);
        matcher->knnMatch(descriptors_2,descriptors_1,knn_matchesB,2);

        for(uint i=0;i<knn_matchesA.size();i++){
            if(knn_matchesA[i].size()>1){
                assert(knn_matchesA[i][0].distance<=knn_matchesA[i][1].distance);
                if(knn_matchesA[i][0].distance/knn_matchesA[i][1].distance<0.8){
                    sym_matchesA.push_back(knn_matchesA[i][0]);
                }
            }
        }

        for(uint i=0;i<knn_matchesB.size();i++){
            if(knn_matchesB[i].size()>1){
                if(knn_matchesB[i][0].distance/knn_matchesB[i][1].distance<0.8){
                    sym_matchesB.push_back(knn_matchesB[i][0]);
                }
            }
        }

        for(uint i=0;i<sym_matchesA.size();i++){
            for(uint j=0;j<sym_matchesB.size();j++){
                if(sym_matchesA[i].queryIdx==sym_matchesB[j].trainIdx&&
                        sym_matchesA[i].trainIdx==sym_matchesB[j].queryIdx){
                    good_matches.push_back(sym_matchesA[i]);
                    break;
                }
            }
        }
    }

    static void getGoodMatches_Step4(vector<DMatch> &good_matches,const vector<Point2f> &ps1,const vector<Point2f> &ps2,const vector<Point3i> &colors1,const vector<Point3i> &colors2,int MaxColorMatchDistance=10,int MaxPosMatchDistance=100){
        assert(ps1.size()==colors1.size());
        assert(ps2.size()==colors2.size());
        for(auto it=good_matches.begin();it!=good_matches.end();it++){
            int qi=it->queryIdx;
            int ti=it->trainIdx;
            float dis=cv::norm(colors1[qi]-colors2[ti]);
            if(dis>MaxColorMatchDistance){
                auto pre=std::prev(it,1);
                good_matches.erase(it);
                it=pre;
            }
            else{
                ///avoid repeat erase.
                float positionDist=cv::norm(ps1[qi]-ps2[ti]);
                if(positionDist>MaxPosMatchDistance){
                    auto pre=std::prev(it,1);
                    good_matches.erase(it);
                    it=pre;
                }
            }
        }

        //TODO we can add distance threshold for match here.
    }

    void getGoodMatches(const cv::Mat &img1,const cv::Mat &mask1,const cv::Mat &img2,const cv::Mat &mask2){
        std::cout<<"image type: "<<getImgType(mask1.type())<<std::endl;
        std::cout<<"image type: "<<getImgType(mask2.type())<<std::endl;
        assert(img1.channels()==3&&img2.channels()==3);
        assert(mask1.type()==CV_8UC1&&mask2.type()==CV_8UC1);

        vector<KeyPoint> keypoints_1,keypoints_2;
        getFeaturePoint_Step1(img1,mask1,keypoints_1);
        getFeaturePoint_Step1(img2,mask2,keypoints_2);
        if(keypoints_1.empty()||keypoints_2.empty()){
            return;
        }

        //        cv::initModule_contrib();
        //        cv::initModule_features2d();
        Mat descriptors_1,descriptors_2;
        getDescriptorMat_Step2(img1,keypoints_1,descriptors_1);
        getDescriptorMat_Step2(img2,keypoints_2,descriptors_2);

        //img1-->img2
        vector<DMatch> good_matches;
        getGoodMatches_Step3(descriptors_1, descriptors_2, good_matches);

        Mat img_matches;
        drawMatches( img1, keypoints_1, img2, keypoints_2,
                     good_matches, img_matches);
        //-- Show detected matches
        imshow( "Good Matches", img_matches);

        //-- set global matches
        std::swap(global_keypoints_1,keypoints_1);
        std::swap(global_keypoints_2,keypoints_2);
        std::swap(global_good_matches,good_matches);
    }

    void getLIFMat(cv::Mat &LIFMat,std::vector<Point_t> &LIFPos,const cv::Mat &img1,const cv::Mat &mask1){
        vector<KeyPoint> keypoints_1;
        getFeaturePoint_Step1(img1,mask1,keypoints_1);

        if(keypoints_1.empty()){
            return;
        }

        if(!LIFMat.empty()) LIFMat.release();
        getDescriptorMat_Step2(img1,keypoints_1,LIFMat);
        if(!LIFPos.empty()) LIFPos.clear();
        for(uint i=0;i<keypoints_1.size();i++){
            LIFPos.push_back(keypoints_1[i].pt);
        }
        assert((int)LIFPos.size()==LIFMat.rows);
    }

    double global_match_ratio=0.8;
    vector<KeyPoint> global_keypoints_1,global_keypoints_2;
    vector<DMatch> global_good_matches;
};

#endif // OBJECTLOCALFEATUREMATCH_H
