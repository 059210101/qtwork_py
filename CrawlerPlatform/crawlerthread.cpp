#include "crawlerthread.h"
#include <QDebug>
#include <QUrl>
#include <QDateTime>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <cstdlib>

CrawlerThread::CrawlerThread(int taskId, QObject *parent)
    : QThread(parent)
    , m_taskId(taskId)
    , m_isRunning(false)
    , m_interval(5)
    , m_url("")
    , m_rule("")
    , m_nam(new QNetworkAccessManager(this))
{
    // 加载任务信息
    CrawlerTask task = DatabaseManager::getTaskById(taskId);
    if (task.id != 0) {
        m_url = task.url;
        m_rule = task.rule;
        m_interval = task.interval > 0 ? task.interval : 5;
        qDebug() << "线程初始化成功，任务ID：" << taskId << "URL：" << m_url;
    } else {
        qWarning() << "任务ID" << taskId << "不存在，线程无法启动";
        m_isRunning = false;
    }

    // 绑定网络响应槽函数
    connect(m_nam, &QNetworkAccessManager::finished, this, &CrawlerThread::onReplyFinished);

    // 初始化随机数
    srand(static_cast<unsigned int>(QDateTime::currentMSecsSinceEpoch()));
}

CrawlerThread::~CrawlerThread()
{
    stopCrawling();
    if (m_nam) {
        m_nam->deleteLater();
        m_nam = nullptr;
    }
    qDebug() << "线程销毁，任务ID：" << m_taskId;
}

void CrawlerThread::startCrawling()
{
    if (m_isRunning) {
        qDebug() << "任务" << m_taskId << "已在运行";
        emit logMessage(QString("任务[%1] 已在运行，无需重复启动").arg(m_taskId));
        return;
    }

    if (m_url.isEmpty()) {
        qWarning() << "任务" << m_taskId << "URL为空，无法启动";
        emit logMessage(QString("任务[%1] URL为空，启动失败").arg(m_taskId));
        return;
    }

    m_isRunning = true;
    if (!isRunning()) {
        start(QThread::LowPriority);
    }

    emit statusUpdated(m_taskId, "已启动");
    emit logMessage(QString("任务[%1] 启动爬虫，目标URL：%2").arg(m_taskId).arg(m_url));
}

void CrawlerThread::stopCrawling()
{
    if (!m_isRunning) {
        qDebug() << "任务" << m_taskId << "未运行";
        emit logMessage(QString("任务[%1] 未运行，无需停止").arg(m_taskId));
        return;
    }

    m_isRunning = false;
    if (isRunning()) {
        quit();
        wait(5000);
    }

    emit statusUpdated(m_taskId, "已停止");
    emit logMessage(QString("任务[%1] 停止爬虫").arg(m_taskId));
}

void CrawlerThread::run()
{
    emit logMessage(QString("任务[%1] 线程启动，间隔：%2秒").arg(m_taskId).arg(m_interval));

    while (m_isRunning) {
        crawlOnce();
        if (m_isRunning && m_interval > 0) {
            msleep(static_cast<unsigned int>(m_interval * 1000));
        }
    }

    emit logMessage(QString("任务[%1] 线程退出").arg(m_taskId));
}

void CrawlerThread::crawlOnce()
{
    if (!m_isRunning) return;

    emit statusUpdated(m_taskId, "正在爬取...");
    emit logMessage(QString("任务[%1] 模拟爬取：%2").arg(m_taskId).arg(m_url));

    // 生成随机数模拟爬取结果
    double randomValue = generateRandomValue();

    // 构造数据
    CrawlerData data;
    data.taskId = m_taskId;
    data.content = QString::number(randomValue, 'f', 2);
    data.value = randomValue;
    data.crawlTime = QDateTime::currentDateTime();

    // 保存数据（调用线程安全的静态方法）
    bool saveOk = DatabaseManager::saveCrawlerData(data);
    if (saveOk) {
        emit statusUpdated(m_taskId, "爬取成功");
        emit logMessage(QString("任务[%1] 模拟爬取成功：数值=%2").arg(m_taskId).arg(randomValue));
        emit dataCrawled(m_taskId, data);
    } else {
        emit statusUpdated(m_taskId, "数据保存失败");
        emit logMessage(QString("任务[%1] 数据写入数据库失败").arg(m_taskId));
    }
}

double CrawlerThread::generateRandomValue()
{
    return static_cast<double>(rand() % 1000) / 10.0; // 0~99.9
}

double CrawlerThread::parseValue(const QString& html, const QString& rule)
{
    if (html.isEmpty()) return generateRandomValue();

    QString regexStr = rule.isEmpty() ? "\\d+\\.?\\d*" : rule;
    QRegularExpression re(regexStr);
    QRegularExpressionMatchIterator it = re.globalMatch(html);

    double value = 0.0;
    if (it.hasNext()) {
        bool ok = false;
        value = it.next().captured().toDouble(&ok);
        if (!ok) value = generateRandomValue();
    } else {
        value = generateRandomValue();
    }
    return value;
}

void CrawlerThread::onReplyFinished(QNetworkReply* reply)
{
    if (!reply) {
        emit logMessage(QString("任务[%1] 爬取失败：空响应").arg(m_taskId));
        emit statusUpdated(m_taskId, "爬取失败");
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        emit logMessage(QString("任务[%1] 爬取失败：%2（URL：%3）")
                            .arg(m_taskId)
                            .arg(reply->errorString())
                            .arg(reply->url().toString()));
        emit statusUpdated(m_taskId, "爬取失败");
        reply->deleteLater();
        return;
    }

    // 解析响应
    QByteArray data = reply->readAll();
    QString html = QString::fromUtf8(data.isEmpty() ? "0" : data);
    double value = parseValue(html, m_rule);

    // 保存数据
    CrawlerData crawlerData;
    crawlerData.taskId = m_taskId;
    crawlerData.content = QString::number(value, 'f', 2);
    crawlerData.value = value;
    crawlerData.crawlTime = QDateTime::currentDateTime();

    bool saveOk = DatabaseManager::saveCrawlerData(crawlerData);
    if (saveOk) {
        emit statusUpdated(m_taskId, "爬取成功");
        emit logMessage(QString("任务[%1] 爬取成功：数值=%2").arg(m_taskId).arg(value));
        emit dataCrawled(m_taskId, crawlerData);
    } else {
        emit statusUpdated(m_taskId, "数据保存失败");
    }

    reply->deleteLater();
}
