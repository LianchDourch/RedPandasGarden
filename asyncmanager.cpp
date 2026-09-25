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

void AsyncTask::run()
{
    setProgression(0.0);
    emit stateChanged(getCurrentStep(), getProgression());

    run();

    setProgression(1.0);
    emit stateChanged(getCurrentStep(), getProgression());


}


AsyncTaskWorker::AsyncTaskWorker(QObject* parent)
    : QObject(parent)
{
}

void AsyncTaskWorker::enqueue(AsyncTask* task)
{
    if (!task)
        return;

    // Cette fonction doit toujours être appelée dans le worker thread.
    Q_ASSERT(thread() == QThread::currentThread());

    if (m_stopping)
    {
        task->deleteLater();
        return;
    }

    m_queue.enqueue(task);

    // Si aucune tâche n'est actuellement exécutée,
    // on démarre immédiatement.
    if (!m_currentTask)
        startNext();
}

void AsyncTaskWorker::startNext()
{
    Q_ASSERT(thread() == QThread::currentThread());

    if (m_currentTask)
        return;

    if (m_queue.isEmpty())
    {
        emit idle();
        return;
    }

    m_currentTask = m_queue.dequeue();

    AsyncTask* task = m_currentTask;

    /*
     * La task a déjà été déplacée dans notre thread par
     * AsyncTaskManager::addTask().
     *
     * On peut donc maintenant lui donner un parent.
     */
    task->setParent(this);

    /*
     * Connexion directe car task et worker sont dans le même thread.
     *
     * On utilise des lambdas afin de savoir quelle task vient
     * de terminer.
     */
    m_currentFinishedConnection =
        connect(
            task,
            &AsyncTask::finished,
            this,
            &AsyncTaskWorker::onTaskFinished,
            Qt::DirectConnection
            );

    m_currentFailedConnection =
        connect(
            task,
            &AsyncTask::failed,
            this,
            &AsyncTaskWorker::onTaskFailed,
            Qt::DirectConnection
            );

    emit taskStarted(task);

    Util::println("Running Task ", task->getName());
    task->run();
    Util::println("Task ", task->getName(), " ended");
}

void AsyncTaskWorker::onTaskFinished()
{
    Q_ASSERT(thread() == QThread::currentThread());

    if (!m_currentTask)
        return;

    AsyncTask* task = m_currentTask;

    disconnect(m_currentFinishedConnection);
    disconnect(m_currentFailedConnection);

    m_currentFinishedConnection = {};
    m_currentFailedConnection = {};

    m_currentTask = nullptr;

    emit taskFinished(task);

    /*
     * deleteLater() est parfaitement adapté ici :
     * la task appartient au worker thread.
     */
    task->deleteLater();

    /*
     * On passe à la suivante.
     */
    startNext();
}

void AsyncTaskWorker::onTaskFailed(const QString& error)
{
    Q_ASSERT(thread() == QThread::currentThread());

    if (!m_currentTask)
        return;

    AsyncTask* task = m_currentTask;

    disconnect(m_currentFinishedConnection);
    disconnect(m_currentFailedConnection);

    m_currentFinishedConnection = {};
    m_currentFailedConnection = {};

    m_currentTask = nullptr;

    emit taskFailed(task, error);

    task->deleteLater();

    startNext();
}

void AsyncTaskWorker::stop()
{
    Q_ASSERT(thread() == QThread::currentThread());

    m_stopping = true;

    /*
     * Les tâches qui n'ont pas encore commencé sont supprimées.
     */
    while (!m_queue.isEmpty())
    {
        AsyncTask* task = m_queue.dequeue();

        task->deleteLater();
    }

    /*
     * La tâche courante est laissée terminer.
     *
     * On pourrait aussi implémenter une annulation explicite,
     * mais ce n'est pas possible génériquement pour un QObject.
     */
}


AsyncTaskManager::AsyncTaskManager(QObject* parent)
    : QObject(parent)
{
    /*
     * Le worker est créé dans le thread appelant.
     *
     * Il sera ensuite déplacé dans m_thread.
     */
    m_worker = new AsyncTaskWorker();

    m_worker->moveToThread(&m_thread);

    /*
     * Quand le thread démarre, le worker est déjà dans le bon thread.
     */

    connect(
        &m_thread,
        &QThread::finished,
        m_worker,
        &QObject::deleteLater
        );

    connect(
        m_worker,
        &AsyncTaskWorker::taskStarted,
        this,
        [this](AsyncTask* task)
        {
            {
                QMutexLocker lock(&m_mutex);
                m_running = true;
            }

            emit taskStarted(task);
        },
        Qt::QueuedConnection
        );

    connect(
        m_worker,
        &AsyncTaskWorker::taskFinished,
        this,
        [this](AsyncTask* task)
        {
            {
                QMutexLocker lock(&m_mutex);

                --m_taskCount;
                m_running = false;
            }

            emit taskFinished(task);
        },
        Qt::QueuedConnection
        );

    connect(
        m_worker,
        &AsyncTaskWorker::taskFailed,
        this,
        [this](AsyncTask* task, const QString& error)
        {
            {
                QMutexLocker lock(&m_mutex);

                --m_taskCount;
                m_running = false;
            }

            emit taskFailed(task, error);
        },
        Qt::QueuedConnection
        );

    connect(
        m_worker,
        &AsyncTaskWorker::idle,
        this,
        &AsyncTaskManager::idle,
        Qt::QueuedConnection
        );

    m_thread.start();
}

AsyncTaskManager::~AsyncTaskManager()
{
    /*
     * Demande au worker de ne plus accepter de nouvelles tâches.
     */
    if (m_worker)
    {
        QMetaObject::invokeMethod(
            m_worker,
            &AsyncTaskWorker::stop,
            Qt::BlockingQueuedConnection
            );
    }

    /*
     * On attend que la tâche courante et les événements du worker
     * soient terminés.
     */
    m_thread.quit();
    m_thread.wait();

    m_worker = nullptr;
}

void AsyncTaskManager::addTask(AsyncTask* task)
{
    if (!task)
        return;

    /*
     * Très important :
     *
     * addTask() est exécuté dans le thread qui possède la task.
     *
     * On peut donc appeler moveToThread() ici.
     */
    Q_ASSERT(task->parent() == nullptr);

    QThread* currentThread = QThread::currentThread();

    if (task->thread() != currentThread)
    {
        qWarning()
        << "AsyncTaskManager::addTask(): task is not owned "
           "by the calling thread.";

        return;
    }

    /*
     * On déplace la task vers le worker.
     *
     * C'est volontairement fait AVANT d'envoyer la task au worker.
     */
    task->moveToThread(&m_thread);

    {
        QMutexLocker lock(&m_mutex);
        ++m_taskCount;
    }

    Util::println("Adding task " + task->getName());
    /*
     * enqueue() sera exécuté dans le worker thread.
     */
    QMetaObject::invokeMethod(
        m_worker,
        [worker = m_worker, task]()
        {
            Util::println("Queuing task " + task->getName());
            worker->enqueue(task);
        },
        Qt::QueuedConnection
        );
}

qsizetype AsyncTaskManager::taskCount() const
{
    QMutexLocker lock(&m_mutex);
    return m_taskCount;
}

bool AsyncTaskManager::isRunning() const
{
    QMutexLocker lock(&m_mutex);
    return m_running;
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

    return widgets.contains(manager);
}
