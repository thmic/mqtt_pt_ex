#ifndef MQTTSENDER_H
#define MQTTSENDER_H

#include <QObject>
#include <QMqttClient>
#include <QTimer>
#include <QThread>

class MqttSender : public QObject
{
    Q_OBJECT

public:
    explicit MqttSender(QObject *parent = nullptr);
    ~MqttSender();

    void start();
    void stop();

Q_SIGNALS:
    void finished();

private Q_SLOTS:
    void init();              // 新增：在目标线程中执行初始化
    void connectToBroker();
    void onConnected();
    void sendData();

private:
    QThread *m_thread;
    QMqttClient *m_client;
    QTimer *m_timer;
    int m_counter;
};

#endif // MQTTSENDER_H