
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>

// #include "MLNN.hpp"
#include "cJSON.h"
#include "logctrl.h"

#include "acvImage_BasicTool.hpp"
#include "mjpegLib.h"

#include <sys/stat.h>
#include <libgen.h>
#include <main.h>
#include <playground.h>
#include <stdexcept>
#include <compat_dirent.h>
#include <smem_channel.hpp>
#include <ctime>
#include "CameraLayerManager.hpp"

#include <opencv2/calib3d.hpp>
#include "opencv2/imgproc.hpp"
#include <opencv2/imgcodecs.hpp>

using namespace cv;

class InspectionTarget_DimMeasure : public InspectionTarget
{

public:
	void INIT(std::string id, cJSON *def, InspectionTargetManager *belongMan, std::string local_env_path);

	static std::string sTYPE()
	{
		return "DimMeasure";
	}
	virtual std::string TYPE()
	{
		return sTYPE();
	}

	shared_ptr<StageInfo_Orientation> template_input_info;

	int dbg_feature_id = -1;
	cJSON *dbg_feature_info = NULL;

	// enum FeatureType{

	// 	TYPE_LineFit=0,
	// 	TYPE_ArcFit=1,
	// 	TYPE_SearchPoint=2,

	// 	TYPE_Measure_Distance=3,
	// 	TYPE_Calc=4,
	// 	TYPE_NA=-999999
	// };

	struct id_idx
	{
		int id;
		int idx;
	};
	struct itemInfo
	{
		cJSON *cached_feature_def;
		int id;
		int idx;

		vector<id_idx> refIdIdx;

		float template_angle;

		shared_ptr<StageInfo_Orientation> inputInfo;
		int orientation_idx;

		bool processed;
		StageInfo_DimMeasure::MeasureResultInfo result;
	};
	vector<itemInfo> featureQuickList;

	
	struct cat_itemInfo
	{
		cJSON *cached_def;
		int idx;
		int id;

		struct limits_setup_t
		{
			id_idx ref_id_idx;
			float low_limit;
			float high_limit;
			string NG_as;
		};
		vector<limits_setup_t> limits_setup;
		StageInfo_DimMeasure::CategoryResultInfo result;
	};
	vector<cat_itemInfo> categoryQuickList;

	vector<cv::Mat> dbg_Images;

	virtual cJSON *genITIOInfo()
	{
		cJSON *info = genITInfo();

		{
			cJSON_AddItemToObject(info, "io", genIOInfo({StageInfo::stypeName()}, {StageInfo_DimMeasure::stypeName()}));
		}
		return info;
	}

	// future<int> futureInputStagePool();

	void setInspDef(cJSON *def);
	bool exchangeCMD(cJSON *info, int id, exchangeCMD_ACT &act);

	void run();

	bool executeFeature(const cv::Mat &mat_img2template,float mmpp,vector<itemInfo> &fqList, int featureIdx);

	bool executeCategory(const vector<itemInfo> &fqList,vector<cat_itemInfo> &catList, int categoryIdx);
	shared_ptr<StageInfo_DimMeasure> singleProcess(shared_ptr<StageInfo> sinfo,bool skipCache);

	virtual ~InspectionTarget_DimMeasure();
};
