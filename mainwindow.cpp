#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "mqttsender.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QChart>
#include <QChartView>
#include <QLineSeries>
#include <QValueAxis>
#include <QDateTimeAxis>
#include <QPainter>
#include <QDateTime>
#include <QMqttTopicFilter>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_client(new QMqttClient(this))
    , m_sender(new MqttSender(nullptr))
{
    ui->setupUi(this);

    m_client->setHostname("broker-cn.emqx.io");
    m_client->setPort(1883);
    m_client->setClientId("receiver");

    connect(m_client, &QMqttClient::connected, this, &MainWindow::onConnected);
    connect(m_client, &QMqttClient::messageReceived, this, &MainWindow::onMessageReceived);

    m_chart = new QChart();

    m_temperatureSeries = new QLineSeries();
    m_humiditySeries = new QLineSeries();
    m_pressureSeries = new QLineSeries();

    m_temperatureSeries->setName("温度 (°C)");
    m_humiditySeries->setName("湿度 (%)");
    m_pressureSeries->setName("压力 (hPa)");

    QPen tempPen(Qt::red);
    tempPen.setWidth(2);
    m_temperatureSeries->setPen(tempPen);

    QPen humPen(Qt::blue);
    humPen.setWidth(2);
    m_humiditySeries->setPen(humPen);

    QPen pressPen(Qt::green);
    pressPen.setWidth(2);
    m_pressureSeries->setPen(pressPen);

    m_chart->addSeries(m_temperatureSeries);
    m_chart->addSeries(m_humiditySeries);
    m_chart->addSeries(m_pressureSeries);

    // X轴：时间轴
    m_xAxis = new QDateTimeAxis();
    m_xAxis->setTitleText("时间");
    m_xAxis->setFormat("HH:mm:ss");
    m_xAxis->setTickCount(6);
    m_xAxis->setLabelsAngle(-45);

    // 初始显示范围
    QDateTime now = QDateTime::currentDateTime();
    m_xAxis->setRange(now.addSecs(-300), now.addSecs(60));

    // Y轴
    m_yAxis = new QValueAxis();
    m_yAxis->setTitleText("数值");

    m_chart->addAxis(m_xAxis, Qt::AlignBottom);
    m_chart->addAxis(m_yAxis, Qt::AlignLeft);

    m_temperatureSeries->attachAxis(m_xAxis);
    m_temperatureSeries->attachAxis(m_yAxis);
    m_humiditySeries->attachAxis(m_xAxis);
    m_humiditySeries->attachAxis(m_yAxis);
    m_pressureSeries->attachAxis(m_xAxis);
    m_pressureSeries->attachAxis(m_yAxis);

    m_chart->legend()->setVisible(true);
    m_chart->legend()->setAlignment(Qt::AlignTop);

    QChartView *chartView = new QChartView(m_chart);
    chartView->setRenderHint(QPainter::Antialiasing);
    ui->verticalLayout->addWidget(chartView);

    m_sender->start();
    connectToBroker();
}

MainWindow::~MainWindow()
{
    delete ui;
    delete m_client;
}

void MainWindow::connectToBroker()
{
    m_client->connectToHost();
}

void MainWindow::onConnected()
{
    qDebug() << "Receiver connected to broker, subscribing...";

    QMqttSubscription *subscription = m_client->subscribe(QMqttTopicFilter("industrial/data"), 0);

    if (!subscription) {
        qDebug() << "Failed to create subscription";
        return;
    }

    connect(subscription, &QMqttSubscription::stateChanged, this, [this](QMqttSubscription::SubscriptionState state) {
        if (state == QMqttSubscription::SubscriptionState::Subscribed) {
            qDebug() << "Successfully subscribed to topic!";
        }
    });
}

void MainWindow::onMessageReceived(const QByteArray &message, const QMqttTopicName &topic)
{
    (void) topic;

    QJsonDocument doc = QJsonDocument::fromJson(message);
    QJsonObject obj = doc.object();

    double temperature = obj["temperature"].toDouble();
    double humidity = obj["humidity"].toDouble();
    double pressure = obj["pressure"].toDouble();

    // 解析时间戳
    QString timestampStr = obj["timestamp"].toString();
    QDateTime dataTime = QDateTime::fromString(timestampStr, "ddd MMM dd HH:mm:ss yyyy");

    if (!dataTime.isValid()) {
        dataTime = QDateTime::currentDateTime();
    }

    // 添加数据点（不过滤，先全部保存）
    DataPoint point;
    point.timestamp = dataTime;
    point.temperature = temperature;
    point.humidity = humidity;
    point.pressure = pressure;
    m_dataList.append(point);

    qDebug() << "Added data:" << dataTime.toString("HH:mm:ss")
             << "Total:" << m_dataList.size();

    // 更新图表显示
    updateChart();

    // 更新UI显示
    ui->temperatureLabel->setText(QString("温度: %1 °C").arg(temperature, 0, 'f', 2));
    ui->humidityLabel->setText(QString("湿度: %1 %").arg(humidity, 0, 'f', 2));
    ui->pressureLabel->setText(QString("压力: %1 hPa").arg(pressure, 0, 'f', 2));
}

void MainWindow::updateChart()
{
    if (m_dataList.isEmpty()) return;

    // ========== 关键修复：使用当前系统时间作为窗口基准 ==========
    QDateTime now = QDateTime::currentDateTime();

    // 6分钟窗口：5分钟历史（左侧） + 1分钟未来（右侧）
    // 当前时间在 80% 位置（5/6 ≈ 0.833）
    // qint64 windowSizeMs = 6 * 60 * 1000;  // 6分钟

    QDateTime windowStart = now.addMSecs(-5 * 60 * 1000);  // 现在 - 5分钟
    QDateTime windowEnd = now.addMSecs(1 * 60 * 1000);     // 现在 + 1分钟

    qDebug() << "Window:" << windowStart.toString("HH:mm:ss")
             << "-" << windowEnd.toString("HH:mm:ss")
             << "Now at 80%:" << now.toString("HH:mm:ss");

    // 只清理完全在窗口左侧之外的数据（保留窗口内的和稍微超出的）
    // 使用 5分钟前作为清理界限
    int removedCount = 0;
    while (!m_dataList.isEmpty()) {
        // 如果最早的数据比窗口开始时间还早超过1分钟，就删除
        if (m_dataList.first().timestamp < windowStart.addSecs(-60)) {
            m_dataList.removeFirst();
            removedCount++;
        } else {
            break;
        }
    }

    if (removedCount > 0) {
        qDebug() << "Removed old data:" << removedCount;
    }

    // 清空曲线并重新填充
    m_temperatureSeries->clear();
    m_humiditySeries->clear();
    m_pressureSeries->clear();

    int plottedCount = 0;
    for (const auto &point : m_dataList) {
        // 只绘制在窗口内的数据（稍微放宽一点边界）
        if (point.timestamp >= windowStart.addSecs(-30) &&
            point.timestamp <= windowEnd.addSecs(30)) {

            qreal xValue = point.timestamp.toMSecsSinceEpoch();
            m_temperatureSeries->append(xValue, point.temperature);
            m_humiditySeries->append(xValue, point.humidity);
            m_pressureSeries->append(xValue, point.pressure);
            plottedCount++;
        }
    }

    qDebug() << "Plotted points:" << plottedCount;

    // 更新X轴范围（强制滚动）
    m_xAxis->setRange(windowStart, windowEnd);

    // 更新Y轴
    updateYAxis();
}

void MainWindow::updateYAxis()
{
    if (m_dataList.isEmpty()) return;

    double minTemp = 9999, maxTemp = -9999;
    double minHum = 9999, maxHum = -9999;
    double minPress = 9999, maxPress = -9999;

    // 只计算可见窗口内的Y轴范围
    QDateTime now = QDateTime::currentDateTime();
    QDateTime windowStart = now.addSecs(-5 * 60);
    QDateTime windowEnd = now.addSecs(1 * 60);

    int count = 0;
    for (const auto &point : m_dataList) {
        if (point.timestamp >= windowStart && point.timestamp <= windowEnd) {
            minTemp = qMin(minTemp, point.temperature);
            maxTemp = qMax(maxTemp, point.temperature);
            minHum = qMin(minHum, point.humidity);
            maxHum = qMax(maxHum, point.humidity);
            minPress = qMin(minPress, point.pressure);
            maxPress = qMax(maxPress, point.pressure);
            count++;
        }
    }

    if (count == 0) return;  // 没有可见数据就不更新Y轴

    double tempMargin = qMax((maxTemp - minTemp) * 0.1, 1.0);
    double humMargin = qMax((maxHum - minHum) * 0.1, 1.0);
    double pressMargin = qMax((maxPress - minPress) * 0.1, 10.0);

    double yMin = qMin(minTemp - tempMargin, qMin(minHum - humMargin, minPress - pressMargin));
    double yMax = qMax(maxTemp + tempMargin, qMax(maxHum + humMargin, maxPress + pressMargin));

    m_yAxis->setRange(yMin, yMax);
}
