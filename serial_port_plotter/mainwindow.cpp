/***************************************************************************
**  This file is part of Serial Port Plotter                              **
**                                                                        **
**                                                                        **
**  Serial Port Plotter is a program for plotting integer data from       **
**  serial port using Qt and QCustomPlot                                  **
**                                                                        **
**  This program is free software: you can redistribute it and/or modify  **
**  it under the terms of the GNU General Public License as published by  **
**  the Free Software Foundation, either version 3 of the License, or     **
**  (at your option) any later version.                                   **
**                                                                        **
**  This program is distributed in the hope that it will be useful,       **
**  but WITHOUT ANY WARRANTY; without even the implied warranty of        **
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         **
**  GNU General Public License for more details.                          **
**                                                                        **
**  You should have received a copy of the GNU General Public License     **
**  along with this program.  If not, see http://www.gnu.org/licenses/.   **
**                                                                        **
****************************************************************************
**           Author: Borislav                                             **
**           Contact: b.kereziev@gmail.com                                **
**           Date: 29.12.14                                               **
****************************************************************************/

#include "mainwindow.hpp"
#include "ui_mainwindow.h"
#include <x86intrin.h>

/**
 * @brief Constructor
 * @param parent
 */
MainWindow::MainWindow (QWidget *parent) :
    QMainWindow (parent),
    ui (new Ui::MainWindow),

    /* 通道线的颜色初始化 */
    line_colors{
        /* Light */
        QColor ("#fb4934"),
        QColor ("#b8bb26"),
        QColor ("#fabd2f"),
        QColor ("#83a598"),
        QColor ("#d3869b"),
        QColor ("#8ec07c"),
        QColor ("#fe8019"),
        /* Light */
        QColor ("#cc241d"),
        QColor ("#98971a"),
        QColor ("#d79921"),
        QColor ("#458588"),
        QColor ("#b16286"),
        QColor ("#689d6a"),
        QColor ("#d65d0e"),
        QColor ("#0000FF"),
        },
    gui_colors {
        /* 单色UI 深色的UI*/
        QColor (48,  47,  47,  255), /**<  0: 背景颜色：最黑色 */
        QColor (80,  80,  80,  255), /**<  1: 网格颜色：中间黑 */
        QColor (170, 170, 170, 255), /**<  2: 文字颜色：白色 */
        QColor (48,  47,  47,  200)  /**<  3: 背景颜色：黑色，和0的透明度不同 */
        },

    /* 初始化变量 */
    connected (false),//默认没有开始
    plotting (false),//默认没有绘图
    dataPointNumber (0),//默认接到0次数据
    channels(0),//默认0个通道
    serialPort (nullptr),//默认没有打开串口
    STATE (WAIT_START)//默认没有解析到帧头
{
    ui->setupUi (this);

    /* 初始化UI */
    createUI();

    /* 设置绘图区 */
    setupPlot();

    /* 鼠标滚轮在绘图区滚动槽函数 */
    connect (ui->plot, SIGNAL (mouseWheel (QWheelEvent*)), this, SLOT (on_mouse_wheel_in_plot (QWheelEvent*)));

    /* 打印鼠标在绘图区中坐标槽函数 */
    connect (ui->plot, SIGNAL (mouseMove (QMouseEvent*)), this, SLOT (onMouseMoveInPlot (QMouseEvent*)));

    /* 鼠标选择图例或绘图区中曲线的槽函数 */
    connect (ui->plot, SIGNAL(selectionChangedByUser()), this, SLOT(channel_selection()));
    /* 鼠标双击图例的槽函数 */
    connect (ui->plot, SIGNAL(legendDoubleClick (QCPLegend*, QCPAbstractLegendItem*, QMouseEvent*)), this, SLOT(legend_double_click (QCPLegend*, QCPAbstractLegendItem*, QMouseEvent*)));

    /* 定时刷新绘图区 */
    connect (&updateTimer, SIGNAL (timeout()), this, SLOT (replot()));

    m_csvFile = nullptr;
}

/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/**
 * @brief Destructor
 */
MainWindow::~MainWindow()
{
    closeCsvFile();

    if (serialPort != nullptr)//删除串口
    {
        delete serialPort;
    }
    delete ui;
}
/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/**
 * @brief 初始化UI
 */
void MainWindow::createUI()
{
    if (QSerialPortInfo::availablePorts().size() == 0)//电脑上没有插入任何串口
    {
        enable_com_controls (false);
        ui->statusBar->showMessage ("没有串口.");
        ui->savePNGButton->setEnabled (false);
        return;
    }


    for (QSerialPortInfo port : QSerialPortInfo::availablePorts())//填充串口号
    {
        ui->comboPort->addItem (port.portName());
    }

    /* 增加波特率 */
    ui->comboBaud->addItem ("1200");
    ui->comboBaud->addItem ("2400");
    ui->comboBaud->addItem ("4800");
    ui->comboBaud->addItem ("9600");
    ui->comboBaud->addItem ("19200");
    ui->comboBaud->addItem ("38400");
    ui->comboBaud->addItem ("57600");
    ui->comboBaud->addItem ("115200");

    /* 默认选择115200波特率 */
    ui->comboBaud->setCurrentIndex (7);

    /* 清空用于显示通道的控件 */
    ui->listWidget_Channels->clear();
}
/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/**
 * @brief 设置绘图区
 */
void MainWindow::setupPlot()
{
    /* 清空绘图区所有内容 */
    ui->plot->clearItems();

    /* 设置绘图区背景颜色 */
    ui->plot->setBackground (gui_colors[0]);

    /* 设置为高性能模式 */
    ui->plot->setNotAntialiasedElements (QCP::aeAll);
    QFont font;
    font.setStyleStrategy (QFont::NoAntialias);
    ui->plot->legend->setFont (font);//设置绘图区字体


    /* 设置X轴风格 */
    ui->plot->xAxis->grid()->setPen (QPen(gui_colors[2], 1, Qt::DotLine));
    ui->plot->xAxis->grid()->setSubGridPen (QPen(gui_colors[1], 1, Qt::DotLine));
    ui->plot->xAxis->grid()->setSubGridVisible (true);
    ui->plot->xAxis->setBasePen (QPen (gui_colors[2]));
    ui->plot->xAxis->setTickPen (QPen (gui_colors[2]));
    ui->plot->xAxis->setSubTickPen (QPen (gui_colors[2]));
    ui->plot->xAxis->setUpperEnding (QCPLineEnding::esSpikeArrow);
    ui->plot->xAxis->setTickLabelColor (gui_colors[2]);
    ui->plot->xAxis->setTickLabelFont (font);
    /* 显示范围 */
    ui->plot->xAxis->setRange (dataPointNumber - ui->spinPoints->value(), dataPointNumber);

    /* 设置Y轴风格 */
    ui->plot->yAxis->grid()->setPen (QPen(gui_colors[2], 1, Qt::DotLine));
    ui->plot->yAxis->grid()->setSubGridPen (QPen(gui_colors[1], 1, Qt::DotLine));
    ui->plot->yAxis->grid()->setSubGridVisible (true);
    ui->plot->yAxis->setBasePen (QPen (gui_colors[2]));
    ui->plot->yAxis->setTickPen (QPen (gui_colors[2]));
    ui->plot->yAxis->setSubTickPen (QPen (gui_colors[2]));
    ui->plot->yAxis->setUpperEnding (QCPLineEnding::esSpikeArrow);
    ui->plot->yAxis->setTickLabelColor (gui_colors[2]);
    ui->plot->yAxis->setTickLabelFont (font);
    /* Range */
    //ui->plot->yAxis->setRange (ui->spinAxesMin->value(), ui->spinAxesMax->value());
    /* User can change Y axis tick step with a spin box */
    //ui->plot->yAxis->setAutoTickStep (false);
    //ui->plot->yAxis->(ui->spinYStep->value());

    /* User interactions Drag and Zoom are allowed only on X axis, Y is fixed manually by UI control */
    ui->plot->setInteraction (QCP::iRangeDrag, true);
    //ui->plot->setInteraction (QCP::iRangeZoom, true);
    ui->plot->setInteraction (QCP::iSelectPlottables, true);
    ui->plot->setInteraction (QCP::iSelectLegend, true);
    ui->plot->axisRect()->setRangeDrag (Qt::Horizontal);
    ui->plot->axisRect()->setRangeZoom (Qt::Horizontal);

    /* 设置图例 */
    QFont legendFont;
    legendFont.setPointSize (9);
    ui->plot->legend->setVisible (true);
    ui->plot->legend->setFont (legendFont);
    ui->plot->legend->setBrush (gui_colors[3]);
    ui->plot->legend->setBorderPen (gui_colors[2]);
    /* By default, the legend is in the inset layout of the main axis rect. So this is how we access it to change legend placement */
    ui->plot->axisRect()->insetLayout()->setInsetAlignment (0, Qt::AlignTop|Qt::AlignRight);
}
/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/**
 * @brief 使能或失能串口的控件
 * @param enable true enable, false disable
 */
void MainWindow::enable_com_controls (bool enable)
{
    /* 端口和波特率控件 */
    ui->comboBaud->setEnabled (enable);
    ui->comboPort->setEnabled (enable);
    ui->pushButton->setEnabled (enable);

    ui->actionConnect->setEnabled (enable);//开始按钮
    ui->actionPause_Plot->setEnabled (!enable);//暂停按钮
    ui->actionDisconnect->setEnabled (!enable);//停止按钮
}
/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/**
 * @brief 打开串口
 * @param portInfo 串口信息
 * @param baudRate 波特率
 * @param dataBits 数据位
 * @param parity 校验位
 * @param stopBits 停止位
 */
void MainWindow::openPort (QSerialPortInfo portInfo, int baudRate, QSerialPort::DataBits dataBits, QSerialPort::Parity parity, QSerialPort::StopBits stopBits)
{
    serialPort = new QSerialPort(portInfo, nullptr);//创建串口

    /*串口打开成功槽函数*/
    connect (this, SIGNAL(portOpenOK()), this, SLOT(portOpenedSuccess()));
    /*串口打开失败槽函数*/
    connect (this, SIGNAL(portOpenFail()), this, SLOT(portOpenedFail()));
    /*串口关闭槽函数*/
    connect (this, SIGNAL(portClosed()), this, SLOT(onPortClosed()));
    /*向绘图区增加新的数据槽函数*/
    connect (this, SIGNAL(newData(QStringList)), this, SLOT(onNewDataArrived(QStringList)));
    /*串口数据读取槽函数*/
    connect (serialPort, SIGNAL(readyRead()), this, SLOT(readData()));
    
    /*保存绘图数据到csv文件*/
    connect (this, SIGNAL(newData(QStringList)), this, SLOT(saveStream(QStringList)));

    if (serialPort->open (QIODevice::ReadWrite))
    {
        serialPort->setBaudRate (baudRate);
        serialPort->setParity (parity);
        serialPort->setDataBits (dataBits);
        serialPort->setStopBits (stopBits);
        emit portOpenOK();
    }
    else
    {
        emit portOpenedFail();
        qDebug() << serialPort->errorString();
    }
}
/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/**
 * @brief 串口关闭槽函数
 */
void MainWindow::onPortClosed()
{
    updateTimer.stop();
    connected = false;
    plotting = false;
    
    closeCsvFile();//关闭CSV文件
    
    disconnect (serialPort, SIGNAL(readyRead()), this, SLOT(readData()));
    disconnect (this, SIGNAL(portOpenOK()), this, SLOT(portOpenedSuccess()));
    disconnect (this, SIGNAL(portOpenFail()), this, SLOT(portOpenedFail()));
    disconnect (this, SIGNAL(portClosed()), this, SLOT(onPortClosed()));
    disconnect (this, SIGNAL(newData(QStringList)), this, SLOT(onNewDataArrived(QStringList)));

    disconnect (this, SIGNAL(newData(QStringList)), this, SLOT(saveStream(QStringList)));
}
/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/**
 * @brief 当选择com改变，状态栏显示com的信息
 * @param arg1
 */
void MainWindow::on_comboPort_currentIndexChanged (const QString &arg1)
{
    QSerialPortInfo selectedPort (arg1);
    ui->statusBar->showMessage (selectedPort.description());
}
/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/**
 * @brief 串口成功打开槽函数
 */
void MainWindow::portOpenedSuccess()
{
    setupPlot(); //设置绘图区，也就是重新清空一下绘图区
    ui->statusBar->showMessage ("串口成功打开!");
    enable_com_controls (false); //控件控制
    
    if(ui->actionRecord_stream->isChecked())//是否要保存数据到csv文件
    {
        openCsvFile();
    }
    ui->actionRecord_stream->setEnabled(false);//锁定保存数据的按钮，不能让用户操作了

    updateTimer.start (20); //20ms重新绘制
    connected = true; //正在连接flg
    plotting = true;//正在绘图flg
}
/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/**
 * @brief 串口打开失败槽函数
 */
void MainWindow::portOpenedFail()
{
    ui->statusBar->showMessage ("串口打开失败!");
}
/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/**
 * @brief 定时刷新绘图区
 */
void MainWindow::replot()
{
    /*刷新X轴坐标范围*/
    ui->plot->xAxis->setRange (dataPointNumber - ui->spinPoints->value(), dataPointNumber);
    ui->plot->replot();
}
/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/**
 * @brief 向绘图区增加新的数据
 * @param newData 串口数据，这是解包后的数据，真正用于绘图的数据
 */
void MainWindow::onNewDataArrived(QStringList newData)
{
    static int data_members = 0;
    static int channel = 0;
    static int i = 0;
    volatile bool you_shall_NOT_PASS = false;

    /* When a fast baud rate is set (921kbps was the first to starts to bug),
       this method is called multiple times (2x in the 921k tests), so a flag
       is used to throttle
       TO-DO: Separate processes, buffer data (1) and process data (2) */
    while (you_shall_NOT_PASS) {}
    you_shall_NOT_PASS = true;

    if (plotting)//正在绘图
    {
        data_members = newData.size();//获取数据的长度

        for (i = 0; i < data_members; i++)//遍历数据，解析数据
        {
            /* 第一次进入，添加所有通道 */
            while (ui->plot->plottableCount() <= channel)//新的数据，通道是否比之前的多
            {
                /* 添加新的通道数据 */
                ui->plot->addGraph();
                ui->plot->graph()->setPen (line_colors[channels % CUSTOM_LINE_COLORS]);
                ui->plot->graph()->setName (QString("Channel %1").arg(channels));
                if(ui->plot->legend->item(channels))
                {
                    ui->plot->legend->item (channels)->setTextColor (line_colors[channels % CUSTOM_LINE_COLORS]);
                }
                ui->listWidget_Channels->addItem(ui->plot->graph()->name());
                ui->listWidget_Channels->item(channel)->setForeground(QBrush(line_colors[channels % CUSTOM_LINE_COLORS]));
                channels++;
            }

            /* [TODO] Method selection and plotting */
            /* X-Y */
            if (0)
            {

            }
            /* Rolling (v1.0.0 compatible) */
            else
            {
                /* Add data to Graph 0 */
                ui->plot->graph(channel)->addData (dataPointNumber, newData[channel].toDouble());
                /* Increment data number and channel */
                channel++;
            }
        }

        /* Post-parsing */
        /* X-Y */
        if (0)
        {

        }
        /* Rolling (v1.0.0 compatible) */
        else
        {
            dataPointNumber++;
            channel = 0;
        }
    }
    you_shall_NOT_PASS = false;
}
/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/**
 * @brief 当Y轴最小值变化时，也就是spinAxesMin控件变化
 * @param arg1
 */
void MainWindow::on_spinAxesMin_valueChanged(int arg1)
{
    ui->plot->yAxis->setRangeLower (arg1);
    ui->plot->replot();
}
/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/**
 * @brief 当Y轴最大值变化时，也就是spinAxesMax控件变化
 * @param arg1
 */
void MainWindow::on_spinAxesMax_valueChanged(int arg1)
{
    ui->plot->yAxis->setRangeUpper (arg1);
    ui->plot->replot();
}
/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/**
 * @brief 从串口中读取数据
 */
void MainWindow::readData()
{
    if(serialPort->bytesAvailable()) {//串口中是否有数据
        QByteArray data = serialPort->readAll(); //读取所有数据

        if(!data.isEmpty()) {//得到的数据是否为空
            char *temp = data.data();//获取以 '\0' 结尾的 char* 到数据

            if (!filterDisplayedData){//是否要显示过滤后的数据
                ui->textEdit_UartWindow->append(data);
            }
            for(int i = 0; temp[i] != '\0'; i++) {//遍历数组
                switch(STATE) {
                case WAIT_START://等待帧头状态
                    if(temp[i] == START_MSG) {//接收到帧头
                        STATE = IN_MESSAGE;
                        receivedData.clear(); //清空数据
                        break;
                    }
                    break;
                case IN_MESSAGE://接收到帧头
                    if(temp[i] == END_MSG) {//接收到帧尾
                        STATE = WAIT_START;
                        QStringList incomingData = receivedData.split(' '); //使用空格将它们分割
                        if(filterDisplayedData){
                            ui->textEdit_UartWindow->append(receivedData);
                        }
                        emit newData(incomingData); //发送信号，解析到数据用于显示到绘图区
                        break;
                    }
                    else if (isdigit (temp[i]) || isspace (temp[i]) || temp[i] =='-' || temp[i] =='.')//检查字符是否为数字，空格，'-'，'.'
                    {
                        receivedData.append(temp[i]);
                    }
                    break;
                default: break;
                }
            }
        }
    }
}
/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/**
 * @brief 当Y轴单元格长度变化，也就是spinYStep控件发生变化
 * @param arg1
 */
void MainWindow::on_spinYStep_valueChanged(int arg1)
{
    ui->plot->yAxis->ticker()->setTickCount(arg1);
    ui->plot->replot();
    ui->spinYStep->setValue(ui->plot->yAxis->ticker()->tickCount());
}
/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/**
 * @brief 保存png图片到exe的运行目录
 */
void MainWindow::on_savePNGButton_clicked()
{
    ui->plot->savePng (QString::number(dataPointNumber) + ".png", 1920, 1080, 2, 50);
}
/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/**
 * @brief 打印鼠标在绘图区中的坐标
 * @param event
 */
void MainWindow::onMouseMoveInPlot(QMouseEvent *event)
{
    int xx = int(ui->plot->xAxis->pixelToCoord(event->x()));
    int yy = int(ui->plot->yAxis->pixelToCoord(event->y()));
    QString coordinates("X: %1 Y: %2");
    coordinates = coordinates.arg(xx).arg(yy);
    ui->statusBar->showMessage(coordinates);
}
/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/**
 * @brief 鼠标滚轮在绘图区滚动
 * @param event
 */
void MainWindow::on_mouse_wheel_in_plot (QWheelEvent *event)
{
    QWheelEvent inverted_event = QWheelEvent(event->posF(), event->globalPosF(),
                                             -event->pixelDelta(), -event->angleDelta(),
                                             0, Qt::Vertical, event->buttons(), event->modifiers());
    QApplication::sendEvent (ui->spinPoints, &inverted_event);
}
/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/**
 * @brief 鼠标选择图例或绘图区中曲线
 * @param plottable
 * @param event
 */
void MainWindow::channel_selection (void)
{
    /* 同步用于选择图例和选择曲线 */
    for (int i = 0; i < ui->plot->graphCount(); i++)
    {
        QCPGraph *graph = ui->plot->graph(i);
        QCPPlottableLegendItem *item = ui->plot->legend->itemWithPlottable(graph);

        if (item->selected() || graph->selected())
        {
            item->setSelected(true);

            QPen pen;
            pen.setWidth(3);
            pen.setColor(line_colors[14]);
            graph->selectionDecorator()->setPen(pen);

            graph->setSelection(QCPDataSelection(graph->data()->dataRange()));
        }
    }
}
/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/**
 * @brief 双击图例时，用于给图例修改名字
 * @param legend
 * @param item
 */
void MainWindow::legend_double_click(QCPLegend *legend, QCPAbstractLegendItem *item, QMouseEvent *event)
{
    Q_UNUSED (legend)
    Q_UNUSED(event)
    if (item)
    {
        QCPPlottableLegendItem *plItem = qobject_cast<QCPPlottableLegendItem*>(item);
        bool ok;
        QString newName = QInputDialog::getText (this, "Change channel name", "New name:", QLineEdit::Normal, plItem->plottable()->name(), &ok, Qt::Widget);//最有一个参数是Qt::Popup，无法输入中文
        if (ok)
        {
            plItem->plottable()->setName(newName);
            for(int i=0; i<ui->plot->graphCount(); i++)
            {
                ui->listWidget_Channels->item(i)->setText(ui->plot->graph(i)->name());
            }
            ui->plot->replot();
        }
    }
}
/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/**
 * @brief 当X轴单元格宽度变化时，也就是spinPoints控件发生变化
 * @param arg1
 */
void MainWindow::on_spinPoints_valueChanged (int arg1)
{
    Q_UNUSED(arg1)
    ui->plot->xAxis->setRange (dataPointNumber - ui->spinPoints->value(), dataPointNumber);
    ui->plot->replot();
}
/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/**
 * @brief Shows a window with instructions
 */
void MainWindow::on_actionHow_to_use_triggered()
{
    helpWindow = new HelpWindow (this);
    helpWindow->setWindowTitle ("如何使用虚拟串口示波器工具");
    helpWindow->show();
}

/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/**
 * @brief 打开串口按钮触发
 */
void MainWindow::on_actionConnect_triggered()
{
    if (connected)
    {
        if (!plotting)//如果没有在绘制，就打开绘制
        {
            updateTimer.start();  //打开刷新绘图区定时器
            plotting = true;
            ui->actionConnect->setEnabled (false);
            ui->actionPause_Plot->setEnabled (true);
            ui->statusBar->showMessage ("重新开始绘制!");
        }
    }
    else
    {
        /* 打开串口 */
        QSerialPortInfo portInfo (ui->comboPort->currentText());
        int baudRate = ui->comboBaud->currentText().toInt();
        QSerialPort::DataBits dataBits;
        QSerialPort::Parity parity;
        QSerialPort::StopBits stopBits;

        dataBits = QSerialPort::Data8;
        parity = QSerialPort::NoParity;
        stopBits = QSerialPort::OneStop;

        /* 实例化串口 */
        serialPort = new QSerialPort (portInfo, nullptr);

        /* 打开串口 */
        openPort (portInfo, baudRate, dataBits, parity, stopBits);
    }
}
/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/**
 * @brief 停止绘图按钮触发
 */
void MainWindow::on_actionPause_Plot_triggered()
{
    if (plotting)
    {
        updateTimer.stop();
        plotting = false;
        ui->actionConnect->setEnabled (true);
        ui->actionPause_Plot->setEnabled (false);
        ui->statusBar->showMessage ("绘图停止，新的数据将不绘图");
    }
}
/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/**
 * @brief 保存数据按钮触发
 */
void MainWindow::on_actionRecord_stream_triggered()
{
    if (ui->actionRecord_stream->isChecked())
    {
        ui->statusBar->showMessage ("数据将被保存到csv文件");
    }
    else
    {
        ui->statusBar->showMessage ("数据将不会被保存");
    }
}
/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/**
 * @brief 关闭串口按钮触发
 */
void MainWindow::on_actionDisconnect_triggered()
{
    if (connected)
    {
        serialPort->close();
        emit portClosed();
        delete serialPort;
        serialPort = nullptr;

        ui->statusBar->showMessage ("关闭串口!");

        connected = false;
        ui->actionConnect->setEnabled (true);

        plotting = false;
        ui->actionPause_Plot->setEnabled (false);
        ui->actionDisconnect->setEnabled (false);
        ui->actionRecord_stream->setEnabled(true);
        receivedData.clear();

        ui->savePNGButton->setEnabled (false);
        enable_com_controls (true);
    }
}
/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/**
 * @brief 清空数据按钮触发，将清空所有数据
 */
void MainWindow::on_actionClear_triggered()
{
    ui->plot->clearPlottables();
    ui->listWidget_Channels->clear();
    channels = 0;
    dataPointNumber = 0;
    emit setupPlot();
    ui->plot->replot();
}
/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/**
 * @brief 使用当前日期/时间戳创建新的 CSV 文件
 *
 */
void MainWindow::openCsvFile(void)
{
    m_csvFile = new QFile(QDateTime::currentDateTime().toString("yyyy-MM-d-HH-mm-ss-")+"data-out.csv");//创建文件
    if(!m_csvFile)
        return;
    if (!m_csvFile->open(QIODevice::ReadWrite | QIODevice::Text))
        return;

}
/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/**
 * @brief 关闭CSV文件
 *
 */
void MainWindow::closeCsvFile(void)
{
    if(!m_csvFile) return;
    m_csvFile->close();
    if(m_csvFile) delete m_csvFile;
    m_csvFile = nullptr;
}
/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/**
 * @brief 保存接收到数据
 *
 */
void MainWindow::saveStream(QStringList newData)
{
    if(!m_csvFile)
        return;
    if(ui->actionRecord_stream->isChecked())
    {
        QTextStream out(m_csvFile);
        foreach (const QString &str, newData) {
            out << str << ",";
        }
        out << "\n";
    }
}

/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/**
 * @brief 是否隐藏text文本框
 */
void MainWindow::on_pushButton_TextEditHide_clicked()
{
    if(ui->pushButton_TextEditHide->isChecked())
    {
        ui->textEdit_UartWindow->setVisible(false);
        ui->pushButton_TextEditHide->setText("显示 TextBox");
    }
    else
    {
        ui->textEdit_UartWindow->setVisible(true);
        ui->pushButton_TextEditHide->setText("隐藏 TextBox");
    }
}
/**
 * @brief 是否显示过滤前的数据
 */
void MainWindow::on_pushButton_ShowallData_clicked()
{
    if(ui->pushButton_ShowallData->isChecked())
    {
        filterDisplayedData = false;
        ui->pushButton_ShowallData->setText("显示过滤数据");
    }
    else
    {
        filterDisplayedData = true;
        ui->pushButton_ShowallData->setText("显示所有数据");
    }
}
/**
 * @brief Y轴自动缩放到合适比例
 */
void MainWindow::on_pushButton_AutoScale_clicked()
{
    ui->plot->yAxis->rescale(true);
    ui->spinAxesMax->setValue(int(ui->plot->yAxis->range().upper) + int(ui->plot->yAxis->range().upper*0.1));
    ui->spinAxesMin->setValue(int(ui->plot->yAxis->range().lower) + int(ui->plot->yAxis->range().lower*0.1));
}
/**
 * @brief 显示所有通道
 */
void MainWindow::on_pushButton_ResetVisible_clicked()
{
    for(int i=0; i<ui->plot->graphCount(); i++)
    {
        ui->plot->graph(i)->setVisible(true);
        ui->listWidget_Channels->item(i)->setBackground(Qt::NoBrush);
    }
}
/**
 * @brief 双击通道列表，用于关闭某个通道显示
 * @param item
 */
void MainWindow::on_listWidget_Channels_itemDoubleClicked(QListWidgetItem *item)
{
    int graphIdx = ui->listWidget_Channels->currentRow();

    if(ui->plot->graph(graphIdx)->visible())
    {
        ui->plot->graph(graphIdx)->setVisible(false);
        item->setBackgroundColor(Qt::black);
    }
    else
    {
        ui->plot->graph(graphIdx)->setVisible(true);
        item->setBackground(Qt::NoBrush);
    }
    ui->plot->replot();
}
/**
 * @brief 扫描串口
 */
void MainWindow::on_pushButton_clicked()
{
    ui->comboPort->clear();
    for (QSerialPortInfo port : QSerialPortInfo::availablePorts())
    {
        ui->comboPort->addItem (port.portName());
    }
}
