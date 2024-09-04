#pragma once

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>






int32_t approx_euclidean_distance(int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
    int32_t absx = x2 - x1;
    if(absx<0)absx=-absx;
    int32_t absdy = y2 - y1;
    if(absdy<0)absdy=-absdy;
    int32_t minabsd=(absx < absdy ? absx : absdy);
    
    return absx + absdy - (minabsd>> 1);
}

//mapPointPairs=[sx1,sy1,dx1,dy1,  sx2,sy2,dx2,dy2,   ......]
//the use case is when you want an executor go sx,sy but it ends up at dx,dy
//Then you sample multiple points as an offset table

//Example 1: input pre-offset
//having a target point sx,sy we wanna find the offseted point and feed to the executor 
//so the executor will end up at target point  sx,sy
//Then, convert_point(sx, sy, ret_x_out, ret_y_out, 0,mapPointPairs, mapSize);


//Example 2: input target estimation
//having a current point dx,dy from executor(the point it got) 
//we wanna find sx,sy(The target point that we wanted it go) 
//Then, convert_point(dx, dy, ret_sx_est, ret_sy_est, 1,mapPointPairs, mapSize);


//Example 3
//Try to put it in a loop back
//convert_point(sx, sy, &dx, &dy, 0,mapPointPairs, mapSize);
//convert_point(dx, dx, &sx_est, &sy_est, 1,mapPointPairs, mapSize);
//(sx_est,sy_est).should be close to original (sx,sy)




int convert_point(int32_t x, int32_t y, int32_t* ret_x_out, int32_t* ret_y_out,int dir,int32_t *mapPointPairs,int32_t mapSize) {
    //
    //dir==0 means you wanna find the pre-offset point to get to the 
    if(mapSize==0)
    {
        *ret_x_out = x;
        *ret_y_out = y;
        return 0;
    }
    int64_t s_sum_x = 0;
    int64_t s_sum_y = 0;
    int64_t total_s = 0;
    
    
    
    int64_t weighted_sum_x = 0;
    int64_t weighted_sum_y = 0;
    int64_t total_weight = 0;
    
    int32_t* dat0=mapPointPairs;
    int32_t* dat2=mapPointPairs+2;
    
    
    
    int32_t* datMapFrom=(dir==0)?dat0:dat2;
    int32_t* datMapTo=(dir==0)?dat2:dat0;
    
    for (int i = 0; i < mapSize; ++i) {
        
        
        int32_t dist = approx_euclidean_distance(x, y, datMapTo[0],datMapTo[1]);
        if (dist == 0) {
            // Direct match found
            *ret_x_out = datMapFrom[0];
            *ret_y_out = datMapFrom[1];
            return 0;
        }

        int64_t weight = (1<<16) / dist;
        // printf("weight:%d\n",weight);
        // if(weight==0)
        // {
        //     s_sum_x+= (datMapFrom[0]-datMapTo[0]);
        //     s_sum_y+=(datMapFrom[1]-datMapTo[1]);
        //     total_s+=1;
        // }
        // else
        // {
        //     weighted_sum_x += (int64_t)weight * (datMapFrom[0]-datMapTo[0]);
        //     weighted_sum_y += (int64_t)weight *(datMapFrom[1]-datMapTo[1]);
        //     total_weight += weight;
        // }
        if(weight==0)weight=1;
        weighted_sum_x += (int64_t)weight * (datMapFrom[0]-datMapTo[0]);
        weighted_sum_y += (int64_t)weight *(datMapFrom[1]-datMapTo[1]);
        total_weight += weight;
        datMapTo+=4;
        datMapFrom+=4;
    }
    // printf("weighted_sum_x:%d\n",weighted_sum_x);
    // printf("weighted_sum_y:%d\n",weighted_sum_y);
    
    // if((total_weight>>5)<total_s)//if the weight isn't that big, blend the small part in
    // {
    //     weighted_sum_x+=s_sum_x;
    //     weighted_sum_y+=s_sum_y;
    //     total_weight+=total_s;
    // }



    if (total_weight != 0) {
        *ret_x_out = x+weighted_sum_x / total_weight;
        *ret_y_out = y+weighted_sum_y / total_weight;
        return 0;
    }
    else{
        // Handle the case when total_weight is zero
        *ret_x_out = 0;
        *ret_y_out = 0;
    }
    return -1;
}


int convert_point_back(int32_t dx, int32_t dy, int32_t* ret_sx_est, int32_t* ret_sy_est,int iterationLimit,int tolerance,int32_t *mapPointPairs,int32_t mapSize)
{
    int32_t x_in_est, y_in_est;
    
    int32_t correct_dx=dx;
    int32_t correct_dy=dy;
    
    for(int i=0;;i++)
    {
        
        convert_point(correct_dx, correct_dy, &x_in_est, &y_in_est,1,mapPointPairs,mapSize);
        
        int32_t x_out_recover, y_out_recover;
        
        convert_point(x_in_est, y_in_est, &x_out_recover, &y_out_recover,0,mapPointPairs,mapSize);
        
        int32_t diffx=dx-x_out_recover;
        int32_t diffy=dy-y_out_recover;
        int diffM=abs(diffx)+abs(diffy);
        // printf("diff:%d,%d,  dx:%d x_out_recover:%d\n",diffx,diffy,dx,x_out_recover);
        if(diffM<=tolerance)
        {
            *ret_sx_est=x_in_est;
            *ret_sy_est=y_in_est;
            
            return i;
        }

        if(i<iterationLimit){}
        else{
            break;
        }
        //correction 
        
        int ctrlBase=55;
        int ctrlVae=48;
        
        if(diffM>ctrlVae)diffM=ctrlVae;
        correct_dx+=diffx*(diffM+ctrlBase)/(ctrlVae+ctrlBase);
        correct_dy+=diffy*(diffM+ctrlBase)/(ctrlVae+ctrlBase);
    }
        
    *ret_sx_est=x_in_est;
    *ret_sy_est=y_in_est;
    
    return -1;
}





float approx_euclidean_distance(float xdiff, float ydiff) {
    int32_t absx = xdiff;
    if(absx<0)absx=-absx;
    int32_t absdy = ydiff;
    if(absdy<0)absdy=-absdy;
    int32_t minabsd=(absx < absdy ? absx : absdy);
    
    return absx + absdy - (minabsd>> 1);
}


int convert_point_f(float x, float y, float* ret_x_out, float* ret_y_out,int dir,float *mapPointPairs,int mapSize) {
    if(mapSize==0)
    {
        *ret_x_out = x;
        *ret_y_out = y;
        return 0;
    }



    float s_sum_x = 0;
    float s_sum_y = 0;
    float total_s = 0;

    float weighted_sum_x = 0;
    float weighted_sum_y = 0;
    float total_weight = 0;

    float* dat0=mapPointPairs;
    float* dat2=mapPointPairs+2;

    float* datMapFrom=(dir==0)?dat0:dat2;
    float* datMapTo=(dir==0)?dat2:dat0;


    for (int i = 0; i < mapSize; ++i) {

        float dist = hypotf((x - datMapTo[0]),(y - datMapTo[1]));
        if (dist == 0) {
            // Direct match found
            *ret_x_out = datMapFrom[0];
            *ret_y_out = datMapFrom[1];
            return 0;
        }

        float weight = 1024 / dist;
        
        
        // printf("w%f  ", weight);
        if(weight<0.0001)weight=0.0001;
        weighted_sum_x += weight * (datMapFrom[0]-datMapTo[0]);
        weighted_sum_y += weight *(datMapFrom[1]-datMapTo[1]);
        total_weight += weight;
        datMapTo+=4;
        datMapFrom+=4;
    }
    // printf("\n");

    if (total_weight != 0) {
        *ret_x_out = x+weighted_sum_x / total_weight;
        *ret_y_out = y+weighted_sum_y / total_weight;
        return 0;
    }
    else{
        // Handle the case when total_weight is zero
        *ret_x_out = 0;
        *ret_y_out = 0;
    }
    return -1;
}


// int convert_point_back_f(float dx, float dy, float* ret_sx_est, float* ret_sy_est,int iterationLimit,int tolerance,float *mapPointPairs,float mapSize)
// {
//     float x_in_est, y_in_est;
    
//     float correct_dx=dx;
//     float correct_dy=dy;
    
//     for(int i=0;;i++)
//     {
        
//         convert_point_f(correct_dx, correct_dy, &x_in_est, &y_in_est,1,mapPointPairs,mapSize);
        
//         float x_out_recover, y_out_recover;
        
//         convert_point_f(x_in_est, y_in_est, &x_out_recover, &y_out_recover,0,mapPointPairs,mapSize);
        
//         float diffx=dx-x_out_recover;
//         float diffy=dy-y_out_recover;
//         float diffM=abs(diffx)+abs(diffy);
//         if(diffM<=tolerance)
//         {
//             *ret_sx_est=x_in_est;
//             *ret_sy_est=y_in_est;
            
//             return i;
//         }

//         if(i<iterationLimit){}
//         else{
//             break;
//         }
//         //correction 
        
//         float ctrlBase=0.055;
//         float ctrlVae=0.048;
        
//         if(diffM>ctrlVae)diffM=ctrlVae;
//         correct_dx+=diffx*(diffM+ctrlBase)/(ctrlVae+ctrlBase);
//         correct_dy+=diffy*(diffM+ctrlBase)/(ctrlVae+ctrlBase);
//     }
        
//     *ret_sx_est=x_in_est;
//     *ret_sy_est=y_in_est;
    
//     return -1;
// }





// int32_t calibMap[] = {
//     1000, 2000, 1500, 2000,
//     1500, 2500, 2200, 2500,
//     // Initialize other entries
// };

// int main() {

//     // Convert using Approximate Euclidean distance
//     int32_t sX=100,dX=2400;
//     int32_t sY=2000,dY=2500;
    
//     int segs=10;
//     for(int i=0;i<10;i++)
//     {
//         int32_t x = sX+(dX-sX)*i/(segs-1);
//         int32_t y = sY+(dY-sY)*i/(segs-1);
//         int32_t x_out, y_out;
//         convert_point(x, y, &x_out, &y_out,0,calibMap,2);
        
//         int32_t x_in_est, y_in_est;
        
//         convert_point_back(x_out, y_out, &x_in_est, &y_in_est,5,4,calibMap,2);
        
        
//         int32_t x_out_recover, y_out_recover;
        
//         convert_point(x_in_est, y_in_est, &x_out_recover, &y_out_recover,0,calibMap,2);
        
        
//         printf("Converted point using Approximate Euclidean distance: (%d, %d) -> (%d, %d) -> (%d, %d) -> (%d, %d)\n", x, y, x_out, y_out,x_in_est, y_in_est,x_out_recover, y_out_recover);
        
//         //correction 
//         x_out-=(x_out_recover-x_out);
//         y_out-=(y_out_recover-y_out);
        
//         convert_point(x_out, y_out, &x_in_est, &y_in_est,1,calibMap,2);
//         printf("(%d, %d) -> (%d, %d)\n",x_out, y_out,x_in_est, y_in_est);
        
        
//     }

//     return 0;
// }




// float calibMap[] = {
//     1000, 2000, 1010, 2000,
//     1500, 2500, 1500, 2500,
//     // Initialize other entries
// };

// int main() {

//     // Convert using Approximate Euclidean distance
//     float sX=1010,sY=2010;
//     float dX=1500,dY=2500;
    
//     int segs=5;
//     for(int i=0;i<segs;i++)
//     {
//         float x = sX+(dX-sX)*i/(segs-1);
//         float y = sY+(dY-sY)*i/(segs-1);
//         float x_out, y_out;
//         convert_point_f(x, y, &x_out, &y_out,0,calibMap,2);
        
//         float x_in_est, y_in_est;
        
//         convert_point_f(x_out, y_out, &x_in_est, &y_in_est,1,calibMap,2);
        
        
//         printf("Converted point : tar:(%f, %f) -> act:(%f, %f) -> rec:(%f, %f)   diff=(%f, %f) \n", x, y, x_out, y_out,x_in_est, y_in_est,  x_in_est-x, y_in_est-y);
        
//     }

//     return 0;
// }
