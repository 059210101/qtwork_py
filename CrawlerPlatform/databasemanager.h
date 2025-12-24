#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QObject>
#include <QList>
#include <QString>
#include <QDateTime>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QMutex>
#include <QThread>
#include <QUuid>

// 爬虫任务结构体
struct CrawlerTask {
    int id = 0;
    QString name = "";
    QString url = "";
    int interval = 5;
    QString rule = "";
};

// 爬虫数据结构体
struct CrawlerData {
    int taskId = 0;
    QString content = "";
    double value = 0.0;
    QDateTime crawlTime = QDateTime::currentDateTime();
};

// 数据库管理类（多线程安全）
class DatabaseManager {
public:
    // 获取当前线程的独立数据库连接
    static QSqlDatabase getThreadDatabase();

    // 初始化数据表结构（主线程调用一次）
    static bool initDatabaseSchema();

    // 任务管理接口
    static bool saveCrawlerTask(const CrawlerTask& task);
    static QList<CrawlerTask> getAllTasks();
    static CrawlerTask getTaskById(int taskId);

    // 数据管理接口
    static bool saveCrawlerData(const CrawlerData& data);
    static QList<CrawlerData> getTaskData(int taskId);

private:
    // 禁止实例化
    DatabaseManager() = delete;
    ~DatabaseManager() = delete;
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    static QMutex m_mutex; // 线程安全锁
};

#endif // DATABASEMANAGER_H
