#pragma once

#include "../../include/common.hpp"
#include "../../include/predictor.hpp"
#include "../perspective_mapping.cpp"
#include "../recognize/track_recognition.cpp"
#include <cmath>
#include <fstream>
#include <iostream>
#include <opencv2/highgui.hpp>
#include <opencv2/opencv.hpp>

#include "../motion_control.cpp"



using namespace cv;
using namespace std;

class Rescue {
public:
  Rescue()
	{
		loadParams();
	}
public:
  struct Params   //ExitSpeed  CruiseSpeed  EnterSpeed  EnableSpeed  StopTime
    {
		uint16_t DepotCheck = 3;
		uint16_t DepotDir = 0;
		double DangerClose = 100.0;       // 智能车危险距离
		uint16_t ServoRow = 120;
		uint16_t ServoValue = 15;
		float DepotSpeed = 0.5;
		uint16_t DelayCnt = 3;
		uint16_t BrakeCnt = 5;

    uint16_t StopTime = 30;
    uint16_t EnterTime = 50;
    float EnableSpeed = 0.7;
    float EnterSpeed = 1.0;
    float CruiseSpeed = 1.2;
    float ExitSpeed = 1.0;
    uint16_t RescueCount = 1;
    uint16_t LeftCount = 1;
    uint16_t RightCount = 1;
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(
            Params, DepotCheck, DepotDir, DangerClose, ServoRow, ServoValue, DepotSpeed, DelayCnt, BrakeCnt,
            StopTime, EnterTime, EnableSpeed, EnterSpeed, CruiseSpeed, ExitSpeed, RescueCount, LeftCount, RightCount); // 添加构造函数
    };
  Params params;                   // 读取控制参数
  bool carStoping = false;  // 停车标志
  bool carExitting = false; // 出库标志
  bool coneDetected = false;
  bool BackSpeedFlag = false;
  bool EnterFlag = false;     //允许入库标志位

  int counterStop = 0;
  float _speed = 0;
  bool dangerRec = true;    //救援区出库屏蔽危险区标志位
  

  enum Step {
    None = 0, // AI检测
    Enable,   // 使能（标志识别成功）
    Enter,    // 进站
    Cruise,   // 巡航
    Stop,     // 停车
    Exit      // 出站
  };

  Step step = Step::None;
  bool entryLeft = true; // 左入库使能标志
  /**
   * @brief 检测初始化
   *
   */
  void reset(void) {
    carStoping = false;
    carExitting = false;
    step = Step::None;
    counterSession = 0;         // 图像场次计数器
    counterRec = 0;             // 标志检测计数器
    lastPointsEdgeLeft.clear(); // 记录上一场边缘点集（丢失边）
    lastPointsEdgeRight.clear();
    counterExit = 0;
    counterImmunity = 0;
  }

  /**
   * @brief 检测与路径规划  没用
   *
   * @param track 赛道识别结果
   * @param detection AI检测结果
   */
  bool rescueDetection(TrackRecognition &track, vector<PredictResult> predict) //, cv::Mat img_rgb
  {
    _pointNearCone = POINT(0, 0);
    _distance = 0;
    pointConeLeft.clear();
    pointConeRight.clear();
    levelCones = 0;
    indexDebug = 0;

    switch (step) {
    case Step::None: //[01] 标志检测
      if ((counterImmunity > 200 && again) ||
          (counterImmunity > 30 && !again)) {
        for (int i = 0; i < predict.size(); i++) {
          if (predict[i].label == LABEL_TUMBLE ||
              predict[i].label == LABEL_PATIENT) // 伤员平民标志检测
          {
            counterRec++;
            break;
          }
        }
        for (int i = 0; i < predict.size(); i++) {
          if (predict[i].label == LABEL_EVIL ||
              predict[i].label == LABEL_THIEF) // 坏人标志检测
          {
            counterExit++;
            break;
          }
        }

        if (counterRec || counterExit) {
          counterSession++;
          if (counterRec >= params.RescueCount && counterSession <= 10)  //3  8  1 5 10(挺好) 8 3 6  2
          {
            step = Step::Enable; // 使能
            entryLeft = true;
            counterRec = 0;
            counterExit = 0;
            counterSession = 0;
          } else if (counterExit >= params.RescueCount && counterSession <= 10) 
          {
            step = Step::Enable; // 使能
            entryLeft = false;
            counterRec = 0;
            counterExit = 0;
            counterSession = 0;
          } else if (counterSession > 20) {
            counterRec = 0;
            counterSession = 0;
          }
        }
      } else
        counterImmunity++;
      break;

    case Step::Enable: //[02] 使能
    {
      counterExit++;
      if (counterExit > 300) // 超时退出  150
      {
        reset();
        return false;
      }

      searchCones(predict); // 搜索赛道左右两边锥桶
      // _coneRects = detectCones(img_rgb);
			// searchCones(_coneRects, track.rowCutUp);
      if (entryLeft)        // 左入库
      {
        _pointNearCone = getConeLeftDown(track.pointsEdgeLeft,
                                         pointConeLeft); // 搜索左下锥桶  
        // if (_pointNearCone.x > params.ServoRow && _pointNearCone.x < params.ServoRow + ROWSIMAGE / 3
				// && _pointNearCone.y != 0 && counterSession > params.DelayCnt/* && _slowdown*/) // 当车辆开始靠近右边锥桶：准备入库                                         
         if (_pointNearCone.x > ROWSIMAGE * 0.5) // 当车辆开始靠近右边锥桶：准备入库  //0.4
        {
          counterRec++;
          if (counterRec >= params.LeftCount)  //2 && EnterFlag
          {
            // if(BackSpeedFlag)     //我加的
            // {
              step = Step::Enter; // 进站使能
              counterRec = 0;
              counterSession = 0;
              counterExit = 0;
              pathsEdgeLeft.clear();
              pathsEdgeRight.clear();
           // }
            
          }
        }
      } else // 右入库
      {
        _pointNearCone = getConeRightDown(track.pointsEdgeRight,
                                          pointConeRight); // 搜索左下锥桶
       //  if (_pointNearCone.x > params.ServoRow && _pointNearCone.x < params.ServoRow + ROWSIMAGE / 3
			// 	&& _pointNearCone.y != 0 && counterSession > params.DelayCnt/* && _slowdown*/) // 当车辆开始靠近右边锥桶：准备入库                                   
        if (_pointNearCone.x > ROWSIMAGE * 0.50) // 当车辆开始靠近右边锥桶：准备入库 //0.4
        {
          counterRec++;
          if (counterRec >= params.RightCount) // && EnterFlag
          {
           // if(BackSpeedFlag)     //我加的
           // {
              step = Step::Enter; // 进站使能
              counterRec = 0;
              counterSession = 0;
              counterExit = 0;
              pathsEdgeLeft.clear();
              pathsEdgeRight.clear();
           //}
            
          }
        }
      }
      break;
    }
    case Step::Enter: //[03] 入库使能
    {
    
        counterSession++; // 屏蔽期:防止提前入库 counterSession
      if (counterSession > params.DepotCheck) //4  8 100
      {
        // if (track.pointsEdgeLeft.size() > ROWSIMAGE / 2 &&
        //     track.pointsEdgeRight.size() > ROWSIMAGE / 2) 
        // {
        //   counterExit++;
        //   if (counterExit > 30)  //30
        //   {
        //     counterExit = 0;
        //     step = Step::Stop; // 停车使能
        //     counterRec = 0;
        //     counterSession = 0;
        //   }
        // }

        if (track.pointsEdgeLeft.size() < ROWSIMAGE / 2 &&
            track.pointsEdgeRight.size() < ROWSIMAGE / 2) // 赛道还未丢失
        {
          counterRec++;
          if (counterRec > 10)  //10
          {
            counterRec = 0;
            step = Step::Cruise; // 巡航使能
            counterSession = 0;
          }
        }

        if (entryLeft) // 左入库
        {
          POINT start = POINT(ROWSIMAGE - 40, COLSIMAGE - 1);
          POINT end = POINT(50, 0);
          POINT middle =
              POINT((start.x + end.x) * 0.4, (start.y + end.y) * 0.6);
          vector<POINT> input = {start, middle, end};
          track.pointsEdgeRight = Bezier(0.05, input); // 补线
          track.pointsEdgeLeft =
              predictEdgeLeft(track.pointsEdgeRight); // 由右边缘补偿左边缘
          lastPointsEdgeLeft = track.pointsEdgeLeft;
          lastPointsEdgeRight = track.pointsEdgeRight;

          pathsEdgeLeft.push_back(track.pointsEdgeLeft); // 记录进厂轨迹
          pathsEdgeRight.push_back(track.pointsEdgeRight);
        } else // 右入库
        {
          POINT start = POINT(ROWSIMAGE - 40, 0);
          POINT end = POINT(50, COLSIMAGE - 1);  //50
          POINT middle = POINT((start.x + end.x) * 0.4, (start.y + end.y) * 0.6);  //0.4 0.6
          vector<POINT> input = {start, middle, end};
          track.pointsEdgeLeft = Bezier(0.05, input); // 补线
          track.pointsEdgeRight =
              predictEdgeRight(track.pointsEdgeLeft); // 由右边缘补偿左边缘
          lastPointsEdgeLeft = track.pointsEdgeLeft;
          lastPointsEdgeRight = track.pointsEdgeRight;

          pathsEdgeLeft.push_back(track.pointsEdgeLeft); // 记录进厂轨迹
          pathsEdgeRight.push_back(track.pointsEdgeRight);
        }
      }
      break;
      
      
    }

    case Step::Cruise: //[04] 巡航使能
    {
      counterSession++;
      if (counterSession > 2) // 超时保护 10 5  2 //如果入库停车太晚，就减小这个值
      {
        step = Step::Stop; // 停车使能 
        counterRec = 0;
        counterSession = 0;
        counterExit = 0;
      }

      if (track.pointsEdgeLeft.size() > ROWSIMAGE / 2 &&
          track.pointsEdgeRight.size() > ROWSIMAGE / 2) {
        counterExit++;
        if (counterExit > 2)  //15 //如果入库停车太晚，就减小这个值
        {
          counterExit = 0;
          step = Step::Stop; // 停车使能
          counterRec = 0;
        }
      }

      lastPointsEdgeLeft = track.pointsEdgeLeft;
      lastPointsEdgeRight = track.pointsEdgeRight;

      pathsEdgeLeft.push_back(track.pointsEdgeLeft); // 记录进厂轨迹
      pathsEdgeRight.push_back(track.pointsEdgeRight);
      break;
    }

    case Step::Stop: //[05] 停车使能
    {
      carStoping = true;
      counterRec++;
      if (counterRec > 20) // 停车：20场 = 2s  40
      {
        carStoping = false;
        carExitting = true;
        step = Step::Exit; // 出站使能
        counterRec = 0;
      }
      track.pointsEdgeLeft = pathsEdgeLeft[pathsEdgeLeft.size() - 1];//维持入库最后的打角
			track.pointsEdgeRight = pathsEdgeRight[pathsEdgeRight.size() - 1];
      break;
    }

    case Step::Exit: //[06] 出站使能
    {
      carExitting = true;
      if (pathsEdgeLeft.size() < 2 || pathsEdgeRight.size() < 2) 
      {
        if((track.pointsEdgeLeft.size() > 60&& track.pointsEdgeRight.size() > 60)) 
				{
					counterRec++;
				}
				track.pointsEdgeLeft = pathsEdgeLeft[pathsEdgeLeft.size() - 1];
				track.pointsEdgeRight = pathsEdgeRight[pathsEdgeRight.size() - 1];

				if(counterRec > params.BrakeCnt)
				{
					step = Step::None; // 出站完成
          carExitting = false;
          again = true; // 第二次进入救援区标志
					reset();
				}
        // step = Step::None; // 出站完成
        // carExitting = false;
        // again = true; // 第二次进入救援区标志
        // reset();
      } else 
      {
        track.pointsEdgeLeft = pathsEdgeLeft[pathsEdgeLeft.size() - 1];
        track.pointsEdgeRight = pathsEdgeRight[pathsEdgeRight.size() - 1];
        pathsEdgeLeft.pop_back();
        pathsEdgeRight.pop_back();
      }
      break;
    }
    }

    if (step == Step::None) // 返回控制模式标志
      return false;
    else
      return true;
  }

  /**
     * @brief 加载配置参数Json
     */
    void loadParams() 
    {
        string jsonPath = "../src/config/rescue.json";
        std::ifstream config_is(jsonPath);
        if (!config_is.good()) 
        {
            std::cout << "Error: Params file path:[" << jsonPath << "] not find .\n";
            exit(-1);
        }

        nlohmann::json js_value;
        config_is >> js_value;

        try 
        {
            params = js_value.get<Params>();
        }
        catch (const nlohmann::detail::exception &e) 
        {
            std::cerr << "Json Params Parse failed :" << e.what() << '\n';
            exit(-1);
        }
    }




  /**
   * @brief 识别结果图像绘制
   *
   */
  void drawImage(TrackRecognition track, Mat &image) {
    // 赛道边缘
    for (int i = 0; i < track.pointsEdgeLeft.size(); i++) {
      circle(image, Point(track.pointsEdgeLeft[i].y, track.pointsEdgeLeft[i].x),
             1, Scalar(0, 255, 0), -1); // 绿色点
    }
    for (int i = 0; i < track.pointsEdgeRight.size(); i++) {
      circle(image,
             Point(track.pointsEdgeRight[i].y, track.pointsEdgeRight[i].x), 1,
             Scalar(0, 255, 255), -1); // 黄色点
    }

    // 入库状态
    string state = "None";
    switch (step) {
    case Step::Enable:
      state = "Enable";
      break;
    case Step::Enter:
      state = "Enter";
      break;
    case Step::Cruise:
      state = "Cruise";
      break;
    case Step::Stop:
      state = "Stop";
      break;
    case Step::Exit:
      state = "Exit";
      break;
    }
    if (entryLeft) {
      // 绘制锥桶坐标
      for (int i = 0; i < pointConeLeft.size(); i++) {
        circle(image, Point(pointConeLeft[i].y, pointConeLeft[i].x), 2,
               Scalar(92, 92, 205), -1); // 锥桶坐标：红色
      }
      putText(image, "[3] RESCUE - LEFT", Point(COLSIMAGE / 2 - 30, 10),
              cv::FONT_HERSHEY_TRIPLEX, 0.3, cv::Scalar(0, 255, 0), 1, CV_AA);
    } else {
      // 绘制锥桶坐标
      for (int i = 0; i < pointConeRight.size(); i++) {
        circle(image, Point(pointConeRight[i].y, pointConeRight[i].x), 2,
               Scalar(92, 92, 205), -1); // 锥桶坐标：红色
      }
      putText(image, "[3] RESCUE - RIGHT", Point(COLSIMAGE / 2 - 30, 10),
              cv::FONT_HERSHEY_TRIPLEX, 0.3, cv::Scalar(0, 255, 0), 1, CV_AA);
    }

    putText(image, state, Point(COLSIMAGE / 2 - 10, 30),
            cv::FONT_HERSHEY_TRIPLEX, 0.3, cv::Scalar(0, 255, 0), 1, CV_AA);

    putText(image, to_string(_distance), Point(COLSIMAGE / 2 - 15, 40),
            cv::FONT_HERSHEY_TRIPLEX, 0.3, cv::Scalar(0, 255, 0), 1,
            CV_AA); // 显示锥桶距离
    if (_pointNearCone.x > 0)
      circle(image, Point(_pointNearCone.y, _pointNearCone.x), 5,
             Scalar(200, 200, 200), -1);

    if (levelCones > 0)
      line(image, Point(0, levelCones), Point(image.cols, levelCones),
           Scalar(255, 255, 255), 1);

    putText(image, to_string(indexDebug),
            Point(COLSIMAGE / 2 - 10, ROWSIMAGE - 20), cv::FONT_HERSHEY_TRIPLEX,
            0.3, cv::Scalar(0, 0, 255), 1, CV_AA);
  }


 /**
	 * @brief 获取救援区速度规划
	 *
	 */
	float get_speed()
	{

		switch(step)
		{
		case Step::Enable:
		{
      counterStop++;
      if(counterStop >= params.EnterTime)
      {
        _speed -= 1.0;
        if(_speed < (params.EnableSpeed * 0.5))
        _speed = (params.EnableSpeed * 0.5);  //params.EnableSpeed
       // EnterFlag = true;
      }
      else 
      {
        _speed -= 2.5;
        if(_speed < 0)
        _speed = 0.0f;
      }  //0
      // else if(counterStop >= params.StopTime )    
      // {
      //   _speed = -0.8;       //1.2挺好  1.1  1.13  0.7
       // BackSpeedFlag = true;
       // counterStop = 0;
      //}
      
			   
			break;
		}
		case Step::Enter:
		{
    //  EnterFlag = false;
			_speed += 0.2;  //0.8  1.0 params.EnterSpeed 1.15
      if(_speed > (params.EnterSpeed * 0.5))
      _speed = (params.EnterSpeed * 0.5);
			break;
		} 
		case Step::Cruise:
		{
			_speed -= params.CruiseSpeed / 3;  //1.2
			if(_speed < 0.0f)
				_speed = 0.0f;
			break;
		}
		case Step::Stop:
		{
			_speed = 0.0f;
			break;
		}
		case Step::Exit:
		{
      counterStop = 0;
      _speed = -(params.ExitSpeed * 0.5);  //倒车速度比进库速度快一点 1.4挺好(3500P)  1.0挺好(3800P) 1.0 params.ExitSpeed
		}
		}
    std::cout << "危险区速度：" << _speed << endl;
		return _speed;
	}

private:

//   PerspectiveMapping ipm =
//       Mapping(Size(COLSIMAGE, ROWSIMAGE), Size(COLSIMAGEIPM, ROWSIMAGEIPM));
  /**
   * @brief 从AI检测结果中检索锥桶坐标集合
   *
   * @param predict AI检测结果
   * @return vector<POINT>
   */
  void searchCones(vector<PredictResult> predict) 
  {
    pointConeLeft.clear();
    pointConeRight.clear();
    for (int i = 0; i < predict.size(); i++) 
    {
      if (predict[i].label == LABEL_CONE) // 锥桶检测
      {
        
       // if(predict[i].y + predict[i].height / 2 > 30)
       // {
          
        if ((predict[i].x + predict[i].width / 2) < COLSIMAGE / 2 )
        {
          pointConeLeft.push_back(POINT(predict[i].y + predict[i].height, predict[i].x + predict[i].width));
      //    coneDetected = true;
       //  std::cout << "检测到锥桶" << endl;
        }
          

        else
          pointConeRight.push_back(
              POINT(predict[i].y + predict[i].height, predict[i].x));
       // }      
      }
    }
  }

  	/**
	 * @brief 从视觉结果中检索锥桶坐标集合
	 *
	 * @param predict 检测结果
	 * @param rowCutUp 滤除掉图片最上方部分的色块
	 * @return vector<POINT>
	 */
	void searchCones(vector<Rect> predict, uint16_t rowCutUp = 0)
	{
		pointConeLeft.clear();
    pointConeRight.clear();
		for (int i = 0; i < predict.size(); i++)
		{
			// if(predict[i].y + predict[i].height / 2 > rowCutUp)
			// 	pointEdgeDet.push_back(POINT(predict[i].y + predict[i].height / 2,
			// 									predict[i].x + predict[i].width / 2));
      //  if(predict[i].y + predict[i].height / 2 > rowCutUp)
      //  {
        if ((predict[i].x + predict[i].width / 2) < COLSIMAGE / 2 )
        {
          pointConeLeft.push_back(POINT(predict[i].y + predict[i].height/2, predict[i].x + predict[i].width/2));
        }
        else
          pointConeRight.push_back(POINT(predict[i].y + predict[i].height/2, predict[i].x + predict[i].width/2));
     //  }
      
		}
	}
  


  //传统视觉识别锥桶
	// std::vector<cv::Rect> detectCones(cv::Mat img_rgb)
	// {
	// 	std::vector<cv::Rect> coneRects;
	// 	// 设置锥桶颜色的RGB范围（黄色）
	// 	cv::Scalar lowerYellow(0, 100, 100);
	// 	cv::Scalar upperYellow(100, 255, 255);

	// 	// 在RGB图像中根据颜色范围提取锥桶区域
	// 	cv::Mat mask;
	// 	cv::inRange(img_rgb, lowerYellow, upperYellow, mask);

	// 	// 进行形态学操作，去除噪声并提取锥桶区域的轮廓
	// 	cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(5, 5));
	// 	cv::morphologyEx(mask, mask, cv::MORPH_OPEN, kernel);

	// 	std::vector<std::vector<cv::Point>> contours;
	// 	cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

	// 	// 查找最大轮廓
	// 	size_t maxContourIndex = 0;
	// 	double maxContourArea = 0.0;
	// 	for (size_t i = 0; i < contours.size(); ++i)
	// 	{
	// 		double contourArea = cv::contourArea(contours[i]);
	// 		if (contourArea > maxContourArea)
	// 		{
	// 			maxContourArea = contourArea;
	// 			maxContourIndex = i;
	// 		}
	// 	}

	// 	// 绘制正方形框选中锥桶区域
	// 	for (const auto& contour : contours)
	// 	{
	// 		cv::Rect boundingRect = cv::boundingRect(contour);
	// 		coneRects.push_back(boundingRect);
	// 	}	
	// 	return coneRects;
	// }

  // //两点之间的像素距离
	// double distance(POINT x1, POINT x2)
	// {
	// 	double dx = x1.x - x2.x;
	// 	double dy = x1.y - x2.y;
	// 	double len = std::sqrt(dx * dx + dy * dy);
	// 	return len;
	// }

    /**
   * @brief 搜索距离赛道左边缘最近的锥桶坐标
   *
   * @param pointsEdgeLeft 赛道边缘点集
   * @param predict AI检测结果
   * @return POINT
   */
  POINT getConeLeftDown(vector<POINT> pointsEdgeLeft,
                        vector<POINT> pointsCone) {
    POINT point(0, 0);
    double disMin = 60; // 右边缘锥桶离赛道左边缘最小距离

    if (pointsCone.size() <= 0 || pointsEdgeLeft.size() < 10)
      return point;
    for (int i = 0; i < pointsCone.size(); i++) 
    {
      if (pointsEdgeLeft[pointsEdgeLeft.size() - 1].x<  pointsCone[i].x) 
      {
        int row = pointsEdgeLeft[0].x - pointsCone[i].x;
        if (row > 0 && row < pointsEdgeLeft.size()) 
        {
          int dis = pointsEdgeLeft[row].y - pointsCone[i].y;
          if (dis < disMin && pointsCone[i].x > ROWSIMAGE / 4 &&
              pointsCone[i].x > point.x) 
          {
            point = pointsCone[i];
            _distance = dis;
          }
        }
      }
    }

    return point;
  }

  /**
   * @brief 搜索距离赛道右边缘最近的锥桶坐标
   *
   * @param pointsEdgeRight 赛道边缘点集
   * @param predict AI检测结果
   * @return POINT
   */
  POINT getConeRightDown(vector<POINT> pointsEdgeRight,
                         vector<POINT> pointsCone) {
    POINT point(0, 0);
    double disMin = 60; // 右边缘锥桶离赛道左边缘最小距离

    if (pointsCone.size() <= 0 || pointsEdgeRight.size() < 10)
      return point;

    for (int i = 0; i < pointsCone.size(); i++) {
      if (pointsEdgeRight[pointsEdgeRight.size() - 1].x < pointsCone[i].x) {
        int row = pointsEdgeRight[0].x - pointsCone[i].x;
        if (row > 0 && row < pointsEdgeRight.size()) {
          int dis = pointsCone[i].y - pointsEdgeRight[row].y;
          if (dis < disMin && pointsCone[i].x > ROWSIMAGE / 4 &&
              pointsCone[i].x > point.x) {
            point = pointsCone[i];
            _distance = dis;
          }
        }
      }
    }

    return point;
  }

  /**
   * @brief 在俯视域由左边缘预测右边缘
   *
   * @param pointsEdgeLeft
   * @return vector<POINT>
   */
  vector<POINT> predictEdgeRight(vector<POINT> &pointsEdgeLeft) {
    int offset = 120; // 右边缘平移尺度
    vector<POINT> pointsEdgeRight;
    if (pointsEdgeLeft.size() < 3)
      return pointsEdgeRight;

    // Start
    Point2d startIpm = ipm.homography(
        Point2d(pointsEdgeLeft[0].y, pointsEdgeLeft[0].x)); // 透视变换
    Point2d prefictRight = Point2d(startIpm.x + offset, startIpm.y);
    Point2d startIipm = ipm.homographyInv(prefictRight); // 反透视变换
    POINT startPoint = POINT(startIipm.y, startIipm.x);

    // Middle
    Point2d middleIpm = ipm.homography(
        Point2d(pointsEdgeLeft[pointsEdgeLeft.size() / 2].y,
                pointsEdgeLeft[pointsEdgeLeft.size() / 2].x)); // 透视变换
    prefictRight = Point2d(middleIpm.x + offset, middleIpm.y);
    Point2d middleIipm = ipm.homographyInv(prefictRight); // 反透视变换
    POINT midPoint = POINT(middleIipm.y, middleIipm.x);   // 补线中点

    // End
    Point2d endIpm = ipm.homography(
        Point2d(pointsEdgeLeft[pointsEdgeLeft.size() - 1].y,
                pointsEdgeLeft[pointsEdgeLeft.size() - 1].x)); // 透视变换
    prefictRight = Point2d(endIpm.x + offset, endIpm.y);
    Point2d endtIipm = ipm.homographyInv(prefictRight); // 反透视变换
    POINT endPoint = POINT(endtIipm.y, endtIipm.x);

    // 补线
    vector<POINT> input = {startPoint, midPoint, endPoint};
    vector<POINT> repair = Bezier(0.05, input);

    for (int i = 0; i < repair.size(); i++) {
      if (repair[i].x >= ROWSIMAGE)
        repair[i].x = ROWSIMAGE - 1;

      else if (repair[i].x < 0)
        repair[i].x = 0;

      else if (repair[i].y >= COLSIMAGE)
        repair[i].y = COLSIMAGE - 1;
      else if (repair[i].y < 0)
        repair[i].y = 0;

      pointsEdgeRight.push_back(repair[i]);
    }

    return pointsEdgeRight;
  }

  /**
   * @brief 在俯视域由右边缘预测左边缘
   *
   * @param pointsEdgeRight
   * @return vector<POINT>
   */
  vector<POINT> predictEdgeLeft(vector<POINT> &pointsEdgeRight) {
    int offset = 120; // 右边缘平移尺度
    vector<POINT> pointsEdgeLeft;
    if (pointsEdgeRight.size() < 3)
      return pointsEdgeLeft;

    // Start
    Point2d startIpm = ipm.homography(
        Point2d(pointsEdgeRight[0].y, pointsEdgeRight[0].x)); // 透视变换
    Point2d prefictLeft = Point2d(startIpm.x - offset, startIpm.y);
    Point2d startIipm = ipm.homographyInv(prefictLeft); // 反透视变换
    POINT startPoint = POINT(startIipm.y, startIipm.x);

    // Middle
    Point2d middleIpm = ipm.homography(
        Point2d(pointsEdgeRight[pointsEdgeRight.size() / 2].y,
                pointsEdgeRight[pointsEdgeRight.size() / 2].x)); // 透视变换
    prefictLeft = Point2d(middleIpm.x - offset, middleIpm.y);
    Point2d middleIipm = ipm.homographyInv(prefictLeft); // 反透视变换
    POINT midPoint = POINT(middleIipm.y, middleIipm.x);  // 补线中点

    // End
    Point2d endIpm = ipm.homography(
        Point2d(pointsEdgeRight[pointsEdgeRight.size() - 1].y,
                pointsEdgeRight[pointsEdgeRight.size() - 1].x)); // 透视变换
    prefictLeft = Point2d(endIpm.x - offset, endIpm.y);
    Point2d endtIipm = ipm.homographyInv(prefictLeft); // 反透视变换
    POINT endPoint = POINT(endtIipm.y, endtIipm.x);

    // 补线

    vector<POINT> input = {startPoint, midPoint, endPoint};
    vector<POINT> repair = Bezier(0.05, input);

    for (int i = 0; i < repair.size(); i++) {
      if (repair[i].x >= ROWSIMAGE)
        repair[i].x = ROWSIMAGE - 1;

      else if (repair[i].x < 0)
        repair[i].x = 0;

      else if (repair[i].y >= COLSIMAGE)
        repair[i].y = COLSIMAGE - 1;
      else if (repair[i].y < 0)
        repair[i].y = 0;

      pointsEdgeLeft.push_back(repair[i]);
    }

    return pointsEdgeLeft;
  }

  /**
   * @brief 按照坐标点的y排序
   *
   * @param points
   * @return vector<int>
   */
  void pointsSortForY(vector<POINT> &points) {
    int n = points.size();
    bool flag = true;

    for (int i = 0; i < n - 1 && flag; i++) {
      flag = false;
      for (int j = 0; j < n - i - 1; j++) {
        if (points[j].y > points[j + 1].y) {
          POINT temp = points[j];
          points[j] = points[j + 1];
          points[j + 1] = temp;
          flag =
              true; // 每次循环i有修改，这里为true
                    // 如果跑了一次I没有发生交换的情况，说明已经排序完成，不需要再跑后面的i
        }
      }
    }
  }
private:
  bool again = false; // 第二次进入救援区标志
  double _distance = 0;
  int levelCones = 0; // 锥桶的平均高度
  POINT _pointNearCone;
  POINT pointHCone;
  std::vector<cv::Rect> _coneRects;
  vector<POINT> pointConeLeft;      // AI元素检测边缘点集
  vector<POINT> pointConeRight;     // AI元素检测边缘点集
  vector<POINT> lastPointsEdgeLeft; // 记录上一场边缘点集（丢失边）
  vector<POINT> lastPointsEdgeRight;

  vector<vector<POINT>> pathsEdgeLeft; // 记录入库路径
  vector<vector<POINT>> pathsEdgeRight;
  int indexDebug = 0;

  uint16_t counterSession = 0;  // 图像场次计数器
  uint16_t counterRec = 0;      // 标志检测计数器
  uint16_t counterExit = 0;     // 标志结束计数器
  uint16_t counterImmunity = 0; // 屏蔽计数器


};

Rescue rescueDetection;  //救援区
