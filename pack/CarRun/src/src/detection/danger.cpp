#pragma once

#include <fstream>
#include <iostream>
#include <cmath>
#include <opencv2/highgui.hpp>
#include <opencv2/opencv.hpp>
#include "../../include/common.hpp"
#include "../../include/predictor.hpp" // Ai模型预测

using namespace std;
using namespace cv;

/**
 * @brief 危险区AI识别与路径规划类
 *
 */
class Danger
{

public:
    //  Danger()
    // {
    //     loadParams();
    // }

     struct Params 
     {
         uint16_t DangerCheck = 3;   //暂时不用
         uint16_t rowCheck = 90;   //标志检测范围限制 90 100  110
         float SpeedUp = 1.0;   //没有用到
        
    //     NLOHMANN_DEFINE_TYPE_INTRUSIVE(
    //         Params, DangerCheck, rowCheck, SpeedUp, ); // 添加构造函数
     };
     bool DangerStop = false;  //危险区缓冲停车标志
     bool enable = false;     // 场景检测使能标志
     bool Turn_Left = false; //识别到锥桶左转向标志位
     bool Turn_Right = false; //识别到锥桶右转向标志位
     bool Bomb = false;   //炸弹检测到标志位，防止救援区误判为危险区
     uint16_t counter_turn = 0; //转向计数 防止转过头
     bool track_control = false;
     bool sideLeft = false;
     
     Params params;

    

   // float speedAI = 1.0;
    void DangerCheck(vector<PredictResult> predict)
    {
        for (int i = 0; i < predict.size(); i++)
        {
            if (predict[i].label == LABEL_BOMB && (predict[i].y + predict[i].height) > 40) // AI标志距离计算 && (predict[i].y + predict[i].height) > params.rowCheck  //低速时加进去
            {
              //  Bomb = true;
                DangerStop = true;
                enable = true;
            }
          //  else DangerStop = false;
            
        }
    }
/**
     * @brief 危险区AI识别与路径规划处理
     *
     * @param track 赛道识别结果
     * @param predict AI检测结果
     * @return true
     * @return false
     */
    bool process(TrackRecognition &track, vector<PredictResult> predict)
    {
        enable = false; // 场景检测使能标志
        if (track.pointsEdgeLeft.size() < ROWSIMAGE / 2 || track.pointsEdgeRight.size() < ROWSIMAGE / 2)
            return enable;

         for (int i = 0; i < predict.size(); i++)
        {
            if (predict[i].label == LABEL_BOMB && (predict[i].y + predict[i].height) > 30) // AI标志距离计算 && (predict[i].y + predict[i].height) > params.rowCheck  //低速时加进去
            {
              //  Bomb = true;
                DangerStop = true;
                enable = true;
            }
          //  else DangerStop = false;
            
        }

        vector<PredictResult> resultsObs; // 锥桶AI检测数据
        for (int i = 0; i < predict.size(); i++)
        {
            if ((predict[i].label == LABEL_CONE || predict[i].label == LABEL_BLOCK) && (predict[i].y + predict[i].height) > params.rowCheck) // AI标志距离计算
                resultsObs.push_back(predict[i]);
        }

        if (resultsObs.size() <= 0)  //如果没有收集到障碍物信息，退出函数
        {
            track_control = false;
            sideLeft = false;
            return enable;
        }
            

        // 选取距离最近的锥桶
        int areaMax = 0; // 框面积
        int index = 0;   // 目标序号
        for (int i = 0; i < resultsObs.size(); i++)
        {
            int area = resultsObs[i].width * resultsObs[i].height;  //计算每个障碍物的框面积
            if (area >= areaMax)
            {
                index = i;
                areaMax = area;
            }
        }
        resultObs = resultsObs[index];  //得到最大面积的锥桶
        enable = true; // 场景检测使能标志

        // 障碍物方向判定（左/右）
        int row = track.pointsEdgeLeft.size() - (resultsObs[index].y + resultsObs[index].height - 10); // track.rowCutUp 记得改回来
        if (row < 0) // 无需规划路径
            return enable;

        int disLeft = resultsObs[index].x + resultsObs[index].width - track.pointsEdgeLeft[row].y;
        int disRight = track.pointsEdgeRight[row].y - resultsObs[index].x;
        if (resultsObs[index].x + resultsObs[index].width > track.pointsEdgeLeft[row].y &&
            track.pointsEdgeRight[row].y > resultsObs[index].x &&
            disLeft <= disRight) //[1] 障碍物靠左 需要优化赛道线到右半边赛道
        {
            track_control = true;
            if(resultsObs[index].label == LABEL_CONE) //下面为锥桶躲避的补线
            {
                sideLeft = false;
                //curtailTracking(track, false); // 缩减优化车道线（双车道→单车道） 简单方法，如果右边锥桶躲不过就打开
            }
            else if (resultsObs[index].label == LABEL_BLOCK) // 黑色路障特殊处理
            {
                sideLeft = false;
               // curtailTracking(track, false); // 缩减优化车道线（双车道→单车道）
            }
        }
        else if (resultsObs[index].x + resultsObs[index].width > track.pointsEdgeLeft[row].y &&
                 track.pointsEdgeRight[row].y > resultsObs[index].x &&
                 disLeft > disRight) //[2] 障碍物靠右 需要优化赛道线到左半边赛道
        {
            track_control = true;
            if(resultsObs[index].label == LABEL_CONE) //下面为锥桶躲避的补线
            {
                sideLeft = true;
            }
            else if (resultsObs[index].label == LABEL_BLOCK) // 黑色路障特殊处理
            {
                sideLeft = true;
                //curtailTracking(track, true); // 缩减优化车道线（双车道→单车道）
            }
        }

        return enable;
    }

    /**
     * @brief 图像绘制禁行区识别结果
     *
     * @param img 需要叠加显示的图像
     */
    void drawImage(Mat &img)
    {
        if (enable)
        {
            putText(img, "[2] DANGER - ENABLE", Point(COLSIMAGE / 2 - 30, 10), cv::FONT_HERSHEY_TRIPLEX, 0.3, cv::Scalar(0, 255, 0), 1, CV_AA);
            cv::Rect rect(resultObs.x, resultObs.y, resultObs.width, resultObs.height);
            cv::rectangle(img, rect, cv::Scalar(0, 0, 255), 1);
        }
    }

    float get_speed()  //没有用到
    {
        if(enable)
        {
            return params.SpeedUp;
        }
    }


    /**
     * @brief 加载配置参数Json
     */
    // void loadParams() 
    // {
    //     string jsonPath = "../src/config/danger.json";
    //     std::ifstream config_is(jsonPath);
    //     if (!config_is.good()) 
    //     {
    //         std::cout << "Error: Params file path:[" << jsonPath << "] not find .\n";
    //         exit(-1);
    //     }

    //     nlohmann::json js_value;
    //     config_is >> js_value;

    //     try 
    //     {
    //         params = js_value.get<Params>();
    //     }
    //     catch (const nlohmann::detail::exception &e) 
    //     {
    //         std::cerr << "Json Params Parse failed :" << e.what() << '\n';
    //         exit(-1);
    //     }
    // }


private:
    
    PredictResult resultObs; // 避障目标锥桶
    

    /**
     * @brief 缩减优化车道线（双车道→单车道）
     *
     * @param track
     * @param left
     */
    void curtailTracking(TrackRecognition &track, bool left)
    {
        if (left) // 向左侧缩进
        {
            if (track.pointsEdgeRight.size() > track.pointsEdgeLeft.size())
                track.pointsEdgeRight.resize(track.pointsEdgeLeft.size());

            for (int i = 0; i < track.pointsEdgeRight.size(); i++)
            {
                track.pointsEdgeRight[i].y = (track.pointsEdgeRight[i].y + track.pointsEdgeLeft[i].y) / 2;
            }
        }
        else // 向右侧缩进
        {
            if (track.pointsEdgeRight.size() < track.pointsEdgeLeft.size())
                track.pointsEdgeLeft.resize(track.pointsEdgeRight.size());

            for (int i = 0; i < track.pointsEdgeLeft.size(); i++)
            {
                track.pointsEdgeLeft[i].y = (track.pointsEdgeRight[i].y + track.pointsEdgeLeft[i].y) / 2;
            }
        }
    }
};
Danger dangerDetection;  //危险区在这里定义

