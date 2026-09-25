#include "asyncmanager.h"

#include <QMutexLocker>

AsyncTask::AsyncTask(
    QObject* parent,
    const QString& name)
    : QObject(parent),
    name(name),
    currentStep(QStringLiteral("Starting..."))
{
}

void AsyncTask::start()
{
    setProgression(0.0);
    emit stateChanged(getCurrentStep(), getProgression());

    run();

    setProgression(1.0);
    emit stateChanged(getCurrentStep(), getProgression());


}

class AsyncTaskManager::Worker : public QObject
{
    Q_OBJECT

public:
    explicit Worker(QObject* parent = nullptr)
        : QObject(parent)
    {
    }


        ~Worker() override
    {
        while (!m_queue.isEmpty()) {
            m_queue.dequeue()->getReference().destroy();
        }
    }

    void addTask(rpt::SafePtr<AsyncTask> task)
    {
        if (task.isNull())
            return;

        try {
            m_busy.store(true);

            task->start();

            emit taskFinished(task);

            // if (!task.isNull()) task->deleteLater();
        }
        catch (const std::exception& e) {
            emit taskFailed(
                task,
                QString::fromUtf8(e.what())
                );
        }
        catch (...) {
            emit taskFailed(
                task,
                QStringLiteral("Unknown exception")
                );
        }

        m_busy.store(false);
    }

    bool isBusy() const
    {
        return m_busy;
    }
/*
    void process()
    {
        for (;;) {
            AsyncTask* task = nullptr;

            {
                QMutexLocker locker(&m_mutex);

                while (m_queue.isEmpty() && !m_stopping) {
                    m_waitCondition.wait(&m_mutex);
                }

                if (m_stopping && m_queue.isEmpty()) {
                    m_busy = false;
                    return;
                }

                task = m_queue.dequeue();
            }

            if (!task)
                continue;

            emit taskStarted(task->getReference());
            Util::println("Task Started: ", task->getName());

            try {
                task->start();
                emit taskFinished(task->getReference());
                Util::println("Task finished: ", task->getName());
            }
            catch (const std::exception& e) {
                emit taskFailed(
                    task->getReference(),
                    QString::fromUtf8(e.what())
                    );
            }
            catch (...) {
                emit taskFailed(
                    task->getReference(),
                    QStringLiteral("Unknown exception")
                    );
            }

            delete task;

            {
                QMutexLocker locker(&m_mutex);

                if (m_queue.isEmpty()) {
                    m_busy = false;
                }
            }
        }
    }
*/
signals:
    void taskStarted(rpt::SafePtr<AsyncTask> task);
    void taskFinished(rpt::SafePtr<AsyncTask> task);
    void taskFailed(rpt::SafePtr<AsyncTask> task, const QString& error);

private:/*
    mutable QMutex m_mutex;
    QWaitCondition m_waitCondition;*/
    QQueue<AsyncTask*> m_queue;

    bool m_stopping = false;
    std::atomic_bool m_busy = false;

};

AsyncTaskManager::AsyncTaskManager(QObject* parent)
    : QObject(parent)
{
    m_worker = new Worker();


        m_worker->moveToThread(&m_thread);

    connect(
        m_worker,
        &Worker::taskStarted,
        this,
        &AsyncTaskManager::taskStarted,
        Qt::QueuedConnection
        );

    connect(
        m_worker,
        &Worker::taskFinished,
        this,
        &AsyncTaskManager::taskFinished,
        Qt::QueuedConnection
        );

    connect(
        m_worker,
        &Worker::taskFailed,
        this,
        &AsyncTaskManager::taskFailed,
        Qt::QueuedConnection
        );

    connect(
        &m_thread,
        &QThread::finished,
        m_worker,
        &QObject::deleteLater
        );

    m_thread.start();


}

AsyncTaskManager::~AsyncTaskManager()
{
    stop();


        m_thread.quit();
    m_thread.wait();

    m_worker = nullptr;


}

void AsyncTaskManager::addTask(AsyncTask* task)
{
    if (!task || !m_worker)
        return;

    QMetaObject::invokeMethod(
        m_worker,
        [this, task]() {
            Util::println("Queueing Task ", task->getName());
            m_worker->addTask(task->getReference());
        },
        Qt::QueuedConnection
        );

    Util::println("Adding task ", task->getName());
}

void AsyncTaskManager::addTask(const QString &name, std::function<void()> func) {
    addTask(new LambdaAsyncTask(func, nullptr, name));
}

void AsyncTaskManager::stop()
{
    if (!m_worker)
        return;

}

bool AsyncTaskManager::isBusy() const
{
    if (!m_worker)
        return false;

    return m_worker->isBusy();

}

AsyncTaskWidget::AsyncTaskWidget(Task task)
    : task(task)
{
    auto* layout = new QHBoxLayout(this);


        layout->setContentsMargins(1, 1, 1, 1);

    labelTaskName = new QLabel(this);
    layout->addWidget(labelTaskName);

    labelTaskName->setText(
        task.isNull() ? QString() : task->getName()
        );

    progressBar = new QProgressBar(this);
    layout->addWidget(progressBar);

    progressBar->setMinimum(0);
    progressBar->setMaximum(1000);

    labelStepName = new QLabel(this);
    layout->addWidget(labelStepName);

    refresh();

    if (!task.isNull()) {
        QObject::connect(
            task.rawPtr(),
            &AsyncTask::stateChanged,
            this,
            [this](
                const QString& step,
                double progress)
            {
                Q_UNUSED(step);
                Q_UNUSED(progress);
                refresh();
            }
            );
    }


}

void AsyncTaskWidget::refresh()
{
    if (task.isNull()) {
        progressBar->setValue(1000);
        labelStepName->setText(QStringLiteral("Task killed"));
        return;
    }


        progressBar->setValue(
            static_cast<int>(
                1000.0 * task->getProgression()
                )
            );

    labelStepName->setText(
        task->getCurrentStep()
        );


}

void AsyncTaskWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
}

AsyncTaskListWidget::AsyncTaskListWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* mainLayout = new QVBoxLayout(this);


        mainLayout->setContentsMargins(1, 1, 1, 1);

    mainLayout->addWidget(
        new QLabel(QStringLiteral("Running Tasks"))
        );

    workspace = new QScrollArea(this);
    mainLayout->addWidget(workspace);

    workspace->setWidgetResizable(true);

    viewport = new QWidget();
    workspace->setWidget(viewport);

    new QVBoxLayout(viewport);


}

void AsyncTaskListWidget::connect(rpt::SafePtr<AsyncTaskManager> manager)
{
    if (!manager || contains(manager))
        return;


        auto* wdg = new AsyncTaskWidget(nullptr);

    viewport->layout()->addWidget(wdg);


}

bool AsyncTaskListWidget::contains(rpt::SafePtr<AsyncTaskManager> manager)
{
    if (!manager)
        return false;

    return widgets.contains(manager->getReference());
}

#include "asyncmanager.moc"
