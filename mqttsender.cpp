#include "mqttsender.h"
#include <QRandomGenerator>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QDebug>
#include <QThread>  // 用于线程 ID 调试

MqttSender::MqttSender(QObject *parent)
    : QObject(nullptr)                    // 不要父对象，以便 moveToThread
    , m_thread(new QThread(this))          // 线程可有父对象，无需移动
    , m_client(nullptr)                    // 延迟创建
    , m_timer(nullptr)
    , m_counter(0)
{
    // 不在构造函数中创建或配置客户端
}

MqttSender::~MqttSender()
{
    stop();
    if (m_client) {
        // m_client 是在 init 中创建的，无父对象，需要手动删除
        delete m_client;
        m_client = nullptr;
    }
}

void MqttSender::start()
{
    // 将自身移入线程
    moveToThread(m_thread);
    // 启动线程
    m_thread->start();
    // 在目标线程中执行初始化
    QMetaObject::invokeMethod(this, &MqttSender::init, Qt::QueuedConnection);
}

void MqttSender::init()
{
    qDebug() << "init() running in thread:" << QThread::currentThreadId();

    // 在正确的线程中创建和配置客户端
    m_client = new QMqttClient(nullptr);  // 无父对象
    m_client->setHostname("broker-cn.emqx.io");
    m_client->setPort(1883);
    m_client->setClientId("sender");

    // 连接信号（此时 m_client 已经属于当前线程）
    connect(m_client, &QMqttClient::connected, this, &MqttSender::onConnected);

    // 开始连接
    connectToBroker();
}

void MqttSender::stop()
{
    if (m_thread->isRunning()) {
        // 可以在这里停止定时器等
        if (m_timer) {
            m_timer->stop();
            delete m_timer;
            m_timer = nullptr;
        }
        m_thread->quit();
        m_thread->wait();
    }
}

void MqttSender::connectToBroker()
{
    if (m_client) {
        m_client->connectToHost();
    }
}

void MqttSender::onConnected()
{
    qDebug() << "onConnected() in thread:" << QThread::currentThreadId();
    m_timer = new QTimer(this);  // 此时 this 已在子线程，timer 自动归属该线程
    connect(m_timer, &QTimer::timeout, this, &MqttSender::sendData);
    m_timer->start(1000);
}

void MqttSender::sendData()
{
    // 生成数据
    QJsonObject data;
    data["timestamp"] = QDateTime::currentDateTime().toString();
    data["sensor_id"] = "sensor_001";
    data["temperature"] = 20.0 + 10.0 * QRandomGenerator::global()->generateDouble();
    data["humidity"]    = 40.0 + 20.0 * QRandomGenerator::global()->generateDouble();
    data["pressure"]    = 1000.0 + 20.0 * QRandomGenerator::global()->generateDouble();
    data["counter"]     = m_counter++;

    QJsonDocument doc(data);
    QByteArray payload = doc.toJson();

    QMqttTopicName topic("industrial/data");
    m_client->publish(topic, payload);
    qDebug() << "Data sent in thread:" << QThread::currentThreadId() << payload;
}