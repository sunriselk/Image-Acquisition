#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMessageBox>
#include "QDebug"
#include <QChar>
#include <process.h>

using std::vector;
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    m_hGrabThread           = NULL;
    m_stDevList             = NULL;
    m_pcMyCamera            = NULL;
    m_pSaveImageBuf         = NULL;
    m_bOpenDevice           = false;
    m_bStartGrabbing        = false;
    m_bSoftWareTriggerCheck = false;
    m_bThreadState          = false;
    m_nTriggerMode          = MV_TRIGGER_MODE_OFF;
    m_nTriggerSource        = MV_TRIGGER_SOURCE_SOFTWARE;
    m_nSaveImageBufSize     = 0;
    m_hwndDisplay = (HWND)ui->DisplayLabel->winId();
    memset(&m_stImageInfo, 0, sizeof(MV_FRAME_OUT_INFO_EX));
    if(m_hwndDisplay)
    {
        EnableControls(false);
    }
    InitializeCriticalSection(&m_hSaveImageMux);    //初始化临界区域对象

    QObject::connect(ui->EnumButton, &QPushButton::clicked, this, &MainWindow::OnEnumDevices);
    QObject::connect(ui->OpenButton, &QPushButton::clicked, this, &MainWindow::OnOpenDevices);
    QObject::connect(ui->GetParamButton, &QPushButton::clicked, this, &MainWindow::OnGetParameter);
    QObject::connect(ui->TriggerModeRadio, &QRadioButton::clicked, this, &MainWindow::OnSelectTriggerMode);
    QObject::connect(ui->ContinuesRadioButton, &QPushButton::clicked, this, &MainWindow::OnSelectContinueMode);
    QObject::connect(ui->CloseButton, &QPushButton::clicked, this, &MainWindow::OnCloseDevice);
    QObject::connect(ui->StartGrabbingButton, &QPushButton::clicked, this, &MainWindow::OnStartGrabbing);
    QObject::connect(ui->StopGrabbingButton, &QPushButton::clicked, this, &MainWindow::OnStopGrabbing);
    QObject::connect(ui->SoftwareTriggerCheck, &QPushButton::clicked, this, &MainWindow::OnSoftwareTriggerChecker);
    QObject::connect(ui->SoftwareOnceButton, &QPushButton::clicked, this, &MainWindow::OnSoftwareOnce);
    QObject::connect(ui->SaveBmpButton, &QPushButton::clicked, this, &MainWindow::OnSaveBmp);
    QObject::connect(ui->SaveJpgButton, &QPushButton::clicked, this, &MainWindow::OnSaveJpg);
    QObject::connect(ui->SavePngButton, &QPushButton::clicked, this, &MainWindow::OnSavePng);
    QObject::connect(ui->SaveTiffButton,&QPushButton::clicked, this, &MainWindow::OnSaveTiff);
    QObject::connect(ui->SetParamButton, &QPushButton::clicked, this, &MainWindow::OnSetParameter);
}

MainWindow::~MainWindow()
{
    delete ui;
    delete m_stDevList;
}

unsigned int __stdcall GrabThread(void* pUser)
{
    if (pUser)
    {
        MainWindow* pCam = (MainWindow*)pUser;          //pUser用于线程通讯
        pCam->GrabThreadProcess();

        return 0;
    }

    return -1;
}

void MainWindow::EnableControls(bool bIsCameraReady)
{
    ui->OpenButton->setEnabled(m_bOpenDevice ? FALSE : (bIsCameraReady ? TRUE : FALSE));
    ui->CloseButton->setEnabled((m_bOpenDevice && bIsCameraReady) ? TRUE : FALSE);
    ui->StartGrabbingButton->setEnabled((m_bStartGrabbing && bIsCameraReady) ? FALSE : (m_bOpenDevice ? TRUE : FALSE));
    ui->StopGrabbingButton->setEnabled(m_bStartGrabbing ? TRUE : FALSE);
    ui->SoftwareTriggerCheck->setEnabled(m_bOpenDevice ? TRUE : FALSE);
    ui->SoftwareOnceButton->setEnabled((m_bStartGrabbing && m_bSoftWareTriggerCheck && ui->TriggerModeRadio->isChecked())? TRUE : FALSE);
    ui->SaveBmpButton->setEnabled(m_bStartGrabbing ? TRUE : FALSE);
    ui->SaveJpgButton->setEnabled(m_bStartGrabbing ? TRUE : FALSE);
    ui->SavePngButton->setEnabled(m_bStartGrabbing ? TRUE : FALSE);
    ui->SaveTiffButton->setEnabled(m_bStartGrabbing ? TRUE : FALSE);
    ui->ExposureLineEdit->setEnabled(m_bOpenDevice ? TRUE : FALSE);
    ui->GainLineEdit->setEnabled(m_bOpenDevice ? TRUE : FALSE);
    ui->FrameRateLineEdit->setEnabled(m_bOpenDevice ? TRUE : FALSE);
    ui->GetParamButton->setEnabled(m_bOpenDevice ? TRUE : FALSE);
    ui->SetParamButton->setEnabled(m_bOpenDevice ? TRUE : FALSE);
    ui->ContinuesRadioButton->setEnabled(m_bOpenDevice ? TRUE : FALSE);
    ui->TriggerModeRadio->setEnabled(m_bOpenDevice ? TRUE : FALSE);
}

int MainWindow::SetTriggerMode()
{
    return m_pcMyCamera->SetEnumValue("TriggerMode", m_nTriggerMode);
}

int MainWindow::GetTriggerMode()
{
    MVCC_ENUMVALUE stEnumValue;

    int nRet = m_pcMyCamera->GetEnumValue("TriggerMode", &stEnumValue);
    if (MV_OK != nRet)
    {
        return nRet;
    }

    m_nTriggerMode = stEnumValue.nCurValue;
    if (MV_TRIGGER_MODE_ON ==  m_nTriggerMode)
    {
        OnSelectTriggerMode();
    }
    else
    {
        m_nTriggerMode = MV_TRIGGER_MODE_OFF;
        OnSelectContinueMode();
    }

    return MV_OK;
}

int MainWindow::GetExposureTime()
{
    MVCC_FLOATVALUE stFloatValue;

    int nRet = m_pcMyCamera->GetFloatValue("ExposureTime", &stFloatValue);
    if (MV_OK != nRet)
    {
        return nRet;
    }
    ui->ExposureLineEdit->setText(QString::number(stFloatValue.fCurValue));
    return MV_OK;
}

int MainWindow::SetExposureTime()
{
    // 调节这两个曝光模式，才能让曝光时间生效
    int nRet = m_pcMyCamera->SetEnumValue("ExposureMode", MV_EXPOSURE_MODE_TIMED);
    if (MV_OK != nRet)
    {
        return nRet;
    }
    m_pcMyCamera->SetEnumValue("ExposureAuto", MV_EXPOSURE_AUTO_MODE_OFF);

    //设置曝光时间
    return m_pcMyCamera->SetFloatValue("ExposureTime", (ui->ExposureLineEdit->displayText()).toFloat());
}

int MainWindow::GetGain()
{
    MVCC_FLOATVALUE stFloatValue;
    int nRet = m_pcMyCamera->GetFloatValue("Gain", &stFloatValue);
    if (MV_OK != nRet)
    {
        return nRet;
    }
    ui->GainLineEdit->setText(QString::number(stFloatValue.fCurValue));
    return MV_OK;
}

int MainWindow::SetGain()
{
    // 设置增益前先把自动增益关闭
    m_pcMyCamera->SetEnumValue("GainAuto", 0);
    return m_pcMyCamera->SetFloatValue("Gain", ui->GainLineEdit->displayText().toFloat());
}

int MainWindow::GetFrameRate()
{
    MVCC_FLOATVALUE stFloatValue;
    int nRet = m_pcMyCamera->GetFloatValue("ResultingFrameRate", &stFloatValue);
    if (MV_OK != nRet)
    {
        return nRet;
    }
    ui->FrameRateLineEdit->setText(QString::number(stFloatValue.fCurValue));
    return MV_OK;
}

int MainWindow::SetFrameRate()
{
    int nRet = m_pcMyCamera->SetBoolValue("AcquisitionFrameRateEnable", true);
    if (MV_OK != nRet)
    {
        return nRet;
    }
    return m_pcMyCamera->SetFloatValue("AcquisitionFrameRate", ui->FrameRateLineEdit->displayText().toFloat());
}

int MainWindow::GetTriggerSource()
{
    MVCC_ENUMVALUE stEnumValue;

    int nRet = m_pcMyCamera->GetEnumValue("TriggerSource", &stEnumValue);
    if (MV_OK != nRet)
    {
        return nRet;
    }

    if ((unsigned int)MV_TRIGGER_SOURCE_SOFTWARE == stEnumValue.nCurValue)
    {
        m_bSoftWareTriggerCheck = TRUE;
    }
    else
    {
        m_bSoftWareTriggerCheck = FALSE;
    }
    ui->SoftwareTriggerCheck->setChecked(m_bSoftWareTriggerCheck);

    return MV_OK;
}

int MainWindow::SetTriggerSource()
{
    int nRet = MV_OK;
    if (m_bSoftWareTriggerCheck)
    {
        m_nTriggerSource = MV_TRIGGER_SOURCE_SOFTWARE;
        nRet = m_pcMyCamera->SetEnumValue("TriggerSource", m_nTriggerSource);
        if (MV_OK != nRet)
        {
            QMessageBox::information(this, "Error message!", "Set Software Trigger Fail");
            return nRet;
        }
        ui->SoftwareOnceButton->setEnabled(true);
    }
    else
    {
        m_nTriggerSource = MV_TRIGGER_SOURCE_LINE0;
        nRet = m_pcMyCamera->SetEnumValue("TriggerSource", m_nTriggerSource);
        if (MV_OK != nRet)
        {
            QMessageBox::information(this, "Error message!", "Set Hardware Trigger Fail");
            return nRet;
        }
        ui->SoftwareOnceButton->setEnabled(false);
    }
    return nRet;
}

int MainWindow::CloseDevice()
{
    m_bThreadState = FALSE;
       if (m_hGrabThread)
       {
           WaitForSingleObject(m_hGrabThread, INFINITE);
           CloseHandle(m_hGrabThread);
           m_hGrabThread = NULL;
       }

       if (m_pcMyCamera)
       {
           m_pcMyCamera->Close();
           delete m_pcMyCamera;
           m_pcMyCamera = NULL;
       }

       m_bStartGrabbing = FALSE;
       m_bOpenDevice = FALSE;

       if (m_pSaveImageBuf)
       {
           free(m_pSaveImageBuf);
           m_pSaveImageBuf = NULL;
       }
       m_nSaveImageBufSize = 0;

       return MV_OK;
}

int MainWindow::GrabThreadProcess()
{

    MV_FRAME_OUT stImageInfo;
    MV_DISPLAY_FRAME_INFO stDisplayInfo;
    int nRet = MV_OK;
    while(m_bThreadState)
    {
        nRet = m_pcMyCamera->GetImageBuffer(&stImageInfo, 1000);
        if (nRet == MV_OK)
        {
            //用于保存图片
            EnterCriticalSection(&m_hSaveImageMux);
            if (NULL == m_pSaveImageBuf || stImageInfo.stFrameInfo.nFrameLen > m_nSaveImageBufSize)
            {
                if (m_pSaveImageBuf)
                {
                    free(m_pSaveImageBuf);
                    m_pSaveImageBuf = NULL;
                }

                m_pSaveImageBuf = (unsigned char *)malloc(sizeof(unsigned char) * stImageInfo.stFrameInfo.nFrameLen);
                if (m_pSaveImageBuf == NULL)
                {
                    LeaveCriticalSection(&m_hSaveImageMux);
                    return 0;
                }
                m_nSaveImageBufSize = stImageInfo.stFrameInfo.nFrameLen;
            }
            memcpy(m_pSaveImageBuf, stImageInfo.pBufAddr, stImageInfo.stFrameInfo.nFrameLen);
            memcpy(&m_stImageInfo, &(stImageInfo.stFrameInfo), sizeof(MV_FRAME_OUT_INFO_EX));
            LeaveCriticalSection(&m_hSaveImageMux);

            //自定义格式不支持显示
            if(RemoveCustomPixelFormats(stImageInfo.stFrameInfo.enPixelType))
            {
                m_pcMyCamera->FreeImageBuffer(&stImageInfo);
                continue;
            }
            stDisplayInfo.hWnd = m_hwndDisplay;
            stDisplayInfo.pData = stImageInfo.pBufAddr;
            stDisplayInfo.nDataLen = stImageInfo.stFrameInfo.nFrameLen;
            stDisplayInfo.nWidth = stImageInfo.stFrameInfo.nWidth;
            stDisplayInfo.nHeight = stImageInfo.stFrameInfo.nHeight;
            stDisplayInfo.enPixelType = stImageInfo.stFrameInfo.enPixelType;
            m_pcMyCamera->DisplayOneFrame(&stDisplayInfo);

            m_pcMyCamera->FreeImageBuffer(&stImageInfo);
        }
        else
        {
            if (MV_TRIGGER_MODE_ON ==  m_nTriggerMode)
            {
                Sleep(5);
            }
        }
    }

    return MV_OK;
}

bool MainWindow::RemoveCustomPixelFormats(MvGvspPixelType enPixelFormat)
{
    int nResult = enPixelFormat & MV_GVSP_PIX_CUSTOM;
   if((int)MV_GVSP_PIX_CUSTOM == nResult)
   {
       return true;
   }
   else
   {
       return false;
   }
}

int MainWindow::SaveImage(MV_SAVE_IAMGE_TYPE enSaveImageType)
{
    MV_SAVE_IMG_TO_FILE_PARAM stSaveFileParam;
    memset(&stSaveFileParam, 0, sizeof(MV_SAVE_IMG_TO_FILE_PARAM));

    EnterCriticalSection(&m_hSaveImageMux);
    if (m_pSaveImageBuf == NULL || m_stImageInfo.enPixelType == 0)
    {
        LeaveCriticalSection(&m_hSaveImageMux);
        return MV_E_NODATA;
    }

    if(RemoveCustomPixelFormats(m_stImageInfo.enPixelType))
    {
        LeaveCriticalSection(&m_hSaveImageMux);
        return MV_E_SUPPORT;
    }

    stSaveFileParam.enImageType = enSaveImageType; // ch:需要保存的图像类型 | en:Image format to save
    stSaveFileParam.enPixelType = m_stImageInfo.enPixelType;  // ch:相机对应的像素格式 | en:Camera pixel type
    stSaveFileParam.nWidth      = m_stImageInfo.nWidth;         // ch:相机对应的宽 | en:Width
    stSaveFileParam.nHeight     = m_stImageInfo.nHeight;          // ch:相机对应的高 | en:Height
    stSaveFileParam.nDataLen    = m_stImageInfo.nFrameLen;
    stSaveFileParam.pData       = m_pSaveImageBuf;
    stSaveFileParam.iMethodValue = 0;

    // ch:jpg图像质量范围为(50-99], png图像质量范围为[0-9] | en:jpg image nQuality range is (50-99], png image nQuality range is [0-9]
    if (MV_Image_Bmp == stSaveFileParam.enImageType)
    {
        sprintf_s(stSaveFileParam.pImagePath, 256, "../images_acquisition/images/Image_w%d_h%d_fn%03d.bmp",
                  stSaveFileParam.nWidth, stSaveFileParam.nHeight, m_stImageInfo.nFrameNum);
    }
    else if (MV_Image_Jpeg == stSaveFileParam.enImageType)
    {
        stSaveFileParam.nQuality = 80;
        sprintf_s(stSaveFileParam.pImagePath, 256, "../images_acquisition/images/Image_w%d_h%d_fn%03d.jpg",
                  stSaveFileParam.nWidth, stSaveFileParam.nHeight, m_stImageInfo.nFrameNum);
    }
    else if (MV_Image_Tif == stSaveFileParam.enImageType)
    {
        sprintf_s(stSaveFileParam.pImagePath, 256, "../images_acquisition/images/Image_w%d_h%d_fn%03d.tif",
                  stSaveFileParam.nWidth, stSaveFileParam.nHeight, m_stImageInfo.nFrameNum);
    }
    else if (MV_Image_Png == stSaveFileParam.enImageType)
    {
        stSaveFileParam.nQuality = 8;
        sprintf_s(stSaveFileParam.pImagePath, 256, "../images_acquisition/images/Image_w%d_h%d_fn%03d.png",
                  stSaveFileParam.nWidth, stSaveFileParam.nHeight, m_stImageInfo.nFrameNum);
    }

    int nRet = m_pcMyCamera->SaveImageToFile(&stSaveFileParam);
    LeaveCriticalSection(&m_hSaveImageMux);

    return nRet;
}

void MainWindow::OnEnumDevices()
{
    QString strMsg;
    ui->comboBox->clear();
    m_stDevList = new MV_CC_DEVICE_INFO_LIST;
    // ch:枚举子网内所有设备 | en:Enumerate all devices within subnet
    int nRet = CMvCamera::EnumDevices(MV_GIGE_DEVICE | MV_USB_DEVICE, m_stDevList);
    if (MV_OK != nRet)
    {
        return;
    }

    // ch:将值加入到信息列表框中并显示出来 | en:Add value to the information list box and display
    for (unsigned int i = 0; i < m_stDevList->nDeviceNum; i++)
    {
        MV_CC_DEVICE_INFO* pDeviceInfo = m_stDevList->pDeviceInfo[i];
        if (NULL == pDeviceInfo)
        {
            continue;
        }

        char strUserName[256] = {0};
        if (pDeviceInfo->nTLayerType == MV_GIGE_DEVICE)
        {
            int nIp1 = ((m_stDevList->pDeviceInfo[i]->SpecialInfo.stGigEInfo.nCurrentIp & 0xff000000) >> 24);
            int nIp2 = ((m_stDevList->pDeviceInfo[i]->SpecialInfo.stGigEInfo.nCurrentIp & 0x00ff0000) >> 16);
            int nIp3 = ((m_stDevList->pDeviceInfo[i]->SpecialInfo.stGigEInfo.nCurrentIp & 0x0000ff00) >> 8);
            int nIp4 = (m_stDevList->pDeviceInfo[i]->SpecialInfo.stGigEInfo.nCurrentIp & 0x000000ff);

            if (strcmp("", (LPCSTR)(pDeviceInfo->SpecialInfo.stGigEInfo.chUserDefinedName)) != 0)
            {
                sprintf_s(strUserName, "%s", pDeviceInfo->SpecialInfo.stGigEInfo.chUserDefinedName);
            }
            else
            {
                sprintf_s(strUserName, 256, "%s %s (%s)", pDeviceInfo->SpecialInfo.stGigEInfo.chManufacturerName,
                    pDeviceInfo->SpecialInfo.stGigEInfo.chModelName,
                    pDeviceInfo->SpecialInfo.stGigEInfo.chSerialNumber);
            }

            strMsg = "[" + QString::number(i) + "]GigE: "
                    + QString(strUserName)
                    + "("
                    + QString::number(nIp1) + "."
                    + QString::number(nIp2) + "."
                    + QString::number(nIp3) + "."
                    + QString::number(nIp4)
                    + ")";
        }
        else if (pDeviceInfo->nTLayerType == MV_USB_DEVICE)
        {
            if (strcmp("", (char*)pDeviceInfo->SpecialInfo.stUsb3VInfo.chUserDefinedName) != 0)
            {
                sprintf_s(strUserName, "%s", pDeviceInfo->SpecialInfo.stGigEInfo.chUserDefinedName);
            }
            else
            {
                sprintf_s(strUserName, 256, "%s %s (%s)", pDeviceInfo->SpecialInfo.stUsb3VInfo.chManufacturerName,
                    pDeviceInfo->SpecialInfo.stUsb3VInfo.chModelName,
                    pDeviceInfo->SpecialInfo.stUsb3VInfo.chSerialNumber);
            }
            strMsg = "[" + QString::number(i) + "]" + QString(strUserName);
        }
        else
        {
            QMessageBox::information(this, "Error Message", "Unknown device enumerated");
        }

        ui->comboBox->addItem(strMsg);
    }

    if (0 == m_stDevList->nDeviceNum)
    {
        QMessageBox::information(this, "Error Message", "No device");
        return;
    }

    ui->comboBox->setCurrentIndex(0);
    EnableControls(TRUE);
}

void MainWindow::OnOpenDevices()
{
    //判断设备是否已经打开
    if (TRUE == m_bOpenDevice || NULL != m_pcMyCamera)
    {
        return;
    }

    //判断设备索引号是否正确
    int nIndex = ui->comboBox->currentIndex();
    if ((nIndex < 0) | (nIndex >= MV_MAX_DEVICE_NUM))
    {
        QMessageBox::information(this,"Error Message", "Please select device!");
        return;
    }

    // ch:由设备信息创建设备实例 | en:Device instance created by device information
    if (NULL == m_stDevList->pDeviceInfo[nIndex])
    {
        QMessageBox::information(this,"Error Message", "Device does not exist!");
        return;
    }

    m_pcMyCamera = new CMvCamera;       //创建相机实例
    if (NULL == m_pcMyCamera)
    {
        return;
    }

    int nRet = m_pcMyCamera->Open(m_stDevList->pDeviceInfo[nIndex]);
    if (MV_OK != nRet)
    {
        delete m_pcMyCamera;
        m_pcMyCamera = NULL;
        QMessageBox::information(this,"Error Message", "Open Fail!");
        return;
    }

    // Detection network optimal package size(It only works for the GigE camera)
    if (m_stDevList->pDeviceInfo[nIndex]->nTLayerType == MV_GIGE_DEVICE)
    {
        unsigned int nPacketSize = 0;
        nRet = m_pcMyCamera->GetOptimalPacketSize(&nPacketSize);
        if (nRet == MV_OK)
        {
            nRet = m_pcMyCamera->SetIntValue("GevSCPSPacketSize",nPacketSize);
            if(nRet != MV_OK)
            {
                QMessageBox::information(this,"Error Message", "Warning: Set Packet Size fail!");
            }
        }
        else
        {
            QMessageBox::information(this,"Error Message", "Warning: Get Packet Size fail!");
        }
    }

    m_bOpenDevice = TRUE;
    EnableControls(TRUE);
    OnGetParameter();
}

void MainWindow::OnCloseDevice()
{
    CloseDevice();
    EnableControls(TRUE);
}

void MainWindow::OnGetParameter()
{
    //获取触发模式, 并初始化相应的界面按钮
    int nRet = GetTriggerMode();
     if (nRet != MV_OK)
     {
         QMessageBox::information(this, "Error message", "Get Trigger Mode Fail");
     }

    //获取曝光时间
     nRet = GetExposureTime();
     if (nRet != MV_OK)
     {
         QMessageBox::information(this, "Error message", "Get Exposure Time Fail");
     }

    //获取曝光增益
     nRet = GetGain();
     if (nRet != MV_OK)
     {
         QMessageBox::information(this, "Error message", "Get Gain Fail");
     }

    //获取帧率
     nRet = GetFrameRate();
     if (nRet != MV_OK)
     {
         QMessageBox::information(this, "Error message", "Get Frame Rate Fail");
     }

    //获取触发源
     nRet = GetTriggerSource();
     if (nRet != MV_OK)
     {
         QMessageBox::information(this, "Error message", "Get Trigger Source Fail");
     }
}

void MainWindow::OnSetParameter()
{
    bool bIsSetSucceed = true;
    int nRet = SetExposureTime();
    if (nRet != MV_OK)
    {
        bIsSetSucceed = false;
        QMessageBox::information(this, "Error message", "Set Exposure Time Fail");
    }
    nRet = SetGain();
    if (nRet != MV_OK)
    {
        bIsSetSucceed = false;
        QMessageBox::information(this, "Error message", "Set Gain Fail");
    }
    nRet = SetFrameRate();
    if (nRet != MV_OK)
    {
        bIsSetSucceed = false;
        QMessageBox::information(this, "Error message", "Set Frame Rate Fail");
    }

    if (true == bIsSetSucceed)
    {
        QMessageBox::information(this, "Error message", "Set Parameter Succeed");
    }
}

void MainWindow::OnSelectTriggerMode()
{
    ui->ContinuesRadioButton->setChecked(false);
    ui->TriggerModeRadio->setChecked(true);
    ui->SoftwareTriggerCheck->setEnabled(true);
    m_nTriggerMode = MV_TRIGGER_MODE_ON;
    int nRet = SetTriggerMode();
    if (MV_OK != nRet)
    {
        QMessageBox::information(this, "Error message", "et Trigger Mode Fail");
        return;
    }

    if (m_bStartGrabbing == TRUE)
    {
        if (TRUE == m_bSoftWareTriggerCheck)
        {
            ui->SoftwareOnceButton->setEnabled(true);
        }
    }
}

void MainWindow::OnSelectContinueMode()
{
    ui->ContinuesRadioButton->setChecked(true);
    ui->TriggerModeRadio->setChecked(false);
    ui->SoftwareTriggerCheck->setEnabled(false);
    m_nTriggerMode = MV_TRIGGER_MODE_OFF;
    int nRet = SetTriggerMode();
    if (MV_OK != nRet)
    {
        return;
    }
    ui->SoftwareOnceButton->setEnabled(false);
}

void MainWindow::OnStartGrabbing()
{
    if (FALSE == m_bOpenDevice || TRUE == m_bStartGrabbing || NULL == m_pcMyCamera)
    {
        return;
    }

    memset(&m_stImageInfo, 0, sizeof(MV_FRAME_OUT_INFO_EX));
    m_bThreadState = TRUE;
    unsigned int nThreadID = 0;
    m_hGrabThread = (void*)_beginthreadex( NULL , 0 , GrabThread, this, 0 , &nThreadID );
    if (NULL == m_hGrabThread)
    {
        m_bThreadState = FALSE;
        QMessageBox::information(this, "Error Message!", "Create thread fail");
        return;
    }

    int nRet = m_pcMyCamera->StartGrabbing();
    if (MV_OK != nRet)
    {
        m_bThreadState = FALSE;
        QMessageBox::information(this, "Error Message!", "Start grabbing fail");
        return;
    }
    m_bStartGrabbing = TRUE;
    EnableControls(TRUE);
}

void MainWindow::OnStopGrabbing()
{
    if (FALSE == m_bOpenDevice || FALSE == m_bStartGrabbing || NULL == m_pcMyCamera)
    {
        return;
    }

    m_bThreadState = FALSE;
    if (m_hGrabThread)
    {
        WaitForSingleObject(m_hGrabThread, INFINITE);
        CloseHandle(m_hGrabThread);
        m_hGrabThread = NULL;
    }

    int nRet = m_pcMyCamera->StopGrabbing();
    if (MV_OK != nRet)
    {
        QMessageBox::information(this, "Error message!", "Stop grabbing fail");
        return;
    }
    m_bStartGrabbing = FALSE;
    EnableControls(TRUE);
}

void MainWindow::OnSoftwareTriggerChecker()
{
    m_bSoftWareTriggerCheck = ui->SoftwareTriggerCheck->isChecked();
    int nRet = SetTriggerSource();
    if (nRet != MV_OK)
    {
        return;
    }
}

void MainWindow::OnSoftwareOnce()
{
    if (TRUE != m_bStartGrabbing)
    {
        return;
    }
    m_pcMyCamera->CommandExecute("TriggerSoftware");
}

void MainWindow::OnSaveBmp()
{
    int nRet = SaveImage(MV_Image_Bmp);
    if (MV_OK != nRet)
    {
        QMessageBox::information(this, "Error message!", "Save bmp fail");
        return;
    }
    QMessageBox::information(this, "Message", "Save bmp succeed");
}

void MainWindow::OnSaveJpg()
{
    int nRet = SaveImage(MV_Image_Jpeg);
    if (MV_OK != nRet)
    {
        QMessageBox::information(this, "Error message!", "Save Jpg fail");
        return;
    }
    QMessageBox::information(this, "Message", "Save Jpg succeed");
}

void MainWindow::OnSaveTiff()
{
    int nRet = SaveImage(MV_Image_Tif);
    if (MV_OK != nRet)
    {
        QMessageBox::information(this, "Error message!", "Save Tiff fail");
        return;
    }
    QMessageBox::information(this, "Message", "Save Tiff succeed");
}

void MainWindow::OnSavePng()
{
    int nRet = SaveImage(MV_Image_Png);
    if (MV_OK != nRet)
    {
        QMessageBox::information(this, "Error message!", "Save Png fail");
        return;
    }
    QMessageBox::information(this, "Message", "Save Png succeed");
}

