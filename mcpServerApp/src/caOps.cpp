#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>

#include <cadef.h>
#include <epicsTime.h>
#include <alarm.h>
#include <alarmString.h>

#include "caOps.h"

static struct ca_client_context *g_caCtx = NULL;

void caOps::init()
{
    ca_context_create(ca_enable_preemptive_callback);
    g_caCtx = ca_current_context();
}

void caOps::shutdown()
{
    ca_context_destroy();
    g_caCtx = NULL;
}

static void caEnsureContext()
{
    if (g_caCtx && !ca_current_context())
        ca_attach_context(g_caCtx);
}

std::string caOps::get(const std::vector<std::string> &pvnames, double timeout)
{
    caEnsureContext();
    std::ostringstream oss;
    size_t nPvs = pvnames.size();

    std::vector<chid> chids(nPvs, NULL);

    for (size_t i = 0; i < nPvs; i++)
        ca_create_channel(pvnames[i].c_str(), NULL, NULL, CA_PRIORITY_DEFAULT, &chids[i]);

    ca_pend_io(timeout);

    oss << "{\"values\":[";

    for (size_t i = 0; i < nPvs; i++) {
        if (i > 0) oss << ",";
        oss << "{\"pv\":\"" << pvnames[i] << "\",";

        if (!chids[i] || ca_state(chids[i]) != cs_conn) {
            oss << "\"status\":\"disconnected\"}";
            continue;
        }

        long nativeType = ca_field_type(chids[i]);
        unsigned long nElems = ca_element_count(chids[i]);

        oss << "\"type\":\"" << dbf_type_to_text(nativeType) << "\",";
        oss << "\"count\":" << nElems << ",";

        struct dbr_time_double tval;
        int rc = ca_array_get(DBR_TIME_DOUBLE, 1, chids[i], &tval);
        if (rc == ECA_NORMAL) rc = ca_pend_io(timeout);

        if (rc == ECA_NORMAL) {
            char tsBuf[64];
            epicsTimeToStrftime(tsBuf, sizeof(tsBuf),
                                "%Y-%m-%d %H:%M:%S.%03f", &tval.stamp);
            oss << "\"value\":" << tval.value << ",";
            oss << "\"severity\":" << tval.severity << ",";
            oss << "\"timestamp\":\"" << tsBuf << "\"}";
        } else {
            oss << "\"error\":\"" << ca_message(rc) << "\"}";
        }
    }

    oss << "]}";

    for (size_t i = 0; i < nPvs; i++)
        if (chids[i]) ca_clear_channel(chids[i]);

    return oss.str();
}

std::string caOps::put(const std::string &pvname, const std::string &value, double timeout)
{
    caEnsureContext();
    std::ostringstream oss;
    chid ch = NULL;

    int rc = ca_create_channel(pvname.c_str(), NULL, NULL, CA_PRIORITY_DEFAULT, &ch);
    if (rc != ECA_NORMAL) {
        oss << "{\"pv\":\"" << pvname << "\",\"error\":\"" << ca_message(rc) << "\"}";
        return oss.str();
    }

    rc = ca_pend_io(timeout);
    if (rc != ECA_NORMAL || ca_state(ch) != cs_conn) {
        oss << "{\"pv\":\"" << pvname << "\",\"status\":\"disconnected\"}";
        if (ch) ca_clear_channel(ch);
        return oss.str();
    }

    rc = ca_put(DBR_STRING, ch, value.c_str());
    if (rc == ECA_NORMAL) rc = ca_pend_io(timeout);

    if (rc == ECA_NORMAL) {
        oss << "{\"pv\":\"" << pvname << "\",\"status\":\"ok\"}";
    } else {
        oss << "{\"pv\":\"" << pvname << "\",\"error\":\"" << ca_message(rc) << "\"}";
    }

    ca_clear_channel(ch);
    return oss.str();
}

struct CaMonEvent {
    std::string pvname;
    double value;
    int severity;
    char timestamp[64];
};

static void caMonCallback(struct event_handler_args args)
{
    std::vector<CaMonEvent> *events = (std::vector<CaMonEvent> *)args.usr;
    if (args.status != ECA_NORMAL || events->size() >= 1000) return;

    CaMonEvent ev;
    const char *name = ca_name(args.chid);
    ev.pvname = name ? name : "";

    if (args.type == DBR_TIME_DOUBLE && args.dbr) {
        const struct dbr_time_double *td = (const struct dbr_time_double *)args.dbr;
        ev.value = td->value;
        ev.severity = td->severity;
        epicsTimeToStrftime(ev.timestamp, sizeof(ev.timestamp),
                            "%Y-%m-%d %H:%M:%S.%03f", &td->stamp);
    } else {
        ev.value = 0.0;
        ev.severity = 0;
        ev.timestamp[0] = '\0';
    }

    events->push_back(ev);
}

std::string caOps::monitor(const std::vector<std::string> &pvnames, double duration)
{
    caEnsureContext();
    size_t nPvs = pvnames.size();
    std::vector<chid> chids(nPvs, NULL);
    std::vector<evid> evids(nPvs, NULL);
    std::vector<CaMonEvent> events;

    for (size_t i = 0; i < nPvs; i++)
        ca_create_channel(pvnames[i].c_str(), NULL, NULL, CA_PRIORITY_DEFAULT, &chids[i]);

    ca_pend_io(5.0);

    for (size_t i = 0; i < nPvs; i++) {
        if (chids[i] && ca_state(chids[i]) == cs_conn) {
            ca_create_subscription(DBR_TIME_DOUBLE, 1, chids[i],
                                   DBE_VALUE | DBE_ALARM,
                                   caMonCallback, &events, &evids[i]);
        }
    }

    ca_pend_event(duration);

    for (size_t i = 0; i < nPvs; i++)
        if (evids[i]) ca_clear_subscription(evids[i]);

    std::ostringstream oss;
    oss << "{\"updates\":[";
    for (size_t i = 0; i < events.size(); i++) {
        if (i > 0) oss << ",";
        oss << "{\"pv\":\"" << events[i].pvname << "\",";
        oss << "\"value\":" << events[i].value << ",";
        oss << "\"severity\":" << events[i].severity;
        if (events[i].timestamp[0])
            oss << ",\"timestamp\":\"" << events[i].timestamp << "\"";
        oss << "}";
    }
    oss << "],\"count\":" << events.size() << "}";

    for (size_t i = 0; i < nPvs; i++)
        if (chids[i]) ca_clear_channel(chids[i]);

    return oss.str();
}

std::string caOps::info(const std::vector<std::string> &pvnames, double timeout)
{
    caEnsureContext();
    size_t nPvs = pvnames.size();
    std::vector<chid> chids(nPvs, NULL);

    for (size_t i = 0; i < nPvs; i++)
        ca_create_channel(pvnames[i].c_str(), NULL, NULL, CA_PRIORITY_DEFAULT, &chids[i]);

    ca_pend_io(timeout);

    std::ostringstream oss;
    oss << "{\"channels\":[";

    for (size_t i = 0; i < nPvs; i++) {
        if (i > 0) oss << ",";
        oss << "{\"pv\":\"" << pvnames[i] << "\",";

        if (chids[i] && ca_state(chids[i]) == cs_conn) {
            oss << "\"connected\":true,\"protocol\":\"ca\",";
            oss << "\"type\":\"" << dbf_type_to_text(ca_field_type(chids[i])) << "\",";
            oss << "\"count\":" << ca_element_count(chids[i]) << ",";
            oss << "\"host\":\"" << ca_host_name(chids[i]) << "\",";
            oss << "\"readAccess\":" << (ca_read_access(chids[i]) ? "true" : "false") << ",";
            oss << "\"writeAccess\":" << (ca_write_access(chids[i]) ? "true" : "false");
        } else {
            oss << "\"connected\":false";
        }

        oss << "}";
    }

    oss << "]}";

    for (size_t i = 0; i < nPvs; i++)
        if (chids[i]) ca_clear_channel(chids[i]);

    return oss.str();
}
