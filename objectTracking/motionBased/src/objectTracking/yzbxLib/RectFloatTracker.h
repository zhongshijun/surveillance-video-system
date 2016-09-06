#ifndef RECTFLOATTRACKER_H
#define RECTFLOATTRACKER_H
#include "HungarianBasedTracking.h"
#include "ObjectLocalFeatureMatch.h"
#include <boost/lexical_cast.hpp>

class RectFloatTracker : public HungarianBasedTracking
{
public:
    enum CostType{
        RectDist,
        LIFDist
    };

    RectFloatTracker();
    void process(const cv::Mat &img_input,const cv::Mat &img_fg,vector<trackingObjectFeature> &fv);
    void tracking(const cv::Mat &img_input,const cv::Mat &img_fg);
private:
    int globalChannel;
    std::list<std::pair<cv::Mat,cv::Mat>> imageList;
    std::list<std::vector<trackingObjectFeature>> featureVectorList;
    int maxListLength;

    assignments_t getHungarainAssignment(vector<trackingObjectFeature> &fv);
    void runInSingleThread();
    void run();
    void doAssignment(assignments_t assignment, vector<trackingObjectFeature> &fv, Mat assignmentMat);
    void outputAndRemove(uint index);
    bool isRectAInRectB(Rect_t A, Rect_t B);
    void showing(const cv::Mat &img_input, const cv::Mat &img_fg, std::vector<trackingObjectFeature> featureVector);

    //return the match feature number for objects-blobs
    void getLocalFeatureAssignment(cv::Mat &matchMat);
    track_t calcPathWeight(std::shared_ptr<trackingObjectFeature> of1, std::shared_ptr<trackingObjectFeature> of2, ObjectLocalFeatureMatch &matcher);
    bool isPointInRect(cv::Point2f p, Rect_t rect);
    void showAssignment(assignments_t &assignments, std::vector<trackingObjectFeature> &fv);
    track_t calcPathWeight(std::shared_ptr<trackingObjectFeature> of1, std::shared_ptr<trackingObjectFeature> of2);
    void getHungarainAssignment(assignments_t &assignment, int costType=RectDist);
    track_t calcCost(std::shared_ptr<trackingObjectFeature> of1, std::shared_ptr<trackingObjectFeature> of2, int costType);


    //get from UrbanTracker, temporary status
    //std::map<tracksIdx,std::set<featureVectorIdx>>
    std::map<int, std::set<int>> ObjectToBlob;
    std::map<int, std::set<int>> BlobToObject;
    std::set<int> matchedBlob;
    std::set<int> matchedObject;

    std::set<int> mNewBlobs;	 // 0-1
    std::set<int> mUnmatchObjects; // 1-0
    std::set<std::pair<int, int>> mOneToOne; //1-1
    std::set<std::pair<int, std::set<int>>> mOneToN; //1-N
    //std::set<std::pair<std::set<trackIdx>,fvIdx>>
    std::set<std::pair<std::set<int>, int>> mNToOne; //N-1

    ///use mNewblobs, mUnmatchedObjects, mOneToOne, ... to do assignment
    void doAssignment();
    void handleOneToOneObjects();

    void handleNewObjects();
    void handleMissedObjects();
    //NOTE split first, then merge!!!
    void handleOneToNObjects();
    //NOTE merge later, split first!!!
    void handleNToOneObjects();

    //split provocation
    const int ObjectSplitPatience=3;
    const int objectMergePatience=10;
    const track_t MinSplitGap=100;
    const track_t MaxDistForMergingTrace=100.0;

    ///Long history status, when detete objects, merge objects, split objects, we need update them!!!
    //NOTE must use objectId here, because trackId will be invalid when remove some object
    //std::map<objectId,std::pari<provocation,displacement>>
    std::map<int,std::pair<int,Point_t>> objectSplitProvocation;
    //std::set<objectId>
    std::set<int> deleteLaterObjects;
    //std::map<objectIdA,std::map<objectIdB,mergedTimes>>, objectIdA < objectIdB
    std::map<int,std::map<int,int>> objectMergeProvocation;
    bool isMergedTrace(vector<Point_t> &traceA, vector<Point_t> &traceB);
    void getUnmatchedHungarainAssignment(cv::Mat matchMat);
    track_t getRectGap(Rect_t ra, Rect_t rb);
};

#endif // RECTFLOATTRACKER_H
