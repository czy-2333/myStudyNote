#pragma once
/**
 * @file ring_recognition.cpp
 * @author LWD
 * @brief 环岛识别（基于track赛道识别后）
 * @version 0.1
 * @date 2024-05-09
 *
 * @copyright Copyright (c) 2022
 *
 * @note  环岛识别步骤（ringStep）：
 *          1：环岛识别（初始化）
 *          2：入环处理
 *          3：环中处理
 *          4：出环处理
 *          5：出环结束
 */

#include <fstream>
#include <iostream>
#include <cmath>
#include <opencv2/highgui.hpp>
#include <opencv2/opencv.hpp>
#include "../../include/common.hpp"
#include "track_recognition.cpp"

using namespace cv;
using namespace std;

class RingRecognition
{
public:
    RingRecognition()
    {
        ;
    }
    ~RingRecognition()
    {
        ;
    }
    /**
     * @brief 环岛识别初始化|复位
     *
     */
    void reset(void)
    {
        ringType = RingType::RingNone; // 环岛类型
        ringStep = RingStep::None;     // 环岛处理阶段
        ring_cnt = 0;
        counterSpurroad = 0;
        counterShield = 0;
    }
    void print(void)
    {
        cout << "RingStep: " << ringStep << endl;
    }

    /**
     * @brief 环岛识别与行径规划
     *
     * @param track 基础赛道识别结果
     * @param imagePath 赛道路径图像
     */
    bool ringRecognition(TrackRecognition &track, Mat img_bin)
    {
        if (track.pointsEdgeRight.size() < ROWSIMAGE / 4 || track.pointsEdgeLeft.size() < ROWSIMAGE / 4) //环岛有效行限制
        {
            return ringType;
        }

        // if (track.garageEnable.x && ringType == RingType::RingNone)
        // {
        //     counterShield = 0;
        //     return false;
        // }
        if (counterShield < 100)  //15
        {
            counterShield++;
            return false;
        }
        // if(ringType == RingType::RingNone)
        // {
        //     for(int i = 0; i < track.spurroad.size(); i++)
        //     {
        //         uint16_t item = abs(track.spurroad[i].x - track.pointsEdgeLeft[0].x);
        //         if(track.spurroad[i].y > track.pointsEdgeLeft[item].y + 10 && track.spurroad[i].y < track.pointsEdgeRight[item].y - 10)
        //         {
        //             counterShield = 0;
        //             return false;
        //         }
        //     }
        // }
        


        pointBreakD = POINT(0, 0);
        pointBreakU = POINT(0, 0);

        //[1]左右环岛判断
        if(ringType == RingType::RingNone && ringStep == RingStep::None)
        {
            uint16_t rowBreakRightDown = searchBreakRightDown(track.pointsEdgeRight, 0, COLSIMAGE / 2 - 20);
            uint16_t rowBreakLeftDown = searchBreakLeftDown(track.pointsEdgeLeft, 0, COLSIMAGE / 2 - 20);

            if(rowBreakLeftDown != 0 && rowBreakRightDown == 0
                && ((track.stdevLeft > 120 && track.stdevRight < 60) || (track.stdevLeft > 200 && track.stdevRight < 80) || (track.stdevLeft > 100 && track.stdevRight < 40))
                && abs(track.pointsEdgeRight[0].y - track.pointsEdgeRight[track.pointsEdgeRight.size() - 1].y) > 10
                && track.widthBlock[rowBreakLeftDown + 10].y > COLSIMAGE / 2 && track.pointsEdgeLeft[rowBreakLeftDown + 10].y < 5
                && track.pointsEdgeRight[track.pointsEdgeRight.size() - 1].y > COLSIMAGE / 2 - 30)
            {
                // for(int i = rowBreakLeftDown; i < rowBreakLeftDown + 50; i++)
                // {
                //     uint16_t counter = 0;
                //     if(track.pointsEdgeLeft[i].y < 5)
                //         counter++;
                //     if(counter> 30)
                //     {
                //         ring_cnt++;
                //         break;
                //     }
                // }
                ring_cnt++;
                if(ring_cnt > 3)
                {
                    // for(int i = 0; i < track.pointsEdgeLeft.size() / 2; i++)
                    // {
                    //     if(track.pointsEdgeLeft[i].y < 5)
                    //         counterSpurroad++;
                    // }
                    // if(counterSpurroad > 30)
                    //     ringStep = RingStep::Entering;

                    counterSpurroad = 0;
                    ringType = RingType::RingLeft;
                }
            }
            else if(rowBreakLeftDown == 0 && rowBreakRightDown != 0 
                && ((track.stdevLeft < 60 && track.stdevRight > 120) || (track.stdevLeft < 80 && track.stdevRight > 200) || (track.stdevLeft < 40 && track.stdevRight > 100))
                && abs(track.pointsEdgeLeft[0].y - track.pointsEdgeLeft[track.pointsEdgeLeft.size() - 1].y) > 10
                && track.widthBlock[rowBreakRightDown + 10].y > COLSIMAGE / 2 && track.pointsEdgeRight[rowBreakRightDown + 10].y > COLSIMAGE - 5
                && track.pointsEdgeLeft[track.pointsEdgeLeft.size() - 1].y < COLSIMAGE / 2 + 30)
            {
                // for(int i = rowBreakRightDown; i < rowBreakRightDown + 50; i++)
                // {
                //     uint16_t counter = 0;
                //     if(track.pointsEdgeRight[i].y > COLSIMAGE - 5)
                //         counter++;
                //     if(counter> 30)
                //     {
                //         ring_cnt++;
                //         break;
                //     }
                // }
                ring_cnt++;
                if(ring_cnt > 3)
                {
                    // for(int i = 0; i < track.pointsEdgeRight.size() / 2; i++)
                    // {
                    //     if(track.pointsEdgeRight[i].y > COLSIMAGE - 5)
                    //         counterSpurroad++;
                    // }
                    // if(counterSpurroad > 30)
                    //     ringStep = RingStep::Entering;

                    counterSpurroad = 0;
                    ringType = RingType::RingRight;
                }
            }
            else
            {
                ring_cnt = 0;
            }

            // /*环岛识别方法2*/
            // if(track.stdevLeft > 120 && track.stdevRight < 60 && abs(track.pointsEdgeRight[0].y - track.pointsEdgeRight[ROWSIMAGE / 2].y) > 5)
            // {
            //     uint16_t rowBreakLeftDown = searchBreakLeftDown(track.pointsEdgeLeft, 0, ROWSIMAGE / 2);
            //     for(int i = rowBreakLeftDown; i < track.pointsEdgeLeft.size(); i++)
            //     {
            //         uint16_t counter = 0, step = 0;
            //         if(track.pointsEdgeLeft[i].y < 5)
            //         {
            //             if(step % 2 == 0)
            //                 counter++;
            //             else if(step % 2 == 1)
            //                 counter = 0;
            //         }
            //         else
            //         {
            //             if(step % 2 == 0)
            //                 counter = 0;
            //             else if(step % 2 == 1)
            //                 counter++;
            //         }
            //         if(counter > 5)
            //         {
            //             step += 1;
            //             counter = 0;
            //             if(step == 3 && rowBreakLeftDown != 0)
            //             {
            //                 ringType = RingType::RingLeft;
            //             }
            //         }
            //     }
            // }
            // else if(track.stdevLeft < 60 && track.stdevRight > 120 && abs(track.pointsEdgeLeft[0].y - track.pointsEdgeLeft[ROWSIMAGE / 2].y) > 5)
            // {
            //     uint16_t rowBreakRightDown = searchBreakRightDown(track.pointsEdgeRight, 0, ROWSIMAGE / 2);
            //     for(int i = rowBreakRightDown; i < track.pointsEdgeRight.size(); i++)
            //     {
            //         uint16_t counter = 0, step = 0;
            //         if(track.pointsEdgeRight[i].y > COLSIMAGE - 5)
            //         {
            //             if(step % 2 == 0)
            //                 counter++;
            //             else if(step % 2 == 1)
            //                 counter = 0;
            //         }
            //         else
            //         {
            //             if(step % 2 == 0)
            //                 counter = 0;
            //             else if(step % 2 == 1)
            //                 counter++;
            //         }
            //         if(counter > 5)
            //         {
            //             step += 1;
            //             counter = 0;
            //             if(step == 3 && rowBreakRightDown != 0)
            //             {
            //                 ringType = RingType::RingRight;
            //             }
            //         }
            // }
        }
        else if(ringType != RingType::RingNone && ringStep == RingStep::None)
        {
            if (track.garageEnable.x)
            {
                reset();
                return false;
            }
			// counterExit++;
			// if (counterExit > 40) {
			//   reset();
			//   return false;
			// }

            if(ringType == RingType::RingLeft)
            {
                uint16_t rowBreakLeftD = searchBreakLeftDown(track.pointsEdgeLeft, 0, ROWSIMAGE / 2);
                uint16_t rowBreakLeftU = searchBreakLeftDown(track.pointsEdgeLeft, rowBreakLeftD + 30, track.pointsEdgeLeft.size());
                
                pointBreakD = track.pointsEdgeLeft[rowBreakLeftD];
                if(rowBreakLeftD && rowBreakLeftU && rowBreakLeftU > rowBreakLeftD)///////////////////////////////
                {
                    counterSpurroad++;
                    pointBreakU = track.pointsEdgeLeft[rowBreakLeftU];
                    line(track.pointsEdgeLeft, rowBreakLeftD, rowBreakLeftU);
                }
                else if(rowBreakLeftD && !rowBreakLeftU)
                {
                    counterSpurroad++;
                    line(track.pointsEdgeLeft, rowBreakLeftD, track.pointsEdgeLeft[track.pointsEdgeLeft.size() - 1]);
                }
                else if(counterSpurroad > 2)
                {
                    counterSpurroad = 0;
                    ringStep = RingStep::Entering;
                }
                else if(rowBreakLeftD == 0 && rowBreakLeftU == 0 && counterSpurroad == 0)
                {
                    track.pointsEdgeLeft = track.predictEdgeLeft(track.pointsEdgeRight);
                }

                {
                    uint16_t edgecounter = 0;
                    for(int i = 0; i < track.pointsEdgeLeft.size(); i++)
                    {
                        if(track.pointsEdgeLeft[i].y < 5)
                            edgecounter++;
                        else
                            edgecounter = 0;
                    }
                    if(edgecounter > track.pointsEdgeLeft.size() / 2)
                    {
                        ringStep = RingStep::Entering;
                        counterSpurroad = 0;
                    }
                }             
            }
            else if(ringType == RingType::RingRight)
            {
                uint16_t rowBreakRightD = searchBreakRightDown(track.pointsEdgeRight, 0, ROWSIMAGE / 2);
                uint16_t rowBreakRightU = searchBreakRightDown(track.pointsEdgeRight, rowBreakRightD + 30, track.pointsEdgeRight.size());

                pointBreakD = track.pointsEdgeRight[rowBreakRightD];
                if(rowBreakRightD && rowBreakRightU && rowBreakRightU > rowBreakRightD)//////////////////////
                {
                    counterSpurroad++;
                    pointBreakU = track.pointsEdgeRight[rowBreakRightU];
                    line(track.pointsEdgeRight, rowBreakRightD, rowBreakRightU);
                }
                else if(rowBreakRightD && !rowBreakRightU)
                {
                    counterSpurroad++;
                    line(track.pointsEdgeRight, rowBreakRightD, track.pointsEdgeRight[track.pointsEdgeRight.size() - 1]);
                }
                else if(counterSpurroad > 2)
                {
                    counterSpurroad = 0;
                    ringStep = RingStep::Entering;
                }
                else if(rowBreakRightD == 0 && rowBreakRightU == 0 && counterSpurroad == 0)
                {
                    track.pointsEdgeRight = track.predictEdgeRight(track.pointsEdgeLeft);
                }
                
                {
                    uint16_t edgecounter = 0;
                    for(int i = 0; i < track.pointsEdgeRight.size(); i++)
                    {
                        if(track.pointsEdgeRight[i].y > COLSIMAGE - 5)
                            edgecounter++;
                        else
                            edgecounter = 0;
                    }
                    if(edgecounter > track.pointsEdgeRight.size() / 2)
                    {
                        ringStep = RingStep::Entering;
                        counterSpurroad = 0;
                    }
                }             
            }

        }
        else if(ringStep == RingStep::Entering)
        {
            bool repaired = false; 
            _corner = POINT(0, 0);
            if(ringType == RingType::RingLeft)
            {
                uint16_t rowBreakLeftD = 0;
                uint16_t spurroad_item = 0;
                if(track.spurroad.size() == 0)
                {
                    rowBreakLeftD = searchBreakLeftDown(track.pointsEdgeLeft, 0, track.pointsEdgeLeft.size());
                    uint16_t rowBreakLeftU = searchBreakLeftUp(track.pointsEdgeLeft);
                    if(rowBreakLeftU > ROWSIMAGE / 2 && track.pointsEdgeLeft[rowBreakLeftU].y > 70)
                    {
                        spurroad_item = rowBreakLeftU;
                        _corner = track.pointsEdgeLeft[rowBreakLeftU];
                    }
                }
                else if(track.spurroad.size() == 1)
                {
                    //寻找岔路行对应边线下标
                    if(track.spurroad[0].y > 70 && track.spurroad[0].x < ROWSIMAGE - 40)
                    {
                        _corner = track.spurroad[0];
                        spurroad_item = abs(_corner.x - track.pointsEdgeLeft[0].x);
                        rowBreakLeftD = searchBreakLeftDown(track.pointsEdgeLeft, 0, spurroad_item);
                    }
                    else
                    {
                        rowBreakLeftD = searchBreakLeftDown(track.pointsEdgeLeft, 0, track.pointsEdgeLeft.size());
                        uint16_t rowBreakLeftU = searchBreakLeftUp(track.pointsEdgeLeft);
                        if(rowBreakLeftU > ROWSIMAGE / 2 && rowBreakLeftU > rowBreakLeftD)
                        {
                            spurroad_item = rowBreakLeftU;
                            _corner = track.pointsEdgeLeft[rowBreakLeftU];
                        }
                    }
                }
                else
                {
                    for(int i = 0; i < track.spurroad.size(); i++)
                    {
                        if(track.spurroad[i].x > _corner.x && track.spurroad[i].y > 70 && track.spurroad[i].x < COLSIMAGE / 2)
                        {
                            _corner = track.spurroad[i];
                        }
                        spurroad_item = abs(_corner.x - track.pointsEdgeLeft[0].x);
                        rowBreakLeftD = searchBreakLeftDown(track.pointsEdgeLeft, 0, spurroad_item);
                    }
                }

                pointBreakD = track.pointsEdgeLeft[0];
                pointBreakU = track.pointsEdgeLeft[rowBreakLeftD];
                if(rowBreakLeftD)
                {
                    line(track.pointsEdgeLeft, 0, rowBreakLeftD);
                    repaired = true;
                }
                else if(_corner.x == 0)
                    line(track.pointsEdgeLeft, 0, track.pointsEdgeLeft[track.pointsEdgeLeft.size() - 1]);

                if(_corner.x)
                {
                    if(_corner.x > ROWSIMAGE / 3)
                        counterSpurroad++;
                    // line(track.pointsEdgeRight, rowBreakLeftD, _corner);
                    {
                        uint16_t rowBreakRightD = rowBreakLeftD;
                        if(rowBreakRightD > 50)
                            rowBreakRightD -= 50;
                        else
                            rowBreakRightD = 0;
                        if(rowBreakRightD < 3)
                        {
                            float rowRate = _corner.x;
                            if(rowRate < 50)
                                rowRate = 0;
                            else
                                rowRate -= 50;
                            track.pointsEdgeRight[rowBreakRightD].y -= (rowRate / ROWSIMAGE) * (COLSIMAGE - _corner.y);
                        }
                        POINT startPoint = track.pointsEdgeRight[rowBreakRightD];
                        POINT endPoint = _corner;
                        POINT midPoint = POINT((0.3*startPoint.x + 0.7*endPoint.x), (0.3*startPoint.y + 0.7*endPoint.y));
                        midPoint.y += abs(startPoint.y - endPoint.y) / 4;
                        uint16_t rowBreakMid = abs(midPoint.x - startPoint.x) + rowBreakRightD - 1;
                        line(track.pointsEdgeRight, rowBreakRightD, midPoint);
                        line(track.pointsEdgeRight, rowBreakMid, endPoint);
                    }
                    track.pointsEdgeLeft.resize(spurroad_item);
                    track.pointsEdgeRight.resize(spurroad_item);

                    const uint16_t width_thresh = track.pointsEdgeRight[spurroad_item - 1].y - track.pointsEdgeLeft[spurroad_item - 1].y;
                    uint16_t mid = (track.pointsEdgeLeft[spurroad_item - 1].y + track.pointsEdgeRight[spurroad_item - 1].y) / 2;
                    POINT left(0, 0);
                    POINT right(0, 0);
                    for(int i = _corner.x - 1; i > ROWSIMAGE / 4; i--)
                    {
                        int j = mid;
                        for(j = mid; j < COLSIMAGE - 5; j++)
                        {
                            if(img_bin.at<uchar>(i, j) == 255 && img_bin.at<uchar>(i, j + 1) == 0 
                                && img_bin.at<uchar>(i, j + 2) == 0 && img_bin.at<uchar>(i, j + 3) == 0)
                            {
                                right = POINT(i, j);
                                break;
                            }
                        }
                        if(j == COLSIMAGE - 5)
                        {
                            right = POINT(i, COLSIMAGE - 1);
                        }

                        for(j = mid; j > 5; j--)
                        {
                            if(img_bin.at<uchar>(i, j) == 255 && img_bin.at<uchar>(i, j - 1) == 0 
                                && img_bin.at<uchar>(i, j - 2) == 0 && img_bin.at<uchar>(i, j - 3) == 0)
                            {
                                left = POINT(i, j);
                                break;
                            }
                        }
                        if(j == 5)
                        {
                            left = POINT(i, 0);
                        }
                        uint16_t width = abs(right.y - left.y);
                        if(width < COLSIMAGE / 10 || width > width_thresh)
                        {
                            break;
                        }
                        if(left.x && right.x)
                        {
                            track.pointsEdgeLeft.push_back(left);
                            track.pointsEdgeRight.push_back(right);
                            track.widthBlock.push_back(POINT(i, width));
                        }

                        mid = (right.y + left.y) / 2;
                    }
                }
                else if(_corner.x == 0 && counterSpurroad > 2)
                {
                    counterSpurroad = 0;
                    track.pointsEdgeLeft.clear(); // 上角点丢失，使用上一帧的数据
                    track.pointsEdgeRight.clear();
                    ringStep = RingStep::Inside;
                }
                else if(repaired)//右边线优化
                {
                    track.pointsEdgeRight = track.predictEdgeRight(track.pointsEdgeLeft, false);
                }

                if(rowBreakLeftD > 10)
                {
                    uint16_t counter = 0;
                    for(int i = rowBreakLeftD; i < track.pointsEdgeLeft.size(); i++)
                    {
                        if(track.pointsEdgeLeft[i].y < 3)
                            counter++;
                        if(counter > 2)
                        {
                            track.pointsEdgeLeft.resize(i);
                            break;
                        }
                    }
                }
                else if(rowBreakLeftD == 0 && _corner.x)
                    track.pointsEdgeLeft.resize(5);
            }
            else if(ringType == RingType::RingRight)
            {
                bool repaired = false;
                uint16_t spurroad_item = 0;
                uint16_t rowBreakRightD = 0;
                if(track.spurroad.size() == 0)
                {
                    rowBreakRightD = searchBreakRightDown(track.pointsEdgeRight, 0, track.pointsEdgeRight.size());
                    uint16_t rowBreakRightU = searchBreakRightUp(track.pointsEdgeRight);
                    if(rowBreakRightU > ROWSIMAGE / 2 && track.pointsEdgeRight[rowBreakRightU].y < COLSIMAGE - 70)
                    {
                        spurroad_item = rowBreakRightU;
                        _corner = track.pointsEdgeRight[rowBreakRightU];
                    }
                }
                else if(track.spurroad.size() == 1)
                {
                    if(track.spurroad[0].y < COLSIMAGE - 70 && track.spurroad[0].x < ROWSIMAGE - 40)
                    {
                        //寻找岔路行对应边线下标
                        _corner = track.spurroad[0];
                        spurroad_item = abs(_corner.x - track.pointsEdgeRight[0].x);
                        rowBreakRightD = searchBreakRightDown(track.pointsEdgeRight, 0, spurroad_item);
                    }
                    else
                    {
                        rowBreakRightD = searchBreakRightDown(track.pointsEdgeRight, 0, track.pointsEdgeRight.size());
                        uint16_t rowBreakRightU = searchBreakRightUp(track.pointsEdgeRight);
                        if(rowBreakRightU > ROWSIMAGE / 2 && rowBreakRightU > rowBreakRightD)
                        {
                            _corner = track.pointsEdgeRight[rowBreakRightU];
                        }
                    }
                }
                else
                {
                    for(int i = 0; i < track.spurroad.size(); i++)
                    {
                        if(track.spurroad[i].x > _corner.x && track.spurroad[i].y < COLSIMAGE - 70 && track.spurroad[i].x < COLSIMAGE / 2)
                        {
                            _corner = track.spurroad[i];
                        }
                        spurroad_item = abs(_corner.x - track.pointsEdgeRight[0].x);
                        rowBreakRightD = searchBreakRightDown(track.pointsEdgeRight, 0, spurroad_item);
                    }
                }

                pointBreakD = track.pointsEdgeRight[0];
                pointBreakU = track.pointsEdgeRight[rowBreakRightD];
                if(rowBreakRightD)
                {
                    repaired = true;
                    line(track.pointsEdgeRight, 0, rowBreakRightD);
                }
                else if(_corner.x == 0)
                    line(track.pointsEdgeRight, 0, track.pointsEdgeRight[track.pointsEdgeRight.size() - 1]);

                if(_corner.x)
                {
                    if(_corner.x > ROWSIMAGE / 3)
                        counterSpurroad++;
                    // line(track.pointsEdgeLeft, rowBreakRightD, _corner);
                    {
                        uint16_t rowBreakLeftD = rowBreakRightD;
                        if(rowBreakLeftD > 50)
                            rowBreakLeftD -= 50;
                        else 
                            rowBreakLeftD = 0;
                        if(rowBreakLeftD < 3)
                        {
                            float rowRate = _corner.x;
                            if(rowRate < 50)
                                rowRate = 0;
                            else
                                rowRate -= 50;
                            track.pointsEdgeLeft[rowBreakLeftD].y += (rowRate / ROWSIMAGE) * (_corner.y);
                        }
                        POINT startPoint = track.pointsEdgeLeft[rowBreakLeftD];
                        POINT endPoint = _corner;
                        POINT midPoint = POINT((0.3*startPoint.x + 0.7*endPoint.x), (0.3*startPoint.y + 0.7*endPoint.y));
                        midPoint.y -= abs(startPoint.y - endPoint.y) / 4;
                        uint16_t rowBreakMid = abs(midPoint.x - startPoint.x) + rowBreakLeftD - 1;
                        line(track.pointsEdgeLeft, rowBreakLeftD, midPoint);
                        line(track.pointsEdgeLeft, rowBreakMid, endPoint);
                    }
                    track.pointsEdgeLeft.resize(spurroad_item);
                    track.pointsEdgeRight.resize(spurroad_item);

                    const uint16_t width_thresh = track.pointsEdgeRight[spurroad_item - 1].y - track.pointsEdgeLeft[spurroad_item - 1].y;
                    uint16_t mid = (track.pointsEdgeLeft[spurroad_item - 1].y + track.pointsEdgeRight[spurroad_item - 1].y) / 2;
                    POINT left(0, 0);
                    POINT right(0, 0);
                    for(int i = _corner.x - 1; i > ROWSIMAGE / 4; i--)
                    {
                        int j = mid;
                        for(j = mid; j < COLSIMAGE - 5; j++)
                        {
                            if(img_bin.at<uchar>(i, j) == 255 && img_bin.at<uchar>(i, j + 1) == 0 
                                && img_bin.at<uchar>(i, j + 2) == 0 && img_bin.at<uchar>(i, j + 3) == 0)
                            {
                                right = POINT(i, j);
                                break;
                            }
                        }
                        if(j == COLSIMAGE - 5)
                        {
                            right = POINT(i, COLSIMAGE - 1);
                        }

                        for(j = mid; j > 5; j--)
                        {
                            if(img_bin.at<uchar>(i, j) == 255 && img_bin.at<uchar>(i, j - 1) == 0 
                                && img_bin.at<uchar>(i, j - 2) == 0 && img_bin.at<uchar>(i, j - 3) == 0)
                            {
                                left = POINT(i, j);
                                break;
                            }
                        }
                        if(j == 5)
                        {
                            left = POINT(i, 0);
                        }
                        uint16_t width = abs(right.y - left.y);
                        if(width < COLSIMAGE / 10 || width > width_thresh)
                        {
                            break;
                        }
                        if(left.x && right.x)
                        {
                            track.pointsEdgeLeft.push_back(left);
                            track.pointsEdgeRight.push_back(right);
                            track.widthBlock.push_back(POINT(i, width));
                        }

                        mid = (right.y + left.y) / 2;
                    }
                    
                }
                else if(_corner.x == 0 && counterSpurroad > 2)
                {
                    counterSpurroad = 0;
                    track.pointsEdgeLeft.clear(); // 上角点丢失，使用上一帧的数据
                    track.pointsEdgeRight.clear();
                    ringStep = RingStep::Inside;
                }
                else if(repaired)//左边线优化
                {
                    track.pointsEdgeLeft = track.predictEdgeLeft(track.pointsEdgeRight, false);
                }

                if(rowBreakRightD > 10)
                {
                    uint16_t counter = 0;
                    for(int i = rowBreakRightD; i < track.pointsEdgeRight.size(); i++)
                    {
                        if(track.pointsEdgeRight[i].y > COLSIMAGE - 3)
                            counter++;
                        if(counter > 2)
                        {
                            track.pointsEdgeRight.resize(i);
                            break;
                        }
                    }
                }
                else if(rowBreakRightD == 0 && _corner.x)
                    track.pointsEdgeRight.resize(5);
            }
        }
        else if(ringStep == RingStep::Inside)
        {
            if(ringType == RingType::RingLeft)
            {
                uint16_t rowBreakRight = searchBreakRightDown(track.pointsEdgeRight, 0, track.pointsEdgeRight.size() - 10);
                //寻找左边跳变点
                uint16_t rowBreakLeft = 0;
                uint16_t counter = 0;
                for(int i = track.pointsEdgeLeft.size() - 3; i > 60; i--)
                {
                    if(track.pointsEdgeLeft[i].y < 5)
                    {
                        counter++;
                    }
                    else
                    {
                        counter = 0;
                    }
                    if(counter > 5)
                    {
                        rowBreakLeft = i + 5;
                        break;
                    }
                }
                if(rowBreakRight && counterSpurroad > 0)
                {
                    // if(track.pointsEdgeRight[rowBreakRight].y > COLSIMAGE / 2 - 30)
                        counterSpurroad++; 

                    pointBreakD = track.pointsEdgeRight[rowBreakRight];
                    pointBreakU = track.pointsEdgeLeft[rowBreakLeft];
                    pointBreakU.y += COLSIMAGE / 8;
                    line(track.pointsEdgeRight, rowBreakRight, pointBreakU);
                    track.pointsEdgeLeft.resize(rowBreakLeft);
                    track.pointsEdgeRight.resize(rowBreakLeft);
                }
                else if(rowBreakRight && counterSpurroad == 0)// 刚进入环岛就开始判断出环，错误，清空边线使用上一帧
                {
                    track.pointsEdgeLeft.clear();
                    track.pointsEdgeRight.clear();
                }
                else if(!rowBreakRight && counterSpurroad > 3)
                {
                    pointBreakD = track.pointsEdgeRight[0];
                    pointBreakU = track.pointsEdgeLeft[rowBreakLeft];
                    pointBreakU.y += COLSIMAGE / 8;
                    line(track.pointsEdgeRight, rowBreakRight, pointBreakU);
                    track.pointsEdgeLeft.resize(rowBreakLeft);
                    track.pointsEdgeRight.resize(rowBreakLeft);

                    if(track.pointsEdgeLeft[track.pointsEdgeLeft.size() - 1].y < COLSIMAGE / 2)
                    {
                        ringStep = RingStep::Exiting;
                        counterSpurroad = 0;
                    }
                }
                else
                {
                    if(counterSpurroad == 0) // 判断正式进入环岛，此时开始允许判断出环
                        counterSpurroad++;

                    if(track.pointsEdgeLeft.size() < 100 && track.pointsEdgeRight.size() < 100)
                    {
                        track.pointsEdgeLeft = pointsEdgeLeftLast;
                        track.pointsEdgeRight = pointsEdgeRightLast;
                        line(track.pointsEdgeRight, 0, track.pointsEdgeRight[track.pointsEdgeRight.size() - 1]);
                    }
                    else if(track.pointsEdgeLeft[0].x < ROWSIMAGE - track.rowCutBottom - 30 ||  track.pointsEdgeRight[0].x < ROWSIMAGE - track.rowCutBottom - 30)
                    {
                        track.pointsEdgeLeft = pointsEdgeLeftLast;
                        track.pointsEdgeRight = pointsEdgeRightLast;
                        line(track.pointsEdgeRight, 0, track.pointsEdgeRight[track.pointsEdgeRight.size() - 1]);
                    }
                    else
                    {
                        // track.pointsEdgeLeft.resize(0);
                        pointsEdgeLeftLast = track.pointsEdgeLeft;
                        pointsEdgeRightLast = track.pointsEdgeRight;
                    }
                }
            }
            else if(ringType == RingType::RingRight)
            {
                uint16_t rowBreakLeft = searchBreakLeftDown(track.pointsEdgeLeft, 0, track.pointsEdgeLeft.size() - 10);
                //寻找右边跳变点
                uint16_t rowBreakRight = 0;
                uint16_t counter = 0;
                for(int i = track.pointsEdgeRight.size() - 3; i > 60; i--)
                {
                    if(track.pointsEdgeRight[i].y > COLSIMAGE - 5)
                    {
                        counter++;
                    }
                    else
                    {
                        counter = 0;
                    }
                    if(counter > 5)
                    {
                        rowBreakRight = i + 5;
                        break;
                    }
                }
                if(rowBreakLeft && counterSpurroad > 0)
                {
                    // if(track.pointsEdgeLeft[rowBreakLeft].y < COLSIMAGE / 2 + 30)
                        counterSpurroad++;

                    pointBreakU = track.pointsEdgeRight[rowBreakRight];
                    pointBreakD = track.pointsEdgeLeft[rowBreakLeft];
                    pointBreakU.y -= COLSIMAGE / 8;
                    line(track.pointsEdgeLeft, rowBreakLeft, pointBreakU);
                    track.pointsEdgeLeft.resize(rowBreakRight);
                    track.pointsEdgeRight.resize(rowBreakRight);
                }
                else if(rowBreakLeft && counterSpurroad == 0)// 刚进入环岛就开始判断出环，错误，清空边线使用上一帧
                {
                    track.pointsEdgeLeft.clear();
                    track.pointsEdgeRight.clear();
                }
                else if(!rowBreakLeft && counterSpurroad > 2)
                {
                    pointBreakU = track.pointsEdgeRight[rowBreakRight];
                    pointBreakD = track.pointsEdgeLeft[0];
                    pointBreakU.y -= COLSIMAGE / 8;
                    line(track.pointsEdgeLeft, rowBreakLeft, pointBreakU);
                    track.pointsEdgeLeft.resize(rowBreakRight);
                    track.pointsEdgeRight.resize(rowBreakRight);

                    if(track.pointsEdgeRight[track.pointsEdgeRight.size() - 1].y > COLSIMAGE / 2)
                    {
                        ringStep = RingStep::Exiting;
                        counterSpurroad = 0;
                    }
                }
                else
                {
                    if(counterSpurroad == 0) // 判断正式进入环岛，此时开始允许判断出环
                        counterSpurroad++;

                    if(track.pointsEdgeLeft.size() < 100 && track.pointsEdgeRight.size() < 100)
                    {
                        track.pointsEdgeLeft = pointsEdgeLeftLast;
                        track.pointsEdgeRight = pointsEdgeRightLast;
                        line(track.pointsEdgeLeft, 0, track.pointsEdgeLeft[track.pointsEdgeLeft.size() - 1]);
                    }
                    else if(track.pointsEdgeLeft[0].x < ROWSIMAGE - track.rowCutBottom - 30 ||  track.pointsEdgeRight[0].x < ROWSIMAGE - track.rowCutBottom - 30)
                    {
                        track.pointsEdgeLeft = pointsEdgeLeftLast;
                        track.pointsEdgeRight = pointsEdgeRightLast;
                        line(track.pointsEdgeLeft, 0, track.pointsEdgeLeft[track.pointsEdgeLeft.size() - 1]);
                    }
                    else
                    {
                        // track.pointsEdgeRight.resize(0);
                        pointsEdgeLeftLast = track.pointsEdgeLeft;
                        pointsEdgeRightLast = track.pointsEdgeRight;
                    }
                }
            }
        }
        else if(ringStep == RingStep::Exiting)
        {
            //寻找左边跳变点
            uint16_t rowBreakLeft = 0;
            uint16_t counter = 0;
            for(int i = track.pointsEdgeLeft.size() - 3; i > 60; i--)
            {
                if(track.pointsEdgeLeft[i].y < 5)
                {
                    counter++;
                }
                else
                {
                    counter = 0;
                }
                if(counter > 5)
                {
                    rowBreakLeft = i + 5;
                    break;
                }
            }
            //寻找右边跳变点
            uint16_t rowBreakRight = 0;
            counter = 0;
            for(int i = track.pointsEdgeRight.size() - 3; i > 60; i--)
            {
                if(track.pointsEdgeRight[i].y > COLSIMAGE - 5)
                {
                    counter++;
                }
                else
                {
                    counter = 0;
                }
                if(counter > 5)
                {
                    rowBreakRight = i + 5;
                    break;
                }
            }

            if(ringType == RingType::RingLeft)
            {
                if((rowBreakRight < ROWSIMAGE / 4 || track.pointsEdgeLeft.size() > 140)
                    && track.pointsEdgeLeft[track.pointsEdgeLeft.size() - 1].y < COLSIMAGE / 2)
                {
                    ringStep = RingStep::Finish;
                }
                else
                {
                    pointBreakU = track.pointsEdgeLeft[rowBreakLeft];
                    pointBreakU.y += COLSIMAGE / 8;
                    pointBreakD = track.pointsEdgeRight[0];
                    line(track.pointsEdgeRight, 0, pointBreakU);
                    track.pointsEdgeLeft.resize(rowBreakLeft);
                    track.pointsEdgeRight.resize(rowBreakLeft);
                }
            }
            else if(ringType == RingType::RingRight)
            {
                if((rowBreakLeft < ROWSIMAGE / 4 || track.pointsEdgeRight.size() > 140)
                    && track.pointsEdgeRight[track.pointsEdgeRight.size() - 1].y > COLSIMAGE / 2)
                {
                    ringStep = RingStep::Finish;
                }
                else
                {
                    pointBreakU = track.pointsEdgeRight[rowBreakRight];
                    pointBreakU.y -= COLSIMAGE / 8;
                    pointBreakD = track.pointsEdgeLeft[0];
                    line(track.pointsEdgeLeft, 0, pointBreakU);
                    track.pointsEdgeLeft.resize(rowBreakRight);
                    track.pointsEdgeRight.resize(rowBreakRight);
                }
            }
        }
        else if(ringStep == RingStep::Finish)
        {
            if(ringType == RingType::RingLeft)
            {
                uint16_t rowBreakLeft = searchBreakLeftUp(track.pointsEdgeLeft);
                pointBreakU = track.pointsEdgeLeft[rowBreakLeft];
                if(rowBreakLeft >= ROWSIMAGE / 2)
                {
                    counterSpurroad++;
                    pointBreakD = track.pointsEdgeLeft[0];
                    line(track.pointsEdgeLeft, 0, pointBreakU);
                }
                else if(rowBreakLeft < ROWSIMAGE / 2 && rowBreakLeft > 0)
                {
                    if(counterSpurroad > 3)
                        reset();
                    else
                        counterSpurroad++;
                }
                else if(counterSpurroad && track.spurroad.size() > 0)
                {
                    track.pointsEdgeLeft = track.predictEdgeLeft(track.pointsEdgeRight);
                }
            }
            else if(ringType == RingType::RingRight)
            {
                uint16_t rowBreakRight = searchBreakRightUp(track.pointsEdgeRight);
                pointBreakU = track.pointsEdgeRight[rowBreakRight];
                if(rowBreakRight >= ROWSIMAGE / 2)
                {
                    counterSpurroad++;
                    pointBreakD = track.pointsEdgeRight[0];
                    line(track.pointsEdgeRight, 0, pointBreakU);
                }
                else if(rowBreakRight < ROWSIMAGE / 2 && rowBreakRight > 0)
                {
                    if(counterSpurroad > 3)
                        reset();
                    else
                        counterSpurroad++;
                }
                else if(counterSpurroad && track.spurroad.size() > 0)
                {
                    track.pointsEdgeRight = track.predictEdgeRight(track.pointsEdgeLeft);                    
                }
            }

            if (track.garageEnable.x)
            {
                reset();
            }
        }

        if (ringType == RingType::RingNone)
            return false;
        else
            return true;
    }

    void drawImage(TrackRecognition track, Mat &Image)
    {
        // 绘制边缘点
        for (int i = 0; i < track.pointsEdgeLeft.size(); i++)
        {
            circle(Image, Point(track.pointsEdgeLeft[i].y, track.pointsEdgeLeft[i].x), 2,
                   Scalar(0, 255, 0), -1); // 绿色点
        }
        for (int i = 0; i < track.pointsEdgeRight.size(); i++)
        {
            circle(Image, Point(track.pointsEdgeRight[i].y, track.pointsEdgeRight[i].x), 2,
                   Scalar(0, 255, 255), -1); // 黄色点
        }

        // 绘制岔路点
        for (int i = 0; i < track.spurroad.size(); i++)
        {
            circle(Image, Point(track.spurroad[i].y, track.spurroad[i].x), 3, Scalar(0, 0, 255), -1); // 红色点
        }
        circle(Image, Point(_corner.y, _corner.x), 6, Scalar(0, 0, 255), -1); // 红色点

        // 绘制补线点
        {
            circle(Image, Point(pointBreakU.y, pointBreakU.x), 5, Scalar(255, 0, 255), -1); // 上补线点：粉色
            circle(Image, Point(pointBreakD.y, pointBreakD.x), 5, Scalar(226, 43, 138), -1); // 下补线点：紫色
        }
        if (track.garageEnable.x)
        {
            line(Image, Point(0, track.garageEnable.y), Point(COLSIMAGE - 1, track.garageEnable.y), Scalar(211, 211, 211), 1);
        }
        putText(Image, to_string(ringStep), Point(COLSIMAGE / 2 - 5, ROWSIMAGE - 40), cv::FONT_HERSHEY_TRIPLEX, 0.5, cv::Scalar(0, 0, 155), 1, CV_AA);
        if(ringType == RingType::RingLeft)
        {
            putText(Image, "Ring L", Point(COLSIMAGE / 2 - 5, 20), cv::FONT_HERSHEY_TRIPLEX, 0.5, cv::Scalar(0, 255, 0), 1, CV_AA); // 显示赛道识别类型
        }
        else if(ringType == RingType::RingRight)
        {
            putText(Image, "Ring R", Point(COLSIMAGE / 2 - 5, 20), cv::FONT_HERSHEY_TRIPLEX, 0.5, cv::Scalar(0, 255, 0), 1, CV_AA); // 显示赛道识别类型
        }
        putText(Image, to_string(track.validRowsRight) + " " + to_string(track.stdevRight), Point(COLSIMAGE - 100, ROWSIMAGE - 50),
                FONT_HERSHEY_TRIPLEX, 0.3, Scalar(0, 0, 255), 1);
        putText(Image, to_string(track.validRowsLeft) + " " + to_string(track.stdevLeft), Point(20, ROWSIMAGE - 50),
                FONT_HERSHEY_TRIPLEX, 0.3, Scalar(0, 0, 255), 1);
    }

    float get_speed(float motionSpeed)
    {
        if(ringStep == RingStep::Entering)
            return (motionSpeed * 0.8f);
        else
            return motionSpeed;
    }

private:

    /**
     * @brief 环岛类型
     *
     */
    enum RingType
    {
        RingNone = 0, // 未知类型
        RingLeft,     // 左入环岛
        RingRight     // 右入环岛
    };

    /**
     * @brief 环岛运行步骤/阶段
     *
     */
    enum RingStep
    {
        None = 0, // 未知类型
        Entering, // 入环
        Inside,   // 环中
        Exiting,  // 出环
        Finish    // 环任务结束
    };

    RingType ringType = RingType::RingNone; // 环岛类型
    RingStep ringStep = RingStep::None;     // 环岛处理阶段

    /**
     * @brief 搜索环岛赛道突变行（左下）
     *
     * @param pointsEdgeLeft
     * @return uint16_t
     */
    uint16_t searchBreakLeftDown(vector<POINT> pointsEdgeLeft, uint16_t row_start, uint16_t row_end)
    {
        bool start = false;
        uint16_t rowBreakLeft = 1;
        uint16_t counter = 0;

        if(row_start == 0)
        {
            row_start++;
        }
        if(row_end > pointsEdgeLeft.size())
        {
            row_end = pointsEdgeLeft.size();
        }
        if(row_start > pointsEdgeLeft.size())
        {
            row_start = pointsEdgeLeft.size();
        }
        for (int i = row_start; i < row_end; i++) // 寻找左边跳变点
        {
            if(pointsEdgeLeft[i].y > pointsEdgeLeft[i - 1].y && start == false)
            {
                start = true;
            }
            if(start)
            {
                if (pointsEdgeLeft[i].y > pointsEdgeLeft[rowBreakLeft].y)
                {
                    rowBreakLeft = i;
                    counter = 0;
                }
                else if (pointsEdgeLeft[i].y <= pointsEdgeLeft[rowBreakLeft].y) // 突变点计数
                {
                    if(row_end > COLSIMAGE / 2 || abs(pointsEdgeLeft[i].y - pointsEdgeLeft[rowBreakLeft].y) > 10 || pointsEdgeLeft[i].y < 3)
                        counter++;
                    if (counter > 8)
                        return rowBreakLeft;
                }
            }
        }

        return 0;
    }

    /**
     * @brief 搜索环岛赛道突变行（右下）
     *
     * @param pointsEdgeRight
     * @return uint16_t
     */
    uint16_t searchBreakRightDown(vector<POINT> pointsEdgeRight, uint16_t row_start, uint16_t row_end)
    {
        bool start = false;
        uint16_t rowBreakRight = 1;
        uint16_t counter = 0;

        if(row_start == 0)
        {
            row_start++;
        }
        if(row_end > pointsEdgeRight.size())
        {
            row_end = pointsEdgeRight.size();
        }
        if(row_start > pointsEdgeRight.size())
        {
            row_start = pointsEdgeRight.size();
        }
        for (int i = row_start; i < row_end; i++) // 寻找右边跳变点
        {
            if(pointsEdgeRight[i].y < pointsEdgeRight[i - 1].y && start == false)
            {
                start = true;
            }
            if(start)
            {
                if (pointsEdgeRight[i].y < pointsEdgeRight[rowBreakRight].y)
                {
                    rowBreakRight = i;
                    counter = 0;
                }
                else if (pointsEdgeRight[i].y >= pointsEdgeRight[rowBreakRight].y) // 突变点计数
                {
                    if(row_end > COLSIMAGE / 2 || abs(pointsEdgeRight[i].y - pointsEdgeRight[rowBreakRight].y) > 10 || pointsEdgeRight[i].y > COLSIMAGE - 3)
                        counter++;
                    if (counter > 8)
                        return rowBreakRight;
                }
            }
        }

        return 0;
    }

    /**
     * @brief 搜索环岛赛道突变行（左上）
     *
     * @param pointsEdgeLeft
     * @return uint16_t
     */
    uint16_t searchBreakLeftUp(vector<POINT> pointsEdgeLeft)
    {
        uint16_t rowBreakLeftUp = pointsEdgeLeft.size() - 1;
        uint16_t counter = 0;
        uint16_t counterFilter = 0;
        for (int i = pointsEdgeLeft.size() - 5; i > 0; i--)
        {
            if (pointsEdgeLeft[i].y > 5 && abs(pointsEdgeLeft[i].y - pointsEdgeLeft[i + 1].y) < 5)
            {
                rowBreakLeftUp = i;
                counter = 0;
                counterFilter++;
            }
            else if (pointsEdgeLeft[i].y <= 5 && abs(pointsEdgeLeft[i].y - pointsEdgeLeft[rowBreakLeftUp].y) > 5 && counterFilter > 5)
            {
                counter++;
                if (counter > 3)
                    return rowBreakLeftUp;
            }
        }

        return 0;
    }

    /**
     * @brief 搜索环岛赛道突变行（右上）
     *
     * @param pointsEdgeRight
     * @return uint16_t
     */
    uint16_t searchBreakRightUp(vector<POINT> pointsEdgeRight)
    {
        uint16_t rowBreakRightUp = pointsEdgeRight.size() - 1;
        uint16_t counter = 0;
        uint16_t counterFilter = 0;
        for (int i = pointsEdgeRight.size() - 5; i > 0; i--)
        {
            if (pointsEdgeRight[i].y < COLSIMAGE - 2 && abs(pointsEdgeRight[i].y - pointsEdgeRight[i + 1].y) < 5)
            {
                rowBreakRightUp = i;
                counter = 0;
                counterFilter++;
            }
            else if (pointsEdgeRight[i].y >= COLSIMAGE - 2 && abs(pointsEdgeRight[i].y - pointsEdgeRight[rowBreakRightUp].y) > 5 && counterFilter > 5)
            {
                counter++;
                if (counter > 3)
                    return rowBreakRightUp;
            }
        }

        return 0;
    }
public:
    uint16_t counterShield = 0; // 环岛检测屏蔽计数器
private:
    POINT _corner;
    uint16_t counterSpurroad = 0; // 岔路计数器
    uint16_t ring_cnt = 0; // 环岛检测确认计数器
	uint16_t counterExit = 0;	  // 异常退出计数器

    POINT pointBreakU;
    POINT pointBreakD;
    vector<POINT> pointsEdgeLeftLast;  // 记录前一场左边缘点集
    vector<POINT> pointsEdgeRightLast; // 记录前一场右边缘点集
};
 