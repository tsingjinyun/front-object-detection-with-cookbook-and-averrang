     //Learning Opencv的作者将这两种算法做了下对比，用的视频是有风吹动树枝的动态背景，一段时间过后的前景是视频中移动的手。
     //当然在这个工程中，作者除了体现上述简单背景差法和codobook算法的一些原理外，还引入了很多细节来优化前景分割效果。比如说误差计算时的方差和协方差计算加速方法，消除像素点内长时间没有被访问过的码元，对检测到的粗糙原始前景图用连通域分析法清楚噪声，其中引入了形态学中的几种操作，使用多边形拟合前景轮廓等细节处理。
     //在看作者代码前，最好先看下下面几个变量的物理含义。
     //maxMod[n]：用训练好的背景模型进行前景检测时用到，判断点是否小于max[n] + maxMod[n])。
     //minMod[n]：用训练好的背景模型进行前景检测时用到，判断点是否小于min[n] -minMod[n])。
     //cbBounds*：训练背景模型时用到，可以手动输入该参数，这个数主要是配合high[n]和low[n]来用的。
     //learnHigh[n]：背景学习过程中当一个新像素来时用来判断是否在已有的码元中，是阈值的上界部分。
     //learnLow[n]：背景学习过程中当一个新像素来时用来判断是否在已有的码元中，是阈值的下界部分。
     //max[n]： 背景学习过程中每个码元学习到的最大值，在前景分割时配合maxMod[n]用的。
     //min[n]： 背景学习过程中每个码元学习到的最小值，在前景分割时配合minMod[n]用的。
     //high[n]：背景学习过程中用来调整learnHigh[n]的，如果learnHigh[n]<high[n],则learnHigh[n]缓慢加1
     //low[n]： 背景学习过程中用来调整learnLow[n]的，如果learnLow[n]>Low[n],则learnLow[缓慢减1
/***************************************************************************************
//函数入口参数说明：
//argv[1]:第一个参数表示视频开始的背景训练时的帧，默认是1
//argv[2]:第二个参数表示的结束背景训练时的，默认为30
//argv[3]:第三个参数是读入视频文件的文件名
//argv[4]:scalehigh设定背景建模时的高阈值函数,默认值为6
//argv[5]:scalelow设定背景建模时的低阈值函数,默认值为7
//argv[6]:HighCB_Y
//argv[7]:LowCB_Y
//argv[8]:HighCB_U
//argv[9]:LowCB_U
//argv[10]: HighCB_V
//argv[11]:LowCB_V
***************************************************************************************//
#include "stdafx.h"

#include <stdio.h>
#include "cv.h"
#include "highgui.h"
#include "avg_background.h"
#include "cv_yuv_codebook.h"

#define CHANNELS 3        
// 设置处理的图像通道数,要求小于等于图像本身的通道数 

//VARIABLES for CODEBOOK METHOD:
codeBook *cB; //This will be our linear model of the image, a vector 
                //of lengh = height*width
int maxMod[CHANNELS];    //Add these (possibly negative) number onto max 
                        // level when code_element determining if new pixel is foreground
int minMod[CHANNELS];     //Subract these (possible negative) number from min 
                        //level code_element when determining if pixel is foreground
unsigned cbBounds[CHANNELS]; //Code Book bounds for learning
bool ch[CHANNELS];        //This sets what channels should be adjusted for background bounds
int nChannels = CHANNELS;
int imageLen = 0;
uchar *pColor; //YUV pointer

void help() {
    printf("\nLearn background and find foreground using simple average and average difference learning method:\n"
        "\nUSAGE:\n  ch9_background startFrameCollection# endFrameCollection# [movie filename, else from camera]\n"
        "If from AVI, then optionally add HighAvg, LowAvg, HighCB_Y LowCB_Y HighCB_U LowCB_U HighCB_V LowCB_V\n\n"
        "***Keep the focus on the video windows, NOT the consol***\n\n"
        "INTERACTIVE PARAMETERS:\n"
        "\tESC,q,Q  - quit the program\n"
        "\th    - print this help\n"
        "\tp    - pause toggle\n"
        "\ts    - single step\n"
        "\tr    - run mode (single step off)\n"
        "=== AVG PARAMS ===\n"
        "\t-    - bump high threshold UP by 0.25\n"
        "\t=    - bump high threshold DOWN by 0.25\n"
        "\t[    - bump low threshold UP by 0.25\n"
        "\t]    - bump low threshold DOWN by 0.25\n"
        "=== CODEBOOK PARAMS ===\n"
        "\ty,u,v- only adjust channel 0(y) or 1(u) or 2(v) respectively\n"
        "\ta    - adjust all 3 channels at once\n"
        "\tb    - adjust both 2 and 3 at once\n"
        "\ti,o    - bump upper threshold up,down by 1\n"
        "\tk,l    - bump lower threshold up,down by 1\n"
        );
}

//
//USAGE:  ch9_background startFrameCollection# endFrameCollection# [movie filename, else from camera]
//If from AVI, then optionally add HighAvg, LowAvg, HighCB_Y LowCB_Y HighCB_U LowCB_U HighCB_V LowCB_V
//
int main(int argc, char** argv)
{
     IplImage* rawImage = 0, *yuvImage = 0; //yuvImage is for codebook method
    IplImage *ImaskAVG = 0,*ImaskAVGCC = 0;
    IplImage *ImaskCodeBook = 0,*ImaskCodeBookCC = 0;
    CvCapture* capture = 0;

    int startcapture = 1;
    int endcapture = 30;
    int c,n;

    maxMod[0] = 3;  //Set color thresholds to default values:argv[6]------argv[11]
    minMod[0] = 10;
    maxMod[1] = 1;
    minMod[1] = 1;
    maxMod[2] = 1;
    minMod[2] = 1;
    float scalehigh = HIGH_SCALE_NUM;//默认值为6
    float scalelow = LOW_SCALE_NUM;//默认值为7
    
    if(argc < 3) {//只有1个参数或者没有参数时，输出错误，并提示help信息，因为该程序本身就算进去了一个参数
        printf("ERROR: Too few parameters\n");
        help();
    }else{//至少有2个参数才算正确
        if(argc == 3){//输入为2个参数的情形是从摄像头输入数据
            printf("Capture from Camera\n");
            capture = cvCaptureFromCAM( 0 );
        }
        else {//输入大于2个参数时是从文件中读入视频数据
            printf("Capture from file %s\n",argv[3]);//第三个参数是读入视频文件的文件名
    //        capture = cvCaptureFromFile( argv[3] );
            capture = cvCreateFileCapture( argv[3] );
            if(!capture) { printf("Couldn't open %s\n",argv[3]); return -1;}//读入视频文件失败
        }
        if(isdigit(argv[1][0])) { //Start from of background capture
            startcapture = atoi(argv[1]);//第一个参数表示视频开始的背景训练时的帧，默认是1
            printf("startcapture = %d\n",startcapture);
        }
        if(isdigit(argv[2][0])) { //End frame of background capture
            endcapture = atoi(argv[2]);//第二个参数表示的结束背景训练时的，默认为30
            printf("endcapture = %d\n"); 
        }
        if(argc > 4){ //See if parameters are set from command line，输入多于4个参数表示后面的算法中用到的参数在这里直接输入
            //FOR AVG MODEL
            if(argc >= 5){
                if(isdigit(argv[4][0])){
                    scalehigh = (float)atoi(argv[4]);
                }
            }
            if(argc >= 6){
                if(isdigit(argv[5][0])){
                    scalelow = (float)atoi(argv[5]);
                }
            }
            //FOR CODEBOOK MODEL, CHANNEL 0
            if(argc >= 7){
                if(isdigit(argv[6][0])){
                    maxMod[0] = atoi(argv[6]);
                }
            }
            if(argc >= 8){
                if(isdigit(argv[7][0])){
                    minMod[0] = atoi(argv[7]);
                }
            }
            //Channel 1
            if(argc >= 9){
                if(isdigit(argv[8][0])){
                    maxMod[1] = atoi(argv[8]);
                }
            }
            if(argc >= 10){
                if(isdigit(argv[9][0])){
                    minMod[1] = atoi(argv[9]);
                }
            }
            //Channel 2
            if(argc >= 11){
                if(isdigit(argv[10][0])){
                    maxMod[2] = atoi(argv[10]);
                }
            }
            if(argc >= 12){
                if(isdigit(argv[11][0])){
                    minMod[2] = atoi(argv[11]);
                }
            }
        }
    }

    //MAIN PROCESSING LOOP:
    bool pause = false;
    bool singlestep = false;

    if( capture )
    {
      cvNamedWindow( "Raw", 1 );//原始视频图像
        cvNamedWindow( "AVG_ConnectComp",1);//平均法连通区域分析后的图像
        cvNamedWindow( "ForegroundCodeBook",1);//codebook法后图像
        cvNamedWindow( "CodeBook_ConnectComp",1);//codebook法连通区域分析后的图像
         cvNamedWindow( "ForegroundAVG",1);//平均法后图像
        int i = -1;
        
        for(;;)
        {
                if(!pause){
//                if( !cvGrabFrame( capture ))
//                    break;
//                rawImage = cvRetrieveFrame( capture );
                rawImage = cvQueryFrame( capture );
                ++i;//count it
//                printf("%d\n",i);
                if(!rawImage) 
                    break;
                //REMOVE THIS FOR GENERAL OPERATION, JUST A CONVIENIENCE WHEN RUNNING WITH THE SMALL tree.avi file
                if(i == 56){//程序开始运行几十帧后自动暂停，以便后面好手动调整参数
                    pause = 1;
                    printf("\n\nVideo paused for your convienience at frame 50 to work with demo\n"
                    "You may adjust parameters, single step or continue running\n\n");
                    help();
                }
            }
            if(singlestep){
                pause = true;
            }
            //First time:
            if(0 == i) {
                printf("\n . . . wait for it . . .\n"); //Just in case you wonder why the image is white at first
                //AVG METHOD ALLOCATION
                AllocateImages(rawImage);//为算法的使用分配内存
                scaleHigh(scalehigh);//设定背景建模时的高阈值函数
                scaleLow(scalelow);//设定背景建模时的低阈值函数
                ImaskAVG = cvCreateImage( cvGetSize(rawImage), IPL_DEPTH_8U, 1 );
                ImaskAVGCC = cvCreateImage( cvGetSize(rawImage), IPL_DEPTH_8U, 1 );
                cvSet(ImaskAVG,cvScalar(255));
                //CODEBOOK METHOD ALLOCATION:
                yuvImage = cvCloneImage(rawImage);
                ImaskCodeBook = cvCreateImage( cvGetSize(rawImage), IPL_DEPTH_8U, 1 );//用来装前景背景图的，当然只要一个通道的图像即可
                ImaskCodeBookCC = cvCreateImage( cvGetSize(rawImage), IPL_DEPTH_8U, 1 );
                cvSet(ImaskCodeBook,cvScalar(255));
                imageLen = rawImage->width*rawImage->height;
                cB= new codeBook [imageLen];//创建一个码本cB数组，每个像素对应一个码本
                for(int f = 0; f<imageLen; f++)
                {
                     cB[f].numEntries = 0;//每个码本的初始码元个数赋值为0
                }
                for(int nc=0; nc<nChannels;nc++)
                {
                    cbBounds[nc] = 10; //Learning bounds factor，初始值为10
                }
                ch[0] = true; //Allow threshold setting simultaneously for all channels
                ch[1] = true;
                ch[2] = true;
            }
            //If we've got an rawImage and are good to go:                
            if( rawImage )
            {
                cvCvtColor( rawImage, yuvImage, CV_BGR2YCrCb );//YUV For codebook method
                //This is where we build our background model
                if( !pause && i >= startcapture && i < endcapture  ){
                    //LEARNING THE AVERAGE AND AVG DIFF BACKGROUND
                    accumulateBackground(rawImage);//平均法累加过程
                    //LEARNING THE CODEBOOK BACKGROUND
                    pColor = (uchar *)((yuvImage)->imageData);//yuvImage矩阵的首位置
                    for(int c=0; c<imageLen; c++)
                    {
                        cv_updateCodeBook(pColor, cB[c], cbBounds, nChannels);//codebook算法建模过程
                        pColor += 3;
                    }
                }
                //When done, create the background model
                if(i == endcapture){
                    createModelsfromStats();//平均法建模过程
                }
                //Find the foreground if any
                if(i >= endcapture) {//endcapture帧后开始检测前景
                    //FIND FOREGROUND BY AVG METHOD:
                    backgroundDiff(rawImage,ImaskAVG);
                    cvCopy(ImaskAVG,ImaskAVGCC);
                    cvconnectedComponents(ImaskAVGCC);//平均法中的前景清除
                    //FIND FOREGROUND BY CODEBOOK METHOD
                    uchar maskPixelCodeBook;
                    pColor = (uchar *)((yuvImage)->imageData); //3 channel yuv image
                    uchar *pMask = (uchar *)((ImaskCodeBook)->imageData); //1 channel image
                    for(int c=0; c<imageLen; c++)
                    {
                         maskPixelCodeBook = cvbackgroundDiff(pColor, cB[c], nChannels, minMod, maxMod);//前景返回255，背景返回0
                        *pMask++ = maskPixelCodeBook;//将前景检测的结果返回到ImaskCodeBook中
                        pColor += 3;
                    }
                    //This part just to visualize bounding boxes and centers if desired
                    cvCopy(ImaskCodeBook,ImaskCodeBookCC);    
                    cvconnectedComponents(ImaskCodeBookCC);//codebook算法中的前景清除
                }
                //Display
                   cvShowImage( "Raw", rawImage );//除了这张是彩色图外，另外4张都是黑白图
                cvShowImage( "AVG_ConnectComp",ImaskAVGCC);
                   cvShowImage( "ForegroundAVG",ImaskAVG);
                 cvShowImage( "ForegroundCodeBook",ImaskCodeBook);
                 cvShowImage( "CodeBook_ConnectComp",ImaskCodeBookCC);

                //USER INPUT:
                 c = cvWaitKey(10)&0xFF;
                //End processing on ESC, q or Q
                if(c == 27 || c == 'q'|| c == 'Q')
                    break;
                //Else check for user input
                switch(c)
                {
                    case 'h':
                        help();
                        break;
                    case 'p':
                        pause ^= 1;
                        break;
                    case 's':
                        singlestep = 1;
                        pause = false;
                        break;
                    case 'r':
                        pause = false;
                        singlestep = false;
                        break;
                    //AVG BACKROUND PARAMS
                    case '-'://调整scalehigh的参数，scalehigh的物理意义是误差累加的影响因子，其倒数为缩放倍数，加0.25实际上是减小其影响力
                        if(i > endcapture){
                            scalehigh += 0.25;
                            printf("AVG scalehigh=%f\n",scalehigh);
                            scaleHigh(scalehigh);
                        }
                        break;
                    case '='://scalehigh减少2.5是增加其影响力
                        if(i > endcapture){
                            scalehigh -= 0.25;
                            printf("AVG scalehigh=%f\n",scalehigh);
                            scaleHigh(scalehigh);
                        }
                        break;
                    case '[':
                        if(i > endcapture){//设置设定背景建模时的低阈值函数，同上
                            scalelow += 0.25;
                            printf("AVG scalelow=%f\n",scalelow);
                            scaleLow(scalelow);
                        }
                        break;
                    case ']':
                        if(i > endcapture){
                            scalelow -= 0.25;
                            printf("AVG scalelow=%f\n",scalelow);
                            scaleLow(scalelow);
                        }
                        break;
                //CODEBOOK PARAMS
                case 'y':
                case '0'://激活y通道
                        ch[0] = 1;
                        ch[1] = 0;
                        ch[2] = 0;
                        printf("CodeBook YUV Channels active: ");
                        for(n=0; n<nChannels; n++)
                                printf("%d, ",ch[n]);
                        printf("\n");
                        break;
                case 'u':
                case '1'://激活u通道
                        ch[0] = 0;
                        ch[1] = 1;
                        ch[2] = 0;
                        printf("CodeBook YUV Channels active: ");
                        for(n=0; n<nChannels; n++)
                                printf("%d, ",ch[n]);
                        printf("\n");
                        break;
                case 'v':
                case '2'://激活v通道
                        ch[0] = 0;
                        ch[1] = 0;
                        ch[2] = 1;
                        printf("CodeBook YUV Channels active: ");
                        for(n=0; n<nChannels; n++)
                                printf("%d, ",ch[n]);
                        printf("\n");
                        break;
                case 'a': //All
                case '3'://激活所有通道
                        ch[0] = 1;
                        ch[1] = 1;
                        ch[2] = 1;
                        printf("CodeBook YUV Channels active: ");
                        for(n=0; n<nChannels; n++)
                                printf("%d, ",ch[n]);
                        printf("\n");
                        break;
                case 'b':  //both u and v together
                        ch[0] = 0;
                        ch[1] = 1;
                        ch[2] = 1;
                        printf("CodeBook YUV Channels active: ");
                        for(n=0; n<nChannels; n++)
                                printf("%d, ",ch[n]);
                        printf("\n");
                        break;
                case 'i': //modify max classification bounds (max bound goes higher)
                    for(n=0; n<nChannels; n++){//maxMod和minMod是最大值和最小值跳动的阈值
                        if(ch[n])
                            maxMod[n] += 1;
                        printf("%.4d,",maxMod[n]);
                    }
                    printf(" CodeBook High Side\n");
                    break;
                case 'o': //modify max classification bounds (max bound goes lower)
                    for(n=0; n<nChannels; n++){
                        if(ch[n])
                            maxMod[n] -= 1;
                        printf("%.4d,",maxMod[n]);
                    }
                    printf(" CodeBook High Side\n");
                    break;
                case 'k': //modify min classification bounds (min bound goes lower)
                    for(n=0; n<nChannels; n++){
                        if(ch[n])
                            minMod[n] += 1;
                        printf("%.4d,",minMod[n]);
                    }
                    printf(" CodeBook Low Side\n");
                    break;
                case 'l': //modify min classification bounds (min bound goes higher)
                    for(n=0; n<nChannels; n++){
                        if(ch[n])
                            minMod[n] -= 1;
                        printf("%.4d,",minMod[n]);
                    }
                    printf(" CodeBook Low Side\n");
                    break;
                }
                
            }
        }        
      cvReleaseCapture( &capture );
      cvDestroyWindow( "Raw" );
        cvDestroyWindow( "ForegroundAVG" );
        cvDestroyWindow( "AVG_ConnectComp");
        cvDestroyWindow( "ForegroundCodeBook");
        cvDestroyWindow( "CodeBook_ConnectComp");
        DeallocateImages();//释放平均法背景建模过程中用到的内存
        if(yuvImage) cvReleaseImage(&yuvImage);
        if(ImaskAVG) cvReleaseImage(&ImaskAVG);
        if(ImaskAVGCC) cvReleaseImage(&ImaskAVGCC);
        if(ImaskCodeBook) cvReleaseImage(&ImaskCodeBook);
        if(ImaskCodeBookCC) cvReleaseImage(&ImaskCodeBookCC);
        delete [] cB;
    }
    else{ printf("\n\nDarn, Something wrong with the parameters\n\n"); help();
    }
    return 0;
}