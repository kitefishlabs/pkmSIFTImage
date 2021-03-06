/*
 *  pkmSIFTFlow.cpp
 *  pkmSIFTImage
 *
 Copyright (C) 2011 Parag K. Mital
 
 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "pkmSIFTFlow.h"
#include "pkmOpenCVUtils.h"

pkmSIFTFlow::pkmSIFTFlow()
{
    alpha           = 2*255;
    d               = alpha*20;
    gamma           = 0.005*255;
    nlevels         = 4;
    wsize           = 2;
    topwsize        = 10;
    nTopIterations  = 60;
    nIterations     = 30;
    
    
    /*
    xx = meshgridX(1, width, 1, height);        
    yy = meshgridX(1, width, 1, height);
     */
    
    
    pEnergyList = new double[nIterations];
}    

void pkmSIFTFlow::allocate(int w, int h)
{
    if (bAllocated) {
        printf("[pkmSIFTFlow]: Reallocating...\n");
    }
    
    width = w;
    height = h;
    
    winSizeX_level4 = Mat::ones(height/8, width/8, CV_32SC1) * topwsize;
    winSizeY_level4 = Mat::ones(height/8, width/8, CV_32SC1) * topwsize;
    winSizeX_level3 = Mat::ones(height/4, width/4, CV_32SC1) * (wsize + 2);
    winSizeY_level3 = Mat::ones(height/4, width/4, CV_32SC1) * (wsize + 2);
    winSizeX_level2 = Mat::ones(height/2, width/2, CV_32SC1) * (wsize + 1);
    winSizeY_level2 = Mat::ones(height/2, width/2, CV_32SC1) * (wsize + 1);
    winSizeX_level1 = Mat::ones(height/1, width/1, CV_32SC1) * (wsize + 0);
    winSizeY_level1 = Mat::ones(height/1, width/1, CV_32SC1) * (wsize + 0);
}

void pkmSIFTFlow::computeFlow(Mat image1, Mat image2, int nchannels)
{
#ifdef _DEBUG
    assert(image1.cols == image2.cols &&
           image1.rows == image2.rows);
#endif
    
    width = image1.cols;
    height = image1.rows;
    
    sift1_level1 = image1;
    sift2_level1 = image2;
    
    pyrDown(sift1_level1, sift1_level2, cv::Size(width/2, height/2));
    pyrDown(sift2_level1, sift2_level2, cv::Size(width/2, height/2));
    
    pyrDown(sift1_level2, sift1_level3, cv::Size(width/4, height/4));
    pyrDown(sift2_level2, sift2_level3, cv::Size(width/4, height/4));
    
    pyrDown(sift1_level3, sift1_level4, cv::Size(width/8, height/8));
    pyrDown(sift2_level3, sift2_level4, cv::Size(width/8, height/8));
    
    if (!sift1_level3.isContinuous()) {
        printf("[ERROR] Matrix not continuous!!!\n");
    }
    if (!sift2_level3.isContinuous()) {
        printf("[ERROR] Matrix not continuous!!!\n");
    }
    if (!sift1_level2.isContinuous()) {
        printf("[ERROR] Matrix not continuous!!!\n");
    }
    if (!sift2_level2.isContinuous()) {
        printf("[ERROR] Matrix not continuous!!!\n");
    }
    if (!sift1_level1.isContinuous()) {
        printf("[ERROR] Matrix not continuous!!!\n");
    }
    if (!sift2_level1.isContinuous()) {
        printf("[ERROR] Matrix not continuous!!!\n");
    }
    bpflow.LoadImages(width, height, nchannels, sift1_level4.ptr<unsigned char>(0), sift2_level4.ptr<unsigned char>(0));
    bpflow.setPara(alpha, d);
    
    // first assume homogeneous setup
    bpflow.setHomogeneousMRF(wsize);    
    
    // level 4
    vx_level4 = Mat::zeros(height/8, width/8, CV_32FC1);
    vy_level4 = Mat::zeros(height/8, width/8, CV_32FC1);
    bpflow.LoadOffset(vx_level4.ptr(0), vy_level4.ptr(0));
    bpflow.LoadWinSize(winSizeX_level4.ptr(0), winSizeY_level4.ptr(0));
    
    bpflow.ComputeDataTerm();
    bpflow.ComputeRangeTerm(gamma);
    bpflow.MessagePassing(nIterations, 2, pEnergyList);
    bpflow.ComputeVelocity();
    {
        Mat flow(height/8, width/8, CV_32FC2, bpflow.flow().pData);
        vector<Mat> flows;
        split(flow, flows);
        
        vx_level4 = flows[0];
        vy_level4 = flows[1];
    }
    
    pyrUp(vx_level4, vx_level3, cv::Size(width/4, height/4));
    pyrUp(vy_level4, vy_level3, cv::Size(width/4, height/4));
    
    // level 3
    bpflow.LoadOffset(vx_level3.ptr(0), vy_level3.ptr(0));
    bpflow.LoadWinSize(winSizeX_level3.ptr(0), winSizeY_level3.ptr(0));
    
    bpflow.ComputeDataTerm();
    bpflow.ComputeRangeTerm(gamma);
    bpflow.MessagePassing(nIterations, 2, pEnergyList);
    bpflow.ComputeVelocity();
    {
        Mat flow(height/4, width/4, CV_32FC2, bpflow.flow().pData);
        vector<Mat> flows;
        split(flow, flows);
        
        vx_level3 = flows[0];
        vy_level3 = flows[1];
    }
    
    pyrUp(vx_level3, vx_level2, cv::Size(width/2, height/2));
    pyrUp(vy_level3, vy_level2, cv::Size(width/2, height/2));
    
    // level 2
    bpflow.LoadOffset(vx_level2.ptr(0), vy_level2.ptr(0));
    bpflow.LoadWinSize(winSizeX_level2.ptr(0), winSizeY_level2.ptr(0));
    
    bpflow.ComputeDataTerm();
    bpflow.ComputeRangeTerm(gamma);
    bpflow.MessagePassing(nIterations, 2, pEnergyList);
    bpflow.ComputeVelocity();
    {
        Mat flow(height/2, width/2, CV_32FC2, bpflow.flow().pData);
        vector<Mat> flows;
        split(flow, flows);
        
        vx_level2 = flows[0];
        vy_level2 = flows[1];
    }
    
    pyrUp(vx_level2, vx_level1, cv::Size(width, height));
    pyrUp(vy_level2, vy_level1, cv::Size(width, height));
    
    // level 1
    bpflow.LoadOffset(vx_level1.ptr(0), vy_level1.ptr(0));
    bpflow.LoadWinSize(winSizeX_level1.ptr(0), winSizeY_level1.ptr(0));
    
    bpflow.ComputeDataTerm();
    bpflow.ComputeRangeTerm(gamma);
    bpflow.MessagePassing(nIterations, 2, pEnergyList);
    bpflow.ComputeVelocity();
    {
        Mat flow(height, width, CV_32FC2, bpflow.flow().pData);
        vector<Mat> flows;
        split(flow, flows);
        
        vx_level1 = flows[0];
        vy_level1 = flows[1];
    }
    
}