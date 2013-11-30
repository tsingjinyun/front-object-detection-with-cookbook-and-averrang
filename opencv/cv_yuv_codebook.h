///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
 // Accumulate average and ~std (really absolute difference) image and use this to detect background and foreground
 //
 // Typical way of using this is to:
 //     AllocateImages();
 ////loop for N images to accumulate background differences
 //    accumulateBackground();
 ////When done, turn this into our avg and std model with high and low bounds
 //    createModelsfromStats();
 ////Then use the function to return background in a mask (255 == foreground, 0 == background)
 //    backgroundDiff(IplImage *I,IplImage *Imask, int num);
 ////Then tune the high and low difference from average image background acceptance thresholds
 //    float scalehigh,scalelow; //Set these, defaults are 7 and 6. Note: scalelow is how many average differences below average
 //    scaleHigh(scalehigh);
 //    scaleLow(scalelow);
 ////That is, change the scale high and low bounds for what should be background to make it work.
 ////Then continue detecting foreground in the mask image
 //    backgroundDiff(IplImage *I,IplImage *Imask, int num);
 //
 //NOTES: num is camera number which varies from 0 ... NUM_CAMERAS - 1.  Typically you only have one camera, but this routine allows
 //          you to index many.
 //
   //该算法为图像中每一个像素点建立一个码本，每个码本可以包括多个码元，
	  // 每个码元有它的学习时最大最小阈值，检测时的最大最小阈值等成员。
	  // 在背景建模期间，每当来了一幅新图片，对每个像素点进行码本匹配，
	  // 也就是说如果该像素值在码本中某个码元的学习阈值内，
	  // 则认为它离过去该对应点出现过的历史情况偏离不大，通过一定的像素值比较，
	  // 如果满足条件，此时还可以更新对应点的学习阈值和检测阈值。
	  // 如果新来的像素值对码本中每个码元都不匹配，则有可能是由于背景是动态的，
	  // 所以我们需要为其建立一个新的码元，并且设置相应的码元成员变量。
	  // 因此，在背景学习的过程中，每个像素点可以对应多个码元，这样就可以学到复杂的动态背景
 #ifndef AVGSEG_1
 #define AVGSEG_1
 
 #include "cv.h"                // define all of the opencv classes etc.
 #include "highgui.h"
 #include "cxcore.h"
 #include "avg_background.h"
#define CHANNELS 3        
// 设置处理的图像通道数,要求小于等于图像本身的通道数  

typedef struct ce {
	uchar	learnHigh[CHANNELS];	// High side threshold for learning
	// 此码元各通道的阀值上限(学习界限)
	uchar	learnLow[CHANNELS];		// Low side threshold for learning
	// 此码元各通道的阀值下限
	// 学习过程中如果一个新像素各通道值x[i],均有 learnLow[i]<=x[i]<=learnHigh[i],则该像素可合并于此码元
	uchar	max[CHANNELS];			// High side of box boundary
	// 属于此码元的像素中各通道的最大值
	uchar	min[CHANNELS];			// Low side of box boundary
	// 属于此码元的像素中各通道的最小值
	int		t_last_update;			// This is book keeping to allow us to kill stale entries
	// 此码元最后一次更新的时间,每一帧为一个单位时间,用于计算stale
	int		stale;					// max negative run (biggest period of inactivity)
	// 此码元最长不更新时间,用于删除规定时间不更新的码元,精简码本
} code_element;						// 码元的数据结构
typedef struct code_book {
	code_element	**cb;
	// 码元的二维指针,理解为指向码元指针数组的指针,使得添加码元时不需要来回复制码元,只需要简单的指针赋值即可
	int				numEntries;
	// 此码本中码元的数目
	int				t;				// count every access
	// 此码本现在的时间,一帧为一个时间单位
} codeBook;							// 码本的数据结构


 //*********************
 int cv_updateCodeBook(uchar *p, codeBook &c, unsigned *cbBounds, int numChannels);
 uchar cvbackgroundDiff(uchar *p, codeBook &c, int numChannels, int *minMod, int *maxMod);
void cvconnectedComponents(IplImage *mask, int poly1_hull0=1, float perimScale=4, int *num=NULL, CvRect *bbs=NULL, CvPoint *centers=NULL);
 #endif