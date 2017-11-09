#include <iostream>  
#include <fstream>  
#include <opencv2/core/core.hpp>  
#include <opencv2/highgui/highgui.hpp>  
#include <opencv2/imgproc/imgproc.hpp>  
#include <opencv2/objdetect/objdetect.hpp>  
#include <opencv2/ml/ml.hpp>  

using namespace std;
using namespace cv;

#define PosSamNO 100    //����������  
#define NegSamNO 700    //����������  
//HardExample�����������������HardExampleNO����0����ʾ�������ʼ���������󣬼�������HardExample����������  
//��ʹ��HardExampleʱ��������Ϊ0����Ϊ������������������������ά����ʼ��ʱ�õ����ֵ  
#define HardExampleNO 0   

#define TRAIN true    //�Ƿ����ѵ��,true��ʾ����ѵ����false��ʾ��ȡxml�ļ��е�SVMģ��  
#define CENTRAL_CROP true   //true:ѵ��ʱ����96*160��INRIA������ͼƬ���ó��м��64*128��С����  


//�̳���CvSVM���࣬��Ϊ����setSVMDetector()���õ��ļ���Ӳ���ʱ����Ҫ�õ�ѵ���õ�SVM��decision_func������  
//��ͨ���鿴CvSVMԴ���֪decision_func������protected���ͱ������޷�ֱ�ӷ��ʵ���ֻ�ܼ̳�֮��ͨ����������  
class MySVM : public CvSVM
{
public:
	//���SVM�ľ��ߺ����е�alpha����  
	double * get_alpha_vector()
	{
		return this->decision_func->alpha;
	}

	//���SVM�ľ��ߺ����е�rho����,��ƫ����  
	float get_rho()
	{
		return this->decision_func->rho;
	}
};
void generateDescriptors(ifstream& imagePath, HOGDescriptor& hog, vector<float>& descriptors, int& descriptorDim, 
	Mat& sampleFeatureMat, Mat& sampleLabelMat, int trainClass, string pathString, Rect rectCrop) {
	string imgName;
	int numLimit;
	if (0 == trainClass)
	{
		numLimit = PosSamNO;
	}
	else if (1 == trainClass)
	{
		numLimit = NegSamNO;
	}
	else if (2 == trainClass)
	{
		numLimit = HardExampleNO;
	}
	for (int num = 0; num < numLimit && getline(imagePath, imgName); num++)
	{
		imgName = pathString + imgName;//������������·����  
		Mat src = imread(imgName);//��ȡͼƬ  

		if (CENTRAL_CROP && 0 == trainClass)
			src = src(rectCrop);//��96*160��INRIA������ͼƬ����Ϊ64*128������ȥ�������Ҹ�16������  
								/*		imshow("....", src);
								waitKey(6000);			*/							 //resize(src,src,Size(64,128));  
		hog.compute(src, descriptors, Size(8, 8));//����HOG�����ӣ���ⴰ���ƶ�����(8,8)  
												  //�����һ������ʱ��ʼ�����������������������Ϊֻ��֪��������������ά�����ܳ�ʼ��������������  
												  //������õ�HOG�����Ӹ��Ƶ�������������sampleFeatureMat  
		if (0 == trainClass)
		{
			if (0 == num)
			{
				descriptorDim = descriptors.size();	//HOG�����ӵ�ά�� 
													//��ʼ������ѵ������������������ɵľ��������������������ĸ�������������HOG������ά��sampleFeatureMat  
				sampleFeatureMat = Mat::zeros(PosSamNO + NegSamNO + HardExampleNO, descriptorDim, CV_32FC1);
				//��ʼ��ѵ����������������������������������ĸ�������������1��1��ʾ���ˣ�0��ʾ����  
				sampleLabelMat = Mat::zeros(PosSamNO + NegSamNO + HardExampleNO, 1, CV_32FC1);
			}
			for (int i = 0; i < descriptorDim; i++)
				sampleFeatureMat.at<float>(num, i) = descriptors[i];//��num�����������������еĵ�i��Ԫ��  
			sampleLabelMat.at<float>(num, 0) = 1;//���������Ϊ1������
		}
		else if (1 == trainClass) {
			if (0 == num)
				descriptorDim = sampleFeatureMat.cols;
			for (int i = 0; i < descriptorDim; i++)
				sampleFeatureMat.at<float>(num + PosSamNO, i) = descriptors[i];//��num�����������������еĵ�i��Ԫ��  
			sampleLabelMat.at<float>(num + PosSamNO, 0) = -1;//���������Ϊ1������
		}
		else if (2 == trainClass)
		{
			if (0 == num)
				descriptorDim = sampleFeatureMat.cols;
			for (int i = 0; i < descriptorDim; i++)
				sampleFeatureMat.at<float>(num + PosSamNO + NegSamNO, i) = descriptors[i];//��num�����������������еĵ�i��Ԫ��  
			sampleLabelMat.at<float>(num + PosSamNO + NegSamNO, 0) = -1;//���������Ϊ1������
		}

	}
	descriptors.clear();
	return;
}

void trainSVM(string posPath,string negPath, string hardPath, HOGDescriptor& hog, Rect rectCrop, char* modelPath, vector<float>& descriptors) {

	ifstream finPos(posPath);
	ifstream finNeg(negPath);
	ifstream finHard(hardPath);
	int DescriptorDim;//HOG�����ӵ�ά������ͼƬ��С����ⴰ�ڴ�С�����С��ϸ����Ԫ��ֱ��ͼbin��������  
	MySVM svm;//SVM������
	//HOG����������
	string ImgName;//ͼƬ��(����·��) 
	Mat sampleFeatureMat;//����ѵ������������������ɵľ��������������������ĸ�������������HOG������ά��      
	Mat sampleLabelMat;//ѵ����������������������������������ĸ�������������1��1��ʾ���ˣ�-1��ʾ����  
	string posDataPath = "D:\\detectProject\\traindata\\";
	string negDataPath = "D:\\detectProject\\negativedata\\";
	string hardDataPath = "";

	generateDescriptors(finPos, hog, descriptors, DescriptorDim, sampleFeatureMat, sampleLabelMat, 0, posDataPath, rectCrop);
	generateDescriptors(finNeg, hog, descriptors, DescriptorDim, sampleFeatureMat, sampleLabelMat, 1, negDataPath, rectCrop);

	if (HardExampleNO > 0)
		//���ζ�ȡHardExample������ͼƬ������HOG������  
		generateDescriptors(finHard, hog, descriptors, DescriptorDim, sampleFeatureMat, sampleLabelMat, 2, hardDataPath, rectCrop);
	
	CvTermCriteria criteria = cvTermCriteria(CV_TERMCRIT_ITER + CV_TERMCRIT_EPS, 1000, FLT_EPSILON);
	//SVM������SVM����ΪC_SVC�����Ժ˺������ɳ�����C=0.01  
	CvSVMParams param(CvSVM::C_SVC, CvSVM::LINEAR, 0, 1, 0, 0.01, 0, 0, 0, criteria);
	cout << "��ʼѵ��SVM������" << endl;
	svm.train(sampleFeatureMat, sampleLabelMat, Mat(), Mat(), param);//ѵ��������  
	cout << "ѵ�����" << endl;
	svm.save(modelPath);//��ѵ���õ�SVMģ�ͱ���Ϊxml�ļ� 
	descriptors.clear();
	return;
}
	/*******************************************************************************************************************
	����SVMѵ����ɺ�õ���XML�ļ����棬��һ�����飬����support vector������һ�����飬����alpha,��һ��������������rho;
	��alpha����ͬsupport vector��ˣ�ע�⣬alpha*supportVector,���õ�һ����������֮���ٸ���������������һ��Ԫ��rho��
	��ˣ���õ���һ�������������ø÷�������ֱ���滻opencv�����˼��Ĭ�ϵ��Ǹ���������cv::HOGDescriptor::setSVMDetector()��
	���Ϳ����������ѵ������ѵ�������ķ������������˼���ˡ�
	********************************************************************************************************************/
void setDetector(MySVM& svm, vector<float>& myDetector, string detectorPath){
	int DescriptorDim = svm.get_var_count();//����������ά������HOG�����ӵ�ά��  
	int supportVectorNum = svm.get_support_vector_count();//֧�������ĸ���  
														  //cout << "֧������������" << supportVectorNum << endl;

	Mat alphaMat = Mat::zeros(1, supportVectorNum, CV_32FC1);//alpha���������ȵ���֧����������  
	Mat supportVectorMat = Mat::zeros(supportVectorNum, DescriptorDim, CV_32FC1);//֧����������  
	Mat resultMat = Mat::zeros(1, DescriptorDim, CV_32FC1);//alpha��������֧����������Ľ��  

														   //��֧�����������ݸ��Ƶ�supportVectorMat������  
	for (int i = 0; i < supportVectorNum; i++)
	{
		const float * pSVData = svm.get_support_vector(i);//���ص�i��֧������������ָ��  
		for (int j = 0; j < DescriptorDim; j++)
		{
			//cout<<pData[j]<<" ";  
			supportVectorMat.at<float>(i, j) = pSVData[j];
		}
	}

	//��alpha���������ݸ��Ƶ�alphaMat��  
	double * pAlphaData = svm.get_alpha_vector();//����SVM�ľ��ߺ����е�alpha����  
	for (int i = 0; i < supportVectorNum; i++)
	{
		alphaMat.at<float>(0, i) = pAlphaData[i];
	}

	//����-(alphaMat * supportVectorMat),����ŵ�resultMat��  
	//gemm(alphaMat, supportVectorMat, -1, 0, 1, resultMat);//��֪��Ϊʲô�Ӹ��ţ�  
	resultMat = -1 * alphaMat * supportVectorMat;

	//��resultMat�е����ݸ��Ƶ�����myDetector��  
	for (int i = 0; i < DescriptorDim; i++)
	{
		myDetector.push_back(resultMat.at<float>(0, i));
	}
	//������ƫ����rho���õ������  
	myDetector.push_back(svm.get_rho());
	cout << "�����ά����" << myDetector.size() << endl;

	//�������Ӳ������ļ�  
	ofstream fout(detectorPath);
	for (int i = 0; i < myDetector.size(); i++)
		fout << myDetector[i] << endl;
	
	return;
}

void DetectAndDraw(Mat& src, Mat &trtd, HOGDescriptor& hog, vector<Rect>& found, vector<Rect>& found_filtered){

	//��ͼƬ���ж�߶����˼��
	hog.detectMultiScale(src(Range(300, 720), Range(0, 1280)), found, 0, Size(8, 8), Size(32, 32), 1.05, 2);
	//!!!!!!!!!!!!!!!!!!!!!!!!!!!�߽�ȷ��ע��

	//�ҳ�����û��Ƕ�׵ľ��ο�r,������found_filtered��,�����Ƕ�׵Ļ�,��ȡ���������Ǹ����ο����found_filtered��  
	for (int i = 0; i < found.size(); i++)
	{
		Rect r = found[i];
		int j = 0;
		for (; j < found.size(); j++)
			if (j != i && (r & found[j]) == r)
				break;
		if (j == found.size())
			found_filtered.push_back(r);
	}

	//�����ο���Ϊhog�����ľ��ο��ʵ�������Ҫ��΢��Щ,����������Ҫ��һЩ����  
	for (int i = 0; i < found_filtered.size(); i++)
	{
		Rect r = found_filtered[i];
		r.x += cvRound(r.width*0.1);
		r.width = cvRound(r.width*0.8);
		r.y += cvRound(r.height*0.07);
		r.y += 300;
		//!!!!������ı߽��Ӧ
		r.height = cvRound(r.height*0.8);
		rectangle(trtd, r.tl(), r.br(), Scalar(0, 255, 0), 3);
	}
	return;
}

int main()
{
	int trainType = 2;
	Rect rectCrop;
	string posPath, negPath, hardPath, detectorPath;
	char* modelPath = "";
	Size winSize, blockSize, blockStride, cellSize;

	if (1 == trainType)
	{
		posPath = "D:\\detectProject\\SmallTrainData.txt";//������ͼƬ���ļ����б�
		negPath = "D:\\detectProject\\NegativeData1.txt";//������ͼƬ���ļ����б�
		hardPath = "";
		modelPath = "D:\\detectProject\\model\\SVM_HOG_S.xml";
		detectorPath = "D:\\detectProject\\model\\HOGDetector_S.txt";
		winSize = Size(48, 96);
		blockSize = Size(16, 16);
		blockStride = Size(8, 8);
		cellSize = Size(8, 8);
		rectCrop = Rect(1, 2, 48, 96);
	}
	else if (2 == trainType)
	{
		posPath = "D:\\detectProject\\MiddleTrainData.txt";//������ͼƬ���ļ����б�
		negPath = "D:\\detectProject\\NegativeData2.txt";//������ͼƬ���ļ����б�
		hardPath = "";
		modelPath = "D:\\detectProject\\model\\SVM_HOG_M.xml";
		detectorPath = "D:\\detectProject\\model\\HOGDetector_S.txt";
		winSize = Size(96, 192);
		blockSize = Size(16, 16);
		blockStride = Size(8, 8);
		cellSize = Size(8, 8);
		rectCrop = Rect(2, 4, 96, 192);
	}
	else if (3 == trainType)
	{
		posPath = "D:\\detectProject\\LargeTrainData.txt";//������ͼƬ���ļ����б�
		negPath = "D:\\detectProject\\NegativeData3.txt";//������ͼƬ���ļ����б�
		hardPath = "";
		modelPath = "D:\\detectProject\\model\\SVM_HOG_L.xml";
		detectorPath = "D:\\detectProject\\model\\HOGDetector_S.txt";
		winSize = Size(192, 384);
		blockSize = Size(16, 16);
		blockStride = Size(8, 8);
		cellSize = Size(8, 8);
		rectCrop = Rect(4, 8, 192, 384);
	}

	//vector<float> descriptors;
	HOGDescriptor hog(winSize, blockSize, blockStride, cellSize, 9);
	//trainSVM(posPath, negPath, hardPath, hog, rectCrop, modelPath, descriptors);

	MySVM svm;
	vector<float> myDetector;
	svm.load(modelPath);
	setDetector(svm, myDetector, detectorPath);
	hog.setSVMDetector(myDetector);

	/**************����ͼƬ����HOG���˼��******************/
	vector<Rect> found, found_filtered;//���ο�����
	string detectDataPath = "D:\\detectProject\\";
	ifstream finDetect(detectDataPath);
	string detectData;
	Mat src,trtd;
	for(int num = 0;getline(finDetect, detectData);num++)
	{
		src = imread("D:\\detectProject\\test.jpg");
		trtd = src.clone();
		DetectAndDraw(src, trtd, hog, found, found_filtered);
		found.clear();
		found_filtered.clear();
		imwrite("D:\\detectProject\\processed\\ImgProcessed.jpg", trtd);
	}
	//namedWindow("src", 0);
	//imshow("src", trtd);
	//waitKey();//ע�⣺imshow֮������waitKey�������޷���ʾͼ��  
	system("pause");
}

//���ζ�ȡ������ͼƬ������HOG������  
//for (int num = 0; num < PosSamNO && getline(finPos, ImgName); num++)
//{
//	//cout << "����" << ImgName << num << endl;
//	ImgName = "D:\\detectProject\\traindata\\" + ImgName;//������������·����  
//	Mat src = imread(ImgName);//��ȡͼƬ  
//	//imshow("....", src);
//	//waitKey(6000);
//	if (CENTRAL_CROP)
//		src = src(Rect(16, 16, 64, 128));//��96*160��INRIA������ͼƬ����Ϊ64*128������ȥ�������Ҹ�16������  
//										 //resize(src,src,Size(64,128));  
//	hog.compute(src, descriptors, Size(8, 8));//����HOG�����ӣ���ⴰ���ƶ�����(8,8)  
//											  //�����һ������ʱ��ʼ�����������������������Ϊֻ��֪��������������ά�����ܳ�ʼ��������������  
//	cout << descriptors.size() << endl;
//	if (0 == num)
//	{
//		DescriptorDim = descriptors.size();//HOG�����ӵ�ά��  
//										   //��ʼ������ѵ������������������ɵľ��������������������ĸ�������������HOG������ά��sampleFeatureMat  
//		sampleFeatureMat = Mat::zeros(PosSamNO + NegSamNO + HardExampleNO, DescriptorDim, CV_32FC1);
//		//��ʼ��ѵ����������������������������������ĸ�������������1��1��ʾ���ˣ�0��ʾ����  
//		sampleLabelMat = Mat::zeros(PosSamNO + NegSamNO + HardExampleNO, 1, CV_32FC1);
//	}
//	//������õ�HOG�����Ӹ��Ƶ�������������sampleFeatureMat  
//	for (int i = 0; i < DescriptorDim; i++)
//		sampleFeatureMat.at<float>(num, i) = descriptors[i];//��num�����������������еĵ�i��Ԫ��  
//	sampleLabelMat.at<float>(num, 0) = 1;//���������Ϊ1������
//	descriptors.clear();
//}

////���ζ�ȡ������ͼƬ������HOG������  
//for (int num = 0; num < NegSamNO && getline(finNeg, ImgName); num++)
//{
//	//cout << "����" << ImgName << num << endl;
//	ImgName = "D:\\detectProject\\negativedata\\" + ImgName;//���ϸ�������·����  
//	Mat src = imread(ImgName);//��ȡͼƬ  
//							  //resize(src,img,Size(64,128));  
//	//imshow("....", src);
//	//waitKey(6000);
//	hog.compute(src, descriptors, Size(8, 8));//����HOG�����ӣ���ⴰ���ƶ�����(8,8)  
//											  //cout<<"������ά����"<<descriptors.size()<<endl;  
//											  //������õ�HOG�����Ӹ��Ƶ�������������sampleFeatureMat  
//	for (int i = 0; i < DescriptorDim; i++)
//		sampleFeatureMat.at<float>(num + PosSamNO, i) = descriptors[i];//��PosSamNO+num�����������������еĵ�i��Ԫ��  
//	sampleLabelMat.at<float>(num + PosSamNO, 0) = -1;//���������Ϊ-1������  
//	descriptors.clear();
//}

//for (int num = 0; num < HardExampleNO && getline(finHardExample, ImgName); num++)
//{
//	cout << "����" << ImgName << endl;
//	ImgName = "D:\\DataSet\\HardExample_2400PosINRIA_12000Neg\\" + ImgName;//����HardExample��������·����  
//	Mat src = imread(ImgName);//��ȡͼƬ  
//							  //resize(src,img,Size(64,128));  
//	hog.compute(src, descriptors, Size(8, 8));//����HOG�����ӣ���ⴰ���ƶ�����(8,8)  
//											  //cout<<"������ά����"<<descriptors.size()<<endl; 
//											  //������õ�HOG�����Ӹ��Ƶ�������������sampleFeatureMat  
//	for (int i = 0; i < DescriptorDim; i++)
//		sampleFeatureMat.at<float>(num + PosSamNO + NegSamNO, i) = descriptors[i];//��PosSamNO+num�����������������еĵ�i��Ԫ��  
//	sampleLabelMat.at<float>(num + PosSamNO + NegSamNO, 0) = -1;//���������Ϊ-1������  
//	descriptors.clear();
//}


////��ⴰ��(64,128),��ߴ�(16,16),�鲽��(8,8),cell�ߴ�(8,8),ֱ��ͼbin����9  
//HOGDescriptor hog(winSize, blockSize, blockStride, cellSize, 9);//HOG���������������HOG�����ӵ�  
//int DescriptorDim;//HOG�����ӵ�ά������ͼƬ��С����ⴰ�ڴ�С�����С��ϸ����Ԫ��ֱ��ͼbin��������  
//MySVM svm;//SVM������
//vector<float> descriptors;//HOG����������
////namedWindow("~.~");
//		  //��TRAINΪtrue������ѵ��������  
//if (TRAIN)
//{
//	string ImgName;//ͼƬ��(����·��)  
//	ifstream finPos("D:\\detectProject\\LargeTrainData.txt");//������ͼƬ���ļ����б�  
//	ifstream finNeg("D:\\detectProject\\NegativeData3.txt");//������ͼƬ���ļ����б�  

//	Mat sampleFeatureMat;//����ѵ������������������ɵľ��������������������ĸ�������������HOG������ά��      
//	Mat sampleLabelMat;//ѵ����������������������������������ĸ�������������1��1��ʾ���ˣ�-1��ʾ����  

//	string trainPath = "D:\\detectProject\\traindata\\";
//	string bgPath = "D:\\detectProject\\negativedata\\";
//	//���ζ�ȡ������ͼƬ������HOG������  
//	generateDescriptors(finPos, hog, descriptors, DescriptorDim, sampleFeatureMat, sampleLabelMat, 0, trainPath);
//	//���ζ�ȡ������ͼƬ������HOG������  
//	generateDescriptors(finNeg, hog, descriptors, DescriptorDim, sampleFeatureMat, sampleLabelMat, 1, bgPath);
//	
//	//����HardExample������  
//	if (HardExampleNO > 0)
//	{
//		ifstream finHardExample("HardExample_2400PosINRIA_12000NegList.txt");//HardExample������ͼƬ���ļ����б�
//		string hardPath = "D:\\DataSet\\HardExample_2400PosINRIA_12000Neg\\";
//		generateDescriptors(finHardExample, hog, descriptors, DescriptorDim, sampleFeatureMat, sampleLabelMat, 2, hardPath);																	 //���ζ�ȡHardExample������ͼƬ������HOG������  
//	}

//	////���������HOG�������������ļ�  
//	/*ofstream fout("D:\\detectProject\\SampleFeatureMat.txt");  
//	for(int i=0; i<PosSamNO+NegSamNO; i++)  
//	{  
//	  fout<<i<<endl;  
//	  for(int j=0; j<DescriptorDim; j++)  
//	      fout<<sampleFeatureMat.at<float>(i,j)<<"  ";  
//	  fout<<endl;  
//	} */ 

//	//ѵ��SVM������  
//	//������ֹ��������������1000�λ����С��FLT_EPSILONʱֹͣ����  
//	CvTermCriteria criteria = cvTermCriteria(CV_TERMCRIT_ITER + CV_TERMCRIT_EPS, 1000, FLT_EPSILON);
//	//SVM������SVM����ΪC_SVC�����Ժ˺������ɳ�����C=0.01  
//	CvSVMParams param(CvSVM::C_SVC, CvSVM::LINEAR, 0, 1, 0, 0.01, 0, 0, 0, criteria);
//	cout << "��ʼѵ��SVM������" << endl;
//	svm.train(sampleFeatureMat, sampleLabelMat, Mat(), Mat(), param);//ѵ��������  
//	cout << "ѵ�����" << endl;
//	svm.save("D:\\detectProject\\model\\SVM_HOG.xml");//��ѵ���õ�SVMģ�ͱ���Ϊxml�ļ�  
//}
//else //��TRAINΪfalse����XML�ļ���ȡѵ���õķ�����  
//{
//	svm.load("D:\\detectProject\\model\\SVM_HOG.xml");//��XML�ļ���ȡѵ���õ�SVMģ��  
//}


//int DescriptorDim = svm.get_var_count();//����������ά������HOG�����ӵ�ά��  
//int supportVectorNum = svm.get_support_vector_count();//֧�������ĸ���  
////cout << "֧������������" << supportVectorNum << endl;

//Mat alphaMat = Mat::zeros(1, supportVectorNum, CV_32FC1);//alpha���������ȵ���֧����������  
//Mat supportVectorMat = Mat::zeros(supportVectorNum, DescriptorDim, CV_32FC1);//֧����������  
//Mat resultMat = Mat::zeros(1, DescriptorDim, CV_32FC1);//alpha��������֧����������Ľ��  

//													   //��֧�����������ݸ��Ƶ�supportVectorMat������  
//for (int i = 0; i < supportVectorNum; i++)
//{
//	const float * pSVData = svm.get_support_vector(i);//���ص�i��֧������������ָ��  
//	for (int j = 0; j < DescriptorDim; j++)
//	{
//		//cout<<pData[j]<<" ";  
//		supportVectorMat.at<float>(i, j) = pSVData[j];
//	}
//}

////��alpha���������ݸ��Ƶ�alphaMat��  
//double * pAlphaData = svm.get_alpha_vector();//����SVM�ľ��ߺ����е�alpha����  
//for (int i = 0; i < supportVectorNum; i++)
//{
//	alphaMat.at<float>(0, i) = pAlphaData[i];
//}

////����-(alphaMat * supportVectorMat),����ŵ�resultMat��  
////gemm(alphaMat, supportVectorMat, -1, 0, 1, resultMat);//��֪��Ϊʲô�Ӹ��ţ�  
//resultMat = -1 * alphaMat * supportVectorMat;

////�õ����յ�setSVMDetector(const vector<float>& detector)�����п��õļ����  
//vector<float> myDetector;
////��resultMat�е����ݸ��Ƶ�����myDetector��  
//for (int i = 0; i < DescriptorDim; i++)
//{
//	myDetector.push_back(resultMat.at<float>(0, i));
//}
////������ƫ����rho���õ������  
//myDetector.push_back(svm.get_rho());
//cout << "�����ά����" << myDetector.size() << endl;
////����HOGDescriptor�ļ����  
//hog.setSVMDetector(myDetector);
////myHOG.setSVMDetector(HOGDescriptor::getDefaultPeopleDetector());  

////�������Ӳ������ļ�  
//ofstream fout("D:\\detectProject\\HOGDetectorForOpenCV.txt");
//for (int i = 0; i < myDetector.size(); i++)
//{
//	fout << myDetector[i] << endl;
//}

/******************���뵥��64*128�Ĳ���ͼ������HOG�����ӽ��з���*********************/
////��ȡ����ͼƬ(64*128��С)����������HOG������  
////Mat testImg = imread("person014142.jpg");  
//Mat testImg = imread("noperson000026.jpg");  
//vector<float> descriptor;  
//hog.compute(testImg,descriptor,Size(8,8));//����HOG�����ӣ���ⴰ���ƶ�����(8,8)  
//Mat testFeatureMat = Mat::zeros(1,3780,CV_32FC1);//����������������������  
////������õ�HOG�����Ӹ��Ƶ�testFeatureMat������  
//for(int i=0; i<descriptor.size(); i++)  
//  testFeatureMat.at<float>(0,i) = descriptor[i];  

////��ѵ���õ�SVM�������Բ���ͼƬ�������������з���  
//int result = svm.predict(testFeatureMat);//�������  
//cout<<"��������"<<result<<endl;  

////cout << "���ж�߶�HOG������" << endl;
//hog.detectMultiScale(src(Range(300, 720), Range(0, 1280)), found, 0, Size(8, 8), Size(32, 32), 1.05, 2);//��ͼƬ���ж�߶����˼��  
////!!!!!!!!!!!!!!!!!!!!!!!!!!!�߽�ȷ��ע��
////cout << "�ҵ��ľ��ο������" << found.size() << endl;

////�ҳ�����û��Ƕ�׵ľ��ο�r,������found_filtered��,�����Ƕ�׵Ļ�,��ȡ���������Ǹ����ο����found_filtered��  
//for (int i = 0; i < found.size(); i++)
//{
//	Rect r = found[i];
//	int j = 0;
//	for (; j < found.size(); j++)
//		if (j != i && (r & found[j]) == r)
//			break;
//	if (j == found.size())
//		found_filtered.push_back(r);
//}
////�����ο���Ϊhog�����ľ��ο��ʵ�������Ҫ��΢��Щ,����������Ҫ��һЩ����  
//for (int i = 0; i < found_filtered.size(); i++)
//{
//	Rect r = found_filtered[i];
//	r.x += cvRound(r.width*0.1);
//	r.width = cvRound(r.width*0.8);
//	r.y += cvRound(r.height*0.07);
//	r.y += 300;
//	//!!!!������ı߽��Ӧ
//	r.height = cvRound(r.height*0.8);
//	rectangle(src, r.tl(), r.br(), Scalar(0, 255, 0), 3);
//}