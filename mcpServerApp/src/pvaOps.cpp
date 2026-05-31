#include <sstream>
#include <stdexcept>

#include <pv/pvaClient.h>
#include <pv/json.h>

#include "pvaOps.h"

namespace pvd = epics::pvData;
namespace pvc = epics::pvaClient;

static pvc::PvaClientPtr client;

void pvaOps::init()
{
    client = pvc::PvaClient::get("pva");
}

void pvaOps::shutdown()
{
    client.reset();
}

std::string pvaOps::get(const std::vector<std::string> &pvnames, double timeout)
{
    std::ostringstream oss;
    oss << "{\"values\":[";

    for (size_t i = 0; i < pvnames.size(); i++) {
        if (i > 0) oss << ",";
        oss << "{\"pv\":\"" << pvnames[i] << "\",";

        try {
            pvc::PvaClientChannelPtr ch = client->channel(pvnames[i], "pva", timeout);
            pvc::PvaClientGetPtr getter = ch->get("field(value,alarm,timeStamp)");
            getter->get();
            pvc::PvaClientGetDataPtr data = getter->getData();
            pvd::PVStructurePtr pvStruct = data->getPVStructure();

            oss << "\"data\":";
            pvd::JSONPrintOptions opts;
            pvd::printJSON(oss, *pvStruct, opts);
        }
        catch (std::exception &e) {
            oss << "\"error\":\"" << e.what() << "\"";
        }

        oss << "}";
    }

    oss << "]}";
    return oss.str();
}

std::string pvaOps::put(const std::string &pvname, const std::string &value, double timeout)
{
    std::ostringstream oss;

    try {
        pvc::PvaClientChannelPtr ch = client->channel(pvname, "pva", timeout);

        pvc::PvaClientPutPtr putter = ch->put("field(value)");
        putter->connect();
        putter->get();

        pvc::PvaClientPutDataPtr data = putter->getData();
        pvd::PVFieldPtr fld = data->getValue();
        pvd::Type ftype = fld->getField()->getType();

        if (ftype == pvd::scalar) {
            pvd::PVScalar *sfld = static_cast<pvd::PVScalar*>(fld.get());
            sfld->putFrom(value);
        } else if (ftype == pvd::scalarArray) {
            pvd::shared_vector<std::string> arr;
            arr.push_back(value);
            pvd::shared_vector<const std::string> carr(freeze(arr));
            data->getScalarArrayValue()->putFrom(carr);
        }

        putter->put();

        oss << "{\"pv\":\"" << pvname << "\",\"status\":\"ok\"}";
    }
    catch (std::exception &e) {
        oss << "{\"pv\":\"" << pvname << "\",\"error\":\"" << e.what() << "\"}";
    }

    return oss.str();
}

std::string pvaOps::monitor(const std::vector<std::string> &pvnames, double duration)
{
    std::ostringstream oss;
    oss << "{\"updates\":[";

    int totalCount = 0;

    for (size_t i = 0; i < pvnames.size(); i++) {
        try {
            pvc::PvaClientChannelPtr ch = client->channel(pvnames[i], "pva", 5.0);
            pvc::PvaClientMonitorPtr mon = ch->monitor("field(value,alarm,timeStamp)");
            mon->connect();
            mon->start();

            while (mon->waitEvent(duration)) {
                if (totalCount > 0) oss << ",";
                oss << "{\"pv\":\"" << pvnames[i] << "\",\"data\":";

                pvc::PvaClientMonitorDataPtr data = mon->getData();
                pvd::PVStructurePtr pvStruct = data->getPVStructure();
                pvd::JSONPrintOptions opts;
                pvd::printJSON(oss, *pvStruct, opts);

                oss << "}";
                totalCount++;
                mon->releaseEvent();

                if (totalCount >= 1000) break;
            }

            mon->stop();
        }
        catch (std::exception &) {
            /* skip failed PVs */
        }
    }

    oss << "],\"count\":" << totalCount << "}";
    return oss.str();
}

std::string pvaOps::info(const std::vector<std::string> &pvnames, double timeout)
{
    std::ostringstream oss;
    oss << "{\"channels\":[";

    for (size_t i = 0; i < pvnames.size(); i++) {
        if (i > 0) oss << ",";
        oss << "{\"pv\":\"" << pvnames[i] << "\",";

        try {
            pvc::PvaClientChannelPtr ch = client->channel(pvnames[i], "pva", timeout);

            pvd::PVStructurePtr pvStruct = ch->get("field()")->getData()->getPVStructure();
            pvd::StructureConstPtr structure = pvStruct->getStructure();

            oss << "\"connected\":true,\"protocol\":\"pva\"";
            oss << ",\"type\":\"" << structure->getID() << "\"";
            oss << ",\"fieldCount\":" << structure->getNumberFields();
        }
        catch (std::exception &) {
            oss << "\"connected\":false";
        }

        oss << "}";
    }

    oss << "]}";
    return oss.str();
}
