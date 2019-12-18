/******************************************************************************
 Copyright© HITwh HERO-RoboMaster2020 Group

 Author: Lu Zhanon on 2019.9.28
 *****************************************************************************/
#include "energy.h"
using namespace cv;
using std::vector;
using std::cout;
using std::to_string;
using std::endl;

#define RED
Energy::Energy() {
    isFindCenterR = false;
    isCalibrated = false;
    energy_centers.reserve(50);
    arrow_centers.reserve(50);
    angle_array.reserve(50);
    direction = DIR_DEFAULT;
    mode = MODE_DEFAULT;
    object_Points.reserve(5);
    CAMERA_MATRIX = (Mat_<float>(3, 3) <<1042.78615688289, 0, 314.531926344953,
            0, 1037.84608163949, 266.675833311165,
            0, 0, 1);
    DISTCOEFFS = (Mat_<float>(5, 1) << -0.136468, 0.1287139, 0, 0, 0);
}

Energy::~Energy() {
}

void Energy::setSrc(cv::Mat src) {
    src.copyTo(this->src);
}

cv::Mat Energy::getSrc() {
    return src;
}

void Energy::run(double &x, double &y, double &z) {
//    Timer clock;
    shoot = false;
    isLoseAllTargets = false;
    isFindEnergy = false;
    isFindArrow = false;
    preprocess();
    //找能量板
    if (findEnergy()) {
        circle(src, target_energy.center, 1, Scalar(0, 255, 0), 1);
        drawRectangle(src, target_energy, Scalar(0, 255, 0));
    }

    //找箭头
    if (findArrow()) {
        circle(src, target_arrow.center, 1, Scalar(0, 255, 0), 1);
        drawRectangle(src, target_arrow, Scalar(0, 255, 0));
    }


    //能量板,箭头任一没有找到,视为丢失目标
    //通常由于仰角过大造成,需要将角度复位至标定的原点
    if (!isFindEnergy || !isFindArrow ){
        isLoseAllTargets = true;
        cout<<"丢失目标!\n";

        return;
    }

    //箭头区域和能量板是否匹配
    if (!match()){
        cout<<"不匹配\n";
        return;
    }

    //数据匹配,更新队列
    updateQueue();
//    findCenterR();
    //方向未知则判断方向
    if (direction == DIR_DEFAULT){
        if (!solveDirection())
            return;
    }else{
        switch (direction){
            case DIR_CW:
            case DIR_CCW:
                mode = MODE_BIG;
                if (!isFindCenterR)
                    findCenterR();
                if (!isFindCenterR)
                    return;
                break;
            case DIR_STATIC:
                mode = MODE_SMALL;
                break;
        }
    }



    putText(src, "direction:" + to_string(direction), Point(10, 100), 1, 1.5, Scalar(255,255,255));
    putText(src, "mode:" + to_string(mode), Point(10, 200), 1, 1.5, Scalar(255,255,255));
    //模式以确定,再进行后续步骤
    if (mode == MODE_DEFAULT)
        return;

//    cout<<"方向:"<<direction<<'\n';
//    circle(src, ellipse_center, 2, Scalar(255, 255, 255), 2);
//    circle(src, ellipse_center, circle_radius, Scalar(255, 0, 0), 1);

    if ((mode == MODE_BIG) || (mode ==MODE_RANDOM))
        solveCurrentCenter();

//    cout<<ellipse_Xaxis<<  "椭圆   "<<ellipse_Yaxis<<'\n';
//    circle(src, current_center, 1, Scalar(255, 10, 0), 2);
//    line(src, target_energy.center, current_center, Scalar(255, 255, 255), 1);
//    RotatedRect fit_rect = RotatedRect(ellipse_center, Size(ellipse_Xaxis * 2, ellipse_Yaxis * 2), 0);
//    draw.drawRectangle(src, fit_rect, Scalar(0, 0, 255));
//    ellipse(src, fit_rect, Scalar(255,255,255));

    predicting();
    circle(src, predicted_energy.center, 2, Scalar(190, 10, 160), 2);
    drawRectangle(src, predicted_energy, Scalar(190, 10, 160));
//    line(src, predicted_energy.center, current_center, Scalar(255, 255, 255), 1);

    solveRealPostiton(predicted_energy);

    x = translation_matrix.at<double>(0,0) / 1000;
    y = (translation_matrix.at<double>(1,0) - 49.19) / 1000;
    z = (translation_matrix.at<double>(2,0) + 115.62) / 1000;
    shoot = true;
}

void Energy::preprocess() {
    vector<Mat> channels(3);
    split(src, channels);

#ifdef RED
    threshold(channels[2] - channels[0], bin, 90, 255, THRESH_BINARY);
#else
    threshold(channels[0] - channels[2], bin, 90, 255, THRESH_BINARY);
#endif
}

bool Energy::findEnergy() {
    isFindEnergy = false;
    double area;
    float ratio;
    RotatedRect temp_rect;
    Mat temp_bin;
    bin.copyTo(temp_bin);

    Mat element = getStructuringElement(MORPH_RECT, Size(3, 3));
    dilate(temp_bin, temp_bin, element, Point(-1, -1), 2);
//    imshow("填充前",temp_bin);
    floodFill(temp_bin, Point(1, 1), Scalar(255), 0, FLOODFILL_FIXED_RANGE);
//    imshow("填充后",temp_bin);
    element = getStructuringElement(MORPH_RECT, Size(5, 5));
    erode(temp_bin, temp_bin, element, Point(-1, -1), 3);
//    imshow("复原后", temp_bin);

    vector<vector<Point>> contours;
    vector<Point> target_contour;       //目标轮廓
    findContours(temp_bin, contours, RETR_CCOMP, CHAIN_APPROX_NONE);
//    cout<<contours.size()<<'\n';

    //筛选目标的能量板区域
    for (size_t i = 0; i < contours.size(); ++i){
//        drawContours(src, contours, i, Scalar(200, 100, 0), 1);
        area = contourArea(contours[i]);
//        cout<<"能量板面积:  "<<area<<'\n';
        //根据面积筛选
        if (area < MIN_ENERGY_AREA || area > MAX_ENERGY_AREA)
            continue;

        target_contour = contours[i];
        if (target_contour.size() <= 5)
            continue;
        temp_rect = fitEllipse(target_contour);
        temp_rect.size.height *= 0.75;
        temp_rect.size.width *= 0.75;
        //按宽高比筛选
        float width, height;
        width = max(temp_rect.size.width, temp_rect.size.height);
        height = min(temp_rect.size.width, temp_rect.size.height);
        ratio = width / height;
//        cout<<"能量板比例："<<ratio<<'\n';
        if (ratio < MIN_ENERGY_RATIO || ratio > MAX_ENERGY_RATIO)
            continue;

        //统一样式
        if (temp_rect.size.width > temp_rect.size.height){
            temp_rect.size = Size2f(height, width);
            temp_rect.angle += 90;
        }
        predicted_energy = target_energy = temp_rect;
        isFindEnergy = true;
        break;
    }
    return isFindEnergy;
}

bool Energy::findArrow() {
    isFindArrow = false;
    double area;
    float ratio;
    RotatedRect temp_rect;
    Mat temp_bin;
    bin.copyTo(temp_bin);

    Mat element = getStructuringElement(MORPH_RECT, Size(3, 3));
    dilate(temp_bin, temp_bin, element, Point(-1, -1), 2);
    imshow("箭头预处理后", temp_bin);

    vector<vector<Point>> contours;
    vector<Point> target_contour;       //目标轮廓
    findContours(temp_bin, contours, RETR_EXTERNAL, CHAIN_APPROX_NONE);
    cout<<"箭头轮廓总数:  "<<contours.size()<<'\n';
    //筛选目标的箭头区域
    for (size_t i = 0; i < contours.size(); ++i){
//        drawContours(src, contours, i, Scalar(200, 100, 0), 1);
        area = contourArea(contours[i]);
        cout<<"箭头面积: "<<area<<'\n';
        //根据面积筛选
        if (area < MIN_ARROW_AREA || area > MAX_ARROW_AREA)
            continue;

        target_contour = contours[i];
//        drawContours(src, contours, i, Scalar(200, 100, 0), 1);
        temp_rect = fitEllipse(target_contour);

        float width, height;
        width = max(temp_rect.size.width, temp_rect.size.height);
        height = min(temp_rect.size.width, temp_rect.size.height);
        ratio = width / height;

        cout<<"箭头比例："<<ratio<<'\n';
        if (ratio < MIN_ARROW_RATIO || ratio > MAX_ARROW_RATIO)
            continue;

        //统一样式
        if (temp_rect.size.width > temp_rect.size.height){
            temp_rect.size = Size2f(height, width);
            temp_rect.angle += 90;
        }
        target_arrow = temp_rect;
//        target_arrow.center = total;
        isFindArrow = true;
        break;
    }
    cout<<"-------------------------------------\n";
    return isFindArrow;

}

inline bool Energy::match() {
    vector<Point2f> intersection;
    if (rotatedRectangleIntersection(target_arrow, target_energy, intersection) > 0)
        return true;
    else
        return false;
}

//更新中心点队列数据
inline void Energy::updateQueue() {
    if (energy_centers.size() >= QUEUE_SIZE){
        energy_centers.erase(energy_centers.begin());
    }
    energy_centers.emplace_back(target_energy.center);

    if (arrow_centers.size() >= QUEUE_SIZE){
        arrow_centers.erase(arrow_centers.begin());
    }
    arrow_centers.emplace_back(target_arrow.center);
}

bool Energy::findCenterR() {
    if (energy_centers.size() < QUEUE_SIZE)
        return false;
    isFindCenterR = false;
//    计算拟合圆
//    isFindCenterR = calculate();
    for (int i =0; i<QUEUE_SIZE; ++i)
        circle(src, energy_centers[i], 1, Scalar(255, 10, 0), 2);
    isFindCenterR = calculate_ellipse();
    //删除异常样本点
    if (!isFindCenterR) {
        energy_centers.pop_back();
        arrow_centers.pop_back();
    }
    return isFindCenterR;
}


bool Energy::calculate_ellipse() {
//    int N = energy_centers.size();
//    float Xi , Yi;
//    float sigmaX = 0 , sigmaY = 0;
//    float sigmaX2 = 0, sigmaXY = 0, sigmaY2 = 0;
//    float sigmaX3 = 0, sigmaX2Y = 0, sigmaXY2 = 0, sigmaY3 = 0;
//    float sigmaX4 = 0, sigmaX3Y = 0, sigmaX2Y2 = 0, sigmaXY3 = 0, sigmaY4 = 0;
//    int i;
//    for (i = 0; i < N; ++i){
//        Xi = energy_centers[i].x;
//        Yi = energy_centers[i].y;
//        sigmaX += Xi;
//        sigmaY += Yi;
//        sigmaX2 += Xi * Xi;
//        sigmaXY += Xi * Yi;
//        sigmaY2 += Yi * Yi;
//        sigmaX3 += Xi * Xi * Xi;
//        sigmaX2Y += Xi * Xi * Yi;
//        sigmaXY2 += Xi * Yi * Yi;
//        sigmaY3 += Yi * Yi * Yi;
//        sigmaX4 += Xi * Xi * Xi * Xi;
//        sigmaX3Y += Xi * Xi * Xi * Yi;
//        sigmaX2Y2 += Xi * Xi * Yi * Yi;
//        sigmaXY3 += Xi * Yi * Yi * Yi;
//        sigmaY4 += Yi * Yi * Yi * Yi;
//    }
//    Mat left_matrix = (Mat_<float>(4, 4) << sigmaY4, sigmaXY2, sigmaY3, sigmaY2,
//                                            sigmaXY2, sigmaX2, sigmaXY, sigmaX,
//                                            sigmaY3, sigmaXY, sigmaY2, sigmaY,
//                                            sigmaY2, sigmaX, sigmaY, 1);
//    cout << "left_matrix[4,4]=" << left_matrix << endl;
//    Mat right_matrix = (Mat_<float>(4, 1) << -sigmaX2Y2, -sigmaX3, -sigmaX2Y, -sigmaX2);
//    cout << "right_matrix[4,1]=" << right_matrix << endl;
//    Mat ellipse_solution(4, 1, CV_32F);
//    solve(left_matrix, right_matrix, ellipse_solution, DECOMP_LU);
//    cout << "solution[4,1]=" << ellipse_solution << endl;
//    double A, B, C, D;
//    A = ellipse_solution.at<float>(0, 0);
//    B = ellipse_solution.at<float>(1, 0);
//    C = ellipse_solution.at<float>(2, 0);
//    D = ellipse_solution.at<float>(3, 0);
//
//    ellipse_Xaxis = sqrt(0.25*B*B + C*C/4.0/A - D);
//    ellipse_Yaxis = sqrt((0.25*B*B + C*C/4.0/A - D) / A);
//    ellipse_center.x = -B / 2.0;
//    ellipse_center.y = -C / 2.0 / A;
//    cout<<"x="<<ellipse_Xaxis<<"   y="<<ellipse_Yaxis<<endl;
//    cout<<"center.x="<<ellipse_center.x<<"   y="<<ellipse_center.y<<endl;
//    if (isnan(ellipse_Xaxis))
//        return false;
//
//    if (isnan(ellipse_Yaxis))
//        return false;
//
//    if (ellipse_center.x < 0 || ellipse_center.x > 640)
//        return false;
//
//    if (ellipse_center.y < 0 || ellipse_center.y > 480)
//        return false;
//
//    return true;
//    cout<<"rad ="<<circle_radius<<'\n';
    int i;
    RotatedRect fit_rect = fitEllipse(energy_centers);
    ellipse(src, fit_rect, Scalar(0, 0, 255), 6, 4);
    ellipse_center = fit_rect.center;
    Point2f vertices[4];
    vector<Point2f> temp_points;
    Point2f temp_point;
    fit_rect.points(vertices);
    for (i = 0; i < 4; ++i)
        temp_points.emplace_back(vertices[i]);
    if (temp_points[0].x > temp_points[1].x){
        temp_point = temp_points[0];
        temp_points.erase(temp_points.begin());
        temp_points.emplace_back(temp_point);
    }

    ellipse_Yaxis = getDistance(temp_points[0], temp_points[1]) / 2;
    ellipse_Xaxis = getDistance(temp_points[1], temp_points[2]) / 2;
//    if (ellipse_Xaxis < 100)
//        return false;
    if (isnan(ellipse_Xaxis) || isnan(ellipse_Yaxis))
        return false;
    else
        return true;
}

//最小二乘法拟合圆，推导过程详见
//https://blog.csdn.net/Jacky_Ponder/article/details/70314919
//bool Energy::calculate() {
//    int N = energy_centers.size();
//    double Xi , Yi;
//    double sigmaX = 0 , sigmaY = 0;
//    double sigmaX2 = 0, sigmaXY = 0, sigmaY2 = 0;
//    double sigmaX3 = 0, sigmaX2Y = 0, sigmaXY2 = 0, sigmaY3 = 0;
//
//    size_t i;
//    for (i = 0; i < N; ++i){
//        Xi = energy_centers[i].x;
//        Yi = energy_centers[i].y;
//        sigmaX += Xi;
//        sigmaY += Yi;
//        sigmaX2 += Xi * Xi;
//        sigmaXY += Xi * Yi;
//        sigmaY2 += Yi * Yi;
//        sigmaX3 += Xi * Xi * Xi;
//        sigmaX2Y += Xi * Xi * Yi;
//        sigmaXY2 += Xi * Yi * Yi;
//        sigmaY3 += Yi * Yi * Yi;
//    }
//
//    double C, D, E, G, H, a, b, c;
//    C = N * sigmaX2 - sigmaX * sigmaX;
//    D = N * sigmaXY - sigmaX * sigmaY;
//    E = N * (sigmaX3 + sigmaXY2) - sigmaX * (sigmaX2 + sigmaY2);
//    G = N * sigmaY2 - sigmaY * sigmaY;
//    H = N * (sigmaY3 + sigmaX2Y) - sigmaY * (sigmaX2 + sigmaY2);
//    a = (H*D - E*G) / (C*G - D*D);
//    b = (H*C - E*D) / (D*D - C*G);
//    if (isnan(a) || isnan(b))
//        return false;
//    c = -(sigmaX2 + sigmaY2 + a*sigmaX + b*sigmaY) / N;
//
//    ellipse_center.x = a / (-2);
//    ellipse_center.y = b / (-2);
//    circle_radius = sqrt(a*a + b*b -4*c) / 2;
////    cout<<"rad ="<<circle_radius<<'\n';
//    if (isnan(circle_radius))
//        return false;
//
//    if (circle_radius < MIN_RADIUS || circle_radius > MAX_RADIUS)
//        return false;
//    else
//        return true;
//}

inline void Energy::solveCurrentCenter() {
    Point2f unitVector;             //单位向量
    float theta = toPolarCoordinates(target_energy.center, target_arrow.center) * PI / 180;
    float radius = sqrt(pow(ellipse_Xaxis * cos(theta), 2.0) + pow(ellipse_Yaxis * sin(theta), 2.0));
    unitVector = target_arrow.center - target_energy.center;
    unitVector /= getDistance(target_arrow.center, target_energy.center);
    current_center = unitVector *  radius + target_energy.center;
    circle(src, current_center, 2, Scalar(255, 255, 255), 2);
//    cout<<"当前圆心:"<<current_center<<endl;
}

bool Energy::solveDirection() {
    if (energy_centers.size() < QUEUE_SIZE)
        return false;
    angle_array.clear();
    float angle;

    if (isFindCenterR) {
        for (int i = 0; i < energy_centers.size(); ++i) {
            angle = toPolarCoordinates(energy_centers[i], ellipse_center);
            angle_array.emplace_back(angle);
        }
    }else{
        if (arrow_centers.size() < QUEUE_SIZE)
            return false;
        for (int i = 0; i < energy_centers.size(); ++i) {
            angle = toPolarCoordinates(energy_centers[i], arrow_centers[i]);
            angle_array.emplace_back(angle);
        }
    }

    judgeDirection();
    if (direction == DIR_DEFAULT)
        return false;
    else
        return true;
}

//返回极坐标下的角度,角度制
inline float Energy::toPolarCoordinates(const cv::Point &temp_point, const Point &origin_point) {
    float angle, rad;
    //x轴从左向右，y轴从下到上
    Point new_point = Point(temp_point.x - origin_point.x, origin_point.y - temp_point.y);
    rad = getDistance(temp_point, origin_point);
    if (new_point.y >= 0)
        angle = acos(new_point.x / rad) * 180 / PI;
    else
        angle = 360 - acos(new_point.x / rad) * 180 / PI;
//    cout<<new_point.x / rad<<'\n';
    return angle;
}

//判断风车转向
//风车转速 10rad/min = 60°/s     摄像头FPS 30帧/s
// 60 / 30 = 2°/帧
void Energy::judgeDirection() {
    size_t i;
    int CW_cnt = 0;
    int CCW_cnt = 0;
    int STATIC_cnt = 0;
    double total = 0;
    int num = 0;
    double delta;
    for (i = 1; i < angle_array.size(); ++i){
        delta = angle_array[i] - angle_array[i-1];
        //对于角度在x轴正半轴附近的跳变进行补偿
        if (delta < -180)
            delta += 360;
        else if (delta > 180)
            delta -= 360;
        //发生目标切换
        if  (abs(delta) > 30)
            continue;
        total += delta;
        num++;
    }
    double average = total / num;
    if (abs(average) < 0.3)
        direction =DIR_STATIC;
    else if (average > 0)
        direction = DIR_CCW;
    else if (average < 0)
        direction = DIR_CW;
//
//    cout<<"旋转方向:"<<direction<<'\n';
//    for (i = 0; i < angle_array.size(); ++i){
//        cout<<i<<"号极坐标角度:"<<angle_array[i]<<'\n';
//    }
}

void Energy::predicting() {
    if (mode == MODE_SMALL){
        predicted_energy = target_energy;
        return;
    }
    Point2f predicted_point;
    float current_angle = toPolarCoordinates(target_energy.center, current_center);
    float next_angle = 0;
    float dx, dy;
    switch (direction){
        case DIR_DEFAULT:
            return;
            break;
        case DIR_CW:
            next_angle = current_angle - ANGLE_OFFSET;
            break;
        case DIR_CCW:
            next_angle = current_angle + ANGLE_OFFSET;
            break;
        case DIR_STATIC:
            next_angle = current_angle;
    }
    if (next_angle > 360) next_angle -= 360;
    if (next_angle < 0) next_angle += 360;
    dx = cos(next_angle * PI / 180) * ellipse_Xaxis;
    dy = sin(next_angle * PI / 180) * ellipse_Yaxis;

    cout<<"next_angle = "<<next_angle<<'\n';
    cout<<"dx,dy:"<<dx<<"   "<<dy<<'\n';
    //从极坐标系转换到图像坐标系
    predicted_point.x = current_center.x + dx;
    predicted_point.y = current_center.y - dy;
    float rect_angle = target_energy.angle;
    if (direction == DIR_CW)
        rect_angle += ANGLE_OFFSET;
    else if (direction == DIR_CCW)
        rect_angle -= ANGLE_OFFSET;
    predicted_energy = RotatedRect(predicted_point, predicted_energy.size, rect_angle);
}


void Energy::solveRealPostiton(const cv::RotatedRect aim) {
    int i,j;
    RotatedRect rect = aim;
    object_Points.clear();
    image_Points.clear();

    Point2f vertices[4];
    rect.points(vertices);
    float dis;
    Point2f temp;
    //按顺时针重新排列四个角点
    for (i = 0; i < 4; ++i)
        image_Points.emplace_back(vertices[i]);
    dis = sqrt(pow(image_Points[0].x - image_Points[1].x, 2) + pow(image_Points[0].y - image_Points[1].y, 2));
    if (abs(dis - rect.size.height) > EXP){
        temp = image_Points[0];
        image_Points.erase(image_Points.begin());
        image_Points.emplace_back(temp);
    }
    if (image_Points[0].x > image_Points[1].x){
        temp = image_Points[0];
        image_Points.erase(image_Points.begin());
        image_Points.emplace_back(temp);
        temp = image_Points[0];
        image_Points.erase(image_Points.begin());
        image_Points.emplace_back(temp);
    }
    String text;
    for (i = 0; i < 4; ++i) {
        text = to_string(i);
        putText(src, text, image_Points[i], FONT_HERSHEY_SIMPLEX, 0.35, Scalar(255, 255, 255));
    }

    object_Points.emplace_back(Point3f(-ENERGY_HALF_LENGTH, -ENERGY_HALF_WIDTH, 0));
    object_Points.emplace_back(Point3f(ENERGY_HALF_LENGTH, -ENERGY_HALF_WIDTH, 0));
    object_Points.emplace_back(Point3f(ENERGY_HALF_LENGTH, ENERGY_HALF_WIDTH, 0));
    object_Points.emplace_back(Point3f(-ENERGY_HALF_LENGTH, ENERGY_HALF_WIDTH, 0));

    solvePnP(object_Points, image_Points, CAMERA_MATRIX, DISTCOEFFS, rotated_vector, translation_matrix, false, SOLVEPNP_EPNP);
//    cout << rotated_vector << "\n" << translation_matrix << "\n";
}