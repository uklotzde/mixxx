#include "aoide/agent.h"

#include "aoide/subsystem.h"

#include "util/logger.h"


namespace mixxx {

namespace aoide {

namespace {

const Logger kLogger("aoide Agent");

} // anonymous namespace

Agent::Agent(
        QPointer<Subsystem> subsystem,
        QObject* parent)
    : QObject(parent),
      m_subsystem(std::move(subsystem)) {
    connect(m_subsystem, &Subsystem::collectionsChanged, this, &Agent::collectionsChanged);
    connect(m_subsystem, &Subsystem::searchTracksResult, this, &Agent::searchTracksResult);
}

void Agent::collectionsChanged(int flags) {
    // Ensure that one collection is active. If no collections
    // exist then create a default one.
    switch (m_subsystem->allCollections().size()) {
    case 0:
    {
        DEBUG_ASSERT(!m_subsystem->hasActiveCollection());
        AoideCollection collection;
        collection.setName("Mixxx Collection");
        collection.setDescription("Created by Mixxx");
        m_subsystem->createCollectionAsync(collection);
        break;
    }
    case 1:
    {
        if (!m_subsystem->hasActiveCollection()) {
            m_subsystem->selectActiveCollection(
                    m_subsystem->allCollections().front().header().uid());
        }
        break;
    }
    default:
        if (!m_subsystem->hasActiveCollection()) {
            kLogger.warning()
                    << "TODO: Choose one of the available collections";
            // Simply activate the first one
            m_subsystem->selectActiveCollection(
                    m_subsystem->allCollections().front().header().uid());
        }
        break;
    }
    if ((flags & Subsystem::CollectionsChangedFlags::ACTIVE_COLLECTION)
            && m_subsystem->hasActiveCollection()) {
        kLogger.info()
                << "Active collection"
                << m_subsystem->activeCollection();
    }
}

void Agent::searchTracksResult(
        QVector<AoideTrackEntity> result) {
    // TODO: Remove this log output or the entire slot function
    for (const auto& track: qAsConst(result)) {
        kLogger.info() << track;
    }
}

} // namespace aoide

} // namespace mixxx
