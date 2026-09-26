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

public slots:
    /**
     * Appelé dans le thread du AsyncTaskManager.
     *
     * La tâche doit effectuer son travail ici, ou déclencher
     * son travail de manière asynchrone.
     *
     * IMPORTANT :
     * run() doit appeler finish() lorsqu'elle est réellement terminée.
     */
    virtual void run() = 0;

signals:
    void finished();
    void failed(const QString& error);
    void stateChanged(const QString& name, double progression);

protected:
    void finish()
    {
        emit finished();
    }

    void fail(const QString& error)
    {
        emit failed(error);
    }
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
        finish();
    }
};

typedef rpt::SafePtr<AsyncTask> Task;

class AsyncTaskWorker : public QObject
{
    Q_OBJECT

public:
    explicit AsyncTaskWorker(QObject* parent = nullptr);

public slots:
    /**
     * Ajoute une task depuis le thread du worker.
     */
    void enqueue(AsyncTask* task);

    /**
     * Demande l'arrêt du worker.
     */
    void stop();

signals:
    void taskStarted(AsyncTask* task);
    void taskFinished(AsyncTask* task);
    void taskFailed(AsyncTask* task, const QString& error);

    void idle();

private slots:
    void startNext();
    void onTaskFinished();
    void onTaskFailed(const QString& error);

private:
    Q_DISABLE_COPY(AsyncTaskWorker)

    struct TaskEntry
    {
        AsyncTask* task = nullptr;
        QMetaObject::Connection finishedConnection;
        QMetaObject::Connection failedConnection;
    };

    QQueue<AsyncTask*> m_queue;

    AsyncTask* m_currentTask = nullptr;

    QMetaObject::Connection m_currentFinishedConnection;
    QMetaObject::Connection m_currentFailedConnection;

    bool m_stopping = false;
};

class AsyncTaskManager : public QObject
{
    Q_OBJECT

public:
    explicit AsyncTaskManager(QObject* parent = nullptr);
    ~AsyncTaskManager() override;

    /**
     * Ajoute une tâche à la file.
     *
     * Cette fonction doit être appelée depuis le thread qui possède
     * actuellement la task.
     *
     * La task doit être sans parent.
     */
    void addTask(AsyncTask* task);

    void addTask(const QString &name, std::function<void()> func) {
        addTask(new LambdaAsyncTask(func, nullptr, name));
    }


    template<typename Func>
    auto addTaskAndWait(const QString& name, Func&& function)
        -> std::invoke_result_t<Func>
    {
        using ReturnType = std::invoke_result_t<Func>;

        /*
     * Appeler cette fonction depuis le worker thread provoquerait
     * un deadlock avec BlockingQueuedConnection / future.wait().
     */
        if (QThread::currentThread() == &m_thread)
        {
            throw std::logic_error(
                "AsyncTaskManager::addTaskAndWait() cannot be called "
                "from the worker thread."
                );
        }

        auto promise = std::make_shared<std::promise<ReturnType>>();
        auto future = promise->get_future();

        QMetaObject::invokeMethod(
            m_worker,
            [promise, function = std::forward<Func>(function), name]() mutable
            {
                Q_UNUSED(name);

                try
                {
                    if constexpr (std::is_void_v<ReturnType>)
                    {
                        std::invoke(function);
                        promise->set_value();
                    }
                    else
                    {
                        promise->set_value(
                            std::invoke(function)
                            );
                    }
                }
                catch (...)
                {
                    promise->set_exception(std::current_exception());
                }
            },
            Qt::QueuedConnection
            );

        /*
     * Bloque le thread appelant.
     *
     * Le worker thread continue quant à lui à traiter ses événements.
     */
        return future.get();
    }

    /**
     * Nombre de tâches actuellement connues du manager.
     */
    qsizetype taskCount() const;

    /**
     * Indique si le worker est en train d'exécuter une tâche.
     */
    bool isRunning() const;

    inline QThread* getWorkerThread() { return &m_thread; }
signals:
    void taskStarted(AsyncTask* task);
    void taskFinished(AsyncTask* task);
    void taskFailed(AsyncTask* task, const QString& error);

    void idle();

private:
    Q_DISABLE_COPY(AsyncTaskManager)

    QThread m_thread;
    AsyncTaskWorker* m_worker = nullptr;

    mutable QMutex m_mutex;
    qsizetype m_taskCount = 0;
    bool m_running = false;
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
