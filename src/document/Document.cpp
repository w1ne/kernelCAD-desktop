#include "Document.h"

namespace document {

Document::Document()
    : m_timeline(std::make_unique<Timeline>())
    , m_paramStore(std::make_unique<ParameterStore>())
{}

Document::~Document() = default;

void Document::newDocument()
{
    m_name = "Untitled";
    m_modified = false;
    m_timeline = std::make_unique<Timeline>();
    m_paramStore = std::make_unique<ParameterStore>();
}

bool Document::save(const std::string& /*path*/)
{
    // TODO: serialize to .kcd (JSON + ZIP)
    m_modified = false;
    return true;
}

bool Document::load(const std::string& /*path*/)
{
    // TODO: deserialize .kcd, trigger recompute
    return true;
}

} // namespace document
