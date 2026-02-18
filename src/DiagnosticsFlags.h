#ifndef DIAGNOSTICSFLAGS_H
#define DIAGNOSTICSFLAGS_H

#include <QtGlobal>
#include <atomic>

namespace DiagnosticsFlags {
inline bool environmentDetailedDiagnosticsEnabled()
{
    return qEnvironmentVariableIsSet("WAVEFLUX_SEEK_DIAG")
        || qEnvironmentVariableIsSet("WAVEFLUX_DETAILED_DIAG");
}

inline bool environmentTransitionTraceEnabled()
{
    return qEnvironmentVariableIsSet("WAVEFLUX_TRANSITION_TRACE");
}

inline std::atomic_bool &detailedDiagnosticsStorage()
{
    static std::atomic_bool enabled(environmentDetailedDiagnosticsEnabled());
    return enabled;
}

inline std::atomic_bool &transitionTraceStorage()
{
    static std::atomic_bool enabled(environmentTransitionTraceEnabled()
                                    || environmentDetailedDiagnosticsEnabled());
    return enabled;
}

inline bool detailedDiagnosticsEnabled()
{
    return detailedDiagnosticsStorage().load(std::memory_order_relaxed);
}

inline bool transitionTraceEnabled()
{
    return transitionTraceStorage().load(std::memory_order_relaxed)
        || detailedDiagnosticsEnabled();
}

inline void setDetailedDiagnosticsEnabled(bool enabled)
{
    detailedDiagnosticsStorage().store(enabled, std::memory_order_relaxed);
    transitionTraceStorage().store(enabled, std::memory_order_relaxed);
}

inline void setTransitionTraceEnabled(bool enabled)
{
    transitionTraceStorage().store(enabled, std::memory_order_relaxed);
}
} // namespace DiagnosticsFlags

#endif // DIAGNOSTICSFLAGS_H
