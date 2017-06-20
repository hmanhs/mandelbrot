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

// Min and max complex plane values,��ƽ���ϳ�ʼ���εĽ���
Complex minC(-2.0, -1.2);
Complex maxC( 1.0, 1.8);
double lowerReal = -2.0;
double upperReal = 1.0;
double lowerImag = -1.2;
double upperImag = 1.8;

//�������Ļ�����ϵ������������
int pressX, pressY;

//��Ļ���ڵĴ�С
int screenSize = 512;
//�����̵߳�����
int totalThreads = 16;
int maxIt = 2000;     // Max iterations for the set computations,���������������ޣ�
//��Ļ�������������أ�512*512�������Եĵ�������
int numIterations[512][512];
//��Ļ�������������ظ��Ե�R G B ��ԭɫֵ�����ݸ��Ե���ֹ��������ֵ���趨Ϊ��Ӧ��ֵ
float colors[512][512][3];
//2000����������ֵ���Զ�Ӧ��R G B ��ԭɫֵ
float colorMatch[2000][3];

void callThreads();

pthread_mutex_t updateMutex;
pthread_mutex_t computationDoneMutex;
//cout �ı����������
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
    //������Ҫ����ÿһ�����ص�
    glBegin(GL_POINTS);
    //ָ�����ص��ֱ��
    glPointSize(4);
    for(int j = 0; j < screenSize; j++) {
        for(int k = 0; k < screenSize; k++) {
            //�º�������(k,j)���ص㴦��R G B��ԭɫ
            glColor3f(colors[j][k][0], colors[j][k][1], colors[j][k][2]);
            //������Ҫ��ʾ�����ص�����
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
    //���ú������ɫ��glClear����glClearColor�������úõĵ�ǰ�����ɫ���ô�����ɫ
    glClearColor(0,0,0, 0.0);
    //������������Ҫ����ͶӰ����(GL_PROJECTION)
    glMatrixMode (GL_PROJECTION);
    //����ͶӰ����������ͶӰ�봰�ڴ�С�ı�����1:1
    gluOrtho2D (0.0, screenSize, 0.0, screenSize);
    //�������Ϊ��ͼ����������
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    //ģ�����ź���������x,y,z�ֱ�Ϊģ����x,y,z�᷽������ű�
    glScalef(1, 1, 1);
}



void reshape(int w, int h)//�Ӽ�����Ŀ�Ⱥ͸߶ȷֱ�Ϊw��h
{
    //����glViewPort�����������Ӽ����򣬸���OpenGLӦ����Ⱦ֮���ͼ�λ����ڴ�����ĸ���λ
    glViewport(0, 0, w, h);
}



void mouse(int button, int state, int x, int y)
{
    // Your mouse click processing here
    //����button�����ĸ����������»��ɿ�
    // ����state == 0 means pressed(����), state != 0 means released(�ɿ�)
    //������x,y���ṩ����갴�»��ɿ�ʱ��ǰ�Ĵ������꣨�����Ͻ�Ϊԭ�㣩
    // Note that the x and y coordinates passed in are in
    // PIXELS, with y = 0 at the top.
    if(state == 0) {
        //cout << "Pressed at x: " << x << "  y: " << y << endl;
        pressX = x;
        //��Ϊ������Ļ�����ϵ����ص㵽��ƽ���ڵ�ӳ���������½�Ϊ��׼�㣬����y������Ҫ���������ת��
        pressY = screenSize - y;
    }
    if(state != 0) {
        int releaseX = x, releaseY = screenSize - y;
        //���ӵ�����ͷ��Ƿ������ƶ�
        bool toRight = (releaseX >= pressX);
        //���ӵ�����ͷ��Ƿ������ƶ�
        bool toTop = (releaseY >= pressY);
        //delta����Ļ�����ηŴ�����ı߳�����Ϊ��Ļ����Ϊ������
        //�������ӵ�����ͷ���ѡ��ľ��β�һ����һ�������Σ��������������ȡ��Сֵ
        int delta = min(abs(releaseX - pressX), abs(releaseY - pressY));
        //������ĵ�����ͷ�����ͬһ�㣬��������Ļ�����ηŴ�����ı߳�Ϊ10
        if(delta == 0)
            delta = 10;
        //(minX,minY)����Ļ�����ηŴ��������½ǵ����꣬
        //(maxX,maxY)����Ļ�����ηŴ��������Ͻǵ����꣨�����������Ļ���ڵ����½ǣ�
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


        //��ѡ������Ļ�����ηŴ�����֮��Ҫ�������Ӧ�ĸ�ƽ��Ŵ�����
        //(lowerReal_new,lowerImag_new)�Ǹ�ƽ��Ŵ��������½ǵ����꣬
        //(upperReal_new,upperImag_new)�Ǹ�ƽ��Ŵ��������Ͻǵ�����
        double lowerReal_new = lowerReal + (upperReal - lowerReal) * minX / (screenSize - 1);
        double upperReal_new = lowerReal + (upperReal - lowerReal) * maxX / (screenSize - 1);
        double lowerImag_new = lowerImag + (upperImag - lowerImag) * minY / (screenSize - 1);
        double upperImag_new = lowerImag + (upperImag - lowerImag) * maxY / (screenSize - 1);
        lowerReal = lowerReal_new;
        upperReal = upperReal_new;
        lowerImag = lowerImag_new;
        upperImag = upperImag_new;
        //����callThreads()���½��ж�����Ļ�����ηŴ�������Mandelbrot���ϵļ���
        callThreads();
        //��ʾ�µ���Ļ�����ηŴ�����
        display();
    }
}

void motion(int x, int y)
{
    // Your mouse motion here, x and y coordinates are as above

}

void keyboard(unsigned char c, int x, int y)
{ // Your keyboard processing here
    //����ƽ������ʾ���εĽ��޻ָ�Ϊ��ʼֵ
    lowerReal = minC.real;
    upperReal = maxC.real;
    lowerImag = minC.imag;
    upperImag = maxC.imag;
    callThreads();
    display();
}

void* ThreadCompute(void* i) {
    int myRank = (int)(long)i;
    //��ȡ��myRank���̶߳�����Ļ���ڵļ���������ֱ�����0--screenSize,ˮƽ�����startCol--endCol
    int startCol = DivideEvenly(screenSize, totalThreads, myRank, false);
    int endCol = DivideEvenly(screenSize, totalThreads, myRank, true);
    pthread_mutex_lock(&coutMutex);
    pthread_mutex_unlock(&coutMutex);
    for(int j = 0; j < screenSize; j++) {
        for(int k = startCol; k <= endCol; k++) {
            //����Ļ�����ϵ�һ������(k,j)ӳ��Ϊ��(��)ƽ���Ͼ����ڵ�һ��(real,imag).
            double real = lowerReal + (upperReal - lowerReal) * k / (screenSize - 1);
            double imag = lowerImag + (upperImag - lowerImag) * j / (screenSize - 1);
            const Complex c(real, imag);
            Complex z(0, 0);
            //һ��������Mandelbrot���ϵ��ҽ�������Ӧ�����У��ɶ���ʽ����f(z)=z^2+c���е��κ�Ԫ�ص�ģ��������2
            //�������еĵ�Ԫ������������ֻ��ȡ���޵ĵ���������ģ���ˣ�������ȡ��������Ϊ2000
            int count = 0;
            while(true) {
                z = z * z + c;
                count++;
                //��ֹ����'count==2000'��ʾ��ƽ���ϵĵ�(real,imag)��ģ������������Mandelbrot����
                //��ֹ����'z.Mag().real>=2'��ʾ��ƽ���ϵĵ�(real,imag)������Mandelbrot����
                if(count == 2000 || z.Mag().real >= 2) {
                    //������Ļ��������(k,j)���ĵ�������Ϊcount
                    numIterations[j][k] = count;
                    //������Ļ��������(k,j)������ԭɫֵΪ��������ֵcount��Ӧ����ԭɫֵ
                    colors[j][k][0] = colorMatch[count - 1][0];
                    colors[j][k][1] = colorMatch[count - 1][1];
                    colors[j][k][2] = colorMatch[count - 1][2];
                    break;
                }
            }
        }
    }
    pthread_mutex_lock(&computationDoneMutex);
    //��i���̵߳ļ���������ɣ���Ծ�߳�����1
    activeThreads--;
    pthread_mutex_unlock(&computationDoneMutex);
    //������е��߳�(16��)������˸��Եļ������񣬸ı���������allDoneCondition�������źţ���callThreads()��������Ӧλ�ô�����ִ��
    if(activeThreads == 0) {
        pthread_cond_signal(&allDoneCondition);
    }

    return NULL;
}
void callThreads() {
    //��̬��ʼ��3��������
    pthread_mutex_init(&updateMutex, 0);
    pthread_mutex_init(&computationDoneMutex, 0);
    pthread_mutex_init(&coutMutex, 0);
    //��̬��ʼ��1����������
    pthread_cond_init(&allDoneCondition, 0);

    //����
    pthread_mutex_lock(&computationDoneMutex);
    pthread_mutex_lock(&coutMutex);
    activeThreads = totalThreads;
    for(int i = 0; i < totalThreads; i++) {
        //�̱߳�ʶ��
        pthread_t t;
        //�����̺߳���
        pthread_create(&t, 0, ThreadCompute, (void*)i);
    }
    cout << "Multi-thread Computing..." << endl;
    //����coutMutex
    pthread_mutex_unlock(&coutMutex);
    //����computationDone�����ȴ�allDoneCondition���������ı�
    pthread_cond_wait(&allDoneCondition, &computationDoneMutex);
    cout << "All threads finished computing!" << endl << endl;
}
int main(int argc, char** argv)
{
    //�õ�ǰʱ��time(0)�������������
    srand(time(0));
    // Computation Process
    //��GLUT���г�ʼ��
    glutInit(&argc, argv);
    //������ʾ��ʽ�������ֱ��ʾ˫�����RGB��ɫ
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(screenSize, screenSize);
    glutInitWindowPosition(0,0);
    //�������ڣ������Ǵ��ڵı��⣬֮�����glutMainLoop��������ʾ����Ļ��
    glutCreateWindow("MBSet Display");
    init();

    //��ʼ��2000����������ֵ���Ե���ԭɫֵ
    cout << "Initiating colors..." << endl;
    for(int i = 0; i < maxIt; i++) {
        //�����ʼ����ԭɫֵ
        for(int j = 0; j < 3; j++) {
            colorMatch[i][j] = (float)rand() / RAND_MAX;
        }
        //��2000�����������ֵ����ԭɫֵ����Ϊ0���Ա�ʾ��ɫ
        if(i == maxIt - 1) {
            colorMatch[i][0] = 0;
            colorMatch[i][1] = 0;
            colorMatch[i][2] = 0;
        }
    }
    cout << "Colors initiated" << endl;
    callThreads();

    //�����ڴ�С�ı�ʱreshape�����������Խ�����Ӧ�ĵ���
    glutReshapeFunc(reshape);
    //����������¼�ʱ�ͻ��Զ����ú���mouse
    glutMouseFunc(mouse);
    //������ƶ�������һ������������ʱ�Զ����ú���motion
    glutMotionFunc(motion);
    //����������һ����������ʱ�Զ����ú���keyboard
    glutKeyboardFunc(keyboard);
    //��������Ҫ���»���ʱ����display����
    glutDisplayFunc(display);
    //glutMainLoop����GLUT�¼�����ѭ�����������롰�¼����йصĺ�����������ѭ��
    glutMainLoop();
    return 0;
}
