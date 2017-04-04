//
// Created by user on 29.03.17.
//

#include <string>
#include <iostream>
#include <chrono>
#include <vector>
#include <cmath>

#include <ros/ros.h>
#include <nav_msgs/GetMap.h>
#include <geometry_msgs/Pose.h>
#include <nav_msgs/OccupancyGrid.h>
#include <tf/transform_listener.h>

#include <Eigen/Dense>

#include "SimpleDraw/SimpleDraw.h"

using namespace std;
using namespace std::chrono;
using namespace Eigen;

struct SearchAreaCoords
{
    /*
         (target)
     E ---- B ---- F
     |             |
     |             |
     |             |
     |             |
     |             |
     C ---- A ---- D
         (robot)
     */

    Vector2f a, b, c, d, e, f;
};

const char response_topic[] = "obstacle_detector/obstacle";

SimpleDraw g(500, 500);
int cellToPixel = 4;

void process(ros::ServiceClient rtabmap_client);

SearchAreaCoords GetSearchArea(float yaw, Vector2f robotPos, int length, int width);

void SearchInArea(SearchAreaCoords area, nav_msgs::OccupancyGrid map);
void SearchInTriangle(Vector2f t0, Vector2f t1, Vector2f t2, nav_msgs::OccupancyGrid map, Color c);

void DrawLine(Vector2f a, Vector2f b, Color c);
void DrawMap(nav_msgs::OccupancyGrid map);
void DrawCell(int x, int y, Color color);

Vector2f v2(int x, int y)
{
    Vector2f v;
    v << x, y;
    return v;
}

int main(int argc, char** argv)
{

    //Инициализация ноды ROS
    /*ros::init(argc, argv, "ObstacleDetector");
    ros::NodeHandle n;
    ros::Rate rate(0.5);

    //ros::Publisher responsePublisher = n.advertise<>(response_topic, 1, true);
    ros::ServiceClient rtabmap_client = n.serviceClient<nav_msgs::GetMap>("/rtabmap/get_proj_map");

    ROS_INFO("%s", "AR-600/obstacle_detector started");*/

    g.Clear(Color::White());
    //g.Update();

    nav_msgs::OccupancyGrid map;
    Vector2f pos = v2(40, 40);

    float yaw = 0;

    while(true){
        g.Clear(Color::White());
        auto area = GetSearchArea(yaw, pos, 40, 20);
        SearchInArea(area, map);
        SearchInArea(area, map);
        yaw+=0.1;
        g.Delay(100);
        g.Tick();
    }

    /*while (ros::ok())
    {
        process(rtabmap_client);

        if(g.Tick())
            return 0;

        rate.sleep();
    }*/

    return 0;
}

void process(ros::ServiceClient rtabmap_client)
{
    //Получение карты
    nav_msgs::GetMap proj_map_srv;
    bool success = rtabmap_client.call(proj_map_srv);
    if (!success)
    {
        ROS_ERROR("Failed to get grid_map from rtabmap");
        return;
    }

    //Получение позиции
    tf::TransformListener listener;
    tf::StampedTransform transform;
    try
    {
        listener.waitForTransform("/odom", "/camera_link", ros::Time(0), ros::Duration(3));
        listener.lookupTransform("/odom", "/camera_link", ros::Time(0), transform);
    }
    catch (tf::TransformException ex)
    {
        ROS_ERROR("%s",ex.what());
        return;
    }

    nav_msgs::OccupancyGrid map = proj_map_srv.response.map;
    float resolution = map.info.resolution;
    geometry_msgs::Pose origin = map.info.origin;

    DrawMap(map);

    int width = 1 / resolution;
    int length = 2 / resolution;


    //Получение угла поворота и координат
    tf::Quaternion q = transform.getRotation();
    q.normalize();

    // Get angle
    tf::Matrix3x3 m(q);
    tfScalar yaw, pitch, roll;
    m.getRPY(roll, pitch, yaw);

    Vector2f a;
    a << (transform.getOrigin().x() - origin.position.x) / resolution,
         (transform.getOrigin().y() - origin.position.y) / resolution;

    auto area = GetSearchArea(yaw, a, length, width);

    SearchInArea(area, map);
}

//Возвращет координаты вершин области, в которой надо искать препятствия
SearchAreaCoords GetSearchArea(float yaw, Vector2f a, int length, int width)
{

    //Положение цели (B)
    Vector2f b;
    b << a(0) + length*cos(yaw),
            a(1) + length*sin(yaw);

    Vector2f ab = b - a;
    float v_l = sqrt(ab(0)*ab(0) + ab(1)*ab(1));

    Vector2f c;
    c << a(0) + ab(1) / v_l * width/2,
            a(1) - ab(0) / v_l * width/2;

    Vector2f d;
    d << a(0) - ab(1) / v_l * width/2,
            a(1) + ab(0) / v_l * width/2;

    Vector2f e = c + ab;

    Vector2f f = d + ab;

    return SearchAreaCoords {a, b, c, d, e, f};
}


void SearchInArea(SearchAreaCoords area, nav_msgs::OccupancyGrid map)
{
    //SearchInTriangle(area.c, area.d, area.e, map, Color(255,0,0));
    //SearchInTriangle(area.d, area.e, area.f, map, Color(0,255,0));

    Color r(255,0,0);
    Color g(0,255,0);

    SearchInTriangle(area.c, area.d, area.e, map, g);

    DrawLine(area.c, area.d, r);
    DrawLine(area.d, area.e, r);
    DrawLine(area.e, area.c, r);

    //DrawLine(area.e, area.f, g);
    //D/rawLine(area.f, area.d, g);
    //DrawLine(area.d, area.e, g);
}

//Ищет препятствие в треугольнике (одновременно раскрашивает)
//https://habrahabr.ru/post/248159/
void SearchInTriangle(Vector2f t0, Vector2f t1, Vector2f t2, nav_msgs::OccupancyGrid map, Color c )
{
    //Color c(255,0,0);;

    if (t0(1)==t1(1) && t0(1)==t2(1)) return; // i dont care about degenerate triangles

    // sort the vertices, t0, t1, t2 lower-to-upper (bubblesort yay!)
    if (t0(1)>t1(1)) std::swap(t0, t1);
    if (t0(1)>t2(1)) std::swap(t0, t2);
    if (t1(1)>t2(1)) std::swap(t1, t2);

    int total_height = t2(1)-t0(1);

    for (int i=0; i<total_height; i++)
    {
        bool second_half = i > t1(1) - t0(1) || t1(1) == t0(1);
        int segment_height = second_half ? t2(1)-t1(1) : t1(1) - t0(1);

        float alpha = (float)i/total_height;
        float beta  = (float)(i-(second_half ? t1(1)-t0(1) : 0))/segment_height; // be careful: with above conditions no division by zero here

        Vector2f A = t0 + (t2-t0)*alpha;
        Vector2f B = second_half ? t1 + (t2-t1)*beta : t0 + (t1-t0)*beta;

        if (A(0)>B(0)) std::swap(A, B);

        for (int j=A(0); j<=B(0); j++)
        {
            //image.set(j, t0(1)+i, color); // attention, due to int casts t0.y+i != A.y
            DrawCell(j, t0(1)+i, c);
        }
    }
}

//Линия алгоритмом Брезенхэма
//https://habrahabr.ru/post/248153/
void DrawLine(Vector2f a, Vector2f b, Color c)
{
    int x0 = a(0), y0 = a(1);
    int x1 = b(0), y1 = b(1);

    bool steep = false;
    if (std::abs(x0-x1)<std::abs(y0-y1))
    {
        std::swap(x0, y0);
        std::swap(x1, y1);
        steep = true;
    }
    if (x0>x1)
    {
        std::swap(x0, x1);
        std::swap(y0, y1);
    }

    int dx = x1-x0;
    int dy = y1-y0;
    int derror2 = std::abs(dy)*2;
    int error2 = 0;
    int y = y0;

    for (int x=x0; x<=x1; x++)
    {
        if (steep)
        {
            //image.set(y, x, color);
            DrawCell(y, x, c);

        }
        else
        {
            //image.set(x, y, color);
            DrawCell(x, y, c);
        }

        error2 += derror2;

        if (error2 > dx)
        {
            y += (y1>y0?1:-1);
            error2 -= dx*2;
        }
    }
}


//Рисует карту
void DrawMap(nav_msgs::OccupancyGrid map)
{
    for(int y = 0; y<map.info.height; y++)
    {
        for(int x = 0; x<map.info.width; x++)
        {
            int8_t cell = map.data[y * map.info.width + x];

            if(cell==0)
                DrawCell(x,y,Color::White());
            else if(cell==-1)
                DrawCell(x,y,Color(128,128,128));
            else
                DrawCell(x,y,Color::Black());

        }
    }
}

//Рисует "пиксель" на карте
void DrawCell(int x, int y, Color color)
{
    int size = cellToPixel;
    g.FillRect(color, x*size, y*size, size, size);
}