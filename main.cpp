// Calculate and display the Mandelbrot set
// ECE4122 final project, Spring 2017
// Kaiming Guan

#include <iostream>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
/*
#include <GLUT/GLUT.h>
#include <OpenGL/glext.h>
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
*/

 #include <GL/glut.h>
 #include <GL/glext.h>
 #include <GL/gl.h>
 #include <GL/glu.h>


#include "complex.h"

using namespace std;

// Min and max complex plane values,复平面上初始矩形的界限
Complex minC(-2.0, -1.2);
Complex maxC( 1.0, 1.8);
double lowerReal = -2.0;
double upperReal = 1.0;
double lowerImag = -1.2;
double upperImag = 1.8;

//鼠标在屏幕窗口上点击的像素坐标
int pressX, pressY;

//屏幕窗口的大小
int screenSize = 512;
//计算线程的总数
int totalThreads = 16;
int maxIt = 2000;     // Max iterations for the set computations,最大迭代次数（上限）
//屏幕窗口上所有像素（512*512个）各自的迭代次数
int numIterations[512][512];
//屏幕窗口上所有像素各自的R G B 三原色值，根据各自的终止迭代次数值来设定为相应的值
float colors[512][512][3];
//2000个迭代次数值各自对应的R G B 三原色值
float colorMatch[2000][3];

void callThreads();

pthread_mutex_t updateMutex;
pthread_mutex_t computationDoneMutex;
//cout 文本输出互斥锁
pthread_mutex_t coutMutex;
pthread_cond_t allDoneCondition;
int activeThreads;

// This function returns the starting or ending index corresponding to each CPU
int DivideEvenly(int numTask, int numCPU, int rank, bool ending) {
    // 1st: Complete number of tasks to be divided
    // 2nd: Exact number of CPUs
    // 3rd: The rank of a CPU in range [0, numCPU - 1]
    // 4th: false for starting index, true for ending index
    double avgTask = (double)numTask / numCPU;
    double previousTask = avgTask * rank; // Number of preceding CPUs equals rank
    double pendingTask = avgTask * (rank + 1);
    int firstRank = (int)previousTask;
    int lastRank = (int)pendingTask - 1;
    if(!ending) return firstRank;
    // Return the index of first corresponding element [0, numTask - 1]
    else return lastRank;
    // Return the index of last corresponding element [0, numTask - 1]
}

void display(void)
{
    // Your OpenGL display code here
    //接下来要绘制每一个像素点
    glBegin(GL_POINTS);
    //指定像素点的直径
    glPointSize(4);
    for(int j = 0; j < screenSize; j++) {
        for(int k = 0; k < screenSize; k++) {
            //下函数设置(k,j)像素点处的R G B三原色
            glColor3f(colors[j][k][0], colors[j][k][1], colors[j][k][2]);
            //设置所要显示的像素点坐标
            glVertex2i(k, j);

        }
    }
    glEnd();
    cout << "Press any key to go back to largest view." << endl;
    cout << "Drag the mouse to zoom in" << endl;
    glutSwapBuffers();
}

void init()
{
    // Your OpenGL initialization code here
    //设置好清除颜色，glClear利用glClearColor函数设置好的当前清除颜色设置窗口颜色
    glClearColor(0,0,0, 0.0);
    //声明接下来需要进行投影操作(GL_PROJECTION)
    glMatrixMode (GL_PROJECTION);
    //正射投影函数，设置投影与窗口大小的比例是1:1
    gluOrtho2D (0.0, screenSize, 0.0, screenSize);
    //可以理解为让图形质量更高
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    //模型缩放函数，参数x,y,z分别为模型在x,y,z轴方向的缩放比
    glScalef(1, 1, 1);
}



void reshape(int w, int h)//视见区域的宽度和高度分别为w和h
{
    //调用glViewPort函数来决定视见区域，告诉OpenGL应把渲染之后的图形绘制在窗体的哪个部位
    glViewport(0, 0, w, h);
}



void mouse(int button, int state, int x, int y)
{
    // Your mouse click processing here
    //参数button表明哪个鼠标键被按下或松开
    // 参数state == 0 means pressed(按下), state != 0 means released(松开)
    //参数（x,y）提供了鼠标按下或松开时当前的窗口坐标（以左上角为原点）
    // Note that the x and y coordinates passed in are in
    // PIXELS, with y = 0 at the top.
    if(state == 0) {
        //cout << "Pressed at x: " << x << "  y: " << y << endl;
        pressX = x;
        //因为计算屏幕窗口上的像素点到复平面内的映射是以左下角为基准点，所以y坐标需要经过下面的转换
        pressY = screenSize - y;
    }
    if(state != 0) {
        int releaseX = x, releaseY = screenSize - y;
        //鼠标从点击到释放是否向右移动
        bool toRight = (releaseX >= pressX);
        //鼠标从点击到释放是否向上移动
        bool toTop = (releaseY >= pressY);
        //delta是屏幕正方形放大区域的边长，因为屏幕窗口为正方形
        //但是鼠标从点击到释放所选择的矩形不一定是一个正方形，所以有了下面的取最小值
        int delta = min(abs(releaseX - pressX), abs(releaseY - pressY));
        //如果鼠标的点击和释放是在同一点，则设置屏幕正方形放大区域的边长为10
        if(delta == 0)
            delta = 10;
        //(minX,minY)是屏幕正方形放大区域左下角的坐标，
        //(maxX,maxY)是屏幕正方形放大区域右上角的坐标（均是相对于屏幕窗口的左下角）
        int minX, maxX, minY, maxY;
        if(toRight) {
            minX = pressX;
            maxX = pressX + delta;
        } else {
            maxX = pressX;
            minX = pressX - delta;
        }

        if(toTop) {
            minY = pressY;
            maxY = pressY + delta;
        } else {
            maxY = pressY;
            minY = pressY - delta;
        }


        //在选定了屏幕正方形放大区域之后，要设置相对应的复平面放大区域
        //(lowerReal_new,lowerImag_new)是复平面放大区域左下角的坐标，
        //(upperReal_new,upperImag_new)是复平面放大区域右上角的坐标
        double lowerReal_new = lowerReal + (upperReal - lowerReal) * minX / (screenSize - 1);
        double upperReal_new = lowerReal + (upperReal - lowerReal) * maxX / (screenSize - 1);
        double lowerImag_new = lowerImag + (upperImag - lowerImag) * minY / (screenSize - 1);
        double upperImag_new = lowerImag + (upperImag - lowerImag) * maxY / (screenSize - 1);
        lowerReal = lowerReal_new;
        upperReal = upperReal_new;
        lowerImag = lowerImag_new;
        upperImag = upperImag_new;
        //调用callThreads()重新进行对于屏幕正方形放大区域上Mandelbrot集合的计算
        callThreads();
        //显示新的屏幕正方形放大区域
        display();
    }
}

void motion(int x, int y)
{
    // Your mouse motion here, x and y coordinates are as above

}

void keyboard(unsigned char c, int x, int y)
{ // Your keyboard processing here
    //将复平面上显示矩形的界限恢复为初始值
    lowerReal = minC.real;
    upperReal = maxC.real;
    lowerImag = minC.imag;
    upperImag = maxC.imag;
    callThreads();
    display();
}

void* ThreadCompute(void* i) {
    int myRank = (int)(long)i;
    //求取第myRank号线程对于屏幕窗口的计算区域，竖直方向从0--screenSize,水平方向从startCol--endCol
    int startCol = DivideEvenly(screenSize, totalThreads, myRank, false);
    int endCol = DivideEvenly(screenSize, totalThreads, myRank, true);
    pthread_mutex_lock(&coutMutex);
    pthread_mutex_unlock(&coutMutex);
    for(int j = 0; j < screenSize; j++) {
        for(int k = startCol; k <= endCol; k++) {
            //将屏幕窗口上的一个像素(k,j)映射为复(数)平面上矩形内的一点(real,imag).
            double real = lowerReal + (upperReal - lowerReal) * k / (screenSize - 1);
            double imag = lowerImag + (upperImag - lowerImag) * j / (screenSize - 1);
            const Complex c(real, imag);
            Complex z(0, 0);
            //一个点属于Mandelbrot集合当且仅当它对应的序列（由二项式定义f(z)=z^2+c）中的任何元素的模都不大于2
            //由于序列的的元素有无穷多个，只能取有限的迭代次数来模拟了，本程序取迭代次数为2000
            int count = 0;
            while(true) {
                z = z * z + c;
                count++;
                //终止条件'count==2000'表示复平面上的点(real,imag)在模拟条件下属于Mandelbrot集合
                //终止条件'z.Mag().real>=2'表示复平面上的点(real,imag)不属于Mandelbrot集合
                if(count == 2000 || z.Mag().real >= 2) {
                    //设置屏幕窗口像素(k,j)处的迭代次数为count
                    numIterations[j][k] = count;
                    //设置屏幕窗口像素(k,j)处的三原色值为迭代次数值count对应的三原色值
                    colors[j][k][0] = colorMatch[count - 1][0];
                    colors[j][k][1] = colorMatch[count - 1][1];
                    colors[j][k][2] = colorMatch[count - 1][2];
                    break;
                }
            }
        }
    }
    pthread_mutex_lock(&computationDoneMutex);
    //第i号线程的计算任务完成，活跃线程数减1
    activeThreads--;
    pthread_mutex_unlock(&computationDoneMutex);
    //如果所有的线程(16个)都完成了各自的计算任务，改变条件变量allDoneCondition并发送信号，让callThreads()函数在相应位置处继续执行
    if(activeThreads == 0) {
        pthread_cond_signal(&allDoneCondition);
    }

    return NULL;
}
void callThreads() {
    //动态初始化3个互斥锁
    pthread_mutex_init(&updateMutex, 0);
    pthread_mutex_init(&computationDoneMutex, 0);
    pthread_mutex_init(&coutMutex, 0);
    //动态初始化1个条件变量
    pthread_cond_init(&allDoneCondition, 0);

    //上锁
    pthread_mutex_lock(&computationDoneMutex);
    pthread_mutex_lock(&coutMutex);
    activeThreads = totalThreads;
    for(int i = 0; i < totalThreads; i++) {
        //线程标识符
        pthread_t t;
        //创建线程函数
        pthread_create(&t, 0, ThreadCompute, (void*)i);
    }
    cout << "Multi-thread Computing..." << endl;
    //解锁coutMutex
    pthread_mutex_unlock(&coutMutex);
    //解锁computationDone，并等待allDoneCondition条件变量改变
    pthread_cond_wait(&allDoneCondition, &computationDoneMutex);
    cout << "All threads finished computing!" << endl << endl;
}
int main(int argc, char** argv)
{
    //用当前时间time(0)设置随机数种子
    srand(time(0));
    // Computation Process
    //对GLUT进行初始化
    glutInit(&argc, argv);
    //设置显示方式，参数分别表示双缓冲和RGB颜色
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(screenSize, screenSize);
    glutInitWindowPosition(0,0);
    //创建窗口，参数是窗口的标题，之后调用glutMainLoop将窗口显示到屏幕上
    glutCreateWindow("MBSet Display");
    init();

    //初始化2000个迭代次数值各自的三原色值
    cout << "Initiating colors..." << endl;
    for(int i = 0; i < maxIt; i++) {
        //随机初始化三原色值
        for(int j = 0; j < 3; j++) {
            colorMatch[i][j] = (float)rand() / RAND_MAX;
        }
        //将2000这个迭代次数值的三原色值均设为0，以表示黑色
        if(i == maxIt - 1) {
            colorMatch[i][0] = 0;
            colorMatch[i][1] = 0;
            colorMatch[i][2] = 0;
        }
    }
    cout << "Colors initiated" << endl;
    callThreads();

    //当窗口大小改变时reshape函数被调用以进行相应的调整
    glutReshapeFunc(reshape);
    //当发生鼠标事件时就会自动调用函数mouse
    glutMouseFunc(mouse);
    //当鼠标移动并且有一个鼠标键被按下时自动调用函数motion
    glutMotionFunc(motion);
    //当键盘上有一个键被按下时自动调用函数keyboard
    glutKeyboardFunc(keyboard);
    //当窗口需要重新绘制时调用display函数
    glutDisplayFunc(display);
    //glutMainLoop进入GLUT事件处理循环，让所有与“事件”有关的函数进行无限循环
    glutMainLoop();
    return 0;
}
