#pragma once

#include "capture.hpp"
#include "predictor.hpp"
#include "stop_watch.hpp"
#include <condition_variable>
#include <mutex>
#include <thread>

#include "../src/detection/bridge_detection.cpp"
#include "../src/detection/parking.cpp"
#include "../src/detection/danger.cpp"
#include "../src/detection/rescue.cpp"
#include "../src/detection/racing.cpp"



//#include "../src/recognize/track_recognition.cpp"

//BridgeDetection bridgeDetection;  //桥
//Parking_detection parkingDetection;                 //停车
//Rescue rescueDetection;  //救援区
//Danger dangerDetection;  //危险区
//Racing racingDetection;  //追逐区

struct DetectionResult
{
    cv::Mat rgb_frame;//摄像机原始图像  由主线程传入
    std::vector<PredictResult> predictor_results;
};

class Detection
{
public:
    bool Startdetect = false;
    std::shared_ptr<PPNCDetection> _predictor;
public:
    Detection() {}
    ~Detection() {}

    int init(std::string model_config_path)
    {
        return _init(model_config_path);
    }

    void Start()
    {
        _loop = true;
        run();
    }

    void Stop()
    {
        _loop = false;
        if(_thread->joinable())
            _thread->join();
        std::cout << "ai exit" << std::endl;
    }

    bool AI_Enable()
    {
        return AI_Captured;
    }

    void run()
    {
        _thread = std::make_unique<std::thread>([this]() {
            while (_loop)
            {
                std::shared_ptr<DetectionResult> result = std::make_shared<DetectionResult>();

                std::unique_lock<std::mutex> lock(_mutex);
                // while(_frame == nullptr)
                // {
                //     cond_.wait(lock);
                // }
                while (_frame == nullptr)
                {
                    // 设置超时时间
                    if (cond_.wait_for(lock, std::chrono::seconds(1)) == std::cv_status::timeout) {
                        // 超时处理逻辑
                        std::cout << "ai wait for frame time out" << std::endl;
                        break;
                    }
                }
                if(_frame != nullptr)
                {
                    result->rgb_frame = _frame->clone();
                    _frame = nullptr;
                    lock.unlock();
                }
                else 
                    continue;

                //ai推理

                if(Startdetect)
                {
                    auto feeds = _predictor->preprocess(result->rgb_frame, {320, 320});
                    _predictor->run(*feeds);
                    _predictor->render();
                    
                    bridgeDetection.bridgeCheck(_predictor->results);  //桥可以不判
                 //   parkingDetection.parkingChack(_predictor->results);
                 //   dangerDetection.DangerCheck(_predictor->results);
                   // racingDetection.racingCheck(_predictor->results);
                }
                else
                {
                    _Cnt = 0;
                    AI_Captured = false;
                }

                bool flag = false;
                for(int i = 0; i < _predictor->results.size(); i++)
                {
                    std::string label_name = _predictor->results[i].label;
                    
                    // if(!rescueDetection.dangerRec && label_name == "cone")
                    // {
                    //     flag = false;
                    //     break; 
                    // }
                    if((label_name == "thief" || label_name == "safety") && _predictor->results[i].score > 0.40 && _predictor->results[i].y + _predictor->results[i].height / 2 > 20)
                    {
                        flag = true;
                        break;
                    }
                    else if((label_name == "crosswalk" || label_name == "bomb" || label_name == "danger" || label_name == "evil" || label_name == "patient" 
                        || label_name == "block" || label_name == "spy" || label_name == "tumble" || label_name == "cone" || label_name == "prop")
                        && _predictor->results[i].score > 0.54 && _predictor->results[i].y + _predictor->results[i].height / 2 > 20)
                    {
                        flag = true;
                        break;
                    }
                    // else if(label_name == "crosswalk" && _predictor->results[i].score > 0.48 && _predictor->results[i].y + _predictor->results[i].height / 2 > 30)  //好像没有意义
                    // {
                    //     flag == true;
                    //     break;
                    // }
                }
                if(_Cnt == 0)
                    AI_Captured = flag;
                if(AI_Captured)
                {
                    _Cnt++;
                    if(_Cnt > 2)
                        _Cnt = 0;
                }
            
                //数据传递
                result->predictor_results = _predictor->results;
                std::unique_lock<std::mutex> lock2(_mutex);
                _lastResult = result;
                cond_.notify_all();
            }
        });
    }

    std::shared_ptr<DetectionResult> getLastFrame()
    {
        std::shared_ptr<DetectionResult> ret = nullptr;
        {
            std::unique_lock<std::mutex> lock(_mutex);

            while (_lastResult == nullptr)
            {
                cond_.wait(lock);
            }
            ret = _lastResult;
            _lastResult = nullptr;
        }
        return ret;
    }

    void setFrame(cv::Mat img)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _frame = std::make_shared<cv::Mat>(img.clone());
        cond_.notify_all();
    }

    void drawbox(cv::Mat& img, std::vector<PredictResult> results)
    {
        for(int i=0;i<results.size();i++)
        {
            PredictResult result = results[i];
        
            auto score = std::to_string(result.score);
            int pointY = result.y - 20;
            if (pointY < 0)
            pointY = 0;
            cv::Rect rectText(result.x, pointY, result.width, 20);
            cv::rectangle(img, rectText, _predictor->getCvcolor(result.type), -1);
            std::string label_name = result.label + " [" + score.substr(0, score.find(".") + 3) + "]";
            cv::Rect rect(result.x, result.y, result.width, result.height);
            cv::rectangle(img, rect, _predictor->getCvcolor(result.type), 1);
            cv::putText(img, label_name, Point(result.x, result.y), cv::FONT_HERSHEY_PLAIN, 1, cv::Scalar(0, 0, 254), 1);
        }
    }

private:
    int _init(std::string model_config_path)
    {
        _predictor = std::make_shared<PPNCDetection>();
        if (_predictor == nullptr)
        {
            std::cout << "Predictor Create failed." << std::endl;
            return -1;
        }
        int ret = _predictor->init(model_config_path);
        if (ret != 0)
        {
            return -1;
        }
        return 0;
    }
    bool _loop = false;
   // std::shared_ptr<PPNCDetection> _predictor;
    std::shared_ptr<DetectionResult> _lastResult;
    std::shared_ptr<cv::Mat> _frame;
    
    uint16_t _Cnt = 0;//计数器
    bool AI_Captured = false;

    std::mutex _mutex;
    std::condition_variable cond_;
    std::unique_ptr<std::thread> _thread;
};