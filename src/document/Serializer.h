#pragma once
#include <string>

namespace document {

class Document;

/// Serializes/deserializes a Document to/from a .kcd file.
/// The .kcd file is a single JSON file containing the full parametric log
/// (document name, parameters, timeline entries with feature params and
/// embedded sketch data). On load, features are reconstructed from their
/// params and recompute() rebuilds the B-Rep model.
class Serializer
{
public:
    /// Save the document to a .kcd file (JSON format).
    /// Returns true on success.
    static bool save(const Document& doc, const std::string& path);

    /// Load a document from a .kcd file.
    /// Clears the document, deserializes JSON, recreates features, and calls recompute().
    /// Returns true on success.
    static bool load(Document& doc, const std::string& path);
};

} // namespace document
