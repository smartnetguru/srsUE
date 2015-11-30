#include "metrics_accord/parser.h"
#include "metrics_accord/tinyxml2.h"
#include <sstream>

using namespace tinyxml2;

template <typename T> std::string tostr(const T& t) {
   std::ostringstream os;
   os<<t;
   return os.str();
}


void add_status(XMLDocument *doc,
                XMLNode *n,
                std::string type,
                std::string id,
                std::string value,
                std::string range,
                std::string measure)
{
    XMLNode *status = n->InsertEndChild( doc->NewElement("status") );
    status->InsertEndChild( doc->NewElement("statusType") )->InsertEndChild( doc->NewText(type.c_str()));
    status->InsertEndChild( doc->NewElement("statusId") )->InsertEndChild( doc->NewText(id.c_str()));
    status->InsertEndChild( doc->NewElement("statusValue") )->InsertEndChild( doc->NewText(value.c_str()));
    status->InsertEndChild( doc->NewElement("statusRange") )->InsertEndChild( doc->NewText(range.c_str()));
    status->InsertEndChild( doc->NewElement("statusMeasure") )->InsertEndChild( doc->NewText(measure.c_str()));
}

int ue_status_to_xml(ue_status_t *status, char *xml, int n)
{
    XMLDocument* doc = new XMLDocument();
    doc->InsertEndChild( doc->NewDeclaration());

    XMLNode* root = doc->InsertEndChild( doc->NewElement( "root" ) );

    XMLNode* spec = root->InsertEndChild( doc->NewElement("specVersion"));
    XMLNode* maj = spec->InsertEndChild( doc->NewElement("major"));
    maj->InsertEndChild( doc->NewText("1"));
    XMLNode* min = spec->InsertEndChild( doc->NewElement("minor"));
    min->InsertEndChild( doc->NewText("0"));

    XMLNode* module = root->InsertEndChild( doc->NewElement("module"));
    XMLNode* modtype = module->InsertEndChild( doc->NewElement("moduleType"));
    modtype->InsertEndChild( doc->NewText("module"));
    XMLNode* friendly = module->InsertEndChild( doc->NewElement("friendlyName"));
    friendly->InsertEndChild( doc->NewText("LTE UE"));
    XMLNode* manu = module->InsertEndChild( doc->NewElement("manufacturer"));
    manu->InsertEndChild( doc->NewText("SRS"));
    XMLNode* manu_url = module->InsertEndChild( doc->NewElement("manufacturerURL"));
    manu_url->InsertEndChild( doc->NewText("http://www.softwareradiosystems.com"));
    XMLNode* statusList = module->InsertEndChild( doc->NewElement("statusList"));
    add_status(doc, statusList, "", "signal_power", tostr(status->signal_power), "", "");
    add_status(doc, statusList, "", "noise_power", tostr(status->noise_power), "", "");
    add_status(doc, statusList, "", "processing_latency", tostr(status->processing_latency), "", "");
    add_status(doc, statusList, "", "wrong_frames", tostr(status->wrong_frames), "", "");
    add_status(doc, statusList, "", "received_frames", tostr(status->received_frames), "", "");
    add_status(doc, statusList, "", "transmitted_frames", tostr(status->transmitted_frames), "", "");
    add_status(doc, statusList, "", "modcod", tostr(status->modcod), "", "");
    add_status(doc, statusList, "", "mabr", tostr(status->mabr), "", "");
    add_status(doc, statusList, "", "sinr", tostr(status->sinr), "", "");
    add_status(doc, statusList, "", "rsrp", tostr(status->rsrp), "", "");
    add_status(doc, statusList, "", "rsrq", tostr(status->rsrq), "", "");
    add_status(doc, statusList, "", "rssi", tostr(status->rssi), "", "");
    add_status(doc, statusList, "", "cfo", tostr(status->cfo), "", "");
    add_status(doc, statusList, "", "sfo", tostr(status->sfo), "", "");
    add_status(doc, statusList, "", "turbo_iters", tostr(status->turbo_iters), "", "");
    add_status(doc, statusList, "", "harq_retxs", tostr(status->harq_retxs), "", "");
    add_status(doc, statusList, "", "arq_retx", tostr(status->arq_retx), "", "");
    add_status(doc, statusList, "", "latency", tostr(status->latency), "", "");
    add_status(doc, statusList, "", "throughput", tostr(status->throughput), "", "");
    add_status(doc, statusList, "", "mcs", tostr(status->mcs), "", "");
    add_status(doc, statusList, "", "radio_buffer_status", tostr(status->radio_buffer_status), "", "");

//    std::stringstream ss;
//    ss << doc;
//    std::string str = ss.str();
//    if(str.length() < n)
//    {
//      strncpy(xml, str.c_str(), str.length());
//      return 0;
//    }

    XMLPrinter printer;
    doc->Print( &printer );
    if(printer.CStrSize() < n)
    {
        strncpy(xml, printer.CStr(), printer.CStrSize());
        return 0;
    }
    printf("Error - buffer not large enough to hold xml string");
    return -1;
}
