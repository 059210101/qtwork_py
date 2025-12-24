#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTextEdit>
#include <QTableWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QInputDialog>
#include <QLabel>
#include <QDateTime>
#include <QTextCursor>
#include <QMap>
#include <QComboBox>
#include <QPainter>

// Qt 6 QtCharts头文件（兼容写法）
#include <QtCharts/QChart>
#include <QtCharts/QLineSeries>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QValueAxis>
#include <QtCharts/QCategoryAxis>
#include <QtCharts/QChartView>
#include <QtCharts/QBarCategoryAxis>

#include "crawlerthread.h"

// 【删除这行】Qt 6不需要这个宏
// QT_CHARTS_USE_NAMESPACE  // Qt 5需要，Qt 6可删除

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void onAddTaskClicked();
    void onEditTaskClicked();
    void onDeleteTaskClicked();
    void onStartTaskClicked();
    void onStopTaskClicked();
    void onTaskStatusUpdated(int taskId, const QString& status);
    void onTaskDataCrawled(int taskId, const CrawlerData& data);
    void onTaskLogMessage(const QString& message);
    void onChartTypeChanged(int index);
    void refreshChart();

private:
    void initUI();
    void refreshTaskList();
    void showTaskData(int taskId);
    int getSelectedTaskId();
    void addLog(const QString& text);

    void initCharts();
    void updateLineChart(int taskId);
    void updateBarChart(int taskId);

    QTableWidget* m_taskTable;
    QTextEdit* m_logText;
    QTextEdit* m_dataText;
    QMap<int, CrawlerThread*> m_threadMap;

    // 图表相关成员（直接使用类名，无需命名空间）
    QChart* m_chart;
    QChartView* m_chartView;
    QLineSeries* m_lineSeries;
    QBarSeries* m_barSeries;
    QComboBox* m_chartTypeCombo;
    QPushButton* m_refreshChartBtn;
    int m_currentChartType; // 0-折线图，1-柱状图
};

#endif // MAINWINDOW_H
