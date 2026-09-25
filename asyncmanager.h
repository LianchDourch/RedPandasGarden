#ifndef ASYNCMANAGER_H
#define ASYNCMANAGER_H

#include <QObject>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QQueue>
#include <QLabel>
#include <QProgressBar>
#include <QAbstractButton>
#include <QScrollArea>
#include <QMap>
#include <QVBoxLayout>
#include <QHBoxLayout>

#include <functional>
#include <qsemaphore.h>

#include "robject.h"
#include "rpointers.h"

class AsyncTask : public QObject, public RSafeObject
{
    Q_OBJECT

private:
    GENERAL_PROPERTY_BIGPOD(QString, name, QString(), getName, setName);
    GENERAL_PROPERTY_BIGPOD(QString, currentStep, QString(), getCurrentStep, setCurrentStep);
    GENERAL_PROPERTY_POD(double, progress, 0., getProgression, setProgression);

public:
    explicit AsyncTask(QObject* parent, const QString& name);
    ~AsyncTask() { Util::println("Deleting task ", name); }

    virtual void run() = 0;

    void start();

signals:
    void stateChanged(const QString& step, double progress);
};

class LambdaAsyncTask : public AsyncTask
{
private:
    std::function<void()> func;

public:
    LambdaAsyncTask(
        std::function<void()> func,
        QObject* parent,
        const QString& name)
        : AsyncTask(parent, name),
        func(std::move(func))
    {
    }

    void run() override
    {
        func();
    }
};

typedef rpt::SafePtr<AsyncTask> Task;


class AsyncTaskManager : public QObject, public RSafeObject
{
    Q_OBJECT

public:
    explicit AsyncTaskManager(QObject* parent = nullptr);
    ~AsyncTaskManager() override;

    void addTask(AsyncTask* task);
    void addTask(const QString& name, std::function<void()> func);
    template<typename F>
    auto addTaskAndWait(const QString& name, F&& func)
    {
        using Result = std::invoke_result_t<F>;

        QSemaphore semaphore(0);
        std::optional<Result> result;

        addTask(name,
            [&]() {
                result.emplace(std::invoke(std::forward<F>(func)));
                semaphore.release();
            }
            );

        semaphore.acquire();

        return std::move(*result);
    }
    void stop();
    bool isBusy() const;

    QThread* getWorkerThread() { return &m_thread; }
signals:
    void taskStarted(rpt::SafePtr<AsyncTask> task);
    void taskFinished(rpt::SafePtr<AsyncTask> task);
    void taskFailed(rpt::SafePtr<AsyncTask> task, const QString& error);

private:
    class Worker;

    QThread m_thread;
    Worker* m_worker = nullptr;

};

class AsyncTaskWidget : public QAbstractButton
{
    Q_OBJECT

private:
    GENERAL_PROPERTY_BIGPOD(
        Task,
        task,
        nullptr,
        getTask,
        overrideTask
        )

    QLabel* labelTaskName = nullptr;
    QLabel* labelStepName = nullptr;
    QProgressBar* progressBar = nullptr;

public:
    explicit AsyncTaskWidget(Task task);

    inline void setTask(Task task)
    {
        overrideTask(task);

        if (!task.isNull()) {
            labelTaskName->setText(task->getName());
            refresh();
        }
    }

    void refresh();

protected:
    void paintEvent(QPaintEvent* event) override;
};

class AsyncTaskListWidget : public QWidget
{
    Q_OBJECT

private:
    QList<rpt::SafePtr<AsyncTaskManager>> tasks;

    QScrollArea* workspace = nullptr;
    QWidget* viewport = nullptr;

    QMap<rpt::SafePtr<AsyncTaskManager>, AsyncTaskWidget*> widgets;

public:
    explicit AsyncTaskListWidget(QWidget* parent = nullptr);

    void connect(rpt::SafePtr<AsyncTaskManager> manager);
    bool contains(rpt::SafePtr<AsyncTaskManager> manager);

    QList<rpt::SafePtr<AsyncTaskManager>> getLinkedManagers() { return tasks; }
};

Q_DECLARE_METATYPE(Task)

#endif
