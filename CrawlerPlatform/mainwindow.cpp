#include "mainwindow.h"
#include <QHeaderView>
#include <QDebug>
#include <QDateTime>
#include <QStringList>
#include <QApplication>

// 【删除这行】Qt 6不需要显式声明using namespace QtCharts;
// using namespace QtCharts;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_taskTable(nullptr)
    , m_logText(nullptr)
    , m_dataText(nullptr)
    , m_chart(nullptr)
    , m_chartView(nullptr)
    , m_lineSeries(nullptr)
    , m_barSeries(nullptr)
    , m_chartTypeCombo(nullptr)
    , m_refreshChartBtn(nullptr)
    , m_currentChartType(0)
{
    setWindowTitle("Qt 6.10.1 爬虫监控平台（数据可视化版）");
    resize(1200, 700);
    initUI();
    initCharts();
    refreshTaskList();
    addLog("程序启动成功，数据库连接正常（Qt 6.10.1）");
}

MainWindow::~MainWindow()
{
    // 停止所有线程
    for (auto thread : m_threadMap) {
        if (thread) {
            thread->stopCrawling();
            thread->deleteLater();
        }
    }
    m_threadMap.clear();

    // Qt 6内存管理优化：手动释放图表资源
    if (m_chart) delete m_chart;
    if (m_lineSeries) delete m_lineSeries;
    if (m_barSeries) delete m_barSeries;

    addLog("程序退出，所有线程已停止");
}

void MainWindow::initUI()
{
    QWidget* centralWidget = new QWidget(this);
    QHBoxLayout* mainLayout = new QHBoxLayout(centralWidget);
    setCentralWidget(centralWidget);

    // 左侧布局
    QVBoxLayout* leftLayout = new QVBoxLayout();
    leftLayout->setSpacing(10);
    leftLayout->setContentsMargins(10, 10, 10, 10);

    // 任务列表
    m_taskTable = new QTableWidget(this);
    m_taskTable->setColumnCount(5);
    m_taskTable->setHorizontalHeaderLabels({"任务ID", "任务名称", "目标URL", "爬取间隔(秒)", "运行状态"});
    m_taskTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_taskTable->setSelectionBehavior(QTableWidget::SelectRows);
    leftLayout->addWidget(new QLabel("任务管理", this), 0, Qt::AlignCenter);
    leftLayout->addWidget(m_taskTable, 1);

    // 操作按钮
    QHBoxLayout* btnLayout = new QHBoxLayout();
    QPushButton* addBtn = new QPushButton("添加任务", this);
    QPushButton* editBtn = new QPushButton("编辑任务", this);
    QPushButton* deleteBtn = new QPushButton("删除任务", this);
    QPushButton* startBtn = new QPushButton("启动任务", this);
    QPushButton* stopBtn = new QPushButton("停止任务", this);

    connect(addBtn, &QPushButton::clicked, this, &MainWindow::onAddTaskClicked);
    connect(editBtn, &QPushButton::clicked, this, &MainWindow::onEditTaskClicked);
    connect(deleteBtn, &QPushButton::clicked, this, &MainWindow::onDeleteTaskClicked);
    connect(startBtn, &QPushButton::clicked, this, &MainWindow::onStartTaskClicked);
    connect(stopBtn, &QPushButton::clicked, this, &MainWindow::onStopTaskClicked);

    btnLayout->addWidget(addBtn);
    btnLayout->addWidget(editBtn);
    btnLayout->addWidget(deleteBtn);
    btnLayout->addWidget(startBtn);
    btnLayout->addWidget(stopBtn);
    leftLayout->addLayout(btnLayout);

    // 日志面板
    leftLayout->addWidget(new QLabel("监控日志", this), 0, Qt::AlignCenter);
    m_logText = new QTextEdit(this);
    m_logText->setReadOnly(true);
    m_logText->setStyleSheet(R"(
        QTextEdit {
            background-color: #202020;
            color: #ffffff;
            font-family: Consolas;
            font-size: 12px;
            padding: 5px;
        }
    )");
    leftLayout->addWidget(m_logText, 2);

    // 右侧布局
    QVBoxLayout* rightLayout = new QVBoxLayout();
    rightLayout->setSpacing(10);
    rightLayout->setContentsMargins(10, 10, 10, 10);

    // 数据展示面板
    rightLayout->addWidget(new QLabel("爬取数据展示", this), 0, Qt::AlignCenter);
    m_dataText = new QTextEdit(this);
    m_dataText->setReadOnly(true);
    m_dataText->setStyleSheet(R"(
        QTextEdit {
            background-color: #f8f8f8;
            color: #333333;
            font-family: Consolas;
            font-size: 12px;
            padding: 5px;
            height: 150px;
        }
    )");
    rightLayout->addWidget(m_dataText);

    // 图表控制栏
    QHBoxLayout* chartCtrlLayout = new QHBoxLayout();
    chartCtrlLayout->addWidget(new QLabel("图表类型：", this));

    m_chartTypeCombo = new QComboBox(this);
    m_chartTypeCombo->addItems({"折线图（数值走势）", "柱状图（时间维度）"});
    connect(m_chartTypeCombo, &QComboBox::currentIndexChanged, this, &MainWindow::onChartTypeChanged);
    chartCtrlLayout->addWidget(m_chartTypeCombo);

    m_refreshChartBtn = new QPushButton("刷新图表", this);
    connect(m_refreshChartBtn, &QPushButton::clicked, this, &MainWindow::refreshChart);
    chartCtrlLayout->addWidget(m_refreshChartBtn);

    chartCtrlLayout->addStretch();
    rightLayout->addLayout(chartCtrlLayout);

    // 图表视图容器（Qt 6抗锯齿优化）
    m_chartView = new QChartView(this);  // 直接使用QChartView，无需命名空间
    m_chartView->setRenderHint(QPainter::Antialiasing);
    m_chartView->setMinimumHeight(300);
    rightLayout->addWidget(m_chartView, 1);

    // 组装布局
    mainLayout->addLayout(leftLayout, 1);
    mainLayout->addLayout(rightLayout, 1);
}

// 初始化图表（Qt 6适配，移除命名空间）
void MainWindow::initCharts()
{
    // 创建折线图系列（直接使用QLineSeries）
    m_lineSeries = new QLineSeries();
    m_lineSeries->setName("爬取数值");

    // 创建柱状图系列（直接使用QBarSeries）
    m_barSeries = new QBarSeries();

    // 创建图表容器（直接使用QChart）
    m_chart = new QChart();
    m_chart->setTitle("爬取数据可视化");
    m_chart->setTitleFont(QFont("Arial", 12, QFont::Bold));
    m_chart->legend()->setVisible(true);
    m_chart->legend()->setAlignment(Qt::AlignBottom);
    m_chart->legend()->setFont(QFont("Arial", 10));

    // 默认显示折线图
    m_chart->addSeries(m_lineSeries);
    m_chartView->setChart(m_chart);

    // 初始化坐标轴（直接使用QValueAxis）
    QValueAxis* axisY = new QValueAxis();
    axisY->setTitleText("数值");
    axisY->setRange(0, 100);
    axisY->setTickCount(11);
    axisY->setLabelFormat("%.0f"); // Qt 6格式化优化

    QValueAxis* axisX = new QValueAxis();
    axisX->setTitleText("数据点序号");
    axisX->setRange(0, 10);
    axisX->setTickCount(11);

    m_chart->addAxis(axisX, Qt::AlignBottom);
    m_chart->addAxis(axisY, Qt::AlignLeft);
    m_lineSeries->attachAxis(axisX);
    m_lineSeries->attachAxis(axisY);
}

// 图表类型切换
void MainWindow::onChartTypeChanged(int index)
{
    m_currentChartType = index;
    int taskId = getSelectedTaskId();
    if (taskId > 0) {
        refreshChart();
        addLog(QString("切换图表类型：%1").arg(m_chartTypeCombo->currentText()));
    }
}

// 刷新图表
void MainWindow::refreshChart()
{
    int taskId = getSelectedTaskId();
    if (taskId <= 0) {
        QMessageBox::warning(this, "提示", "请先选中一个任务！");
        return;
    }

    // 清空原有系列
    m_chart->removeAllSeries();

    if (m_currentChartType == 0) {
        // 折线图
        updateLineChart(taskId);
        m_chart->addSeries(m_lineSeries);
    } else {
        // 柱状图
        updateBarChart(taskId);
        m_chart->addSeries(m_barSeries);
    }

    addLog(QString("刷新图表：任务ID=%1，图表类型=%2").arg(taskId).arg(m_chartTypeCombo->currentText()));
}

// 更新折线图
void MainWindow::updateLineChart(int taskId)
{
    QList<CrawlerData> datas = DatabaseManager::getTaskData(taskId);
    if (datas.isEmpty()) {
        m_lineSeries->clear();
        m_chart->setTitle("爬取数据可视化 - 暂无数据");
        return;
    }

    // 清空原有数据
    m_lineSeries->clear();

    // 填充折线图数据
    for (int i = 0; i < datas.size(); i++) {
        m_lineSeries->append(i, datas[i].value);
    }

    // 更新坐标轴范围
    QValueAxis* axisX = qobject_cast<QValueAxis*>(m_chart->axisX());
    QValueAxis* axisY = qobject_cast<QValueAxis*>(m_chart->axisY());

    if (axisX && axisY) {
        axisX->setRange(0, qMax(10, datas.size()-1));
        axisX->setTickCount(qMin(11, datas.size()));

        // 自动适配Y轴范围
        double minVal = 100, maxVal = 0;
        for (const auto& data : datas) {
            minVal = qMin(minVal, data.value);
            maxVal = qMax(maxVal, data.value);
        }
        axisY->setRange(qMax(0.0, minVal-5), qMin(100.0, maxVal+5));
    }

    // 更新图表标题
    CrawlerTask task = DatabaseManager::getTaskById(taskId);
    m_chart->setTitle(QString("爬取数据可视化 - %1（折线图）").arg(task.name));

    // 重新绑定坐标轴
    if (axisX && axisY) {
        m_lineSeries->attachAxis(axisX);
        m_lineSeries->attachAxis(axisY);
    }
}

// 更新柱状图
void MainWindow::updateBarChart(int taskId)
{
    QList<CrawlerData> datas = DatabaseManager::getTaskData(taskId);
    if (datas.isEmpty()) {
        m_barSeries->clear();
        m_chart->setTitle("爬取数据可视化 - 暂无数据");
        return;
    }

    // 清空原有数据
    qDeleteAll(m_barSeries->barSets());
    m_barSeries->clear();

    // 创建柱状图数据集合（直接使用QBarSet）
    QBarSet* barSet = new QBarSet("数值");
    QStringList categories;

    // 最多显示10个最新数据点
    int startIndex = qMax(0, datas.size()-10);
    for (int i = startIndex; i < datas.size(); i++) {
        barSet->append(datas[i].value);
        categories.append(datas[i].crawlTime.toString("HH:mm:ss"));
    }

    m_barSeries->append(barSet);

    // 更新坐标轴
    m_chart->removeAxis(m_chart->axisX());
    m_chart->removeAxis(m_chart->axisY());

    // 直接使用QBarCategoryAxis
    QBarCategoryAxis* axisX = new QBarCategoryAxis();
    axisX->setTitleText("爬取时间");
    axisX->append(categories);
    axisX->setLabelsAngle(-45); // Qt 6标签旋转优化，避免重叠

    QValueAxis* axisY = new QValueAxis();
    axisY->setTitleText("数值");
    axisY->setRange(0, 100);
    axisY->setTickCount(11);
    axisY->setLabelFormat("%.0f");

    m_chart->addAxis(axisX, Qt::AlignBottom);
    m_chart->addAxis(axisY, Qt::AlignLeft);
    m_barSeries->attachAxis(axisX);
    m_barSeries->attachAxis(axisY);

    // 更新图表标题
    CrawlerTask task = DatabaseManager::getTaskById(taskId);
    m_chart->setTitle(QString("爬取数据可视化 - %1（柱状图）").arg(task.name));
}

// 以下剩余函数（addLog、getSelectedTaskId、refreshTaskList等）完全不变
void MainWindow::addLog(const QString& text)
{
    QString log = QString("[%1] %2").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss")).arg(text);
    m_logText->append(log);
    QTextCursor cursor = m_logText->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_logText->setTextCursor(cursor);
}

int MainWindow::getSelectedTaskId()
{
    int row = m_taskTable->currentRow();
    if (row < 0) {
        return -1;
    }
    return m_taskTable->item(row, 0)->text().toInt();
}

void MainWindow::refreshTaskList()
{
    m_taskTable->setRowCount(0);
    QList<CrawlerTask> tasks = DatabaseManager::getAllTasks();

    for (const auto& task : tasks) {
        int row = m_taskTable->rowCount();
        m_taskTable->insertRow(row);

        m_taskTable->setItem(row, 0, new QTableWidgetItem(QString::number(task.id)));
        m_taskTable->setItem(row, 1, new QTableWidgetItem(task.name));
        m_taskTable->setItem(row, 2, new QTableWidgetItem(task.url));
        m_taskTable->setItem(row, 3, new QTableWidgetItem(QString::number(task.interval)));

        QString status = "已停止";
        if (m_threadMap.contains(task.id) && m_threadMap[task.id]->isRunning()) {
            status = "运行中";
        }
        m_taskTable->setItem(row, 4, new QTableWidgetItem(status));
    }
}

void MainWindow::showTaskData(int taskId)
{
    m_dataText->clear();
    QList<CrawlerData> datas = DatabaseManager::getTaskData(taskId);

    if (datas.isEmpty()) {
        m_dataText->append("暂无爬取数据，请启动任务...");
        m_chart->removeAllSeries();
        m_chart->setTitle("爬取数据可视化 - 暂无数据");
        return;
    }

    // 获取任务名称
    CrawlerTask task = DatabaseManager::getTaskById(taskId);
    m_dataText->append(QString("===== %1 (ID:%2) 爬取数据 =====\n").arg(task.name).arg(taskId));
    m_dataText->append("爬取时间\t\t\t数值");
    m_dataText->append("-------------------------------");

    // 只显示最新10条数据
    int startIndex = qMax(0, datas.size()-10);
    for (int i = startIndex; i < datas.size(); i++) {
        const auto& data = datas[i];
        QString line = QString("%1\t%2").arg(data.crawlTime.toString("yyyy-MM-dd HH:mm:ss")).arg(data.value, 0, 'f', 1);
        m_dataText->append(line);
    }

    // 同步刷新图表
    refreshChart();
}

void MainWindow::onAddTaskClicked()
{
    bool ok1, ok2, ok3;
    QString name = QInputDialog::getText(this, "添加任务", "任务名称：", QLineEdit::Normal, "价格监控任务", &ok1);
    if (!ok1 || name.isEmpty()) return;

    QString url = QInputDialog::getText(this, "添加任务", "目标URL：", QLineEdit::Normal, "https://httpbin.org/get", &ok2);
    if (!ok2 || url.isEmpty()) return;

    int interval = QInputDialog::getInt(this, "添加任务", "爬取间隔(秒)：", 5, 1, 3600, 1, &ok3);
    if (!ok3) return;

    CrawlerTask task;
    task.id = 0;
    task.name = name;
    task.url = url;
    task.interval = interval;

    bool saveOk = DatabaseManager::saveCrawlerTask(task);
    if (saveOk) {
        addLog(QString("添加任务成功：%1").arg(name));
        refreshTaskList();
    } else {
        QMessageBox::critical(this, "错误", "添加任务失败！");
    }
}

void MainWindow::onEditTaskClicked()
{
    int taskId = getSelectedTaskId();
    if (taskId < 0) {
        QMessageBox::warning(this, "提示", "请先选中一个任务！");
        return;
    }

    CrawlerTask task = DatabaseManager::getTaskById(taskId);
    if (task.id == 0) {
        QMessageBox::warning(this, "提示", "任务不存在！");
        return;
    }

    bool ok1, ok2, ok3;
    QString name = QInputDialog::getText(this, "编辑任务", "任务名称：", QLineEdit::Normal, task.name, &ok1);
    if (!ok1 || name.isEmpty()) return;

    QString url = QInputDialog::getText(this, "编辑任务", "目标URL：", QLineEdit::Normal, task.url, &ok2);
    if (!ok2 || url.isEmpty()) return;

    int interval = QInputDialog::getInt(this, "编辑任务", "爬取间隔(秒)：", task.interval, 1, 3600, 1, &ok3);
    if (!ok3) return;

    task.name = name;
    task.url = url;
    task.interval = interval;

    bool saveOk = DatabaseManager::saveCrawlerTask(task);
    if (saveOk) {
        addLog(QString("编辑任务成功：ID=%1").arg(taskId));
        refreshTaskList();
    } else {
        QMessageBox::critical(this, "错误", "编辑任务失败！");
    }
}

void MainWindow::onDeleteTaskClicked()
{
    int taskId = getSelectedTaskId();
    if (taskId < 0) {
        QMessageBox::warning(this, "提示", "请先选中一个任务！");
        return;
    }

    int ret = QMessageBox::question(this, "确认", QString("是否删除任务ID=%1？").arg(taskId),
                                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ret != QMessageBox::Yes) return;

    if (m_threadMap.contains(taskId)) {
        m_threadMap[taskId]->stopCrawling();
        m_threadMap[taskId]->deleteLater();
        m_threadMap.remove(taskId);
    }

    addLog(QString("删除任务成功：ID=%1").arg(taskId));
    refreshTaskList();
    m_dataText->clear();
    m_dataText->append("暂无爬取数据，请启动任务...");

    m_chart->removeAllSeries();
    m_chart->setTitle("爬取数据可视化 - 暂无数据");
}

void MainWindow::onStartTaskClicked()
{
    int taskId = getSelectedTaskId();
    if (taskId <= 0) {
        QMessageBox::warning(this, "提示", "请先选中一个有效任务！");
        return;
    }

    if (m_threadMap.contains(taskId)) {
        m_threadMap[taskId]->startCrawling();
    } else {
        CrawlerThread* thread = new CrawlerThread(taskId, this);
        connect(thread, &CrawlerThread::statusUpdated, this, &MainWindow::onTaskStatusUpdated);
        connect(thread, &CrawlerThread::dataCrawled, this, &MainWindow::onTaskDataCrawled);
        connect(thread, &CrawlerThread::logMessage, this, &MainWindow::onTaskLogMessage);
        m_threadMap[taskId] = thread;
        thread->startCrawling();
    }

    addLog(QString("启动任务：ID=%1").arg(taskId));
    refreshTaskList();
    showTaskData(taskId);
}

void MainWindow::onStopTaskClicked()
{
    int taskId = getSelectedTaskId();
    if (taskId <= 0) {
        QMessageBox::warning(this, "提示", "请先选中一个有效任务！");
        return;
    }

    if (m_threadMap.contains(taskId)) {
        m_threadMap[taskId]->stopCrawling();
    } else {
        QMessageBox::information(this, "提示", "任务未运行！");
        return;
    }

    addLog(QString("停止任务：ID=%1").arg(taskId));
    refreshTaskList();
}

void MainWindow::onTaskStatusUpdated(int taskId, const QString& status)
{
    Q_UNUSED(status);
    refreshTaskList();
}

void MainWindow::onTaskDataCrawled(int taskId, const CrawlerData& data)
{
    Q_UNUSED(data);
    if (getSelectedTaskId() == taskId) {
        showTaskData(taskId);
    }
}

void MainWindow::onTaskLogMessage(const QString& message)
{
    addLog(message);
}
