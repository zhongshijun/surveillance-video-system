#ifndef DATADRIVEFUNCTION002_H
#define DATADRIVEFUNCTION002_H
#include "DataDriveFunctions.h"

namespace DataDrive {
//class Base
//{
//public:
//    GET_CLASS_NAME
//    Base(std::shared_ptr<DataDriveMain> data){
//        this->data=data;
//    }

//    virtual bool run(){
//        assert(false);
//        return true;
//    }

//    virtual ~Base(){}
//    std::shared_ptr<DataDriveMain> data;
//};

class BlobToBlobAssignment_KLT:public Base{
public:
    string getClassName(){return "BlobToBlobAssignment_KLT";}
    BlobToBlobAssignment_KLT(std::shared_ptr<DataDriveMain> data):Base(data){}
    bool run();
    void loadConfig()
    {
        if(!configLoaded){
            configLoaded=true;
            MinMatchThreshold=data->param.MatchNumThreshold;
        }
    }
    void dump();

private:
    unsigned int MinMatchThreshold;
    bool configLoaded=false;
    void matToAssignment(const Mat &matchMat);
};

class BlobToBlobAssignment_Hungarian:public Base{
public:
    string getClassName(){return "BlobToBlobAssignment_Hungarian";}
    BlobToBlobAssignment_Hungarian(std::shared_ptr<DataDriveMain> data):Base(data){}
    bool run();
    void loadConfig()
    {
        if(!configLoaded){
            configLoaded=true;
            MinUnMatchDistanceThreshold=data->param.dist_thres;
        }
    }
    void dump();

private:
    unsigned int MinUnMatchDistanceThreshold;
    bool configLoaded=false;
};


class BlobToBlobAssignment_SplitMerge:public Base{
public:
    string getClassName(){return "BlobToBlobAssignment_SplitMerge";}
    BlobToBlobAssignment_SplitMerge(std::shared_ptr<DataDriveMain> data):Base(data),t2b("track","blob"){}
    bool run();
    void loadConfig()
    {
        if(!configLoaded){
            configLoaded=true;
            weight_hog=data->param.whog;
            weight_color=data->param.wcolor;
            threshold=data->param.rr_threshold;
        }
    }
    void dump();

private:
    bool configLoaded=false;
    double weight_hog,weight_color;
    double threshold;
    std::map<Index_t,std::set<Id_t>> prevB2T;
    std::map<Index_t,std::set<Id_t>> currB2T;
    std::set<Id_t> deleteTSet;
    AssignmentVecSetMap t2b;

    void B2BDiscuss();
    void B2BNToOne();
    void B2BNToN();
    void B2BOneToN();
    void B2BOneToOne();
    void B2BOneToZero();
    void B2BZeroToOne();

    void ReHungarian(const std::set<Index_t> trackSet, const std::set<Index_t> &currSet, bool isB2BOneToN=false);
    double getDistCost(Index_t trackIdx,Index_t currIdx);
    bool needReDetection(Index_t trackIdx);
    bool needRemove(Index_t trackIdx);

//    void T2BOneToN();
    void T2BNToOne(const std::set<Index_t> &trackSet,Index_t currIdx);
    void T2BOneToOne(Index_t trackIdx,Index_t currIdx);
    //please do redetection here!
//    void T2BOneToZero(Index_t trackIdx);
    void T2BOneToZero_Append(Index_t trackIdx);
//    void T2BZeroToOne(Index_t currIdx);
    void T2BZeroToOne_Append(Index_t currIdx);
    void T2BReDetection();

    void dumpBlobToTrack(const std::map<Index_t,std::set<Id_t>> &b2t);
    void gatherMissedTrackSet();
    Index_t id2index(Id_t id);
    void id2index(const std::set<Id_t> &idset, std::set<Index_t> &idxset);
    void update();
    void dumpToTxt();
    bool check();
};

class TrackingDump:public Base{
public:
    string getClassName(){return "TrackingDump";}
    TrackingDump(std::shared_ptr<DataDriveMain> data):Base(data){}
    bool run();
};


}

#endif // DATADRIVEFUNCTION002_H
