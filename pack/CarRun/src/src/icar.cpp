#include <iostream>
#include <memory>
#include <string>
#include <ctime>
#include <signal.h>
#include <opencv2/highgui.hpp>
#include <opencv2/opencv.hpp>
#include <cstdlib> // 包含system函数的头文件

#include "../include/stop_watch.hpp"
#include "../include/uart.hpp"                //串口通信
#include "./recognize/track_recognition.cpp"  //基础巡线
#include "./recognize/cross_recognition.cpp"  //十字赛道
#include "./recognize/ring_recognize.cpp"     //环岛赛道

#include "./detection/parking.cpp"  //停车区
#include "./detection/rescue.cpp"   //救援区
#include "./detection/danger.cpp"   //危险区
#include "./detection/racing.cpp"   //追逐区  国赛任务

#include "image_preprocess.cpp" //图像预处理
#include "controlcenter_cal.cpp"
#include "motion_control.cpp"

//#include "./detection/depot_detection.cpp"

#include "../include/capture.hpp"
#include "../include/serial.hpp"
#include "../include/detection.hpp"
#include "../include/predictor.hpp"


using namespace cv;
using namespace std;

enum RoadType
{
    BaseHandle = 0, // 基础赛道处理0
    RingHandle,     // 环岛赛道处理1
    CrossHandle,    // 十字道路处理2
    BridgeHandle,   // 坡道(桥)处理 3
    ParkingHandle,  //停车处理 4
    RescueHandle,   //救援区处理5
    DangerHandle,   //危险区处理6
    RacingHandle,   //追逐区处理7
};

void callbackSignal(int signum); // 系统退出回调函数
void displayWindowInit(void);    // 显示窗口初始化
void savePicture(Mat &image, RoadType roadType, bool flag);
void ClearFolder(const std::string &folderPath);

CaptureInterface captureInterface("/dev/video0");   //摄像头接口
SerialInterface serialInterface("/dev/ttyUSB0", LibSerial::BaudRate::BAUD_460800);  //460800  115200
Detection detection;

int main(int argc, char *argv[])
{
    RoadType roadType = RoadType::BaseHandle;  // 初始化赛道类型
    ControlCenterCal controlCenterCal;         // 控制中心计算
    MotionController motionController;         // 运动控制
    ImagePreprocess imagePreprocess;           // 图像预处理类
    TrackRecognition trackRecognition;         // 赛道识别
    RingRecognition ringRecognition;           // 环岛识别
    CrossroadRecognition crossroadRecognition; // 十字道路处理
    uint16_t counterRunBegin = 1;              // 智能车启动计数器：等待摄像头图像帧稳定
    uint16_t counterOutTrackA = 0;             // 车辆冲出赛道计数器A
    uint16_t counterOutTrackB = 0;             // 车辆冲出赛道计数器B
    uint16_t counterRescue = 0;                //救援区出库计数器  （救援区太难了T_T）
    bool Cross_SpeedUp = false;                        //十字速控时域开启标志位
    bool Upflag = true;                        //提速标志位   垃圾的代码就是标志位多
    uint16_t counterCross = 0;                 //十字屏蔽计数器
    uint16_t counterDanger = 0;                //危险区减速计数器
    uint16_t counterStop = 0;                  //停车后系统退出计时器，防止停车出界

    if (motionController.params.SaveImage)
    {
        std::string folderPath = "../res/train/"; // 替换为你要清空的文件夹路径
        ClearFolder(folderPath);
    }

    // PPNC初始化
    if (!detection.init(motionController.params.pathModel)) // AI推理初始化
        return 1;

    ipm.init(Size(COLSIMAGE, ROWSIMAGE),
             Size(COLSIMAGEIPM, ROWSIMAGEIPM)); // IPM逆透视变换初始化

    // 下位机初始化通信
    int ret = serialInterface.open();
    if (ret != 0)
        return 0;
    if (motionController.params.CloseLoop)
    {
        cout << "-------- 速度闭环控制 -------" << endl;
        serialInterface.set_PID(motionController.params.Kp_speed, motionController.params.Ki_speed, motionController.params.Kd_speed, 
                                motionController.params.Kp_current, motionController.params.Ki_current, motionController.params.Kd_current);
        cout << "Kp_speed = " << motionController.params.Kp_speed << endl;
        cout << "Ki_speed = " << motionController.params.Ki_speed << endl;
        cout << "Kd_speed = " << motionController.params.Kd_speed << endl;
        cout << "Kp_current = " << motionController.params.Kp_current << endl;
        cout << "Ki_current = " << motionController.params.Ki_current << endl;
        cout << "Kd_current = " << motionController.params.Kd_current << endl;
    }
    else
    {
      //  serialInterface.set_PID(0, 0, 0, 0, 0, 0);
        cout << "-------- 速度开环控制 -------" << endl;
    }
    serialInterface.Start();
    captureInterface.Start();

    signal(SIGINT, callbackSignal); // 程序退出信号

    imagePreprocess.imageCorrecteInit(); // 图像矫正参数初始化
    trackRecognition.rowCutUp = motionController.params.rowCutUp;
    trackRecognition.rowCutBottom = motionController.params.rowCutBottom;

    /*****调试模式初始化图像窗口*****/
    if (motionController.params.Debug)
    {
        displayWindowInit();
    }

    cout << "等待发车!!!" << endl;

    if (motionController.params.Button)
    {
        while (!serialInterface.wait_signal())
            ; // 串口接收下位机-比赛开始信号
    }
    else if (motionController.params.Debug)
    {
        // while (1)    //想用的话自行打开
        // {
        //     Mat frame;
        //     frame = captureInterface.get_frame();
        //     imshow("imageTrack", frame);
        //   /*  char key = waitKey(5);
        //     if (key == 13) // 回车
        //     {
        //         cout << "即将发车！！！" << endl;
        //         break;
        //     }*/
        // }
        Mat frame;
            frame = captureInterface.get_frame();
            imshow("imageTrack", frame);
    }

    for (int i = 3; i > 0; i--) // 3秒后发车
    {
        cout << "------------- " << i << " -----------" << endl;
        serialInterface.set_control(0, PWMSERVOMID); // 智能车停止运动|建立下位机通信
        waitKey(1000);
    }
    detection.Start();
    cout << "--------- System start!!! -------" << endl;

    while (1)
    {
        // 特殊赛道图像显示标志
        bool imshowRec = false;

        // 处理帧时长监测 速度监测
        // {
        static auto preTime = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count();
        auto startTime = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count();
        float detFPS = (float)1000.f / (startTime - preTime);
        float speed = serialInterface.get_speed();
        cout << "run frame time : " << startTime - preTime << "ms  "
             << "FPS: " << (int)detFPS << "\t";
        cout << speed << "m/s  " << roadType << endl;
        cout << "上位机下发的目标速度为" << motionController.motorSpeed <<endl;
        /*****************************标志位调试区*********************************** */
     //   cout << "锥桶检测标志位:" << rescueDetection.coneDetected << endl;
        //cout << "十字速控标志位:" << Cross_SpeedUp << endl;
      //  cout << "救援区进行阶段:" << rescueDetection.step << endl;
      //  cout << "追逐区进行阶段:" << racingDetection.typeRace << endl;
      //  cout << "小车是否靠左:" << racingDetection.sideLeft << endl;
      //  cout << "是否逼停:" << racingDetection.spy_Stop<< racingDetection.spy_Counter << endl;
        //cout << "圆环是否打开:" << Ring_flag << endl;
        serialInterface.display_testflag();  //串口是否成功打开的标志位显示
        /*************************************************************************** */
        preTime = startTime;
        // }

        Mat frame;
        frame = captureInterface.get_frame();
        detection.setFrame(frame);

        /*1.AI推理*/
        bool AI_enable = detection.AI_Enable();
        std::shared_ptr<DetectionResult> ai_results = nullptr;
        if((roadType == RoadType::RescueHandle ) || roadType == RoadType::DangerHandle || roadType == RoadType::RacingHandle || roadType == RoadType::BridgeHandle)  // && rescueDetection.step == 0
            detection.Startdetect = true;
        else if (roadType && roadType != RoadType::CrossHandle) 
            detection.Startdetect = false;  
        else
            detection.Startdetect = true;

        {
            ai_results = std::make_shared<DetectionResult>();
        }

        /*2.图像预处理*/
        // Mat imgaeCorrect = imagePreprocess.imageCorrection(frame);         // RGB
        Mat imgaeCorrect = frame.clone(); // RGB
        Mat imageBinary;
        if (AI_enable || roadType == RoadType::RescueHandle)
            imageBinary = imagePreprocess.imageBinaryzation(imgaeCorrect, false); // Gray
        else
            imageBinary = imagePreprocess.imageBinaryzation(imgaeCorrect); // Gray

        /*3.基础赛道识别*/
        trackRecognition.trackRecognition(imageBinary, 0); // 赛道线识别
        Mat imageTrack = imgaeCorrect.clone();             // RGB

        /*******4.停车识别************************/
        if(motionController.params.ParkingEnable)
        {
            if((roadType == RoadType::ParkingHandle || roadType == RoadType::BaseHandle))
            {
                parkingDetection.parkingChack(detection._predictor->results);
            }
            
            if (parkingDetection.countExit > 20 || parkingDetection.park) 
            {   
                counterStop++;
                roadType == RoadType::ParkingHandle;
                motionController.Angle_Controller(controlCenterCal, trackRecognition, roadType);  //停车后也要继续方向控制，防止冲出赛道
                serialInterface.set_control(0, PWMSERVOMID); 
               // sleep(1);
                if(counterStop >= 25)
                {
                    counterStop = 0;
                    printf("-----> System Exit!!! <-----\n");
                    exit(0); // 程序退出
                }
                
            }
        }

        /******************************救援区检测********************************* */
         if (motionController.params.RescueEnable) // 赛道元素是否使能
        {
            if (roadType == RoadType::RescueHandle || roadType == RoadType::BaseHandle)
            {
                if (rescueDetection.rescueDetection(trackRecognition, detection._predictor->results))//, imgaeCorrect
                {
                    
                    if (roadType == RoadType::RescueHandle && rescueDetection.step == rescueDetection.Step::Exit)
                    {
                        rescueDetection.dangerRec = false;
                    }
                        serialInterface.buzzerSound(1);    // 初次识别-蜂鸣器提醒

                    roadType = RoadType::RescueHandle;
                    if (motionController.params.Debug)
                    {
                        Mat imageRescue = Mat::zeros(Size(COLSIMAGE, ROWSIMAGE), CV_8UC3); // 初始化图像
                        rescueDetection.drawImage(trackRecognition, imageRescue);
                        imshow("imageRecognition", imageRescue); // 添加需要显示的图像
                        imshowRec = true;
                    }
                }
                else
                {
                    if(roadType != RoadType::BaseHandle)
                    {
                        roadType = RoadType::BaseHandle;
                        AI_enable = false;
                    }
                }
            }
        }
        
        if(!rescueDetection.dangerRec)
        {
            counterStop++;
            if(counterStop >= 200)
            {
                rescueDetection.dangerRec = true;
                counterStop = 0;
            }
            
        }

        /*危险区检测与路径规划*/
         if (motionController.params.dangerEnable && rescueDetection.dangerRec) // 赛道元素是否使能
        {
            if (roadType == RoadType::DangerHandle || roadType == RoadType::BaseHandle)
            {
                if (dangerDetection.process(trackRecognition, detection._predictor->results))
                {
                    if (roadType == RoadType::BaseHandle) // 初次识别-蜂鸣器提醒
                        serialInterface.buzzerSound(1);   // OK

                    roadType = RoadType::DangerHandle;
                    if (motionController.params.Debug)
                    {
                        Mat imageDanger = Mat::zeros(Size(COLSIMAGE, ROWSIMAGE), CV_8UC3); // 初始化图像
                        dangerDetection.drawImage(imageDanger);
                        imshow("imageRecognition", imageDanger); // 添加需要显示的图像
                        imshowRec = true;
                    }
                }
                else
                {
                    if(roadType != RoadType::BaseHandle)
                    {
                        roadType = RoadType::BaseHandle;
                        AI_enable = false;
                    } 
                }
            }
        }

        /******************************追逐区检测和路径规划********************************* */
         if (motionController.params.RacingEnable) // 赛道元素是否使能
        {
            if (roadType == RoadType::RacingHandle || roadType == RoadType::BaseHandle)
            {
                if (racingDetection.racingDetection(trackRecognition, detection._predictor->results))
                {
                    roadType = RoadType::RacingHandle;
                    if (motionController.params.Debug)
                    {
                        Mat imageRacing = Mat::zeros(Size(COLSIMAGE, ROWSIMAGE), CV_8UC3); // 初始化图像
                        racingDetection.drawImage(imageRacing);
                        imshow("imageRecognition", imageRacing); // 添加需要显示的图像
                        imshowRec = true;
                    }
                }
                else
                {
                    if(roadType != RoadType::BaseHandle)
                    {
                        roadType = RoadType::BaseHandle;
                        AI_enable = false;
                    }
                }
            }
        }

        /*坡道（桥）检测与路径规划*/
        if (motionController.params.BridgeEnable) // 赛道元素是否使能
        {
            if (roadType == RoadType::BridgeHandle || roadType == RoadType::BaseHandle)
            {
                if (bridgeDetection.bridgeDetection(trackRecognition))
                {
                    if (roadType == RoadType::BaseHandle) // 初次识别-蜂鸣器提醒
                        //serialInterface.buzzerSound(1);   // OK

                    roadType = RoadType::BridgeHandle;
                    if (motionController.params.Debug)
                    {
                        Mat imageBridge = Mat::zeros(Size(COLSIMAGE, ROWSIMAGE), CV_8UC3); // 初始化图像
                        bridgeDetection.drawImage(trackRecognition, imageBridge);
                        imshow("imageRecognition", imageBridge);
                    }
                }
                else
                    roadType = RoadType::BaseHandle;
            }
        }
       
        // /*十字赛道*/
        if (motionController.params.CrossEnable) // 赛道元素是否使能 && !Cross_SpeedUp
        {
            if (roadType == RoadType::CrossHandle || roadType == RoadType::BaseHandle)
            {
                if (crossroadRecognition.crossroadRecognition(trackRecognition))
                {
                    roadType = RoadType::CrossHandle;
                    if (motionController.params.Debug)
                    {
                        Mat imageCross = Mat::zeros(Size(COLSIMAGE, ROWSIMAGE), CV_8UC3); // 初始化图像
                        crossroadRecognition.drawImage(trackRecognition, imageCross);
                        imshow("imageRecognition", imageCross);
                    }
                }
                else
                {
                    roadType = RoadType::BaseHandle;
                }
            }
        }

         /*环岛识别与处理*/
        if (motionController.params.RingEnable /*&& Ring_flag*/) // 赛道元素是否使能
        {
            if (roadType == RoadType::RingHandle || roadType == RoadType::BaseHandle)
            {
                if (ringRecognition.ringRecognition(trackRecognition, imageBinary))
                {
                    if (roadType == RoadType::BaseHandle) // 初次识别-蜂鸣器提醒
                      //  serialInterface.buzzerSound(1);   // OK
                    roadType = RoadType::RingHandle;

                    if (motionController.params.Debug)
                    {
                        Mat imageRing = Mat::zeros(Size(COLSIMAGE, ROWSIMAGE), CV_8UC3); // 初始化图像
                        ringRecognition.drawImage(trackRecognition, imageRing);
                        imshow("imageRecognition", imageRing);
                    }
                }
                else
                    roadType = RoadType::BaseHandle;
            }
        }

        /*控制中心计算*/
        if (trackRecognition.pointsEdgeLeft.size() < 60 &&
            trackRecognition.pointsEdgeRight.size() < 60 &&
            motionController.params.StopEnable &&
            roadType != RoadType::BridgeHandle &&
            roadType != RoadType::RescueHandle) // 防止车辆冲出赛道
        {
            counterOutTrackA++;
            counterOutTrackB = 0;
            if (counterOutTrackA > 20)
                callbackSignal(0);
        }
        else
        {
            counterOutTrackB++;
            if (counterOutTrackB > 50)
            {
                counterOutTrackA = 0;
                counterOutTrackB = 50;
            }
        }
        controlCenterCal.controlCenterCal(trackRecognition); // 根据赛道边缘信息拟合运动控制中心

       
        /**************智能车运动控制,通讯******************/
        if (counterRunBegin > 30)
        {
             // 智能车方向控制
           
            motionController.Angle_Controller(controlCenterCal, trackRecognition, roadType);

        
            // 智能车速度控制
            switch (roadType)
            {
            
            case RoadType::BridgeHandle:
            {
                motionController.motorSpeed = bridgeDetection.get_speed();
                break;
            }
            case RoadType::RescueHandle:
            {
                motionController.motorSpeed = rescueDetection.get_speed();

            }
            case RoadType::RacingHandle:
            {
                motionController.motorSpeed = racingDetection.get_Speed();
                break;
            }
            case RoadType::DangerHandle:
            {
                if(!dangerDetection.DangerStop)
                {
                    motionController.motorSpeed = motionController.params.speedAI;  //如果锥桶之间的距离大，速度可以大一些，反之则小一些 目前最好1.1
                }
                
                break;
            }
            case RoadType::RingHandle:
            {
               // motionController.speedControl(controlCenterCal);
            	motionController.motorSpeed = ringRecognition.get_speed(motionController.params.speedRing);
            	break;
            }
            case RoadType::CrossHandle:
            {
               // motionController.speedControl(controlCenterCal);
            	motionController.motorSpeed = motionController.params.speedCross;
            	break;
            }
            default:
            {
                // 智能车变速度控制
                if (AI_enable)  //当有检测并且确实识别到标志位时,速度自减 && racingDetection.track_control == false
                {
                    motionController.motorSpeed -= 0.1f;  //0.1
                    if (motionController.motorSpeed < motionController.params.speedAI)
                        motionController.motorSpeed = motionController.params.speedAI;
                }
                else if(racingDetection.track_control == false)
                {
                    // motionController.speedController(controlCenterCal, motionController.k);
                    motionController.speedControl(controlCenterCal);
                    // motionController.motorSpeed = motionController.params.speedLow;
                }
                break;
            }
            }
            
            /**********************************重要元素区速度再次给定防止资源占用导致速度刷新************************************* */
            if(rescueDetection.step != rescueDetection.Step::None)     //救援区速度再次给定
            {
                motionController.motorSpeed = rescueDetection._speed;
            }
            else if(dangerDetection.DangerStop)    //危险区马上减速,防止因检测不到锥桶撞上锥桶
            {
                motionController.motorSpeed -= 2.5;
                if(motionController.motorSpeed < 0.6)
                {
                    motionController.motorSpeed = 0.6;
                }
                counterDanger++;
                if(counterDanger >= 20)   //危险区减速时间(约1s)  
                {
                    dangerDetection.DangerStop = false;
                    counterDanger = 0;
                }
            }
            // 串口通信，姿态与速度控制
            // motionController.motorSpeed = 1.2f;
            serialInterface.set_control(-motionController.motorSpeed, motionController.servoPwm); //
        }
        else
        {
            counterRunBegin++;
        }

        // 存图使能
        if (motionController.params.SaveImage)
        {
            // savePicture(frame, roadType);
            switch (roadType)
            {
            case RoadType::BaseHandle:
                trackRecognition.drawImage(imageTrack); // 图像显示赛道线识别结果
                if (AI_enable)
                {
                    detection.drawbox(imageTrack, ai_results->predictor_results);
                }
                break;
            case RoadType::RingHandle:
                ringRecognition.drawImage(trackRecognition, imageTrack);
                break;
            case RoadType::CrossHandle:
                crossroadRecognition.drawImage(trackRecognition, imageTrack);
                break;
            case RoadType::BridgeHandle:
                bridgeDetection.drawImage(trackRecognition, imageTrack);
                break;
            }
            // 绘制中心点集
            for (int i = 0; i < controlCenterCal.centerEdge.size(); i++)
            {
                circle(imageTrack, Point(controlCenterCal.centerEdge[i].y, controlCenterCal.centerEdge[i].x), 1, Scalar(0, 0, 255), -1);
            }
            rectangle(imageTrack, Rect(controlCenterCal.intersectionLeft.y, controlCenterCal.intersectionLeft.x, 10, 10), Scalar(0, 0, 200), 1);
            rectangle(imageTrack, Rect(controlCenterCal.intersectionRight.y, controlCenterCal.intersectionRight.x, 10, 10), Scalar(0, 0, 200), 1);
            // putText(imageTrack, to_string(ringRecognition.counterShield), Point(COLSIMAGE / 2 - 5, ROWSIMAGE - 20), cv::FONT_HERSHEY_TRIPLEX, 0.5, cv::Scalar(0, 0, 155), 1, CV_AA);

            putText(imageTrack, "FPS: " + formatDoble2String(detFPS, 2), Point(20, 20), FONT_HERSHEY_PLAIN, 1, Scalar(0, 0, 255), 1);                   // 车速
            putText(imageTrack, "PWM:" + formatDoble2String(motionController.servoPwm, 2), Point(20, 40), FONT_HERSHEY_PLAIN, 1, Scalar(0, 0, 255), 1); // 下发的pwm值
            putText(imageTrack, "ERROR:" + formatDoble2String(motionController.error, 2), Point(20, 60), FONT_HERSHEY_PLAIN, 1, Scalar(0, 0, 255), 1);
            putText(imageTrack, "K:" + formatDoble2String(motionController.CenterLine_k, 4), Point(20, 80), FONT_HERSHEY_PLAIN, 1, Scalar(0, 0, 255), 1);
            putText(imageTrack, "Mid:" + formatDoble2String(motionController.Mid_line, 2), Point(20, 100), FONT_HERSHEY_PLAIN, 1, Scalar(0, 0, 255), 1);
            putText(imageTrack, "Speed: " + formatDoble2String(motionController.motorSpeed, 2), Point(COLSIMAGE / 2 + 20, 20), FONT_HERSHEY_PLAIN, 1, Scalar(0, 0, 255), 1);
            putText(imageTrack, controlCenterCal.style, Point(COLSIMAGE / 2 + 20, 40), FONT_HERSHEY_PLAIN, 1, Scalar(0, 0, 255), 1); // 赛道类型
            putText(imageTrack, "CERROR: " + formatDoble2String(motionController.compensation_error, 2), Point(COLSIMAGE / 2 + 20, 60), FONT_HERSHEY_PLAIN, 1, Scalar(0, 0, 255), 1);
            putText(imageTrack, "PreSlope: " + formatDoble2String(motionController.Slope_previewPoint, 2), Point(COLSIMAGE / 2 + 20, 80), FONT_HERSHEY_PLAIN, 1, Scalar(0, 0, 255), 1);
            putText(imageTrack, "Sigma: " + formatDoble2String(controlCenterCal.sigmaCenter, 2), Point(COLSIMAGE / 2 + 20, 100), FONT_HERSHEY_PLAIN, 1, Scalar(0, 0, 255), 1);
            putText(imageTrack, "Plan: " + to_string(motionController.speed_plan), Point(COLSIMAGE / 2 + 20, 120), FONT_HERSHEY_PLAIN, 1, Scalar(0, 0, 255), 1);

            line(imageTrack, Point(motionController.Mid_line, 0), Point(motionController.Mid_line, ROWSIMAGE - 1), Scalar(200, 200, 200), 1);
            // line(imageTrack, Point(0, ROWSIMAGE - motionController.params.Control_Up_set), Point(COLSIMAGE - 1, ROWSIMAGE - motionController.params.Control_Up_set), Scalar(200, 200, 200), 1);
            // line(imageTrack, Point(0, ROWSIMAGE - motionController.params.Control_foreword_down), Point(COLSIMAGE - 1, ROWSIMAGE - motionController.params.Control_foreword_down), Scalar(255, 0, 0), 1);
            // line(imageTrack, Point(0, ROWSIMAGE - motionController.params.Control_foreword_up), Point(COLSIMAGE - 1, ROWSIMAGE - motionController.params.Control_foreword_up), Scalar(255, 0, 0), 1);
            savePicture(imageTrack, roadType, AI_enable);
        }

        /**********图像显示************/
        if (motionController.params.Debug)
        {
            imshow("imageTrack", imageTrack);
            {
                char key = waitKey(1);
                if (key == 32) // 空格
                {
                    serialInterface.set_control(0, motionController.servoPwm);
                    key = waitKey(0);
                }
                if (key == 13) // 回车
                {
                    callbackSignal(0);
                }
            }
        }

    }

    serialInterface.set_control(0, PWMSERVOMID); // 智能车停止运动
    detection.Stop();
    captureInterface.Stop();
    serialInterface.Stop();
    destroyAllWindows();

    return 0;
}

/**
 * @brief 系统信号回调函数：系统退出
 * @param signum 信号量
 */
void callbackSignal(int signum)
{
    serialInterface.set_control(0, PWMSERVOMID);                // 智能车停止运动
    std::this_thread::sleep_for(std::chrono::milliseconds(20)); // 延时20ms确保停止信号发出

    detection.Stop();
    captureInterface.Stop();
    serialInterface.Stop();

    cout << "====System Exit!!!  -->  CarStopping! " << signum << endl;
    exit(signum);
}

/**
 * @brief OpenCV图像显示窗口初始化（详细参数/Debug模式）
 */
void displayWindowInit(void)
{
    //[1] 赛道边缘图像：RGB
    string windowName = "imageTrack";
    cv::namedWindow(windowName, WINDOW_NORMAL);         // 图像名称
    cv::resizeWindow(windowName, COLSIMAGE, ROWSIMAGE); // 分辨率
    cv::moveWindow(windowName, 1000, 50);               // 布局位置

    //[2] AI图像/矫正后：RGB
    // windowName = "imageAI";
    // cv::namedWindow(windowName, WINDOW_NORMAL);         // 图像名称
    // cv::resizeWindow(windowName, COLSIMAGE, ROWSIMAGE); // 分辨率
    // cv::moveWindow(windowName, 1340, 50);               // 布局位置

    //[3] 二值化图像
    windowName = "imageRecognition";
    cv::namedWindow(windowName, WINDOW_NORMAL);         // 图像名称
    cv::resizeWindow(windowName, COLSIMAGE, ROWSIMAGE); // 分辨率
    cv::moveWindow(windowName, 1000, 360);              // 布局位置
}

/**
 * @brief 存储图像至本地
 *
 * @param image 需要存储的图像
 */
void savePicture(Mat &image, RoadType roadType, bool flag)
{
    // 存图
    string name = ".jpg";
    static int counter = 0;
    counter++;
    string img_path = "../res/train/";
    name = img_path + to_string(counter) + "-" + to_string(roadType) + "_" + to_string(flag) + ".jpg";
    imwrite(name, image);
}

void ClearFolder(const std::string &folderPath)
{
    std::string command = "rm -rf " + folderPath + "/*";
    int result = std::system(command.c_str());
    if (result == 0)
    {
        std::cout << "文件夹已成功清空。\n";
    }
    else
    {
        std::cerr << "清空文件夹时发生错误。\n";
    }
}
