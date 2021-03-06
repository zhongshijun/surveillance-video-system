#include "DataDriveFunctions.h"

namespace DataDrive {

bool Input::run()
{
    data->frameInput.getNextFrame(data->videoFilePath,data->img_input);
    data->frameNum++;

    if(data->img_input.empty()) return false;
    else {
        Size size=data->img_input.size();
        int newW=480;
        int newH=round((double)newW*size.height/(double)size.width);
        if(data->param.ReSize&&newW<size.width){
            resize(data->img_input,data->img_input,Size(newW,newH));
        }
        return true;
    }
}

bool Bgs::run()
{
    assert(!data->img_input.empty());
    if(!data->img_fg.empty()) data->img_fg.release();

    if(data->frameNum % 50 ==0) qDebug()<<"frameNum="<<data->frameNum;
    cv::Mat img_fg;
    data->bgs->process(data->img_input,img_fg,data->img_background);
    if(img_fg.empty()){
        return false;
    }
    else{
        if(img_fg.channels()==3){
            cv::cvtColor(img_fg,data->img_fg,CV_BGR2GRAY);
        }
        else{
            data->img_fg=img_fg.clone();
        }

        img_fg.release();
        return true;
    }
}

bool BlobFeature::run()
{
    assert(!data->img_fg.empty());
    assert(!data->img_input.empty());
    assert(data->fvlist.size()==data->imglist.size());
    vector<trackingObjectFeature> fv;
    data->blobFeatureDetector.getBlobFeature(data->img_input,data->img_fg,fv);
    data->fv=fv;
    data->fvlist.push_back(fv);
    data->imglist.push_back(std::make_pair(data->img_input,data->img_fg));

    if(data->fvlist.size()>data->MaxListLength){
        data->fvlist.pop_front();
        data->imglist.pop_front();
    }
    return true;
}

bool Tracker::run()
{
    return true;
}

bool KLTTracker::run()
{

    if(data->fvlist.size()<2){
        return false;
    }

    assert(data->img_input.channels()==3);
    auto it=std::prev(data->imglist.end(),2);
    cv::Mat gray,prev_gray;
    cv::cvtColor(data->img_input,gray,CV_BGR2GRAY);
    cv::cvtColor(it->first,prev_gray,CV_BGR2GRAY);

    TermCriteria termcrit(CV_TERMCRIT_ITER|CV_TERMCRIT_EPS, 20, 0.03);
    Size subPixWinSize(10,10), winSize(31,31);
    const int MAX_COUNT = 500;
    vector<Point_KLT> points[2];

    //    goodFeaturesToTrack(gray, points[1], MAX_COUNT, 0.01, 10, Mat(), 3, 0, 0.04);
    //    cornerSubPix(gray, points[1], subPixWinSize, Size(-1,-1), termcrit);
    goodFeaturesToTrack(prev_gray, points[0], MAX_COUNT, 0.01, 10, Mat(), 3, 0, 0.04);
    cornerSubPix(prev_gray, points[0], subPixWinSize, Size(-1,-1), termcrit);

    vector<uchar> status;
    vector<float> err;
    calcOpticalFlowPyrLK(prev_gray, gray, points[0], points[1], status, err, winSize,
            3, termcrit, 0, 0.001);

    assert(points[0].size()==points[1].size());
    size_t i, k;

    Mat image;
    data->img_input.copyTo(image);
    for( i = k = 0; i < points[1].size(); i++ )
    {
        circle( image, points[1][i], 3, Scalar(255,0,0), -1, 8);
        if( !status[i] )
            continue;

        points[0][k]=points[0][i];
        points[1][k++] = points[1][i];
        circle( image, points[1][i], 3, Scalar(0,255,0), -1, 8);
        line(image,points[0][i],points[1][i],Scalar(0,0,255),3,8);
    }
    points[1].resize(k);

    cv::namedWindow("KLT",WINDOW_NORMAL);
    imshow("KLT",image);

    waitKey(30);
    return true;
}

bool MeanShiftTracker::run()
{
    static vector<trackingObjectFeature> fv;
    static cv::Mat choosedImage;
    static int key;
    if(data->fvlist.size()<2){
        return false;
    }
    assert(data->img_input.channels()==3);
    if(key=='n'||fv.empty()){
        fv=data->fvlist.back();
        choosedImage=data->img_input.clone();
        key='0';
    }
    Mat image=data->img_input.clone();

    Mat hsv,hue,hist,backproj,mask;

    int _vmin = 0, _vmax = 255,smin=0;
    float hranges[] = {0,180};
    const float* phranges = hranges;
    int hsize = 16;
    int ch[] = {0, 0};
    cvtColor(image, hsv, COLOR_BGR2HSV);
    inRange(hsv, Scalar(0, smin, MIN(_vmin,_vmax)),
            Scalar(180, 256, MAX(_vmin, _vmax)), mask);
    hue.create(hsv.size(), hsv.depth());
    mixChannels(&hsv, 1, &hue, 1, ch, 1);

    cv::Mat local_hsv,local_hue,local_mask,local_hist;
    cvtColor(choosedImage, local_hsv, COLOR_BGR2HSV);
    inRange(local_hsv, Scalar(0, smin, MIN(_vmin,_vmax)),
            Scalar(180, 256, MAX(_vmin, _vmax)), local_mask);
    local_hue.create(local_hsv.size(), local_hsv.depth());
    mixChannels(&local_hsv, 1, &local_hue, 1, ch, 1);

    for(uint i=0;i<fv.size();i++){
        Rect_t rect=fv[i].rect;
        Rect trackWindow=Rect((int)rect.x,(int)rect.y,(int)rect.width,(int)rect.height);
        if(key=='n'||fv.empty()){

            Mat roi(local_hue, trackWindow), maskroi(local_mask, trackWindow);
            calcHist(&roi, 1, 0, maskroi, local_hist, 1, &hsize, &phranges);
            normalize(local_hist, local_hist, 0, 255, CV_MINMAX);

            calcBackProject(&hue, 1, 0, local_hist, backproj, &phranges);
            backproj &= local_mask;

        }
        else{
            Mat roi(hue, trackWindow), maskroi(mask, trackWindow);
            calcHist(&roi, 1, 0, maskroi, hist, 1, &hsize, &phranges);
            normalize(hist, hist, 0, 255, CV_MINMAX);

            calcBackProject(&hue, 1, 0, hist, backproj, &phranges);
            backproj &= mask;
        }

        meanShift(backproj, trackWindow,
                  TermCriteria( CV_TERMCRIT_EPS | CV_TERMCRIT_ITER, 10, 1 ));
        rectangle(image, trackWindow, Scalar(0,0,255), 3, CV_AA );
    }

    cv::namedWindow("CamShift",WINDOW_NORMAL);
    imshow("CamShift",image);

    key=waitKey(30);
    return true;
}

bool CamShiftTracker::run()
{
    static vector<trackingObjectFeature> fv;
    static cv::Mat choosedImage;
    static int key;
    if(data->fvlist.size()<2){
        return false;
    }
    assert(data->img_input.channels()==3);
    if(key=='n'||fv.empty()){
        fv=data->fvlist.back();
        choosedImage=data->img_input.clone();
        key='0';
    }
    Mat image=data->img_input.clone();

    Mat hsv,hue,hist,backproj,mask;

    int _vmin = 0, _vmax = 255,smin=0;
    float hranges[] = {0,180};
    const float* phranges = hranges;
    int hsize = 16;
    int ch[] = {0, 0};
    cvtColor(image, hsv, COLOR_BGR2HSV);
    inRange(hsv, Scalar(0, smin, MIN(_vmin,_vmax)),
            Scalar(180, 256, MAX(_vmin, _vmax)), mask);
    hue.create(hsv.size(), hsv.depth());
    mixChannels(&hsv, 1, &hue, 1, ch, 1);

    cv::Mat local_hsv,local_hue,local_mask,local_hist;
    cvtColor(choosedImage, local_hsv, COLOR_BGR2HSV);
    inRange(local_hsv, Scalar(0, smin, MIN(_vmin,_vmax)),
            Scalar(180, 256, MAX(_vmin, _vmax)), local_mask);
    local_hue.create(local_hsv.size(), local_hsv.depth());
    mixChannels(&local_hsv, 1, &local_hue, 1, ch, 1);

    for(uint i=0;i<fv.size();i++){
        Rect_t rect=fv[i].rect;
        Rect trackWindow=Rect((int)rect.x,(int)rect.y,(int)rect.width,(int)rect.height);
        RotatedRect trackBox;
        if(key=='n'||fv.empty()){

            Mat roi(local_hue, trackWindow), maskroi(local_mask, trackWindow);
            calcHist(&roi, 1, 0, maskroi, local_hist, 1, &hsize, &phranges);
            normalize(local_hist, local_hist, 0, 255, CV_MINMAX);

            calcBackProject(&hue, 1, 0, local_hist, backproj, &phranges);
            backproj &= local_mask;

        }
        else{
            Mat roi(hue, trackWindow), maskroi(mask, trackWindow);
            calcHist(&roi, 1, 0, maskroi, hist, 1, &hsize, &phranges);
            normalize(hist, hist, 0, 255, CV_MINMAX);

            calcBackProject(&hue, 1, 0, hist, backproj, &phranges);
            backproj &= mask;
        }

        trackBox = CamShift(backproj, trackWindow,
                            TermCriteria( CV_TERMCRIT_EPS | CV_TERMCRIT_ITER, 10, 1 ));
        ellipse( image, trackBox, Scalar(0,0,255), 3, CV_AA );
    }

    cv::namedWindow("CamShift",WINDOW_NORMAL);
    imshow("CamShift",image);

    key=waitKey(30);
    return true;
}

bool KLTAssignment::run()
{
    if(data->fvlist.size()<2){
        //        assert(false);
        return false;
    }

    if(!data->prevBlobToTrack.empty())data->prevBlobToTrack.clear();
    if(!data->trackToPrevBlob.empty())data->trackToPrevBlob.clear();
    if(!data->KLTMatchMat.empty())data->KLTMatchMat.release();
    if(!data->blobToTrackSet.empty())data->blobToTrackSet.clear();
    if(!data->matchedBlobSet.empty())data->matchedBlobSet.clear();
    if(!data->matchedTrackSet.empty())data->matchedTrackSet.clear();
    if(!data->trackToBlobSet.empty())data->trackToBlobSet.clear();


    vector<trackingObjectFeature> &fv=data->fvlist.back();
    vector<trackingObjectFeature> prev_fv;

    //associate prevBlob to track
    {
        std::map<Index_t,Index_t> prevBlobToTrack;
        std::map<Index_t,Index_t> trackToPrevBlob;
        for(uint i=0;i<data->tracks.size();i++){
            //KLT will use previous image and current image.
            if(data->tracks[i]->status!=MISSING_STATUS){
                prev_fv.push_back(*(data->tracks[i]->feature));
                prevBlobToTrack[prev_fv.size()-1]=i;
                trackToPrevBlob[i]=prev_fv.size()-1;
            }
        }
        data->prevBlobToTrack.swap(prevBlobToTrack);
        data->trackToPrevBlob.swap(trackToPrevBlob);
    }

    if(fv.empty()||prev_fv.empty())
    {
        data->prevBlobToTrack.clear();
        data->trackToPrevBlob.clear();
        if(!data->KLTMatchMat.empty()) data->KLTMatchMat.release();

        return true;
    }

    assert(!fv.empty()&&!prev_fv.empty());
    assert(data->img_input.channels()==3);


    auto it=std::prev(data->imglist.end(),2);
    cv::Mat gray,prev_gray;
    cv::cvtColor(data->img_input,gray,CV_BGR2GRAY);
    cv::cvtColor(it->first,prev_gray,CV_BGR2GRAY);

    TermCriteria termcrit(CV_TERMCRIT_ITER|CV_TERMCRIT_EPS, 20, 0.03);
    Size subPixWinSize(10,10), winSize(31,31);
    const int MAX_COUNT = 500;

    for(uint i=0;i<2;i++){
        if(!data->points[i].empty()) data->points[i].clear();
        if(!data->pointIdxToBlobSet[i].empty()) data->pointIdxToBlobSet[i].clear();
        if(!data->blobIdxToPointSet[i].empty()) data->blobIdxToPointSet[i].clear();
    }
    vector<Point_KLT> points[2];
    map<Index_t,set<Index_t>> pointIdxToBlobSet[2];
    map<Index_t,set<Index_t>> blobIdxToPointSet[2];
    //    map<Index_t,Index_t> kltPointMatch;
    //    if(!data->kltPointMatch.empty()) data->kltPointMatch.clear();
    //    data->kltPointMatch &=kltPointMatch;


    goodFeaturesToTrack(prev_gray, points[0], MAX_COUNT, 0.01, 10, Mat(), 3, 0, 0.04);
    cornerSubPix(prev_gray, points[0], subPixWinSize, Size(-1,-1), termcrit);

    vector<uchar> status;
    vector<float> err;
    calcOpticalFlowPyrLK(prev_gray, gray, points[0], points[1], status, err, winSize,
            3, termcrit, 0, 0.001);

    assert(points[0].size()==points[1].size());
    size_t i, k;

    Mat image;
    data->img_input.copyTo(image);
    auto imgIt=std::prev(data->imglist.end(),2);
    Mat prev_image=imgIt->first;
    Mat mergeImage;
    cv::hconcat(prev_image,image,mergeImage);

    // for matchNum > 255
    cv::Mat matchMat(prev_fv.size(),fv.size(),CV_8UC1);
    matchMat=Scalar::all(0);

    for( i = k = 0; i < points[1].size(); i++ )
    {
        circle( image, points[1][i], 3, Scalar(255,0,0), -1, 8);
        if( !status[i] )
            continue;

        points[0][k]=points[0][i];
        points[1][k++] = points[1][i];
        circle(image,points[1][i], 3, Scalar(0,255,0), -1, 8);
        line(image,points[0][i],points[1][i],Scalar(0,0,255),3,8);
        Point2i p=points[1][i];
        Point2i prev_p=points[0][i];
        int matchMat_x=-1,matchMat_y=-1;
        for(uint i=0;i<prev_fv.size();i++){
            cv::Mat mask=prev_fv[i].mask;

            assert(!mask.empty());
            if(mask.at<uchar>(prev_p.y,prev_p.x)==255){
                matchMat_y=i;
                break;
            }
        }
        for(uint i=0;i<fv.size();i++){
            cv::Mat mask=fv[i].mask;

            assert(!mask.empty());
            if(mask.at<uchar>(p.y,p.x)==255){
                matchMat_x=i;
                break;
            }
        }

        if(matchMat_x==-1||matchMat_y==-1){
            //out of mask
        }
        else{
            Index_t pointIdx=i;
            Index_t prev_blobIdx=matchMat_y;
            Index_t blobIdx=matchMat_x;
            Index_t trackIdx=data->prevBlobToTrack[prev_blobIdx];
            int lifetime=data->tracks[trackIdx]->get_lifetime();
            if(lifetime<data->param.MaxFreshObjectLifeTime)
                continue;

            matchMat.at<uchar>(matchMat_y,matchMat_x)+=((uchar)1);
            pointIdxToBlobSet[0][pointIdx].insert(prev_blobIdx);
            pointIdxToBlobSet[1][pointIdx].insert(blobIdx);
            blobIdxToPointSet[0][prev_blobIdx].insert(pointIdx);
            blobIdxToPointSet[1][blobIdx].insert(pointIdx);

            yzbxlib::drawMatch(mergeImage,points[0][i],points[1][i],Scalar(0,0,255),2);
        }
    }
    data->KLTMatchMat=matchMat;
//    yzbxlib::showImageInWindow("matchMat",matchMat);
    //update blob track assignment
    {
        std::map<Index_t,std::set<Index_t>> &blobToTrackSet=data->blobToTrackSet;
        std::map<Index_t,std::set<Index_t>> &trackToBlobSet=data->trackToBlobSet;
        std::set<Index_t> &matchedBlobSet=data->matchedBlobSet;
        std::set<Index_t> &matchedTrackSet=data->matchedTrackSet;

        int m=matchMat.rows;
        int n=matchMat.cols;
        const uint MatchNumThreshold=data->param.MatchNumThreshold;
        for(uint i=0;i<m;i++){
            for(uint j=0;j<n;j++){
                if(matchMat.at<uchar>(i,j)>MatchNumThreshold){
                    int blobIdx=j;
                    int objIdx=data->prevBlobToTrack[i];
                    blobToTrackSet[blobIdx].insert(objIdx);
                    trackToBlobSet[objIdx].insert(blobIdx);
                    matchedBlobSet.insert(blobIdx);
                    matchedTrackSet.insert(objIdx);
                }
            }
        }
    }
    points[0].resize(k);
    points[1].resize(k);

    for(uint i=0;i<2;i++){
        data->points[i] =points[i];
        data->pointIdxToBlobSet[i] =pointIdxToBlobSet[i];
        data->blobIdxToPointSet[i] =blobIdxToPointSet[i];
    }

    cv::namedWindow("KLT",WINDOW_NORMAL);
    imshow("KLT",image);

    cv::namedWindow("merged KLT",WINDOW_NORMAL);
    imshow("merged KLT",mergeImage);

    dump();
    waitKey(30);

    //    gray.release();
    //    prev_gray.release();
    //    image.release();
    return true;
}

void KLTAssignment::dump()
{
    //output assignment result
    vector<trackingObjectFeature> &fv=data->fvlist.back();
    auto imgIt=std::prev(data->imglist.end(),1);
    auto prev_imgIt=std::prev(data->imglist.end(),2);

    Mat input,mask,prev_input,prev_mask;
    input=imgIt->first.clone();
    mask=imgIt->second.clone();
    prev_input=prev_imgIt->first.clone();
    prev_mask=prev_imgIt->second.clone();
    //blob
    for(uint i=0;i<fv.size();i++){
        Rect_t r=fv[i].rect;
        string idxstr=boost::lexical_cast<string>(i);
        //        string title="index="+idxstr;
        string title=idxstr;
        yzbxlib::annotation(input,r,title);
        rectangle(input,r,Scalar(0,0,255),3,8);
        rectangle(mask,r,Scalar::all(255),3,8);
    }

    //object
    for(uint i=0;i<data->tracks.size();i++){
        Rect_t r=data->tracks[i]->feature->rect;
        Index_t trackIdx=data->tracks[i]->track_id;
        string idxstr=boost::lexical_cast<string>(trackIdx);
        string title="id="+idxstr;
        yzbxlib::annotation(prev_input,r,title);
        rectangle(prev_input,r,Scalar(0,0,255),3,8);
        rectangle(prev_mask,r,Scalar::all(255),3,8);
    }

    Mat blob,object;
    cvtColor(mask,mask,CV_GRAY2BGR);
    hconcat(input,mask,blob);
    cvtColor(prev_mask,prev_mask,CV_GRAY2BGR);
    hconcat(prev_input,prev_mask,object);

    Mat klt;
    vconcat(object,blob,klt);
    namedWindow("KLT dump",WINDOW_NORMAL);
    imshow("KLT dump",klt);

    //    qDebug()<<data->frameNum;
    //    //assignment
    //    for(auto it=data->blobToTrackSet.begin();it!=data->blobToTrackSet.end();it++){
    //        Index_t blobIdx=it->first;
    //        set<Index_t> &trackset=it->second;
    //        cout<<"blobIdx="<<blobIdx<<" match :";
    //        for(auto t=trackset.begin();t!=trackset.end();t++){
    //            Index_t trackIdx=*t;
    //            Id_t trackId=data->tracks[trackIdx]->track_id;
    //            cout<<trackId<<",";
    //        }
    //        cout<<endl;
    //    }

    //    for(auto it=data->trackToBlobSet.begin();it!=data->trackToBlobSet.end();it++){
    //        Index_t trackIdx=it->first;
    //        Id_t trackId=data->tracks[trackIdx]->track_id;
    //        set<Index_t> &blobset=it->second;
    //        cout<<"trackIdx="<<trackId<<" match :";
    //        for(auto b=blobset.begin();b!=blobset.end();b++){
    //            Index_t blobIdx=*b;
    //            cout<<blobIdx<<",";
    //        }
    //        cout<<endl;
    //    }

}

bool FilterBadTrack::run()
{
    for(int i=data->tracks.size()-1;i>=0;i--){
//        data->tracks[i]->dumpToScreen();

        if(data->tracks[i]->status==MISSING_STATUS){
            Point_t p1=data->tracks[i]->firstSeePos;
            Point_t p2=data->tracks[i]->lastSeePos;

            if(cv::norm(p1-p2)<20){
                qDebug()<<"delete bad track: "<<data->tracks[i]->track_id;
                data->tracks.erase(data->tracks.begin()+i);
                continue;
            }

            if(data->tracks[i]->feature->onBoundary){
                qDebug()<<"delete bad track: "<<data->tracks[i]->track_id;
                data->tracks.erase(data->tracks.begin()+i);
                continue;
            }
        }
    }
    return true;
}

bool SplitAndMerge::run()
{
    if(data->fvlist.size()<2){
        assert(false);
    }
    vector<trackingObjectFeature> &fv=data->fvlist.back();

    bool emptyBlobs=fv.empty();
    bool emptyObjects=data->tracks.empty();

    if(!data->mOneToZeroSet.empty())data->mOneToZeroSet.clear();
    if(!data->mZeroToOneSet.empty())data->mZeroToOneSet.clear();
    if(!data->mOneToNMap.empty())data->mOneToNMap.clear();
    if(!data->mOneToOneMap.empty())data->mOneToOneMap.clear();
    if(!data->mNToOneMap.empty())data->mNToOneMap.clear();

    if(emptyBlobs&&emptyObjects){
        //        qDebug()<<"empty Blobs and empty objects";
    }
    else if(emptyBlobs){
        //handle missed object
        for(uint i=0;i<data->tracks.size();i++){
            data->mOneToZeroSet.insert(i);
        }
        handleMissedObjects();
    }
    else if(emptyObjects){
        //handle new object
        for(uint i=0;i<fv.size();i++){
            data->mZeroToOneSet.insert(i);
        }
        handleNewObjects();
    }
    else{
        ///update mOneToOneMap mNToOneMap mOneToNMap
        auto &mObjectToBlobMap=data->trackToBlobSet;
        auto &mBlobToObjectMap=data->blobToTrackSet;

        ///remove M-N, do 0-1, 1-0, 1-1, 1-N, N-1
        bool firstTime=true;
        for(auto ia=mObjectToBlobMap.begin();ia!=mObjectToBlobMap.end();ia++){
            Index_t trackIdx=ia->first;
            if(ia->second.size()>1){
                set<Index_t> &blobset=ia->second;
                for(auto ib=blobset.begin();ib!=blobset.end();){
                    Index_t blobIdx=*ib;
                    if(mBlobToObjectMap[blobIdx].size()>1){
                        if(firstTime){
                            qDebug()<<"before remove M-N ...........................";
                            cout<<"object -> blobset"<<endl;
                            yzbxlib::dumpMap(mObjectToBlobMap);
                            cout<<"blob -> objectset"<<endl;
                            yzbxlib::dumpMap(mBlobToObjectMap);
                            firstTime=false;
                        }

                        qDebug()<<"remove M-N: "<<trackIdx<<","<<blobIdx;

                        auto it=mBlobToObjectMap[blobIdx].find(trackIdx);
                        mBlobToObjectMap[blobIdx].erase(it);

                        ib=blobset.erase(ib);
                    }
                    else{
                        ib++;
                    }
                }
            }
        }

        if(firstTime==false){
            qDebug()<<"after remove M-N .............................";
            cout<<"object -> blobset"<<endl;
            yzbxlib::dumpMap(mObjectToBlobMap);
            cout<<"blob -> objectset"<<endl;
            yzbxlib::dumpMap(mBlobToObjectMap);
        }
        for(auto ia=mObjectToBlobMap.begin();ia!=mObjectToBlobMap.end();ia++){
            //tracksIdx
            Index_t trackIdx=ia->first;
            if(ia->second.size()==1){
                Index_t fvIdx=*(ia->second.begin());
                if(mBlobToObjectMap[fvIdx].size()==1){
                    data->mOneToOneMap[trackIdx]=fvIdx;
                }
                else{
                    data->mNToOneMap[fvIdx]=mBlobToObjectMap[fvIdx];
                }
            }
            else if(ia->second.size()>1){
                data->mOneToNMap[trackIdx]=ia->second;

                std::set<Index_t> &blobset=ia->second;
                for(auto ib=blobset.begin();ib!=blobset.end();ib++){
                    Index_t fvIdx=*ib;
                    assert(mBlobToObjectMap[fvIdx].size()==1);
                }

            }
        }

        auto &mMatchedObjectSet=data->matchedTrackSet;
        ///handle missed object, not change tracks.size()
        for(uint i=0;i<data->tracks.size();i++){
            if(mMatchedObjectSet.find(i)==mMatchedObjectSet.end()){
                data->mOneToZeroSet.insert(i);
            }
        }

        bool output;

        auto &mMatchedBlobSet=data->matchedBlobSet;
        //handle new object, will change tracks.size()
        for(uint i=0;i<fv.size();i++){
            if(mMatchedBlobSet.find(i)==mMatchedBlobSet.end()){
                data->mZeroToOneSet.insert(i);
            }
        }

        output=!data->mOneToOneMap.empty();
        //handle one to one object
        //        qDebug()<<"before one to one handle";
        //        DataDrive::dumpObjects(data->tracks,output);
        handleOneToOneObjects();
        //        qDebug()<<"after one to one handle";
        //        DataDrive::dumpObjects(data->tracks,output);

        output=!data->mOneToNMap.empty();
        //handle split object
        //        qDebug()<<"before ont to N handle";
        //        DataDrive::dumpObjects(data->tracks,output);
        handleOneToNObjects();
        //        qDebug()<<"after one to N handle";
        //        DataDrive::dumpObjects(data->tracks,output);

        output=!data->mNToOneMap.empty();
        //handle merged object
        //        qDebug()<<"before N to one handle";
        //        DataDrive::dumpObjects(data->tracks,output);
        handleNToOneObjects();
        //        qDebug()<<"after N to one handle";
        //        DataDrive::dumpObjects(data->tracks,output);

        output=!data->mOneToZeroSet.empty();
        //        qDebug()<<"before miss handle";
        //        DataDrive::dumpObjects(data->tracks,output);
        handleMissedObjects();
        //        qDebug()<<"after miss handle";
        //        DataDrive::dumpObjects(data->tracks,output);

        output=!data->mZeroToOneSet.empty();
        //        qDebug()<<"before new handle";
        //        DataDrive::dumpObjects(data->tracks,output);
        handleNewObjects();
        //        qDebug()<<"after new handle";
        //        DataDrive::dumpObjects(data->tracks,output);
    }

    //    waitKey(0);
    return true;
}

void SplitAndMerge::handleOneToOneObjects()
{
    vector<trackingObjectFeature> &fv=data->fvlist.back();
    for(auto it=data->mOneToOneMap.begin();it!=data->mOneToOneMap.end();it++){
        int trackIdx=it->first;
        int fvIdx=it->second;
        data->tracks[trackIdx]->NormalUpdate(fv[fvIdx],data->frameNum,data->img_input);
    }
}

void SplitAndMerge::handleNewObjects()
{
    vector<trackingObjectFeature> &fv=data->fvlist.back();
    auto &param=data->param;
    for(auto newit=data->mZeroToOneSet.begin();newit!=data->mZeroToOneSet.end();){
        bool flag=redetection(*newit);
        if(flag){
            newit=data->mZeroToOneSet.erase(newit);
        }
        else{
            data->tracks.push_back(std::make_unique<singleObjectTracker>(fv[*newit],
                                   param.dt, param.Accel_noise_mag, data->NextTrackID++,data->frameNum,data->img_input.cols,data->img_input.rows));
            newit++;
        }
    }
}

void SplitAndMerge::handleMissedObjects()
{
    auto &mOneToZeroSet=data->mOneToZeroSet;
    auto &param=data->param;
    auto &deleteLaterObjectSet=data->deleteLaterObjectSet;
    for(auto missit=mOneToZeroSet.begin();missit!=mOneToZeroSet.end();missit++){
        assert(*missit<data->tracks.size());
        data->tracks[*missit]->MissUpdate(data->frameNum);
        if(data->tracks[*missit]->get_lifetime()<param.MaxFreshObjectLifeTime||
                data->tracks[*missit]->get_skipped_frames()>param.maximum_allowed_skipped_frames){
            deleteLaterObjectSet.insert(data->tracks[*missit]->track_id);
        }
    }
}

void SplitAndMerge::handleOneToNObjects()
{
    vector<trackingObjectFeature> &fv=data->fvlist.back();

    ///1. the blob is away from other blobs
    ///2. the blob is go from faraway to nearby, size from small to big!
    ///3. the blob split for many times when moving!

    auto &mOneToNMap=data->mOneToNMap;
    auto &param=data->param;

    for(auto it=mOneToNMap.begin();it!=mOneToNMap.end();it++){
        Index_t trackIdx=it->first;
        std::set<Index_t> &blobset=(it->second);
        /// empty set will be remove, size()==1 is error status!
        assert(blobset.size()>1);

        //assign blobs to missed object
        for(auto blob=blobset.begin();blob!=blobset.end();){
            Index_t blobIdx=*blob;

            //input: new blobIdx, miss trackIdx
            //output: find correspond track return true, else false
            //modify: global missed track set, new blob set
            bool flag=redetection(blobIdx);
            if(flag){
                blob=blobset.erase(blob);
            }
            else{
                blob++;
            }
        }

        //get max blob, assign it to origin objectIdx
        float maxArea=0;
        Index_t maxAreaBlobIdx;
        for(auto blob=blobset.begin();blob!=blobset.end();blob++){
            if(blob==blobset.begin()){
                maxArea=fv[*blob].size;
                maxAreaBlobIdx=*blob;
            }
            else{
                if(maxArea<fv[*blob].size){
                    maxArea=fv[*blob].size;
                    maxAreaBlobIdx=*blob;
                }
            }
        }
        //change blob feature
        data->tracks[trackIdx]->OneToNUpdateForBiggestBlob(fv[maxAreaBlobIdx],data->frameNum);

        for(auto blob=blobset.begin();blob!=blobset.end();blob++){
            if(*blob!=maxAreaBlobIdx){
                data->tracks.push_back(std::make_unique<singleObjectTracker>(fv[*blob],param.dt,
                                       param.Accel_noise_mag,data->NextTrackID++,data->frameNum,data->img_input.cols,data->img_input.rows));
                data->tracks.back()->OneToNUpdateForNewBlob();
            }
        }
    }
}

void SplitAndMerge::handleNToOneObjects()
{
    vector<trackingObjectFeature> &fv=data->fvlist.back();

    auto &mNToOneMap=data->mNToOneMap;

    bool splitBlob=false;
    for(auto it=mNToOneMap.begin();it!=mNToOneMap.end();it++){
        if(!splitBlob){
            qDebug()<<"dump rect before split blob";
            DataDrive::dumpRectInTracker(data->tracks);
            DataDrive::drawRectInTracker(data->tracks,data->img_input,"NToOne before");
            splitBlob=true;
        }
        Index_t blobIdx=it->first;
        std::set<Index_t> &trackset=it->second;

        Rect_t rb=fv[blobIdx].rect;
        bool firstTime=true;
        for(auto track=trackset.begin();track!=trackset.end();track++){
            int trackIdx=*track;

            trackingObjectFeature of;
            of.copy(*(data->tracks[trackIdx]->feature));
//            Rect_t r_before=of.rect;
            //            bool flag=AffineTransform(blobIdx,trackIdx,of);
            bool flag=false;

            //use KLT transform
            if(flag){
                data->tracks[*track]->NToOneUpdate(of,data->frameNum);

                if(firstTime){
                    fv[blobIdx]=of;
                    firstTime=false;
                }
                else{
                    fv.push_back(of);
                }
            }
            else{
                Point_t pos=data->tracks[trackIdx]->feature->pos;
                Rect_t rect=data->tracks[trackIdx]->feature->rect;
                Point_t predict_pos=data->tracks[*track]->KF.GetPrediction();
                Rect_t predict_rect=data->tracks[*track]->GetPredictRect();
                Rect_t subRect=yzbxlib::getSubRect(rb,predict_rect);
                //kalman speed v1
                Point_t speed=predict_pos-pos;
                //in rect adjust speed v2
                Point_t adjustSpeed=subRect.tl()-rect.tl();

                //v1 and v2 is close!!!
                double vec_norm=cv::norm(speed)*cv::norm(adjustSpeed);
                double cos_theta;
                if(vec_norm==0) cos_theta=0;
                else cos_theta=(speed.x*adjustSpeed.x+speed.y*adjustSpeed.y)/vec_norm;

                if(cos_theta>0.7){
                    of.rect=subRect;
                    of.pos+=(subRect.tl()-rect.tl());
                }
                else{
                    of.rect=predict_rect;
                    of.pos+=(predict_rect.tl()-rect.tl());
                }

                data->tracks[*track]->NToOneUpdate(of,data->frameNum);
                if(firstTime){
                    fv[blobIdx]=of;
                    firstTime=false;
                }
                else{
                    fv.push_back(of);
                }
                assert(data->tracks[*track]->feature->LIFMat.empty());
            }

        }
    }

    if(splitBlob){
        qDebug()<<"dump rect after split blob";
        DataDrive::dumpRectInTracker(data->tracks);
        DataDrive::drawRectInTracker(data->tracks,data->img_input,"NToOne after");
    }
}

bool SplitAndMerge::redetection(Index_t newBlobIdx)
{
    //check for boundary!!!
    vector<trackingObjectFeature> &fv=data->fvlist.back();
    if(fv[newBlobIdx].onBoundary) return false;

    set<Index_t> &missedTrackSet=data->mOneToZeroSet;
    cv::Mat newBlobImg=data->img_input(fv[newBlobIdx].rect);
    cv::Mat newBlobMask=fv[newBlobIdx].mask(fv[newBlobIdx].rect);
    if(newBlobImg.empty()||newBlobMask.empty()) return false;

    ReDetection RR;
    for(auto track=missedTrackSet.begin();track!=missedTrackSet.end();track++){
        Index_t trackIdx=*track;
        Rect_t missTrackRect=data->tracks[trackIdx]->rect_last_normal;
        cv::Mat missTrackImg=data->tracks[trackIdx]->img_last_normal(missTrackRect);
        cv::Mat missTrackMask=data->tracks[trackIdx]->mask_last_normal(missTrackRect);
        if(missTrackImg.empty()||missTrackMask.empty()) continue;

        cv::Mat histDistImg,histDistMask;
        cout<<"newBlobType="<<getImgType(newBlobImg.type())<<","<<getImgType(newBlobMask.type())<<endl;
        cout<<"missTrackType="<<getImgType(missTrackImg.type())<<","<<getImgType(missTrackMask.type())<<endl;
        yzbxlib::MyHconcat(newBlobImg,missTrackImg,histDistImg);
        yzbxlib::MyHconcat(newBlobMask,missTrackMask,histDistMask);
        cv::Mat histDistShow;
        cvtColor(histDistMask,histDistMask,CV_GRAY2BGR);
        yzbxlib::MyVconcat(histDistImg,histDistMask,histDistShow);
        yzbxlib::showImageInWindow("histDistShow",histDistShow);

        double histDist=RR.myCompareHist(newBlobImg,newBlobMask,missTrackImg,missTrackMask,3);
        qDebug()<<"histDist="<<histDist;

        //        double surfDist=RR.myCompareLocalFeature(newBlobImg,newBlobMask,missTrackImg,missTrackMask,3);
        double surfDist=RR.myCompareLocalFeature(newBlobImg,missTrackImg,300,0.3);
        QStringList outlist;
        outlist<<QString::number(data->frameNum)
              <<QString::number(trackIdx)
             <<QString::number(newBlobIdx)
            <<QString::number(histDist)
           <<QString::number(surfDist);
        qCritical()<<outlist.join('\t');
        string basename=data->getBaseFileName().toStdString();
        string saveImgName=basename+"_"+boost::lexical_cast<string>(data->frameNum)+
                "_"+boost::lexical_cast<string>((int)trackIdx)+
                "_"+boost::lexical_cast<string>((int)newBlobIdx)+".jpg";
        imwrite(saveImgName,histDistShow);

        if(histDist<0.5){
            //erase iter here!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
            missedTrackSet.erase(track);
            data->tracks[trackIdx]->NormalUpdate(fv[newBlobIdx],data->frameNum,data->img_input);
            return true;
        }
    }

    return false;
}

bool SplitAndMerge::AffineTransform(const Index_t blobIdx, const Index_t trackIdx, trackingObjectFeature &of)
{
    set<Index_t> blobSet[2];
    assert(data->trackToPrevBlob.find(trackIdx)!=data->trackToPrevBlob.end());

    int prev_blobIdx=data->trackToPrevBlob[trackIdx];
    blobSet[0] =data->blobIdxToPointSet[0][prev_blobIdx];
    blobSet[1] =data->blobIdxToPointSet[1][blobIdx];

    vector<Point_KLT> points[2];
    for(auto prevIt=blobSet[0].begin();prevIt!=blobSet[0].end();prevIt++){
        int prevPtIdx=*prevIt;
        for(auto it=blobSet[1].begin();it!=blobSet[1].end();it++){
            int ptIdx=*it;
            if(ptIdx==prevPtIdx){
                points[0].push_back(data->points[0][prevPtIdx]);
                points[1].push_back(data->points[1][ptIdx]);
            }
        }
    }

    if(points[0].size()<3||points[1].size()<3){
        return false;
    }

    int psize=points[0].size();
    Point_KLT dif_sum(0,0);
    for(uint i=0;i<psize;i++){
        Point_KLT dif=points[0][i]-points[1][i];
        dif_sum+=dif;
    }

    Point_KLT mean_dif(dif_sum.x/psize,dif_sum.y/psize);
    Rect_t r_before=of.rect;
    Rect_t r_after=of.rect;
    r_after.x+=mean_dif.x;
    r_after.y+=mean_dif.y;

    Mat before=data->img_input.clone();
    Mat after=data->img_input.clone();
    rectangle(before,r_before,Scalar(0,0,255),3);
    rectangle(after,r_after,Scalar(0,0,255),3);
    Mat beforeAfter;
    hconcat(before,after,beforeAfter);
    namedWindow("offset before and after",WINDOW_NORMAL);
    imshow("offset before and after",beforeAfter);
    cout<<r_before.x<<","<<r_before.y<<","<<r_before.width<<","<<r_before.height<<endl;
    cout<<r_after.x<<","<<r_after.y<<","<<r_after.width<<","<<r_after.height<<endl;

    //    cv::Mat T=cv::getAffineTransform(points[0],points[1]);
    cv::Mat T=cv::estimateRigidTransform(points[0],points[1],false);
    string type=getImgType(T.type());
    //    qDebug()<<"type="<<QString::fromStdString(type);
    assert(T.type()==CV_64F);
    cv::Mat pointMat(3,1,CV_64FC1);
    cv::Mat newPointMat;
    vector<Point_t> srcPt;
    vector<Point_t> desPt;
    srcPt.push_back(of.rect.tl());
    srcPt.push_back(of.rect.br());
    srcPt.push_back(of.pos);
    for(uint i=0;i<3;i++){
        pointMat.at<double>(0,0)=srcPt[i].x;
        pointMat.at<double>(1,0)=srcPt[i].y;
        pointMat.at<double>(2,0)=1.0f;
        newPointMat=T*pointMat;
        Point_t p;
        p.x=(float)newPointMat.at<double>(0,0);
        p.y=(float)newPointMat.at<double>(0,1);

        desPt.push_back(p);
    }
    of.rect=Rect_t(desPt[0],desPt[1]);
    of.pos=desPt[2];
    of.KLTPos.clear();
    of.KLTPos =points[1];
    return true;
}

void SplitAndMerge::dump()
{
    vector<trackingObjectFeature> &fv=data->fvlist.back();
    auto imgIt=std::prev(data->imglist.end(),1);
    auto prev_imgIt=std::prev(data->imglist.end(),2);

    Mat input,mask,prev_input,prev_mask;
    input=imgIt->first.clone();
    mask=imgIt->second.clone();
    prev_input=prev_imgIt->first.clone();
    prev_mask=prev_imgIt->second.clone();
    //blob
    for(uint i=0;i<fv.size();i++){
        Rect_t r=fv[i].rect;
        string idxstr=boost::lexical_cast<string>(i);
        //        string title="index="+idxstr;
        string title=idxstr;
        yzbxlib::annotation(input,r,title);
        rectangle(input,r,Scalar(0,0,255),3,8);
        rectangle(mask,r,Scalar::all(255),3,8);
    }

    //object
    for(uint i=0;i<data->tracks.size();i++){
        Rect_t r=data->tracks[i]->feature->rect;
        Index_t trackIdx=data->tracks[i]->track_id;
        string idxstr=boost::lexical_cast<string>(trackIdx);
        string title="id="+idxstr;
        yzbxlib::annotation(prev_input,r,title);
        rectangle(prev_input,r,Scalar(0,0,255),3,8);
        rectangle(prev_mask,r,Scalar::all(255),3,8);
    }

    Mat blob,object;
    cvtColor(mask,mask,CV_GRAY2BGR);
    hconcat(input,mask,blob);
    cvtColor(prev_mask,prev_mask,CV_GRAY2BGR);
    hconcat(prev_input,prev_mask,object);

    Mat klt;
    vconcat(object,blob,klt);
    namedWindow("SplitAndMerge dump",WINDOW_NORMAL);
    imshow("SplitAndMerge dump",klt);
}

track_t HungarianAssignment::calcDist(std::shared_ptr<trackingObjectFeature> of1, std::shared_ptr<trackingObjectFeature> of2, int costType)
{
    if(costType==RectDist){
        Rect_t &r1=of1->rect;
        Rect_t &r2=of2->rect;
        Point_t p1=0.5f*(r1.tl()+r1.br());
        Point_t p2=0.5f*(r2.tl()+r2.br());
        track_t dist=fabs(r1.width-r2.width)+fabs(r1.height-r2.height)+cv::norm(p1-p2);

        return dist;
    }
    else if(costType==PosDist){
        Point_t p1=of1->pos;
        Point_t p2=of2->pos;
        return cv::norm(p1-p2);
    }
    else if(costType==KLTDist){
        assert(false);
        return -1.0f;
    }
    else{
        assert(false);
        return -1.0f;
    }
}
void HungarianAssignment::dump(assignments_t unMatchedAssignment,vector<Index_t> unMatchedObjects,vector<Index_t> unMatchedBlobs)
{
    qDebug()<<data->frameNum;
    //assignment
    for (size_t i = 0; i < unMatchedAssignment.size(); i++)
    {
        if (unMatchedAssignment[i] != -1)
        {
            Index_t trackIdx=unMatchedObjects[i];
            Index_t blobIdx=unMatchedBlobs[unMatchedAssignment[i]];
            cout<<"blobIdx="<<blobIdx<<" match :"<<trackIdx<<endl;
        }
    }

    //TODO missed track

    //TODO new blob
}

bool HungarianAssignment::run()
{
    if(data->fvlist.size()<2){
        return false;
    }

    vector<trackingObjectFeature> &fv=data->fvlist.back();

    std::set<Index_t> &mMatchedObjectSet=data->matchedTrackSet;
    std::set<Index_t> &mMatchedBlobSet=data->matchedBlobSet;

    int m=data->tracks.size();
    int n=fv.size();
    int unMatchedObjectNum=data->tracks.size()-mMatchedObjectSet.size();
    int unMatchedBlobNum=fv.size()-mMatchedBlobSet.size();
    if(unMatchedBlobNum==0||unMatchedObjectNum==0){
        //            qDebug()<<"nothing for unmatched hungarain";
    }
    else{
        //vector<trackIdx>
        vector<Index_t> unMatchedObjects;
        //vector<featureVectorIdx>
        vector<Index_t> unMatchedBlobs;

        for(uint i=0;i<m;i++){
            if(mMatchedObjectSet.find(i)==mMatchedObjectSet.end()){
                unMatchedObjects.push_back(i);
            }
        }
        for(uint j=0;j<n;j++){
            if(mMatchedBlobSet.find(j)==mMatchedBlobSet.end()){
                unMatchedBlobs.push_back(j);
            }
        }

        size_t N = unMatchedObjects.size();
        size_t M = unMatchedBlobs.size();
        assert(N>0&&M>0);

        distMatrix_t Cost(N * M);
        for(uint i=0;i<unMatchedObjects.size();i++){
            for(uint j=0;j<unMatchedBlobs.size();j++){
                Cost[i+j*N]=calcDist(std::make_shared<trackingObjectFeature>(*(data->tracks[unMatchedObjects[i]]->feature)),
                        std::make_shared<trackingObjectFeature>(fv[unMatchedBlobs[j]]),RectDist);
            }
        }

        AssignmentProblemSolver APS;
        assignments_t unMatchedAssignment;
        APS.Solve(Cost, N, M, unMatchedAssignment, AssignmentProblemSolver::optimal);
        for (size_t i = 0; i < unMatchedAssignment.size(); i++)
        {
            if (unMatchedAssignment[i] != -1)
            {
                if (Cost[i + unMatchedAssignment[i] * N] > data->param.dist_thres)
                {
                    unMatchedAssignment[i] = -1;
                }
                else{
                    Index_t trackIdx=unMatchedObjects[i];
                    Index_t featureVectorIdx=unMatchedBlobs[unMatchedAssignment[i]];
                    data->trackToBlobSet[trackIdx].insert(featureVectorIdx);
                    data->blobToTrackSet[featureVectorIdx].insert(trackIdx);
                    data->matchedBlobSet.insert(featureVectorIdx);
                    data->matchedTrackSet.insert(trackIdx);
                }
            }
        }

        //        dump(unMatchedAssignment,unMatchedObjects,unMatchedBlobs);
    }


    return true;
}

bool ShowAssignment::run()
{
    if(data->fvlist.size()<2){
        assert(false);
    }
    vector<trackingObjectFeature> &fv=data->fvlist.back();
    Mat showImg=data->img_input.clone();
    //show track
    {
        for(uint i=0;i<data->tracks.size();i++){
            Rect rect=data->tracks[i]->feature->rect;
            int id=data->tracks[i]->track_id;
            string title=boost::lexical_cast<string>(id);
            yzbxlib::annotation(showImg,rect,title);
            rectangle(showImg,rect,Scalar(255,0,0),3,8);
        }
    }

    //show blob
    {
        for(uint i=0;i<fv.size();i++){
            Rect rect=fv[i].rect;
            //            string title=boost::lexical_cast<string>(i);
            //            yzbxlib::annotation(showImg,rect,title);
            rectangle(showImg,rect,Scalar(0,255,255),3,8);
        }
    }

    //show track To Blob
    {
        for(auto it=data->trackToBlobSet.begin();it!=data->trackToBlobSet.end();it++){
            Index_t trackIdx=it->first;
            set<Index_t> &blobset=it->second;
            Point p1=data->tracks[trackIdx]->feature->pos;
            for(auto it=blobset.begin();it!=blobset.end();it++){
                Index_t blobIdx=*it;
                Point p2=fv[blobIdx].pos;
                cv::line(showImg,p1,p2,Scalar(0,0,255),3,8);
            }
        }
    }

    string title=boost::lexical_cast<string>(data->frameNum);
    yzbxlib::annotation(showImg,Point(20,20),title);
    namedWindow("assignment",WINDOW_NORMAL);
    imshow("assignment",showImg);
    waitKey(300);
    return true;
}

bool OverLapAssignment::run()
{
    if(data->fvlist.size()<2){
        return false;
    }
    vector<trackingObjectFeature> &fv=data->fvlist.back();

    map<Index_t,set<Index_t>> &trackToBlobMap=data->trackToBlobSet;
    map<Index_t,set<Index_t>> &blobToTrackMap=data->blobToTrackSet;
    for(uint i=0;i<data->tracks.size();i++){
        Rect_t ra=data->tracks[i]->GetPredictRect();
        int lifetime=data->tracks[i]->get_lifetime();
        if(lifetime<data->param.MaxFreshObjectLifeTime||
                data->tracks[i]->status!=NORMAL_STATUS) continue;

        for(uint j=0;j<fv.size();j++){
            Rect_t rb=fv[j].rect;

            //TODO add some check

            if(yzbxlib::getOverlapRatio(ra,rb)>0.5){
                trackToBlobMap[i].insert(j);
                blobToTrackMap[j].insert(i);
            }
        }
    }

    return true;
}

bool FilterDeleteObjectToDump::run()
{
    auto &deleteLaterObjectSet=data->deleteLaterObjectSet;
    ///remove objects which need delete later!!!
    for(auto it=data->tracks.begin();it!=data->tracks.end();it++){
        Id_t id=(*it)->track_id;
        if(deleteLaterObjectSet.find(id)!=deleteLaterObjectSet.end()){
            bool flag=isGoodObject(id);
            if(flag){
                dumpTracjectoryAfterFilter(it-data->tracks.begin());
            }
            auto pre=prev(it,1);
            data->tracks.erase(it);
            it=pre;
        }
    }
    deleteLaterObjectSet.clear();

    return true;
}

void FilterDeleteObjectToDump::dumpTracjectoryAfterFilter(int trackIdx)
{
    if(data->tracks[trackIdx]->get_lifetime()>data->param.MinDumpLifeTime){
        vector<Rect_t> &rects=data->tracks[trackIdx]->rects;
        vector<Point_t> &trace=data->tracks[trackIdx]->trace;
        vector<int> &frames=data->tracks[trackIdx]->frames;
        vector<STATUS> &vec_status=data->tracks[trackIdx]->vec_status;

        int n=rects.size();
        assert(n==trace.size());
        assert(n==frames.size());
        assert(n==vec_status.size());

        QString idstr=QString::number(data->tracks[trackIdx]->track_id);

        assert(!data->recordFile.isEmpty());
        QFile file(data->recordFile);
        if(!file.open(QIODevice::WriteOnly|QIODevice::Append)){
            assert(false);
        }
        QTextStream out(&file);
        for(uint i=0;i<n;i++){
            if(vec_status[i]!=MISSING_STATUS){
                QStringList line;
                int x=(int)rects[i].x;
                int y=(int)rects[i].y;
                int width=(int)rects[i].width;
                int height=(int)rects[i].height;
                line<<QString::number(frames[i])<<idstr<<QString::number(x)<<QString::number(y)
                   <<QString::number(width)<<QString::number(height);

                out<<line.join(",")<<"\n";
            }
        }

        file.close();
    }
}

bool RestOverLapAssignment::run()
{
    return true;
}

bool SaveToVideo::run()
{
    if(data->img_fg.empty()) return false;

    vector<trackingObjectFeature> &fv=data->fvlist.back();
    Mat img_input=data->img_input.clone();
    Mat img_fg;
    cvtColor(data->img_fg,img_fg,CV_GRAY2BGR);
    for(uint i=0;i<fv.size();i++){
        Rect_t r=fv[i].rect;
        string idxstr=boost::lexical_cast<string>(i);
        //        string title="index="+idxstr;
        string title=idxstr;
        yzbxlib::annotation(img_input,r,title);
        rectangle(img_input,r,Scalar(0,0,255),3,8);
        rectangle(img_fg,r,Scalar(0,0,255),3,8);
    }
    hconcat(img_input,img_fg,data->imgToSave);
    namedWindow("Save To Video",WINDOW_NORMAL);
    imshow("Save To Video",data->imgToSave);
    waitKey(30);

    if(firstTime){
        QString fullpath=data->videoFilePath;
        QFileInfo info(fullpath);
        QString filename=info.baseName()+".avi";
        SaveVideoName=data->globalPt.get<std::string>("SaveToVideo.SaveVideoNamePrefix")+filename.toStdString();
        cout<<"save video to "<<SaveVideoName<<endl;
        cv::Size s=data->imgToSave.size();
        assert(videoWriter.open(SaveVideoName,CV_FOURCC('D', 'I', 'V', 'X'),50,s,true));
        firstTime=false;
    }

    assert(videoWriter.isOpened());
    videoWriter<<data->imgToSave;

    return true;
}

bool Countor::run()
{
    if(data->fvlist.size()<2){
        return false;
    }

    initLine();
    vector<trackingObjectFeature> &fv=data->fvlist.back();
    vector<trackingObjectFeature> &prev_fv=*(std::prev(data->fvlist.end(),2));

    if(fv.empty()||prev_fv.empty())
    {
        return true;
    }

    cv::Mat gray,prev_gray;
    {
        auto it=std::prev(data->imglist.end(),2);
        cv::cvtColor(data->img_input,gray,CV_BGR2GRAY);
        cv::cvtColor(it->first,prev_gray,CV_BGR2GRAY);
    }

    TermCriteria termcrit(CV_TERMCRIT_ITER|CV_TERMCRIT_EPS, 20, 0.03);
    Size subPixWinSize(10,10), winSize(31,31);
    const int MAX_COUNT = 500;
    vector<Point_KLT> points[2];

    goodFeaturesToTrack(prev_gray, points[0], MAX_COUNT, 0.01, 10, Mat(), 3, 0, 0.04);
    cornerSubPix(prev_gray, points[0], subPixWinSize, Size(-1,-1), termcrit);

    vector<uchar> status;
    vector<float> err;
    calcOpticalFlowPyrLK(prev_gray, gray, points[0], points[1], status, err, winSize,
            3, termcrit, 0, 0.001);
    uint k=0;
    Mat image=data->img_input.clone();
    for(int i=0; i < points[1].size(); i++ )
    {
        circle(image, points[1][i], 5, Scalar(255,255,255), -1, 8);
        if( !status[i] )
            continue;

        points[0][k]=points[0][i];
        points[1][k++] = points[1][i];

        circle(image,points[1][i], 3, Scalar(0,255,0), -1, 8);
        line(image,points[0][i],points[1][i],Scalar(0,0,255),3,8);
    }
    points[0].resize(k);
    points[1].resize(k);

    // for matchNum > 255
    cv::Mat matchMat(prev_fv.size(),fv.size(),CV_8UC1);
    matchMat=Scalar::all(0);
    for(int i=0;i<k;i++){
        Point2i p=points[1][i];
        Point2i prev_p=points[0][i];
        int matchMat_x=-1,matchMat_y=-1;
        for(uint i=0;i<prev_fv.size();i++){
            cv::Mat mask=prev_fv[i].mask;

            assert(!mask.empty());
            if(mask.at<uchar>(prev_p.y,prev_p.x)==255){
                matchMat_y=i;
                break;
            }
        }
        for(uint i=0;i<fv.size();i++){
            cv::Mat mask=fv[i].mask;

            assert(!mask.empty());
            if(mask.at<uchar>(p.y,p.x)==255){
                matchMat_x=i;
                break;
            }
        }

        if(matchMat_x==-1||matchMat_y==-1){
            //out of mask
        }
        else{
            //            Index_t prev_blobIdx=matchMat_y;
            //            Index_t blobIdx=matchMat_x;

            matchMat.at<uchar>(matchMat_y,matchMat_x)+=((uchar)1);
        }
    }

    for(int i=0;i<matchMat.rows;i++){
        for(int j=0;j<matchMat.cols;j++){
            uchar m=matchMat.at<uchar>(i,j);
            if(m>data->param.MatchNumThreshold){
                Point_t p=fv[j].pos;
                Point_t prev_p=prev_fv[i].pos;
                double A,B,C;
                A=B=C=0.0;
                getABC(A,B,C);
                if(yzbxlib::isLineCrossed(p,prev_p,A,B,C)){
                    Mat objectImg=data->img_input(fv[j].rect);
                    string filename=boost::lexical_cast<string>(count)+".jpg";
                    imwrite(filename,objectImg);
                    yzbxlib::showImageInWindow("object",objectImg);

                    count++;
                }
            }
        }
    }

    dump();

    return true;
}

void Countor::initLine()
{
    if(isLineInited){
        return;
    }

    center.y=data->img_input.rows*0.5f;
    center.x=data->img_input.cols*0.5f;
    angle=0.0;

    bool loop=true;
    while(loop){
        dump();
        int key=waitKey(0);

        const int left=1113937;
        const int up=1113938;
        const int right=up+1;
        const int down=right+1;
        const int r=1048690;
        const int R=1114194;
        const int q=1048689;
        const int Q=1114193;
        const int esc=1048603;



        switch (key) {
        case r:
        case 'r':
            angle=angle-1;
            if(angle<0) angle+=360;
            cout<<"angle ="<<angle<<endl;
            break;
        case R:
        case 'R':
            angle=angle+1;
            if(angle>=360) angle-=360;
            cout<<"angle ="<<angle<<endl;
            break;
        case q:
        case 'q':
        case Q:
        case 'Q':
        case esc:
        case 27:
            loop=false;
            break;
        case left:
        case 'a':
        case 'A':
            center.x--;
            if(center.x<0) center.x=0;
            cout<<"center=("<<center.x<<","<<center.y<<")"<<endl;
            break;
        case right:
        case 'D':
        case 'd':
            center.x++;
            if(center.x>=data->img_input.cols) center.x=data->img_input.cols-1;
            cout<<"center=("<<center.x<<","<<center.y<<")"<<endl;
            break;
        case up:
        case 'w':
        case 'W':
            center.y--;
            if(center.y<0) center.y=0;
            cout<<"center=("<<center.x<<","<<center.y<<")"<<endl;
            break;
        case down:
        case 's':
        case 'S':
            center.y++;
            if(center.y>=data->img_input.rows) center.y=data->img_input.rows-1;
            cout<<"center=("<<center.x<<","<<center.y<<")"<<endl;
            break;
        default:
            cout<<"press key is "<<key<<endl;
            cout<<"press r/R to rotate line"<<endl;
            cout<<"press q/Q/Esc to save and quit line"<<endl;
            cout<<"press left/right/up/down to move center point"<<endl;
            break;
        }
    }

    isLineInited=true;
}

void Countor::getABC(double &A, double &B, double &C)
{
    if((int)angle==0||(int)angle==180){
        A=0;
        B=1;
        C=-center.y;
    }
    else if((int)angle==90||(int)angle==270){
        A=1;
        B=0;
        C=-center.x;
    }
    else{
        double radian=angle*M_PI/180.0;
        A=std::tan(radian);
        B=1;
        C=-center.y-A*center.x;
    }
}

void Countor::getBoundaryPoint(Point &p1, Point &p2)
{
    if((int)angle==0||(int)angle==180){
        p1.x=0;
        p1.y=center.y;

        p2.x=data->img_input.cols;
        p2.y=center.y;
    }
    else if( ((int)angle==90) || ((int)angle==270) ){
        p1.x=center.x;
        p1.y=0;

        p2.x=center.x;
        p2.y=data->img_input.rows;
    }
    else{
        double row=data->img_input.rows;
        double col=data->img_input.cols;

        double radian=angle*M_PI/180.0;
        double A=std::tan(radian);

        p1.x=0;
        p1.y=(int)round(center.y+A*center.x);
        p2.x=col;
        p2.y=(int)round(center.y+A*(center.x-col));
    }
}

void Countor::dump()
{
    Point p1,p2;
    getBoundaryPoint(p1,p2);
    Mat img=data->img_input.clone();
    cv::line(img,p1,p2,Scalar(0,0,255),3,8);
    cv::circle(img,center,3,Scalar(0,255,255),-1,8);
    string text="id="+boost::lexical_cast<string>(count);
    yzbxlib::annotation(img,Point(20,20),text);
    string frameStr="N="+boost::lexical_cast<string>(data->frameNum);
    yzbxlib::annotation(img,Point(data->img_input.cols-200,20),frameStr);

    //draw fv pos
    vector<trackingObjectFeature> &fv=data->fvlist.back();
    for(int i=0;i<fv.size();i++){
        cv::circle(img,fv[i].pos,3,Scalar(255,0,255),-1,8);
    }

    //draw mask
    yzbxlib::getMaskedRGB(img,data->img_fg);
    yzbxlib::showImageInWindow("countor",img);
}

}
