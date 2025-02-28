#pragma once
/**
* @file racing.cpp
* @author LWD
* @brief 追逐区路径处理
* @version 0.1
* @date 2024-07-21
*
* @copyright Copyright (c) 2024
*
*/

#include <fstream>
#include <iostream>
#include <cmath>
#include <opencv2/highgui.hpp>
#include <opencv2/opencv.hpp>
#include "../../include/common.hpp"
#include "../../include/predictor.hpp"
#include "../recognize/track_recognition.cpp"
//#include "../motion_control.cpp"

class Racing
{
public:
    Racing()
	{
		loadParams();
	}
    ~Racing() {}
public:
  struct Params   
    {
		uint16_t RacingCheck = 1;
        uint16_t SafeCheck = 1;
        uint16_t SpyCheck = 1;
        uint16_t DangerCheck = 1;
        uint16_t rowCheck = 1;
        uint16_t trun_Count = 40;
        uint16_t stop_Count = 40;
        uint16_t spy_ExitCount = 30;
        uint16_t safe_ExitCount = 30;
        uint16_t danger_ExitCount = 30;
        float safty_Speed = 1;
        float spy_Speed = 1;
        float danger_Speed = 1;
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(Params, RacingCheck, SafeCheck, SpyCheck, DangerCheck, rowCheck, trun_Count, stop_Count, spy_ExitCount, safe_ExitCount, danger_ExitCount,
                                       safty_Speed, spy_Speed, danger_Speed); // 添加构造函数 
    };
    Params params;  

    /**
     * @brief 场景类型
     *
     */
    enum TypeRace
    {
        None = 0, // AI检测
      //  Enable,   // 追逐使能
        Safe,     // 普通车辆
        Spy,      // 嫌疑车辆
        Danger    // 危险车辆
    };
    TypeRace typeRace = TypeRace::None; // 场景类型

    /**
     * @brief 嫌疑车辆逼停阶段
     *
     */
    // enum StepSpy  //没有用
    // {
    //     Det = 0, // AI检测
    //     Bypass,  // 车辆绕行
    //     Inside,  // 变道
    //     Resist   // 阻挡
    // };

    // StepSpy stepSpy = StepSpy::Det; // 嫌疑车辆逼停阶段

    bool carStoping = false; // 停车标志
    bool sideLeft = true;    // AI标识左右侧使能
    bool track_control = false;  //赛道规划控制权
    float _speed = 0.0f;     //用于规划追逐区的速度
    bool turn_func = false;   //spy任务是否完成二次变道
    bool turn_Flag = true;   //准备好变道标志位，防止重复变道  （暂时没用到）
    bool spy_Stop = false;   //嫌疑车任务停车标志位
    uint16_t spy_Counter = 0;  //失去嫌疑车图像计场次数器  避免由于AI识别对嫌疑车图像的误判造成的停车

    /**
     * @brief 检测与路径规划
     *
     * @param track 赛道识别结果
     * @param detection AI检测结果
     */
    bool racingDetection(TrackRecognition &track, vector<PredictResult> predicts)
    {
        _index = 0;
        carStoping = false; // 停车标志
        if(counterShield < 30)  //避免发车就误判
        {
            counterShield++;
            return false;
        }

        switch (typeRace)
        {
        case TypeRace::None:          // AI检测
            searchTypeRace(predicts); // 检索AI场景类型  
            break;

        case TypeRace::Safe: // 普通车辆
        {
            PredictResult predict = searchSign(predicts, LABEL_SAFETY);
            counterSes[0]++;
            if (predict.x > 0 && predict.y > 0) // && turn_Flag 检测到有效的AI标志  这里有bug，找不出来，所以只能写的很僵硬
            {
                // 检索小车靠近左/右方向
                turn_Flag = false;  //变道一次就行
                sideLeft = false; // 靠左侧标志
                int row = track.pointsEdgeLeft[0].x - predict.y - predict.height;
                if (row < 0)
                {
                    _index = 1;
                    if (abs(track.pointsEdgeLeft[0].y - predict.x) > abs(track.pointsEdgeRight[0].y - predict.x - predict.width))//如果左边界距离标志点的距离比右边界的近，则判断道具车在左边
                        sideLeft = true;
                }
                else if (row >= track.pointsEdgeLeft.size() || row >= track.pointsEdgeRight.size())
                {
                    _index = 2;
                    if (abs(track.pointsEdgeLeft[track.pointsEdgeLeft.size() - 1].y - predict.x) > abs(track.pointsEdgeRight[track.pointsEdgeRight.size() - 1].y - predict.x - predict.width))
                        sideLeft = true;
                }
                else
                {
                    _index = 3;
                    if (abs(track.pointsEdgeLeft[row].y - predict.x) > abs(track.pointsEdgeRight[row].y - predict.x - predict.width))
                        sideLeft = true;
                }

                track_control = true;  //追逐区中线控制开启
                // predict.x = 0;
                // predict.y = 0;
              //  curtailTracking(track, sideLeft); // 缩减优化车道线（双车道→单车道）
                counterSes[0] = 0;
            }
            else if (counterSes[0] > params.safe_ExitCount) // AI检测异常：退出该场景    params.ExitCount 50
            {
                track_control = false;  
                turn_Flag = true;    //刷新变道标志位
                counterRec[0] = 0;
                counterSes[0] = 0;
                typeRace = TypeRace::None;
            }
            break;
        }

        case TypeRace::Spy: // 嫌疑车辆
        {
            /**
             *嫌疑车辆逼停策略：绕行至车辆前方阻挡其运行
             */
             PredictResult predict = searchSign(predicts, LABEL_SPY);
            //counterSes[1]++;
            if (predict.x > 0 && predict.y > 0) // && turn_Flag 检测到有效的AI标志  这里有bug，找不出来，所以只能写的很僵硬
            {
                // 检索小车靠近左/右方向
                turn_Flag = false;  //变道一次就行  
                sideLeft = false; // 靠左侧标志
                int row = track.pointsEdgeLeft[0].x - predict.y - predict.height;
                if (row < 0)
                {
                    _index = 1;
                    if (abs(track.pointsEdgeLeft[0].y - predict.x) > abs(track.pointsEdgeRight[0].y - predict.x - predict.width))//如果左边界距离标志点的距离比右边界的近，则判断道具车在左边
                        sideLeft = true;
                }
                else if (row >= track.pointsEdgeLeft.size() || row >= track.pointsEdgeRight.size())
                {
                    _index = 2;
                    if (abs(track.pointsEdgeLeft[track.pointsEdgeLeft.size() - 1].y - predict.x) > abs(track.pointsEdgeRight[track.pointsEdgeRight.size() - 1].y - predict.x - predict.width))
                        sideLeft = true;
                }
                else
                {
                    _index = 3;
                    if (abs(track.pointsEdgeLeft[row].y - predict.x) > abs(track.pointsEdgeRight[row].y - predict.x - predict.width))
                        sideLeft = true;
                }

                track_control = true;  //追逐区中线控制开启
                // predict.x = 0;
                // predict.y = 0;
              //  curtailTracking(track, sideLeft); // 缩减优化车道线（双车道→单车道）
               // counterSes[1] = 0;
                spy_Counter = 0;  //对这个标志位清理是为了保证在连续的看不到spy的情况下才可以判停车
            }
            else if(predict.x == 0 && predict.y == 0)
            {
                spy_Counter++;
                if(spy_Counter >= params.trun_Count && turn_func == false)  //这里数字一定要比ExitCount小
                {
                    if(sideLeft)
                    {
                        sideLeft = false;
                        turn_func = true;
                    }
                    else{
                        sideLeft = true;
                        turn_func = true;
                    }
                    spy_Counter = 0;
                   // counterSes[1] = 0;
                }
                if(turn_func) //如果完成了二次变道
                {
                    counterShield++;
                    counterSes[1]++;
                    if(counterShield >= params.stop_Count && spy_Stop == false)  //**场后停车 40
                    {
                        //turn_func = false;
                        spy_Stop = true;
                        counterShield = 0;
                    }
                    else if(counterSes[1] > params.spy_ExitCount)
                    {
                        spy_Stop = false;
                        track_control = false;  
                        turn_Flag = true;    //刷新变道标志位
                        counterRec[1] = 0;
                        counterSes[1] = 0;
                        spy_Counter = 0;
                        typeRace = TypeRace::None;
                        turn_func = false;
                    }
                }
                
            }
            break;
        }

        case TypeRace::Danger: // 危险车辆
        {
            /**
             *恐怖车辆逼停策略：沿赛道左/右侧通行，强行撞击车辆逼停
             */
            PredictResult predict = searchSign(predicts, LABEL_DANGER);
            counterSes[2]++;
            if (predict.x > 0 && predict.y > 0) // 检测到有效的AI标志 && turn_Flag
            {
                // 检索小车靠近左/右方向
                turn_Flag = false;  //变道一次就行
                sideLeft = true; // 靠左侧标志
                int row = track.pointsEdgeLeft[0].x - predict.y - predict.height;
                if (row < 0)
                {
                    if (abs(track.pointsEdgeLeft[0].y - predict.x) > abs(track.pointsEdgeRight[0].y - predict.x - predict.width))
                        sideLeft = false;
                }
                else if (row >= track.pointsEdgeLeft.size() || row >= track.pointsEdgeRight.size())
                {
                    if (abs(track.pointsEdgeLeft[track.pointsEdgeLeft.size() - 1].y - predict.x) > abs(track.pointsEdgeRight[track.pointsEdgeRight.size() - 1].y - predict.x - predict.width))
                        sideLeft = false;
                }
                else
                {
                    if (abs(track.pointsEdgeLeft[row].y - predict.x) > abs(track.pointsEdgeRight[row].y - predict.x - predict.width))
                        sideLeft = false;
                }

                track_control = true;  //追逐区中线控制开启
              //  curtailTracking(track, sideLeft); // 缩减优化车道线（双车道→单车道）
                counterSes[2] = 0;
            }
            else if (counterSes[2] > params.danger_ExitCount) // 退出该场景
            {
                track_control = false;  
                turn_Flag = true;    //刷新变道标志位
                counterSes[2] = 0;
                typeRace = TypeRace::None;
            }
            break;
        }
        }

        if (typeRace == TypeRace::None)
            return false;
        else
            return true;
    }



        /**
     * @brief 加载配置参数Json
     */
    void loadParams() 
    {
        string jsonPath = "../src/config/racing.json";
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
     * @brief 图像绘制禁行区识别结果
     *
     * @param img 需要叠加显示的图像
     */
    void drawImage(Mat &img)
    {
        if (typeRace == TypeRace::Spy)
        {
            // switch (stepSpy)
            // {
            // case StepSpy::Det:
            //     putText(img, "[4] RACE - SPY - Det", Point(COLSIMAGE / 2 - 50, 10), cv::FONT_HERSHEY_TRIPLEX, 0.3, cv::Scalar(0, 255, 0), 1, CV_AA);
            //     break;
            // case StepSpy::Bypass:
            //     putText(img, "[4] RACE - SPY - Bypass", Point(COLSIMAGE / 2 - 50, 10), cv::FONT_HERSHEY_TRIPLEX, 0.3, cv::Scalar(0, 255, 0), 1, CV_AA);
            //     break;
            // case StepSpy::Inside:
            //     putText(img, "[4] RACE - SPY - Inside", Point(COLSIMAGE / 2 - 50, 10), cv::FONT_HERSHEY_TRIPLEX, 0.3, cv::Scalar(0, 255, 0), 1, CV_AA);
            //     break;
            // case StepSpy::Resist:
            //     putText(img, "[4] RACE - SPY - Resist", Point(COLSIMAGE / 2 - 50, 10), cv::FONT_HERSHEY_TRIPLEX, 0.3, cv::Scalar(0, 255, 0), 1, CV_AA);
            //     break;
            // default:
            //     break;
            // }
        }
        else if (typeRace == TypeRace::Danger)
            putText(img, "[4] RACE - DANGER", Point(COLSIMAGE / 2 - 30, 10), cv::FONT_HERSHEY_TRIPLEX, 0.3, cv::Scalar(0, 255, 0), 1, CV_AA);
        else if (typeRace == TypeRace::Safe)
            putText(img, "[4] RACE - Safe", Point(COLSIMAGE / 2 - 30, 10), cv::FONT_HERSHEY_TRIPLEX, 0.3, cv::Scalar(0, 255, 0), 1, CV_AA);

        putText(img, to_string(_index), Point(COLSIMAGE / 2 - 10, 40), cv::FONT_HERSHEY_TRIPLEX, 0.3, cv::Scalar(0, 255, 0), 1, CV_AA);
    }
    /**
     * @brief 根据当前追逐区任务阶段规划速度
     *
     * @param 无
     */
    float get_Speed()
    {
        // if(typeRace == TypeRace::Enable) 
        // {
        //     _speed = 1.5;
        // }
       // else 
        if(typeRace == TypeRace::Safe)
        {
            _speed-=0.1;
            if(_speed <= params.safty_Speed)
            _speed = params.safty_Speed;
        }
        else if(typeRace == TypeRace::Spy)
        {
            // if(stepSpy == StepSpy::Bypass || stepSpy == StepSpy::Inside)
            // {
            //     _speed = 0.8;
            // }
            if(spy_Stop)
            {
                _speed-=0.8;
                if(_speed <= 0)
                _speed = 0;
            }
            else if(turn_func) 
            { 
               _speed = (params.spy_Speed * 0.8);
            } 
            else
            _speed = params.spy_Speed;
        }
        else if(typeRace == TypeRace::Danger)
        {
            _speed-=0.1;
            if(_speed <= params.danger_Speed)            
            _speed = params.danger_Speed;
        }

        return _speed;
    }

private:
    uint16_t counterSes[3] = {0, 0, 0}; // 图像场次计数器
    uint16_t counterRec[3] = {0, 0, 0}; // 标志检测计数器
    uint16_t CounterRec = 0;     // 道具车标志检测计数器
    uint16_t counterShield = 0;    //屏蔽计数器
    
    int _index = 0;



    /**
     * @brief 检索AI场景类型
     *
     */
    void searchTypeRace(vector<PredictResult> predicts)  //注意：此函数起效后 typeRace必定不为None
    {
        // 普通车辆AI连续帧检测
        for (int i = 0; i < predicts.size(); i++)
        {
            if (predicts[i].label == LABEL_SAFETY /*&& predict[i].y + predict[i].height / 2 > params.rowCheck*/) // 安全车辆 考虑加入限制行 
            {
                counterRec[0]++;
                break;
            }
        }
        // 嫌疑车辆AI连续帧检测
        for (int i = 0; i < predicts.size(); i++)
        {
            if (predicts[i].label == LABEL_SPY/*&& predict[i].y + predict[i].height / 2 > params.rowCheck*/) // 嫌疑车辆
            {
                counterRec[1]++;
                break;
            }
        }
        // 危险车辆AI连续帧检测
        for (int i = 0; i < predicts.size(); i++)
        {
            if (predicts[i].label == LABEL_DANGER/*&& predict[i].y + predict[i].height / 2 > params.rowCheck*/) // 危险车辆
            {
                counterRec[2]++;
                break;
            }
        }

        if (counterRec[0]) // 安全车辆场景检测
        {
            counterSes[0]++;
            if (counterRec[0] > params.SafeCheck && counterSes[0] <= 20) // 3/8
            {
                typeRace = TypeRace::Safe; // 场景类型
                counterRec[0] = 0;
                counterSes[0] = 0;
            }
            else if (counterSes[0] > 20)
            {
                counterRec[0] = 0;
                counterSes[0] = 0;
            }
        }
        else if (counterRec[1]) // 嫌疑车辆场景检测
        {
            counterSes[1]++;
            if (counterRec[1] > params.SpyCheck && counterSes[1] <= 20)
            {
                typeRace = TypeRace::Spy; // 场景类型
                counterRec[1] = 0;
                counterSes[1] = 0;
               // stepSpy = StepSpy::Det;
            }
            else if (counterSes[1] > 20)
            {
                counterRec[1] = 0;
                counterSes[1] = 0;
            }
        }
        else if (counterRec[2]) // 危险车辆场景检测
        {
            counterSes[2]++;
            if (counterRec[2] > params.DangerCheck && counterSes[2] <= 20)
            {
                typeRace = TypeRace::Danger; // 场景类型
                counterRec[2] = 0;
                counterSes[2] = 0;
            }
            else if (counterSes[2] > 20)
            {
                counterRec[2] = 0;
                counterSes[2] = 0;
            }
        }
        else 
        {
            counterRec[0] = 0;
            counterSes[0] = 0;
            counterRec[1] = 0;
            counterSes[1] = 0;
            counterRec[2] = 0;
            counterSes[2] = 0;
            typeRace = TypeRace::None;
        }
    }

    /**
     * @brief 检索目标图像坐标
     *
     * @param predicts AI识别结果
     * @param index 检索序号
     * @return PredictResult
     */
    PredictResult searchSign(vector<PredictResult> predicts, std::string index)
    {
        PredictResult predict;
        predict.x = 0;
        predict.y = 0;
        predict.height = 0;
        predict.width = 0;
        predict.label = "";
        // AI连续帧检测
        for (int i = 0; i < predicts.size(); i++)
        {
            if (predicts[i].label == index)  // && predicts[i].y + predicts[i].height / 2 > 40
            {
                // 通过框大小过滤最佳目标
                std::cout << "检测到" << index << endl;
                if (predicts[i].height * predicts[i].width > predict.height * predict.width)
                {
                    predict = predicts[i];
                }
            }
        }
        return predict;
    }

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

Racing racingDetection;  //追逐区对象初始化

