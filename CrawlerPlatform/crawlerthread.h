#ifndef CRAWLERTHREAD_H
#define CRAWLERTHREAD_H

#include <QThread>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QString>
#include "databasemanager.h"

class CrawlerThread : public QThread
{
    Q_OBJECT

public:
    explicit CrawlerThread(int taskId, QObject *parent = nullptr);
    ~CrawlerThread() override;

    // 控制接口
    void startCrawling();
    void stopCrawling();

signals:
    void statusUpdated(int taskId, const QString& status);
    void logMessage(const QString& message);
    void dataCrawled(int taskId, const CrawlerData& data);

protected:
    void run() override;

private:
    void crawlOnce();
    double generateRandomValue();
    double parseValue(const QString& html, const QString& rule);
    void onReplyFinished(QNetworkReply* reply);

    // 成员变量
    int m_taskId;
    bool m_isRunning;
    int m_interval;
    QString m_url;
    QString m_rule;
    QNetworkAccessManager* m_nam;
};

#endif // CRAWLERTHREAD_H
