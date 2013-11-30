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
   //���㷨Ϊͼ����ÿһ�����ص㽨��һ���뱾��ÿ���뱾���԰��������Ԫ��
	  // ÿ����Ԫ������ѧϰʱ�����С��ֵ�����ʱ�������С��ֵ�ȳ�Ա��
	  // �ڱ�����ģ�ڼ䣬ÿ������һ����ͼƬ����ÿ�����ص�����뱾ƥ�䣬
	  // Ҳ����˵���������ֵ���뱾��ĳ����Ԫ��ѧϰ��ֵ�ڣ�
	  // ����Ϊ�����ȥ�ö�Ӧ����ֹ�����ʷ���ƫ�벻��ͨ��һ��������ֵ�Ƚϣ�
	  // ���������������ʱ�����Ը��¶�Ӧ���ѧϰ��ֵ�ͼ����ֵ��
	  // �������������ֵ���뱾��ÿ����Ԫ����ƥ�䣬���п��������ڱ����Ƕ�̬�ģ�
	  // ����������ҪΪ�佨��һ���µ���Ԫ������������Ӧ����Ԫ��Ա������
	  // ��ˣ��ڱ���ѧϰ�Ĺ����У�ÿ�����ص���Զ�Ӧ�����Ԫ�������Ϳ���ѧ�����ӵĶ�̬����
 #ifndef AVGSEG_1
 #define AVGSEG_1
 
 #include "cv.h"                // define all of the opencv classes etc.
 #include "highgui.h"
 #include "cxcore.h"
 #include "avg_background.h"
#define CHANNELS 3        
// ���ô����ͼ��ͨ����,Ҫ��С�ڵ���ͼ�����ͨ����  

typedef struct ce {
	uchar	learnHigh[CHANNELS];	// High side threshold for learning
	// ����Ԫ��ͨ���ķ�ֵ����(ѧϰ����)
	uchar	learnLow[CHANNELS];		// Low side threshold for learning
	// ����Ԫ��ͨ���ķ�ֵ����
	// ѧϰ���������һ�������ظ�ͨ��ֵx[i],���� learnLow[i]<=x[i]<=learnHigh[i],������ؿɺϲ��ڴ���Ԫ
	uchar	max[CHANNELS];			// High side of box boundary
	// ���ڴ���Ԫ�������и�ͨ�������ֵ
	uchar	min[CHANNELS];			// Low side of box boundary
	// ���ڴ���Ԫ�������и�ͨ������Сֵ
	int		t_last_update;			// This is book keeping to allow us to kill stale entries
	// ����Ԫ���һ�θ��µ�ʱ��,ÿһ֡Ϊһ����λʱ��,���ڼ���stale
	int		stale;					// max negative run (biggest period of inactivity)
	// ����Ԫ�������ʱ��,����ɾ���涨ʱ�䲻���µ���Ԫ,�����뱾
} code_element;						// ��Ԫ�����ݽṹ
typedef struct code_book {
	code_element	**cb;
	// ��Ԫ�Ķ�άָ��,���Ϊָ����Ԫָ�������ָ��,ʹ�������Ԫʱ����Ҫ���ظ�����Ԫ,ֻ��Ҫ�򵥵�ָ�븳ֵ����
	int				numEntries;
	// ���뱾����Ԫ����Ŀ
	int				t;				// count every access
	// ���뱾���ڵ�ʱ��,һ֡Ϊһ��ʱ�䵥λ
} codeBook;							// �뱾�����ݽṹ


 //*********************
 int cv_updateCodeBook(uchar *p, codeBook &c, unsigned *cbBounds, int numChannels);
 uchar cvbackgroundDiff(uchar *p, codeBook &c, int numChannels, int *minMod, int *maxMod);
void cvconnectedComponents(IplImage *mask, int poly1_hull0=1, float perimScale=4, int *num=NULL, CvRect *bbs=NULL, CvPoint *centers=NULL);
 #endif