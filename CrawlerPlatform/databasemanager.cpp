#include "databasemanager.h"

QMutex DatabaseManager::m_mutex;

// 核心：获取线程独立的数据库连接
QSqlDatabase DatabaseManager::getThreadDatabase() {
    QMutexLocker locker(&m_mutex);

    // 每个线程唯一连接名
    QString connectionName = QString("sqlite_conn_%1_%2")
                                 .arg((quintptr)QThread::currentThreadId())
                                 .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));

    // 连接已存在且打开，直接返回
    if (QSqlDatabase::contains(connectionName)) {
        QSqlDatabase db = QSqlDatabase::database(connectionName);
        if (db.isOpen()) {
            return db;
        }
        // 连接存在但未打开，重新打开
        if (db.open()) {
            return db;
        } else {
            qCritical() << "线程" << QThread::currentThreadId()
                << "重新打开数据库失败：" << db.lastError().text();
            QSqlDatabase::removeDatabase(connectionName); // 移除无效连接
        }
    }

    // 创建新连接
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
    db.setDatabaseName("crawler_data.db"); // 共享数据库文件
    db.setConnectOptions("QSQLITE_OPEN_READWRITE"); // 读写模式

    // 打开数据库
    if (!db.open()) {
        qCritical() << "线程" << QThread::currentThreadId()
            << "创建数据库连接失败：" << db.lastError().text();
        return QSqlDatabase();
    }

    // 启用外键约束
    QSqlQuery foreignKeyQuery(db);
    if (!foreignKeyQuery.exec("PRAGMA foreign_keys = ON;")) {
        qWarning() << "线程" << QThread::currentThreadId()
            << "启用外键约束失败：" << foreignKeyQuery.lastError().text();
    }

    return db;
}

// 初始化数据表结构（主线程调用）
bool DatabaseManager::initDatabaseSchema() {
    QSqlDatabase db = getThreadDatabase();
    if (!db.isOpen()) {
        qCritical() << "数据库初始化失败：连接未打开";
        return false;
    }

    // 创建任务表
    QSqlQuery taskQuery(db);
    QString taskSql = R"(
        CREATE TABLE IF NOT EXISTS crawler_tasks (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            url TEXT NOT NULL,
            interval INTEGER DEFAULT 5,
            rule TEXT DEFAULT ''
        )
    )";
    if (!taskQuery.exec(taskSql)) {
        qCritical() << "创建任务表失败：" << taskQuery.lastError().text();
        return false;
    }

    // 创建数据表
    QSqlQuery dataQuery(db);
    QString dataSql = R"(
        CREATE TABLE IF NOT EXISTS crawler_data (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            taskId INTEGER NOT NULL,
            content TEXT NOT NULL,
            value REAL NOT NULL,
            crawlTime TEXT NOT NULL,
            FOREIGN KEY (taskId) REFERENCES crawler_tasks(id) ON DELETE CASCADE
        )
    )";
    if (!dataQuery.exec(dataSql)) {
        qCritical() << "创建数据表失败：" << dataQuery.lastError().text();
        return false;
    }

    qInfo() << "数据库表结构初始化成功";
    return true;
}

// 保存任务（新增/更新）
bool DatabaseManager::saveCrawlerTask(const CrawlerTask& task) {
    QSqlDatabase db = getThreadDatabase();
    if (!db.isOpen()) {
        qCritical() << "保存任务失败：数据库未打开";
        return false;
    }

    QSqlQuery query(db);
    // 新增任务（ID=0）
    if (task.id == 0) {
        query.prepare(R"(
            INSERT INTO crawler_tasks (name, url, interval, rule)
            VALUES (:name, :url, :interval, :rule)
        )");
        query.bindValue(":name", task.name);
        query.bindValue(":url", task.url);
        query.bindValue(":interval", task.interval);
        query.bindValue(":rule", task.rule);
    }
    // 更新任务（ID>0）
    else {
        query.prepare(R"(
            UPDATE crawler_tasks
            SET name = :name, url = :url, interval = :interval, rule = :rule
            WHERE id = :id
        )");
        query.bindValue(":id", task.id);
        query.bindValue(":name", task.name);
        query.bindValue(":url", task.url);
        query.bindValue(":interval", task.interval);
        query.bindValue(":rule", task.rule);
    }

    if (!query.exec()) {
        qWarning() << "保存任务失败：" << query.lastError().text();
        return false;
    }

    // 新增任务返回自增ID
    if (task.id == 0) {
        qDebug() << "新增任务成功，自动生成ID：" << query.lastInsertId().toInt();
    }

    return true;
}

// 获取所有任务
QList<CrawlerTask> DatabaseManager::getAllTasks() {
    QList<CrawlerTask> tasks;
    QSqlDatabase db = getThreadDatabase();
    if (!db.isOpen()) {
        qCritical() << "获取任务列表失败：数据库未打开";
        return tasks;
    }

    QSqlQuery query("SELECT id, name, url, interval, rule FROM crawler_tasks", db);
    while (query.next()) {
        CrawlerTask task;
        task.id = query.value(0).toInt();
        task.name = query.value(1).toString();
        task.url = query.value(2).toString();
        task.interval = query.value(3).toInt();
        task.rule = query.value(4).toString();
        tasks.append(task);
    }

    return tasks;
}

// 根据ID获取任务
CrawlerTask DatabaseManager::getTaskById(int taskId) {
    CrawlerTask task;
    QSqlDatabase db = getThreadDatabase();
    if (!db.isOpen()) {
        qCritical() << "查询任务失败：数据库未打开";
        return task;
    }

    QSqlQuery query(db);
    query.prepare("SELECT id, name, url, interval, rule FROM crawler_tasks WHERE id = :id");
    query.bindValue(":id", taskId);

    if (!query.exec()) {
        qWarning() << "查询任务失败：" << query.lastError().text();
        return task;
    }

    if (query.next()) {
        task.id = query.value(0).toInt();
        task.name = query.value(1).toString();
        task.url = query.value(2).toString();
        task.interval = query.value(3).toInt();
        task.rule = query.value(4).toString();
    } else {
        qWarning() << "未找到任务ID：" << taskId;
    }

    return task;
}

// 保存爬取数据（多线程安全）
bool DatabaseManager::saveCrawlerData(const CrawlerData& data) {
    QSqlDatabase db = getThreadDatabase();
    if (!db.isOpen()) {
        qCritical() << "线程" << QThread::currentThreadId()
            << "保存数据失败：数据库未打开";
        return false;
    }

    QSqlQuery query(db);
    query.prepare(R"(
        INSERT INTO crawler_data (taskId, content, value, crawlTime)
        VALUES (:taskId, :content, :value, :crawlTime)
    )");
    query.bindValue(":taskId", data.taskId);
    query.bindValue(":content", data.content);
    query.bindValue(":value", data.value);
    query.bindValue(":crawlTime", data.crawlTime.toString("yyyy-MM-dd HH:mm:ss"));

    if (!query.exec()) {
        qCritical() << "线程" << QThread::currentThreadId()
            << "保存爬取数据失败："
            << "错误信息：" << query.lastError().text()
            << "任务ID：" << data.taskId;
        return false;
    }

    qDebug() << "线程" << QThread::currentThreadId()
             << "保存数据成功，任务ID：" << data.taskId;
    return true;
}

// 根据任务ID获取数据
QList<CrawlerData> DatabaseManager::getTaskData(int taskId) {
    QList<CrawlerData> datas;
    QSqlDatabase db = getThreadDatabase();
    if (!db.isOpen()) {
        qCritical() << "查询爬取数据失败：数据库未打开";
        return datas;
    }

    QSqlQuery query(db);
    query.prepare("SELECT taskId, content, value, crawlTime FROM crawler_data WHERE taskId = :taskId");
    query.bindValue(":taskId", taskId);

    if (!query.exec()) {
        qWarning() << "查询爬取数据失败：" << query.lastError().text();
        return datas;
    }

    while (query.next()) {
        CrawlerData data;
        data.taskId = query.value(0).toInt();
        data.content = query.value(1).toString();
        data.value = query.value(2).toDouble();
        data.crawlTime = QDateTime::fromString(query.value(3).toString(), "yyyy-MM-dd HH:mm:ss");
        datas.append(data);
    }

    return datas;
}
