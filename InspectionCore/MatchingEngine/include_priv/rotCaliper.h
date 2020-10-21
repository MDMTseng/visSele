
#include "acvImage_ToolBox.hpp"
#include <vector>
#include <float.h>

struct MinAreaState
{
    int bottom;
    int left;
    float height;
    float width;
    float base_a;
    float base_b;
};

enum { CALIPERS_MAXHEIGHT=0, CALIPERS_MINAREARECT=1, CALIPERS_MAXDIST=2 };

/*F///////////////////////////////////////////////////////////////////////////////////////
 //    Name:    rotatingCalipers
 //    Purpose:
 //      Rotating calipers algorithm with some applications
 //
 //    Context:
 //    Parameters:
 //      points      - convex hull vertices ( any orientation )
 //      n           - number of vertices
 //      mode        - concrete application of algorithm
 //                    can be  CV_CALIPERS_MAXDIST   or
 //                            CV_CALIPERS_MINAREARECT
 //      left, bottom, right, top - indexes of extremal points
 //      out         - output info.
 //                    In case CV_CALIPERS_MAXDIST it points to float value -
 //                    maximal height of polygon.
 //                    In case CV_CALIPERS_MINAREARECT
 //                    ((CvPoint2D32f*)out)[0] - corner
 //                    ((CvPoint2D32f*)out)[1] - vector1
 //                    ((CvPoint2D32f*)out)[2] - vector2
 //
 //                      ^
 //                      |
 //              vector2 |
 //                      |
 //                      |____________\
 //                    corner         /
 //                               vector1
 //
 //    Returns:
 //    Notes:
 //F*/



/* we will use usual cartesian coordinates */
static void rotatingCalipers( const acv_XY* points, int n, int mode, float* out )
{
    float minarea = FLT_MAX;
    float max_dist = 0;
    char buffer[32] = {};
    int i, k;
    vector<float> abuf(n*3);
    float* inv_vect_length = abuf.data();
    acv_XY* vect = (acv_XY*)(inv_vect_length + n);
    int left = 0, bottom = 0, right = 0, top = 0;
    int seq[4] = { -1, -1, -1, -1 };

    /* rotating calipers sides will always have coordinates
     (a,b) (-b,a) (-a,-b) (b, -a)
     */
    /* this is a first base vector (a,b) initialized by (1,0) */
    float orientation = 0;
    float base_a;
    float base_b = 0;

    float left_x, right_x, top_y, bottom_y;
    acv_XY pt0 = points[0];

    left_x = right_x = pt0.X;
    top_y = bottom_y = pt0.Y;

    for( i = 0; i < n; i++ )
    {
        double dx, dy;

        if( pt0.X < left_x )
            left_x = pt0.X, left = i;

        if( pt0.X > right_x )
            right_x = pt0.X, right = i;

        if( pt0.Y > top_y )
            top_y = pt0.Y, top = i;

        if( pt0.Y < bottom_y )
            bottom_y = pt0.Y, bottom = i;

        acv_XY pt = points[(i+1) & (i+1 < n ? -1 : 0)];

        dx = pt.X - pt0.X;
        dy = pt.Y - pt0.Y;

        vect[i].X = (float)dx;
        vect[i].Y = (float)dy;
        inv_vect_length[i] = (float)(1./std::sqrt(dx*dx + dy*dy));

        pt0 = pt;
    }

    // find convex hull orientation
    {
        double ax = vect[n-1].X;
        double ay = vect[n-1].Y;

        for( i = 0; i < n; i++ )
        {
            double bx = vect[i].X;
            double by = vect[i].Y;

            double convexity = ax * by - ay * bx;

            if( convexity != 0 )
            {
                orientation = (convexity > 0) ? 1.f : (-1.f);
                break;
            }
            ax = bx;
            ay = by;
        }
        // CV_Assert( orientation != 0 );
    }
    base_a = orientation;

    /*****************************************************************************************/
    /*                         init calipers position                                        */
    seq[0] = bottom;
    seq[1] = right;
    seq[2] = top;
    seq[3] = left;
    /*****************************************************************************************/
    /*                         Main loop - evaluate angles and rotate calipers               */

    /* all of edges will be checked while rotating calipers by 90 degrees */
    for( k = 0; k < n; k++ )
    {
        /* sinus of minimal angle */
        /*float sinus;*/

        /* compute cosine of angle between calipers side and polygon edge */
        /* dp - dot product */
        float dp[4] = {
            +base_a * vect[seq[0]].X + base_b * vect[seq[0]].Y,
            -base_b * vect[seq[1]].X + base_a * vect[seq[1]].Y,
            -base_a * vect[seq[2]].X - base_b * vect[seq[2]].Y,
            +base_b * vect[seq[3]].X - base_a * vect[seq[3]].Y,
        };

        float maxcos = dp[0] * inv_vect_length[seq[0]];

        /* number of calipers edges, that has minimal angle with edge */
        int main_element = 0;

        /* choose minimal angle */
        for ( i = 1; i < 4; ++i )
        {
            float cosalpha = dp[i] * inv_vect_length[seq[i]];
            if (cosalpha > maxcos)
            {
                main_element = i;
                maxcos = cosalpha;
            }
        }

        /*rotate calipers*/
        {
            //get next base
            int pindex = seq[main_element];
            float lead_x = vect[pindex].X*inv_vect_length[pindex];
            float lead_y = vect[pindex].Y*inv_vect_length[pindex];
            switch( main_element )
            {
            case 0:
                base_a = lead_x;
                base_b = lead_y;
                break;
            case 1:
                base_a = lead_y;
                base_b = -lead_x;
                break;
            case 2:
                base_a = -lead_x;
                base_b = -lead_y;
                break;
            case 3:
                base_a = -lead_y;
                base_b = lead_x;
                break;
            default:
              {

              }
                // CV_Error(CV_StsError, "main_element should be 0, 1, 2 or 3");
            }
        }
        /* change base point of main edge */
        seq[main_element] += 1;
        seq[main_element] = (seq[main_element] == n) ? 0 : seq[main_element];

        switch (mode)
        {
        case CALIPERS_MAXHEIGHT:
            {
            /* now main element lies on edge aligned to calipers side */

            /* find opposite element i.e. transform  */
            /* 0->2, 1->3, 2->0, 3->1                */
            int opposite_el = main_element ^ 2;

            float dx = points[seq[opposite_el]].X - points[seq[main_element]].X;
            float dy = points[seq[opposite_el]].Y - points[seq[main_element]].Y;
            float dist;

            if( main_element & 1 )
                dist = (float)fabs(dx * base_a + dy * base_b);
            else
                dist = (float)fabs(dx * (-base_b) + dy * base_a);

            if( dist > max_dist )
                max_dist = dist;
            }
            break;
        case CALIPERS_MINAREARECT:
            /* find area of rectangle */
            {
            float height;
            float area;

            /* find vector left-right */
            float dx = points[seq[1]].X - points[seq[3]].X;
            float dy = points[seq[1]].Y - points[seq[3]].Y;

            /* dotproduct */
            float width = dx * base_a + dy * base_b;

            /* find vector left-right */
            dx = points[seq[2]].X - points[seq[0]].X;
            dy = points[seq[2]].Y - points[seq[0]].Y;

            /* dotproduct */
            height = -dx * base_b + dy * base_a;

            area = width * height;
            if( area <= minarea )
            {
                float *buf = (float *) buffer;

                minarea = area;
                /* leftist point */
                ((int *) buf)[0] = seq[3];
                buf[1] = base_a;
                buf[2] = width;
                buf[3] = base_b;
                buf[4] = height;
                /* bottom point */
                ((int *) buf)[5] = seq[0];
                buf[6] = area;
            }
            }
            break;
        }                       /*switch */
    }                           /* for */

    switch (mode)
    {
    case CALIPERS_MINAREARECT:
        {
        float *buf = (float *) buffer;

        float A1 = buf[1];
        float B1 = buf[3];

        float A2 = -buf[3];
        float B2 = buf[1];

        float C1 = A1 * points[((int *) buf)[0]].X + points[((int *) buf)[0]].Y * B1;
        float C2 = A2 * points[((int *) buf)[5]].X + points[((int *) buf)[5]].Y * B2;

        float idet = 1.f / (A1 * B2 - A2 * B1);

        float px = (C1 * B2 - C2 * B1) * idet;
        float py = (A1 * C2 - A2 * C1) * idet;

        out[0] = px;
        out[1] = py;

        out[2] = A1 * buf[2];
        out[3] = B1 * buf[2];

        out[4] = A2 * buf[4];
        out[5] = B2 * buf[4];
        }
        break;
    case CALIPERS_MAXHEIGHT:
        {
        out[0] = max_dist;
        }
        break;
    }
}



// cv::RotatedRect cv::minAreaRect( InputArray _points )
// {
//     CV_INSTRUMENT_REGION();

//     Mat hull;
//     Point2f out[3];
//     RotatedRect box;

//     convexHull(_points, hull, true, true);

//     if( hull.depth() != CV_32F )
//     {
//         Mat temp;
//         hull.convertTo(temp, CV_32F);
//         hull = temp;
//     }

//     int n = hull.checkVector(2);
//     const Point2f* hpoints = hull.ptr<Point2f>();

//     if( n > 2 )
//     {
//         rotatingCalipers( hpoints, n, CALIPERS_MINAREARECT, (float*)out );
//         box.center.X = out[0].X + (out[1].X + out[2].X)*0.5f;
//         box.center.Y = out[0].Y + (out[1].Y + out[2].Y)*0.5f;
//         box.size.width = (float)std::sqrt((double)out[1].X*out[1].X + (double)out[1].Y*out[1].Y);
//         box.size.height = (float)std::sqrt((double)out[2].X*out[2].X + (double)out[2].Y*out[2].Y);
//         box.angle = (float)atan2( (double)out[1].Y, (double)out[1].X );
//     }
//     else if( n == 2 )
//     {
//         box.center.X = (hpoints[0].X + hpoints[1].X)*0.5f;
//         box.center.Y = (hpoints[0].Y + hpoints[1].Y)*0.5f;
//         double dx = hpoints[1].X - hpoints[0].X;
//         double dy = hpoints[1].Y - hpoints[0].Y;
//         box.size.width = (float)std::sqrt(dx*dx + dy*dy);
//         box.size.height = 0;
//         box.angle = (float)atan2( dy, dx );
//     }
//     else
//     {
//         if( n == 1 )
//             box.center = hpoints[0];
//     }

//     box.angle = (float)(box.angle*180/CV_PI);
//     return box;
// }


// CV_IMPL CvBox2D
// cvMinAreaRect2( const CvArr* array, CvMemStorage* /*storage*/ )
// {
//     cv::AutoBuffer<double> abuf;
//     cv::Mat points = cv::cvarrToMat(array, false, false, 0, &abuf);

//     cv::RotatedRect rr = cv::minAreaRect(points);
//     return cvBox2D(rr);
// }

// void cv::boxPoints(cv::RotatedRect box, OutputArray _pts)
// {
//     CV_INSTRUMENT_REGION();

//     _pts.create(4, 2, CV_32F);
//     Mat pts = _pts.getMat();
//     box.points(pts.ptr<Point2f>());
// }

void ComputeConvexHull(const acv_XY *polygon,const int L,std::vector<int> &ret_chIdxs);