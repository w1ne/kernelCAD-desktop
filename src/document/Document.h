#pragma once
#include <string>
#include <memory>
#include "Timeline.h"
#include "ParameterStore.h"

namespace document {

class Document
{
public:
    Document();
    ~Document();

    bool save(const std::string& path);
    bool load(const std::string& path);
    void newDocument();

    Timeline&       timeline()       { return *m_timeline; }
    ParameterStore& parameters()     { return *m_paramStore; }

    const std::string& name() const  { return m_name; }
    bool isModified() const          { return m_modified; }
    void setModified(bool v)         { m_modified = v; }

private:
    std::string                    m_name = "Untitled";
    bool                           m_modified = false;
    std::unique_ptr<Timeline>      m_timeline;
    std::unique_ptr<ParameterStore> m_paramStore;
};

} // namespace document
