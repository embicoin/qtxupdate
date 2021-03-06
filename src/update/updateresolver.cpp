#include "updateresolver.h"
#include "abstractupdatechecker.h"
#include "abstractupdatefilter.h"
#include "abstractversioncomparator.h"
#include "semverversioncomparator.h"
#include "update.h"

QTX_BEGIN_NAMESPACE


class UpdateResolverPrivate
{
public:
    UpdateResolverPrivate(UpdateResolver *q);
    virtual ~UpdateResolverPrivate();

public:
    UpdateResolver *q_ptr;
    Q_DECLARE_PUBLIC(UpdateResolver);

    QString version;
    AbstractUpdateChecker *checker;
    QList<AbstractUpdateFilter *> filters;
    AbstractVersionComparator *comparator;
    
    QString errorString;
};


UpdateResolver::UpdateResolver(QObject *parent /* = 0 */)
    : QObject(parent),
      d_ptr(new UpdateResolverPrivate(this))
{
}

UpdateResolver::~UpdateResolver()
{
    if (d_ptr) {
        delete d_ptr;
        d_ptr = 0;
    }
}

void UpdateResolver::resolve()
{
    QCoreApplication *app = QCoreApplication::instance();
    if (app) {
        resolve(app->applicationVersion());
    }
}

void UpdateResolver::resolve(const QString & version)
{
    if (!d_ptr->checker) {
        emit error(InvalidCheckerError);
        return;
    }
    
    d_ptr->version = version;
    d_ptr->checker->check();
}

Update *UpdateResolver::updateFrom(const QString & version)
{
    if (version.isEmpty()) {
        return 0;
    }
    if (!d_ptr->checker) { 
        return 0;
    }
    
    if (!d_ptr->comparator) {
        d_ptr->comparator = new SemVerVersionComparator(this);
    }
    
    // The checker is expected to build a list of available updates.  The
    // server that is queried for updates may filter this list based on
    // compatiblity information ascertained from the request (for example,
    // by inspecting a `User-Agent` header).
    QList<Update *> candidates = d_ptr->checker->updates();
    
    // Filter the list of candidates.  Filters are typically used to inspect
    // candidates, ensuring they satisfy minimum system requirements and
    // runtime compatibility.  If the server applied its own filtering, these
    // filters further refine the list of candidates based on information known
    // only by the client system.
    foreach (AbstractUpdateFilter *filter, d_ptr->filters) {
        candidates = filter->filter(candidates);
    }
    
    if (candidates.isEmpty()) {
        return 0;
    }
    
    // Candidates are assumed to be sorted in priority order.  Take the first
    // candidate (ie, the highest priority candidate) and check if it is more
    // recent than the given version.  If so, and update is available;
    // otherwise, no update is available.
    Update *update = candidates.at(0);
    int rv = d_ptr->comparator->compare(update->version(), version);
    if (rv == 1) {
        return update;
    }
    return 0;
}

void UpdateResolver::setUpdateChecker(AbstractUpdateChecker *checker)
{
    if (d_ptr->checker) {
        d_ptr->checker->disconnect(this);
        d_ptr->checker->deleteLater();
    }
    
    checker->setParent(this);
    d_ptr->checker = checker;
    connect(d_ptr->checker, SIGNAL(finished()), SLOT(onCheckerFinished()));
    connect(d_ptr->checker, SIGNAL(error(qint32)), SLOT(onCheckerError(qint32)));
}

void UpdateResolver::addUpdateFilter(AbstractUpdateFilter *filter)
{
    filter->setParent(this);
    d_ptr->filters.append(filter);
}

void UpdateResolver::setVersionComparator(AbstractVersionComparator *comparator)
{
    if (d_ptr->comparator) {
        d_ptr->comparator->deleteLater();
    }
    
    comparator->setParent(this);
    d_ptr->comparator = comparator;
}

QString UpdateResolver::errorString() const
{
    return d_ptr->errorString;
}

void UpdateResolver::setErrorString(const QString & str)
{
    d_ptr->errorString = str;
}

void UpdateResolver::onCheckerFinished()
{
    d_ptr->checker->disconnect(this);
    
    Update *update = updateFrom(d_ptr->version);
    if (update) {
        emit updateAvailable(update);
    } else {
        emit updateNotAvailable();
    }
}

void UpdateResolver::onCheckerError(qint32 code)
{
    Q_UNUSED(code);

    d_ptr->checker->disconnect(this);
    
    setErrorString(d_ptr->checker->errorString());
    emit error(UnknownCheckError);
}


UpdateResolverPrivate::UpdateResolverPrivate(UpdateResolver *q)
    : q_ptr(q),
      checker(0),
      comparator(0)
{
}

UpdateResolverPrivate::~UpdateResolverPrivate()
{
}


QTX_END_NAMESPACE
