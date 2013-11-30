////////YUV CODEBOOK
 // Gary Bradski, July 14, 2005
 
 
 #include "stdafx.h"
 #include "cv_yuv_codebook.h"
 
 //GLOBALS FOR ALL CAMERA MODELS
 
 //For connected components:
 int CVCONTOUR_APPROX_LEVEL = 2;   // Approx.threshold - the bigger it is, the simpler is the boundary
 int CVCLOSE_ITR = 1;                // How many iterations of erosion and/or dialation there should be
 //#define CVPERIMSCALE 4            // image (width+height)/PERIMSCALE.  If contour lenght < this, delete that contour
 
 //For learning background
 
 //Just some convienience macros
 #define CV_CVX_WHITE    CV_RGB(0xff,0xff,0xff)
 #define CV_CVX_BLACK    CV_RGB(0x00,0x00,0x00)
 
 
 ///////////////////////////////////////////////////////////////////////////////////
 // int updateCodeBook(uchar *p, codeBook &c, unsigned cbBounds)
 // Updates the codebook entry with a new data point
 //
 // p            Pointer to a YUV pixel
 // c            Codebook for this pixel
 // cbBounds        Learning bounds for codebook (Rule of thumb: 10)
 // numChannels    Number of color channels we're learning
 //
 // NOTES:
 //        cvBounds must be of size cvBounds[numChannels]
 //
 // RETURN
 //    codebook index
 int cv_updateCodeBook(uchar *p, codeBook &c, unsigned *cbBounds, int numChannels)
 {
 
     if(c.numEntries == 0) c.t = 0;//说明每个像素如果遍历了的话至少对应一个码元
     c.t += 1;        //Record learning event，遍历该像素点的次数加1
 //SET HIGH AND LOW BOUNDS
     int n;
     unsigned int high[3],low[3];
     for(n=0; n<numChannels; n++)//为该像素点的每个通道设置最大阈值和最小阈值，后面用来更新学习的高低阈值时有用
     {
         high[n] = *(p+n)+*(cbBounds+n);
         if(high[n] > 255) high[n] = 255;
         low[n] = *(p+n)-*(cbBounds+n);
         if(low[n] < 0) low[n] = 0;
     }
     int matchChannel;
     //SEE IF THIS FITS AN EXISTING CODEWORD
     int i;
     for(i=0; i<c.numEntries; i++)//需要对所有的码元进行扫描
     {
         matchChannel = 0;
         for(n=0; n<numChannels; n++)
         {
             //这个地方要非常小心，if条件不是下面表达的
 //if((c.cb[i]->min[n]-c.cb[i]->learnLow[n] <= *(p+n)) && (*(p+n) <= c.cb[i]->max[n]+c.cb[i]->learnHigh[n]))
 //原因是因为在每次建立一个新码元的时候，learnHigh[n]和learnLow[n]的范围就在max[n]和min[n]上扩展了cbBounds[n]，所以说
 //learnHigh[n]和learnLow[n]的变化范围实际上比max[n]和min[n]的大
             if((c.cb[i]->learnLow[n] <= *(p+n)) && (*(p+n) <= c.cb[i]->learnHigh[n])) //Found an entry for this channel
             {
                 matchChannel++;
             }
         }
         if(matchChannel == numChannels) //If an entry was found over all channels，找到了该元素此刻对应的码元
         {
             c.cb[i]->t_last_update = c.t;
             //adjust this codeword for the first channel
 //更新每个码元的最大最小阈值，因为这2个阈值在后面的前景分离过程要用到
             for(n=0; n<numChannels; n++)
             {
                 if(c.cb[i]->max[n] < *(p+n))//用该点的像素值更新该码元的最大值，所以max[n]保存的是实际上历史出现过的最大像素值
                 {
                     c.cb[i]->max[n] = *(p+n);//因为这个for语句是在匹配成功了的条件阈值下的，所以一般来说改变后的max[n]和min[n]
 //也不会过学习的高低阈值，并且学习的高低阈值也一直在缓慢变化  
                 }
                 else if(c.cb[i]->min[n] > *(p+n))//用该点的像素值更新该码元的最小值，所以min[n]保存的是实际上历史出现过的最小像素值
                 {
                     c.cb[i]->min[n] = *(p+n);
                 }
             }
             break;//一旦找到了该像素的一个码元后就不用继续往后找了，加快算法速度。因为最多只有一个码元与之对应
         }
     }
 
     //OVERHEAD TO TRACK POTENTIAL STALE ENTRIES
     for(int s=0; s<c.numEntries; s++)
     {
         //This garbage is to track which codebook entries are going stale
         int negRun = c.t - c.cb[s]->t_last_update;//negRun表示码元没有更新的时间间隔
         if(c.cb[s]->stale < negRun) c.cb[s]->stale = negRun;//更新每个码元的statle
     }
 
 
     //ENTER A NEW CODE WORD IF NEEDED
     if(i == c.numEntries)  //No existing code word found, make a new one，只有当该像素码本中的所有码元都不符合要求时才满足if条件
     {
         code_element **foo = new code_element* [c.numEntries+1];//创建一个新的码元序列
         for(int ii=0; ii<c.numEntries; ii++)
         {
             foo[ii] = c.cb[ii];//将码本前面所有的码元地址赋给foo
         }
         foo[c.numEntries] = new code_element;//创建一个新码元并赋给foo指针的下一个空位
         if(c.numEntries) delete [] c.cb;//？
         c.cb = foo;
         for(n=0; n<numChannels; n++)//给新建立的码元结构体元素赋值
         {
             c.cb[c.numEntries]->learnHigh[n] = high[n];//当建立一个新码元时，用当前值附近cbBounds范围作为码元box的学习阈值
             c.cb[c.numEntries]->learnLow[n] = low[n];
             c.cb[c.numEntries]->max[n] = *(p+n);//当建立一个新码元时，用当前值作为码元box的最大最小边界值
             c.cb[c.numEntries]->min[n] = *(p+n);
         }
         c.cb[c.numEntries]->t_last_update = c.t;
         c.cb[c.numEntries]->stale = 0;//因为刚建立，所有为0
         c.numEntries += 1;//码元的个数加1
     }
 
     //SLOWLY ADJUST LEARNING BOUNDS
     for(n=0; n<numChannels; n++)//每次遍历该像素点就将每个码元的学习最大阈值变大，最小阈值变小，但是都是缓慢变化的
     {                           //如果是新建立的码元，则if条件肯定不满足
         if(c.cb[i]->learnHigh[n] < high[n]) c.cb[i]->learnHigh[n] += 1;                
         if(c.cb[i]->learnLow[n] > low[n]) c.cb[i]->learnLow[n] -= 1;
     }
 
     return(i);//返回所找到码本中码元的索引
 }
 
 ///////////////////////////////////////////////////////////////////////////////////
 // uchar cvbackgroundDiff(uchar *p, codeBook &c, int minMod, int maxMod)
 // Given a pixel and a code book, determine if the pixel is covered by the codebook
 //
 // p        pixel pointer (YUV interleaved)
 // c        codebook reference
 // numChannels  Number of channels we are testing
 // maxMod    Add this (possibly negative) number onto max level when code_element determining if new pixel is foreground
 // minMod    Subract this (possible negative) number from min level code_element when determining if pixel is foreground
 //
 // NOTES:
 // minMod and maxMod must have length numChannels, e.g. 3 channels => minMod[3], maxMod[3].
 //
 // Return
 // 0 => background, 255 => foreground
 uchar cvbackgroundDiff(uchar *p, codeBook &c, int numChannels, int *minMod, int *maxMod)
 {
     int matchChannel;
     //SEE IF THIS FITS AN EXISTING CODEWORD
     int i;
     for(i=0; i<c.numEntries; i++)
     {
         matchChannel = 0;
         for(int n=0; n<numChannels; n++)
         {
             if((c.cb[i]->min[n] - minMod[n] <= *(p+n)) && (*(p+n) <= c.cb[i]->max[n] + maxMod[n]))
             {
                 matchChannel++; //Found an entry for this channel
             }
             else
             {
                 break;//加快速度，当一个通道不满足时提前结束
             }
         }
         if(matchChannel == numChannels)
         {
             break; //Found an entry that matched all channels，加快速度，当一个码元找到时，提前结束
         }
     }
     if(i >= c.numEntries) return(255);//255代表前景，因为所有的码元都不满足条件
     return(0);//0代表背景，因为至少有一个码元满足条件
 }
 
 
 //UTILITES/////////////////////////////////////////////////////////////////////////////////////
 /////////////////////////////////////////////////////////////////////////////////
 //int clearStaleEntries(codeBook &c)
 // After you've learned for some period of time, periodically call this to clear out stale codebook entries
 //
 //c        Codebook to clean up
 //
 // Return
 // number of entries cleared
 int cvclearStaleEntries(codeBook &c)//对每一个码本进行检查
 {
     int staleThresh = c.t>>1;//阈值设置为访问该码元的次数的一半，经验值
     int *keep = new int [c.numEntries];
     int keepCnt = 0;
     //SEE WHICH CODEBOOK ENTRIES ARE TOO STALE
     for(int i=0; i<c.numEntries; i++)
     {
         if(c.cb[i]->stale > staleThresh)//当在背景建模期间有一半的时间内，codebook的码元条目没有被访问，则该条目将被删除
             keep[i] = 0; //Mark for destruction
         else
         {
             keep[i] = 1; //Mark to keep，为1时，该码本的条目将被保留
             keepCnt += 1;//keepCnt记录了要保持的codebook的数目
         }
     }
     //KEEP ONLY THE GOOD
     c.t = 0;                        //Full reset on stale tracking
     code_element **foo = new code_element* [keepCnt];//重新建立一个码本的双指针
     int k=0;
     for(int ii=0; ii<c.numEntries; ii++)
     {
         if(keep[ii])
         {
             foo[k] = c.cb[ii];//要保持该码元的话就要把码元结构体复制到fook
             foo[k]->stale = 0;        //We have to refresh these entries for next clearStale，不被访问的累加器stale重新赋值0
             foo[k]->t_last_update = 0;//
             k++;
         }
     }
     //CLEAN UP
     delete [] keep;
     delete [] c.cb;
     c.cb = foo;
     int numCleared = c.numEntries - keepCnt;//numCleared中保存的是被删除码元的个数
     c.numEntries = keepCnt;//最后新的码元数为保存下来码元的个数
     return(numCleared);//返回被删除的码元个数
 }
 
 /////////////////////////////////////////////////////////////////////////////////
 //int countSegmentation(codeBook *c, IplImage *I)
 //
 //Count how many pixels are detected as foreground
 // c    Codebook
 // I    Image (yuv, 24 bits)
 // numChannels  Number of channels we are testing
 // maxMod    Add this (possibly negative) number onto max level when code_element determining if new pixel is foreground
 // minMod    Subract this (possible negative) number from min level code_element when determining if pixel is foreground
 //
 // NOTES:
 // minMod and maxMod must have length numChannels, e.g. 3 channels => minMod[3], maxMod[3].
 //
 //Return
 // Count of fg pixels
 //
 int cvcountSegmentation(codeBook *c, IplImage *I, int numChannels, int *minMod, int *maxMod)
 {
     int count = 0,i;
     uchar *pColor;
     int imageLen = I->width * I->height;
 
     //GET BASELINE NUMBER OF FG PIXELS FOR Iraw
     pColor = (uchar *)((I)->imageData);
     for(i=0; i<imageLen; i++)
     {
         if(cvbackgroundDiff(pColor, c[i], numChannels, minMod, maxMod))//对每一个像素点都要检测其是否为前景，如果是的话，计数器count就加1
             count++;
         pColor += 3;
     }
     return(count);//返回图像I的前景像素点的个数
 }
 
 
 ///////////////////////////////////////////////////////////////////////////////////////////
 //void cvconnectedComponents(IplImage *mask, int poly1_hull0, float perimScale, int *num, CvRect *bbs, CvPoint *centers)
 // This cleans up the forground segmentation mask derived from calls to cvbackgroundDiff
 //
 // mask            Is a grayscale (8 bit depth) "raw" mask image which will be cleaned up
 //
 // OPTIONAL PARAMETERS:
 // poly1_hull0    If set, approximate connected component by (DEFAULT) polygon, or else convex hull (0)
 // perimScale     Len = image (width+height)/perimScale.  If contour len < this, delete that contour (DEFAULT: 4)
 // num            Maximum number of rectangles and/or centers to return, on return, will contain number filled (DEFAULT: NULL)
 // bbs            Pointer to bounding box rectangle vector of length num.  (DEFAULT SETTING: NULL)
 // centers        Pointer to contour centers vectore of length num (DEFULT: NULL)
 //
 void cvconnectedComponents(IplImage *mask, int poly1_hull0, float perimScale, int *num, CvRect *bbs, CvPoint *centers)
 {
 static CvMemStorage*    mem_storage    = NULL;
 static CvSeq*            contours    = NULL;
 //CLEAN UP RAW MASK
 //开运算作用：平滑轮廓，去掉细节,断开缺口
     cvMorphologyEx( mask, mask, NULL, NULL, CV_MOP_OPEN, CVCLOSE_ITR );//对输入mask进行开操作，CVCLOSE_ITR为开操作的次数，输出为mask图像
 //闭运算作用：平滑轮廓，连接缺口
     cvMorphologyEx( mask, mask, NULL, NULL, CV_MOP_CLOSE, CVCLOSE_ITR );//对输入mask进行闭操作，CVCLOSE_ITR为闭操作的次数，输出为mask图像
 
 //FIND CONTOURS AROUND ONLY BIGGER REGIONS
     if( mem_storage==NULL ) mem_storage = cvCreateMemStorage(0);
     else cvClearMemStorage(mem_storage);
 
     //CV_RETR_EXTERNAL=0是在types_c.h中定义的，CV_CHAIN_APPROX_SIMPLE=2也是在该文件中定义的
     CvContourScanner scanner = cvStartFindContours(mask,mem_storage,sizeof(CvContour),CV_RETR_EXTERNAL,CV_CHAIN_APPROX_SIMPLE);
     CvSeq* c;
     int numCont = 0;
     while( (c = cvFindNextContour( scanner )) != NULL )
     {
         double len = cvContourPerimeter( c );
         double q = (mask->height + mask->width) /perimScale;   //calculate perimeter len threshold
         if( len < q ) //Get rid of blob if it's perimeter is too small
         {
             cvSubstituteContour( scanner, NULL );
         }
         else //Smooth it's edges if it's large enough
         {
             CvSeq* c_new;
             if(poly1_hull0) //Polygonal approximation of the segmentation
                 c_new = cvApproxPoly(c,sizeof(CvContour),mem_storage,CV_POLY_APPROX_DP, CVCONTOUR_APPROX_LEVEL,0);
             else //Convex Hull of the segmentation
                 c_new = cvConvexHull2(c,mem_storage,CV_CLOCKWISE,1);
             cvSubstituteContour( scanner, c_new );
             numCont++;
         }
     }
     contours = cvEndFindContours( &scanner );
 
 // PAINT THE FOUND REGIONS BACK INTO THE IMAGE
     cvZero( mask );
     IplImage *maskTemp;
     //CALC CENTER OF MASS AND OR BOUNDING RECTANGLES
     if(num != NULL)
     {
         int N = *num, numFilled = 0, i=0;
         CvMoments moments;
         double M00, M01, M10;
         maskTemp = cvCloneImage(mask);
         for(i=0, c=contours; c != NULL; c = c->h_next,i++ )
         {
             if(i < N) //Only process up to *num of them
             {
                 cvDrawContours(maskTemp,c,CV_CVX_WHITE, CV_CVX_WHITE,-1,CV_FILLED,8);
                 //Find the center of each contour
                 if(centers != NULL)
                 {
                     cvMoments(maskTemp,&moments,1);
                     M00 = cvGetSpatialMoment(&moments,0,0);
                     M10 = cvGetSpatialMoment(&moments,1,0);
                     M01 = cvGetSpatialMoment(&moments,0,1);
                     centers[i].x = (int)(M10/M00);
                     centers[i].y = (int)(M01/M00);
                 }
                 //Bounding rectangles around blobs
                 if(bbs != NULL)
                 {
                     bbs[i] = cvBoundingRect(c);
                 }
                 cvZero(maskTemp);
                 numFilled++;
             }
             //Draw filled contours into mask
             cvDrawContours(mask,c,CV_CVX_WHITE,CV_CVX_WHITE,-1,CV_FILLED,8); //draw to central mask
         } //end looping over contours
         *num = numFilled;
         cvReleaseImage( &maskTemp);
     }
     //ELSE JUST DRAW PROCESSED CONTOURS INTO THE MASK
     else
     {
         for( c=contours; c != NULL; c = c->h_next )
         {
             cvDrawContours(mask,c,CV_CVX_WHITE, CV_CVX_BLACK,-1,CV_FILLED,8);
         }
     }
 }