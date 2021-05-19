#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <vector>
#include <QString>
#include "MvCamera.h"
#include <Windows.h>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void EnableControls(bool bIsCameraReady);

    int GetTriggerMode();               //Get Trigger Mode
    int SetTriggerMode();               //Set Trigger Mode

    int GetExposureTime();              //Get Exposure Time
    int SetExposureTime();              //Set Exposure Time

    int GetGain();                      //Get Gain
    int SetGain();                      //Set Gain

    int GetFrameRate();                 //Get Frame Rate
    int SetFrameRate();                 //Set Frame Rate

    int GetTriggerSource();             //Get Trigger Source
    int SetTriggerSource();             //Set Trigger Source

    int CloseDevice();                  //Close Device
    int GrabThreadProcess();
    bool RemoveCustomPixelFormats(enum MvGvspPixelType enPixelFormat);
    int SaveImage(MV_SAVE_IAMGE_TYPE enSaveImageType);

private:
    Ui::MainWindow          *ui;                        //主窗口指针
    MV_CC_DEVICE_INFO_LIST  *m_stDevList;               //相机设备列表
    MV_FRAME_OUT_INFO_EX    m_stImageInfo;              //图像信息
    CMvCamera*              m_pcMyCamera;               //设备句柄
    CRITICAL_SECTION        m_hSaveImageMux;            //临界空间
    HWND                    m_hwndDisplay;              //图像显示标签句柄
    unsigned char*          m_pSaveImageBuf;            //一帧图像数据缓存
    void*                   m_hGrabThread;              //线程句柄
    bool                    m_bOpenDevice;              //设备状态标志
    bool                    m_bStartGrabbing;           //设备抓取状态标志
    bool                    m_bSoftWareTriggerCheck;    //软件触发状态标志
    bool                    m_bThreadState;             //线程状态
    int                     m_nTriggerMode;             //触发模式标志【连续模式   触发模式】
    int                     m_nTriggerSource;           //触发源标志【软件触发 其他触发】
    unsigned int            m_nSaveImageBufSize;        //图像缓存器大小


private slots:
    void OnEnumDevices();               //枚举设备
    void OnOpenDevices();               //打开设备
    void OnCloseDevice();               //关闭设备
    void OnGetParameter();              //获取当前参数
    void OnSetParameter();              //设置参数
    void OnSelectTriggerMode();         //选择点动触发模式
    void OnSelectContinueMode();        //选择连续触发模式
    void OnStartGrabbing();             //开始采集
    void OnStopGrabbing();              //停止采集
    void OnSoftwareTriggerChecker();    //勾选软件触发
    void OnSoftwareOnce();              //软触发一次
    void OnSaveBmp();                   //保存BMP图像
    void OnSaveJpg();                   //保存JPG图像
    void OnSaveTiff();                  //保存TIFF图像
    void OnSavePng();                   //保存PNG图像
};
#endif // MAINWINDOW_H
