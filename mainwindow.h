#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMqttClient>
#include <QChartView>
#include <QLineSeries>
#include <QValueAxis>
#include <QChart>
#include <QDateTimeAxis>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QTimer>

class MqttSender;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

// 数据结构体
struct DataPoint {
    QDateTime timestamp;
    double temperature;
    double humidity;
    double pressure;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    
private Q_SLOTS:
    void connectToBroker();
    void onConnected();
    void onMessageReceived(const QByteArray &message, const QMqttTopicName &topic);

private:
    void updateChart();
    void updateYAxis();
    Ui::MainWindow *ui;
    QMqttClient *m_client;
    MqttSender *m_sender;
    QChart *m_chart;
    QLineSeries *m_temperatureSeries;
    QLineSeries *m_humiditySeries;
    QLineSeries *m_pressureSeries;
    QDateTimeAxis *m_xAxis;  // 改为 QDateTimeAxis 用于时间显示
    QValueAxis *m_yAxis;
    QList<DataPoint> m_dataList;  // 存储最近1小时的数据点
    QTimer *m_updateTimer;  // 添加定时器
};

#endif // MAINWINDOW_H
