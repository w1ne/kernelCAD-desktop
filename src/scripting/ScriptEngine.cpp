#include "ScriptEngine.h"
#include "../document/Document.h"
#include "../kernel/OCCTKernel.h"
#include "../kernel/BRepModel.h"
#include "../document/JsonReader.h"
#include "../document/JsonWriter.h"
#include "../sketch/SketchConstraint.h"
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <functional>
#include <unordered_map>

using document::JsonValue;
using document::JsonReader;
using document::JsonWriter;

namespace scripting {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static std::string featureTypeStr(features::FeatureType t)
{
    switch (t) {
    case features::FeatureType::Extrude:            return "Extrude";
    case features::FeatureType::Revolve:            return "Revolve";
    case features::FeatureType::Fillet:             return "Fillet";
    case features::FeatureType::Chamfer:            return "Chamfer";
    case features::FeatureType::Shell:              return "Shell";
    case features::FeatureType::Loft:               return "Loft";
    case features::FeatureType::Sweep:              return "Sweep";
    case features::FeatureType::Mirror:             return "Mirror";
    case features::FeatureType::RectangularPattern: return "RectangularPattern";
    case features::FeatureType::CircularPattern:    return "CircularPattern";
    case features::FeatureType::Sketch:             return "Sketch";
    case features::FeatureType::Hole:               return "Hole";
    case features::FeatureType::Combine:            return "Combine";
    case features::FeatureType::SplitBody:          return "SplitBody";
    case features::FeatureType::OffsetFaces:        return "OffsetFaces";
    case features::FeatureType::Move:               return "Move";
    case features::FeatureType::Draft:              return "Draft";
    case features::FeatureType::Thicken:            return "Thicken";
    case features::FeatureType::Thread:             return "Thread";
    case features::FeatureType::Scale:              return "Scale";
    case features::FeatureType::PathPattern:        return "PathPattern";
    case features::FeatureType::Coil:               return "Coil";
    case features::FeatureType::DeleteFace:         return "DeleteFace";
    case features::FeatureType::ReplaceFace:        return "ReplaceFace";
    case features::FeatureType::ReverseNormal:      return "ReverseNormal";
    case features::FeatureType::Joint:              return "Joint";
    case features::FeatureType::ConstructionPlane:  return "ConstructionPlane";
    case features::FeatureType::ConstructionAxis:   return "ConstructionAxis";
    case features::FeatureType::ConstructionPoint:  return "ConstructionPoint";
    case features::FeatureType::Stitch:             return "Stitch";
    case features::FeatureType::SplitFace:          return "SplitFace";
    case features::FeatureType::Patch:              return "Patch";
    case features::FeatureType::Rib:                return "Rib";
    case features::FeatureType::Web:                return "Web";
    default:                                        return "Unknown";
    }
}

static sketch::ConstraintType constraintTypeFromStr(const std::string& s)
{
    if (s == "Coincident")        return sketch::ConstraintType::Coincident;
    if (s == "PointOnLine")       return sketch::ConstraintType::PointOnLine;
    if (s == "PointOnCircle")     return sketch::ConstraintType::PointOnCircle;
    if (s == "Distance")          return sketch::ConstraintType::Distance;
    if (s == "DistancePointLine") return sketch::ConstraintType::DistancePointLine;
    if (s == "Horizontal")        return sketch::ConstraintType::Horizontal;
    if (s == "Vertical")          return sketch::ConstraintType::Vertical;
    if (s == "Parallel")          return sketch::ConstraintType::Parallel;
    if (s == "Perpendicular")     return sketch::ConstraintType::Perpendicular;
    if (s == "Tangent")           return sketch::ConstraintType::Tangent;
    if (s == "Equal")             return sketch::ConstraintType::Equal;
    if (s == "Symmetric")         return sketch::ConstraintType::Symmetric;
    if (s == "Midpoint")          return sketch::ConstraintType::Midpoint;
    if (s == "Concentric")        return sketch::ConstraintType::Concentric;
    if (s == "FixedAngle")        return sketch::ConstraintType::FixedAngle;
    if (s == "AngleBetween")      return sketch::ConstraintType::AngleBetween;
    if (s == "Radius")            return sketch::ConstraintType::Radius;
    if (s == "Fix")               return sketch::ConstraintType::Fix;
    throw std::runtime_error("Unknown constraint type: " + s);
}

/// Read an array of strings from a JsonValue.
static std::vector<std::string> getStringArray(const JsonValue& obj, const std::string& key)
{
    std::vector<std::string> out;
    const JsonValue* arr = obj.getArray(key);
    if (!arr) return out;
    for (auto& elem : arr->arrayVal) {
        if (elem && elem->type == JsonValue::Type::String)
            out.push_back(elem->stringVal);
    }
    return out;
}

/// Read an array of ints from a JsonValue.
static std::vector<int> getIntArray(const JsonValue& obj, const std::string& key)
{
    std::vector<int> out;
    const JsonValue* arr = obj.getArray(key);
    if (!arr) return out;
    for (auto& elem : arr->arrayVal) {
        if (elem && elem->type == JsonValue::Type::Number)
            out.push_back(static_cast<int>(elem->numberVal));
    }
    return out;
}

/// Build a success JSON response.
static std::string okResponse(int id, std::function<void(JsonWriter&)> fillResult)
{
    JsonWriter w;
    w.beginObject();
    w.writeInt("id", id);
    w.writeBool("ok", true);
    w.writeKey("result");
    w.beginObject();
    if (fillResult) fillResult(w);
    w.endObject();
    w.endObject();
    return w.result();
}

/// Build an error JSON response.
static std::string errResponse(int id, const std::string& msg)
{
    JsonWriter w;
    w.beginObject();
    w.writeInt("id", id);
    w.writeBool("ok", false);
    w.writeString("error", msg);
    w.endObject();
    return w.result();
}

// ---------------------------------------------------------------------------
// Impl
// ---------------------------------------------------------------------------

struct ScriptEngine::Impl {
    document::Document doc;
    LogCallback logCb;

    void log(const std::string& msg) {
        if (logCb) logCb(msg);
    }

    // ── Sketch lookup helper ────────────────────────────────────────────
    sketch::Sketch& resolveSketch(const std::string& sketchId) {
        auto* sf = doc.findSketch(sketchId);
        if (!sf)
            throw std::runtime_error("Sketch not found: " + sketchId);
        return sf->sketch();
    }

    // ── Export helpers ───────────────────────────────────────────────────
    TopoDS_Shape compoundAllBodies() {
        auto ids = doc.brepModel().bodyIds();
        if (ids.empty())
            throw std::runtime_error("No bodies to export");
        if (ids.size() == 1)
            return doc.brepModel().getShape(ids[0]);

        // Fuse all bodies into a compound
        TopoDS_Shape result = doc.brepModel().getShape(ids[0]);
        for (size_t i = 1; i < ids.size(); ++i) {
            result = doc.kernel().booleanUnion(result, doc.brepModel().getShape(ids[i]));
        }
        return result;
    }

    // ── Meta commands for LLM agents ─────────────────────────────────
    std::string helpCommand(int id, const std::string& about);
    std::string stateCommand(int id);

    // ── Command dispatch ────────────────────────────────────────────────
    std::string dispatch(const JsonValue& cmd);
};

// ---------------------------------------------------------------------------
// Meta commands for LLM agents
// ---------------------------------------------------------------------------

std::string ScriptEngine::Impl::helpCommand(int id, const std::string& about)
{
    // Per-command help with exact parameters
    struct CmdHelp { const char* name; const char* params; const char* returns; const char* hint; };
    static const CmdHelp cmds[] = {
        {"newDocument", "{}", "{}", "Start here. Creates an empty document."},
        {"createSketch", "{plane:\"XY\"|\"XZ\"|\"YZ\"}", "{sketchId,featureId}", "Next: add geometry with sketchAddRectangle/Line/Circle, then sketchSolve."},
        {"sketchAddPoint", "{sketchId,x,y}", "{pointId}", "Returns pointId like 'pt_1'. Use it in sketchAddLine."},
        {"sketchAddLine", "{sketchId,startPointId,endPointId}", "{lineId}", "Connects two existing points."},
        {"sketchAddRectangle", "{sketchId,x1,y1,x2,y2}", "{pointIds,lineIds}", "Creates 4 points + 4 lines. Fastest way to make a closed profile."},
        {"sketchAddCircle", "{sketchId,centerPointId,radius}", "{circleId}", "Center must be an existing point (use sketchAddPoint first)."},
        {"sketchAddConstraint", "{sketchId,type,entity1,entity2?,value?}", "{constraintId}", "Types: Coincident,Horizontal,Vertical,Distance,Radius,Parallel,Perpendicular,Equal,Fix"},
        {"sketchSolve", "{sketchId}", "{status,freeDOF}", "MUST call before extrude. status='Solved' means ready."},
        {"sketchDetectProfiles", "{sketchId}", "{profiles:[[ids]]}", "Check if sketch has closed loops for extrusion."},
        {"extrude", "{sketchId,distance,symmetric?}", "{featureId,bodyId}", "Pushes 2D sketch into 3D. Use returned bodyId for fillet/chamfer/shell."},
        {"fillet", "{bodyId,radius,edgeIds?}", "{featureId,bodyId}", "Rounds edges. Omit edgeIds to fillet ALL edges."},
        {"chamfer", "{bodyId,distance,edgeIds?}", "{featureId,bodyId}", "Bevels edges."},
        {"shell", "{bodyId,thickness}", "{featureId,bodyId}", "Hollows the body with given wall thickness."},
        {"mirror", "{bodyId,planeNormalX,Y,Z}", "{featureId,bodyId}", "Mirror about a plane through origin. Example: planeNormalX=1,Y=0,Z=0 mirrors about YZ."},
        {"circularPattern", "{bodyId,count,angle}", "{featureId,bodyId}", "Repeats body around Z axis."},
        {"hole", "{bodyId,x,y,z,dx,dy,dz,diameter,depth}", "{featureId,bodyId}", "Drill a hole at position along direction."},
        {"combine", "{targetBodyId,toolBodyId,operation}", "{featureId,bodyId}", "operation: 0=join(fuse), 1=cut(subtract), 2=intersect."},
        {"createBox", "{dx,dy,dz}", "{featureId,bodyId}", "Quick box primitive. All dims in mm."},
        {"createCylinder", "{radius,height}", "{featureId,bodyId}", "Quick cylinder."},
        {"createSphere", "{radius}", "{featureId,bodyId}", "Quick sphere."},
        {"listBodies", "{}", "{bodyIds:[{id}]}", "See what bodies exist."},
        {"listFeatures", "{}", "{features:[{id,name,type}]}", "See the feature timeline."},
        {"getProperties", "{bodyId}", "{volume,surfaceArea,mass,cogX/Y/Z,...}", "Physical properties. Volume in mm^3, mass in grams."},
        {"faceCount", "{bodyId}", "{count}", "Number of B-Rep faces."},
        {"edgeCount", "{bodyId}", "{count}", "Number of B-Rep edges."},
        {"exportStep", "{path}", "{}", "Export all bodies to STEP file."},
        {"exportStl", "{path}", "{}", "Export all bodies to STL file."},
        {"undo", "{}", "{}", "Undo last operation."},
        {"redo", "{}", "{}", "Redo."},
        {"state", "{}", "{bodies,features,sketches}", "Dump current document state — use this to understand what you have."},
        {"getMesh", "{bodyId?,deflection?}", "{vertexCount,triangleCount,bbox,sampleVertices}", "Get mesh data. Returns bounding box + 100 sample vertices for spatial reasoning."},
        {"screenshot", "{path,width?,height?}", "{stlPath,bodies:[{volume,size,cog}]}", "Export STL + return body descriptions. LLM can reason about shape from dimensions."},
        {"help", "{about?}", "{commands:[...]}", "This command. Pass about='extrude' for specific help."},
    };

    return okResponse(id, [&](JsonWriter& w) {
        if (about.empty()) {
            // List all commands
            w.beginArray("commands");
            for (const auto& c : cmds) {
                w.beginObject();
                w.writeString("cmd", c.name);
                w.writeString("params", c.params);
                w.writeString("returns", c.returns);
                w.writeString("hint", c.hint);
                w.endObject();
            }
            w.endArray();
            w.writeString("tip", "Send {\"cmd\":\"help\",\"about\":\"extrude\"} for detailed help on a specific command.");
        } else {
            // Find specific command
            bool found = false;
            for (const auto& c : cmds) {
                if (about == c.name) {
                    w.writeString("cmd", c.name);
                    w.writeString("params", c.params);
                    w.writeString("returns", c.returns);
                    w.writeString("hint", c.hint);
                    found = true;
                    break;
                }
            }
            if (!found)
                w.writeString("error", "Unknown command: " + about);
        }
    });
}

std::string ScriptEngine::Impl::stateCommand(int id)
{
    return okResponse(id, [&](JsonWriter& w) {
        // Bodies
        auto bodyIds = doc.brepModel().bodyIds();
        w.beginArray("bodies");
        for (const auto& bid : bodyIds) {
            w.beginObject();
            w.writeString("id", bid);
            auto props = doc.brepModel().getProperties(bid);
            w.writeNumber("volume", props.volume);
            w.writeNumber("faces", static_cast<double>(doc.kernel().faceCount(doc.brepModel().getShape(bid))));
            w.writeNumber("edges", static_cast<double>(doc.kernel().edgeCount(doc.brepModel().getShape(bid))));
            w.endObject();
        }
        w.endArray();

        // Features
        auto& tl = doc.timeline();
        w.beginArray("features");
        for (size_t i = 0; i < tl.count(); ++i) {
            const auto& entry = tl.entry(i);
            w.beginObject();
            w.writeString("id", entry.id);
            w.writeString("name", entry.name);
            if (entry.feature)
                w.writeString("type", std::to_string(static_cast<int>(entry.feature->type())));
            w.writeBool("suppressed", entry.isSuppressed);
            w.writeBool("rolledBack", entry.isRolledBack);
            w.endObject();
        }
        w.endArray();

        // Summary
        w.writeNumber("bodyCount", static_cast<double>(bodyIds.size()));
        w.writeNumber("featureCount", static_cast<double>(tl.count()));
        w.writeNumber("markerPosition", static_cast<double>(tl.markerPosition()));

        // Next step hint
        if (bodyIds.empty() && tl.count() == 0)
            w.writeString("hint", "Empty document. Start with createSketch or createBox.");
        else if (bodyIds.empty())
            w.writeString("hint", "Sketches exist but no bodies. Try extrude to create 3D geometry.");
        else
            w.writeString("hint", "Bodies exist. You can fillet, chamfer, shell, mirror, or exportStep.");
    });
}

// ---------------------------------------------------------------------------
// Command dispatcher
// ---------------------------------------------------------------------------

std::string ScriptEngine::Impl::dispatch(const JsonValue& cmd)
{
    std::string cmdName = cmd.getString("cmd");
    int id = cmd.getInt("id", 0);

    if (cmdName.empty())
        return errResponse(id, "Missing 'cmd' field");

    try {
        // ── Meta commands (for LLM agents) ──────────────────────────────
        if (cmdName == "help") {
            std::string about = cmd.getString("about");
            return helpCommand(id, about);
        }
        if (cmdName == "state") {
            return stateCommand(id);
        }

        // ── Document commands ───────────────────────────────────────────
        if (cmdName == "newDocument") {
            doc.newDocument();
            return okResponse(id, nullptr);
        }
        if (cmdName == "save") {
            std::string path = cmd.getString("path");
            if (path.empty()) return errResponse(id, "Missing 'path'");
            bool ok = doc.save(path);
            if (!ok) return errResponse(id, "Failed to save");
            return okResponse(id, nullptr);
        }
        if (cmdName == "load") {
            std::string path = cmd.getString("path");
            if (path.empty()) return errResponse(id, "Missing 'path'");
            bool ok = doc.load(path);
            if (!ok) return errResponse(id, "Failed to load");
            return okResponse(id, nullptr);
        }
        if (cmdName == "importStep") {
            std::string path = cmd.getString("path");
            if (path.empty()) return errResponse(id, "Missing 'path'");
            int count = doc.importFile(path);
            auto bodyIds = doc.brepModel().bodyIds();
            return okResponse(id, [&](JsonWriter& w) {
                w.writeInt("count", count);
                w.beginArray("bodyIds");
                for (auto& bid : bodyIds) {
                    w.beginObject();
                    w.writeString("id", bid);
                    w.endObject();
                }
                w.endArray();
            });
        }
        if (cmdName == "exportStep") {
            std::string path = cmd.getString("path");
            if (path.empty()) return errResponse(id, "Missing 'path'");
            TopoDS_Shape compound = compoundAllBodies();
            bool ok = doc.kernel().exportSTEP(compound, path);
            if (!ok) return errResponse(id, "STEP export failed");
            return okResponse(id, nullptr);
        }
        if (cmdName == "exportStl") {
            std::string path = cmd.getString("path");
            if (path.empty()) return errResponse(id, "Missing 'path'");
            double deflection = cmd.getNumber("deflection", 0.1);
            TopoDS_Shape compound = compoundAllBodies();
            bool ok = doc.kernel().exportSTL(compound, path, deflection);
            if (!ok) return errResponse(id, "STL export failed");
            return okResponse(id, nullptr);
        }
        if (cmdName == "undo") {
            bool ok = doc.history().undo(doc);
            if (!ok) return errResponse(id, "Nothing to undo");
            return okResponse(id, nullptr);
        }
        if (cmdName == "redo") {
            bool ok = doc.history().redo(doc);
            if (!ok) return errResponse(id, "Nothing to redo");
            return okResponse(id, nullptr);
        }

        // ── Sketch commands ─────────────────────────────────────────────
        if (cmdName == "createSketch") {
            features::SketchParams sp;
            sp.planeId = cmd.getString("plane", "XY");
            sp.originX = cmd.getNumber("originX", 0);
            sp.originY = cmd.getNumber("originY", 0);
            sp.originZ = cmd.getNumber("originZ", 0);
            std::string featureId = doc.addSketch(sp);
            return okResponse(id, [&](JsonWriter& w) {
                w.writeString("sketchId", featureId);
                w.writeString("featureId", featureId);
            });
        }
        if (cmdName == "sketchAddPoint") {
            std::string skId = cmd.getString("sketchId");
            auto& sk = resolveSketch(skId);
            double x = cmd.getNumber("x", 0);
            double y = cmd.getNumber("y", 0);
            bool fixed = cmd.getBool("fixed", false);
            std::string ptId = sk.addPoint(x, y, fixed);
            return okResponse(id, [&](JsonWriter& w) {
                w.writeString("pointId", ptId);
            });
        }
        if (cmdName == "sketchAddLine") {
            std::string skId = cmd.getString("sketchId");
            auto& sk = resolveSketch(skId);
            std::string startPtId = cmd.getString("startPointId");
            std::string endPtId = cmd.getString("endPointId");
            if (!startPtId.empty() && !endPtId.empty()) {
                bool isCon = cmd.getBool("isConstruction", false);
                std::string lineId = sk.addLine(startPtId, endPtId, isCon);
                return okResponse(id, [&](JsonWriter& w) {
                    w.writeString("lineId", lineId);
                });
            }
            // Convenience overload with coordinates
            double x1 = cmd.getNumber("x1", 0);
            double y1 = cmd.getNumber("y1", 0);
            double x2 = cmd.getNumber("x2", 0);
            double y2 = cmd.getNumber("y2", 0);
            std::string lineId = sk.addLine(x1, y1, x2, y2);
            return okResponse(id, [&](JsonWriter& w) {
                w.writeString("lineId", lineId);
            });
        }
        if (cmdName == "sketchAddCircle") {
            std::string skId = cmd.getString("sketchId");
            auto& sk = resolveSketch(skId);
            std::string centerPtId = cmd.getString("centerPointId");
            double radius = cmd.getNumber("radius", 10);
            if (!centerPtId.empty()) {
                bool isCon = cmd.getBool("isConstruction", false);
                std::string circId = sk.addCircle(centerPtId, radius, isCon);
                return okResponse(id, [&](JsonWriter& w) {
                    w.writeString("circleId", circId);
                });
            }
            double cx = cmd.getNumber("cx", 0);
            double cy = cmd.getNumber("cy", 0);
            std::string circId = sk.addCircle(cx, cy, radius);
            return okResponse(id, [&](JsonWriter& w) {
                w.writeString("circleId", circId);
            });
        }
        if (cmdName == "sketchAddArc") {
            std::string skId = cmd.getString("sketchId");
            auto& sk = resolveSketch(skId);
            std::string centerPtId = cmd.getString("centerPointId");
            std::string startPtId = cmd.getString("startPointId");
            std::string endPtId = cmd.getString("endPointId");
            double radius = cmd.getNumber("radius", 10);
            if (!centerPtId.empty() && !startPtId.empty() && !endPtId.empty()) {
                bool isCon = cmd.getBool("isConstruction", false);
                std::string arcId = sk.addArc(centerPtId, startPtId, endPtId, radius, isCon);
                return okResponse(id, [&](JsonWriter& w) {
                    w.writeString("arcId", arcId);
                });
            }
            double cx = cmd.getNumber("cx", 0);
            double cy = cmd.getNumber("cy", 0);
            double startAngle = cmd.getNumber("startAngle", 0);
            double endAngle = cmd.getNumber("endAngle", 3.14159265);
            std::string arcId = sk.addArc(cx, cy, radius, startAngle, endAngle);
            return okResponse(id, [&](JsonWriter& w) {
                w.writeString("arcId", arcId);
            });
        }
        if (cmdName == "sketchAddRectangle") {
            std::string skId = cmd.getString("sketchId");
            auto& sk = resolveSketch(skId);
            double x1 = cmd.getNumber("x1", 0);
            double y1 = cmd.getNumber("y1", 0);
            double x2 = cmd.getNumber("x2", 10);
            double y2 = cmd.getNumber("y2", 10);

            // Create 4 corner points
            std::string p0 = sk.addPoint(x1, y1);
            std::string p1 = sk.addPoint(x2, y1);
            std::string p2 = sk.addPoint(x2, y2);
            std::string p3 = sk.addPoint(x1, y2);

            // Create 4 lines forming a closed rectangle
            std::string l0 = sk.addLine(p0, p1);
            std::string l1 = sk.addLine(p1, p2);
            std::string l2 = sk.addLine(p2, p3);
            std::string l3 = sk.addLine(p3, p0);

            return okResponse(id, [&](JsonWriter& w) {
                w.beginArray("pointIds");
                for (auto& pid : {p0, p1, p2, p3}) {
                    w.beginObject();
                    w.writeString("id", pid);
                    w.endObject();
                }
                w.endArray();
                w.beginArray("lineIds");
                for (auto& lid : {l0, l1, l2, l3}) {
                    w.beginObject();
                    w.writeString("id", lid);
                    w.endObject();
                }
                w.endArray();
            });
        }
        if (cmdName == "sketchAddConstraint") {
            std::string skId = cmd.getString("sketchId");
            auto& sk = resolveSketch(skId);
            std::string typeStr = cmd.getString("type");
            sketch::ConstraintType ctype = constraintTypeFromStr(typeStr);
            std::vector<std::string> entityIds;

            // Accept entity1/entity2 shorthand or entityIds array
            if (cmd.has("entityIds")) {
                entityIds = getStringArray(cmd, "entityIds");
            } else {
                std::string e1 = cmd.getString("entity1");
                std::string e2 = cmd.getString("entity2");
                if (!e1.empty()) entityIds.push_back(e1);
                if (!e2.empty()) entityIds.push_back(e2);
            }

            double value = cmd.getNumber("value", 0);
            std::string cid = sk.addConstraint(ctype, entityIds, value);
            return okResponse(id, [&](JsonWriter& w) {
                w.writeString("constraintId", cid);
            });
        }
        if (cmdName == "sketchSolve") {
            std::string skId = cmd.getString("sketchId");
            auto& sk = resolveSketch(skId);
            auto result = sk.solve();
            const char* statusStr = "Failed";
            if (result.status == sketch::SolveStatus::Solved)
                statusStr = "Solved";
            else if (result.status == sketch::SolveStatus::OverConstrained)
                statusStr = "OverConstrained";

            return okResponse(id, [&](JsonWriter& w) {
                w.writeString("status", statusStr);
                w.writeInt("freeDOF", sk.freeDOF());
                w.writeInt("iterations", result.iterations);
                w.writeNumber("residual", result.residual);
            });
        }
        if (cmdName == "sketchDetectProfiles") {
            std::string skId = cmd.getString("sketchId");
            auto& sk = resolveSketch(skId);
            auto profiles = sk.detectProfiles();
            return okResponse(id, [&](JsonWriter& w) {
                w.beginArray("profiles");
                for (auto& profile : profiles) {
                    w.beginArrayAnon();
                    for (auto& eid : profile) {
                        w.beginObject();
                        w.writeString("id", eid);
                        w.endObject();
                    }
                    w.endArray();
                }
                w.endArray();
            });
        }

        // ── Feature commands ────────────────────────────────────────────
        if (cmdName == "extrude") {
            features::ExtrudeParams p;
            p.sketchId = cmd.getString("sketchId");
            p.profileId = cmd.getString("profileId");
            p.distanceExpr = cmd.getString("distance", "10 mm");
            std::string opStr = cmd.getString("operation", "NewBody");
            if (opStr == "Join")       p.operation = features::FeatureOperation::Join;
            else if (opStr == "Cut")   p.operation = features::FeatureOperation::Cut;
            else if (opStr == "Intersect") p.operation = features::FeatureOperation::Intersect;
            else                       p.operation = features::FeatureOperation::NewBody;
            p.targetBodyId = cmd.getString("targetBodyId");

            std::string dirStr = cmd.getString("direction", "Positive");
            if (dirStr == "Negative")    p.direction = features::ExtentDirection::Negative;
            else if (dirStr == "Symmetric") p.direction = features::ExtentDirection::Symmetric;

            p.taperAngleDeg = cmd.getNumber("taperAngle", 0);

            std::string bodyId = doc.addExtrude(p);
            // Find the feature id from the timeline (last entry)
            std::string featureId;
            if (doc.timeline().count() > 0)
                featureId = doc.timeline().entry(doc.timeline().count() - 1).id;

            return okResponse(id, [&](JsonWriter& w) {
                w.writeString("featureId", featureId);
                w.writeString("bodyId", bodyId);
            });
        }
        if (cmdName == "revolve") {
            features::RevolveParams p;
            p.sketchId = cmd.getString("sketchId");
            p.profileId = cmd.getString("profileId");
            p.angleExpr = cmd.getString("angle", "360 deg");
            std::string axisStr = cmd.getString("axis", "Y");
            if (axisStr == "X")       p.axisType = features::AxisType::XAxis;
            else if (axisStr == "Y")  p.axisType = features::AxisType::YAxis;
            else if (axisStr == "Z")  p.axisType = features::AxisType::ZAxis;
            else                      p.axisType = features::AxisType::YAxis;

            std::string opStr = cmd.getString("operation", "NewBody");
            if (opStr == "Join")       p.operation = features::FeatureOperation::Join;
            else if (opStr == "Cut")   p.operation = features::FeatureOperation::Cut;
            else if (opStr == "Intersect") p.operation = features::FeatureOperation::Intersect;

            std::string bodyId = doc.addRevolve(p);
            std::string featureId;
            if (doc.timeline().count() > 0)
                featureId = doc.timeline().entry(doc.timeline().count() - 1).id;

            return okResponse(id, [&](JsonWriter& w) {
                w.writeString("featureId", featureId);
                w.writeString("bodyId", bodyId);
            });
        }
        if (cmdName == "fillet") {
            features::FilletParams p;
            p.targetBodyId = cmd.getString("bodyId");
            p.edgeIds = getIntArray(cmd, "edgeIds");
            p.radiusExpr = cmd.getString("radius", "2 mm");

            std::string bodyId = doc.addFillet(p);
            std::string featureId;
            if (doc.timeline().count() > 0)
                featureId = doc.timeline().entry(doc.timeline().count() - 1).id;

            return okResponse(id, [&](JsonWriter& w) {
                w.writeString("featureId", featureId);
                w.writeString("bodyId", bodyId);
            });
        }
        if (cmdName == "chamfer") {
            features::ChamferParams p;
            p.targetBodyId = cmd.getString("bodyId");
            p.edgeIds = getIntArray(cmd, "edgeIds");
            p.distanceExpr = cmd.getString("distance", "1 mm");

            std::string bodyId = doc.addChamfer(p);
            std::string featureId;
            if (doc.timeline().count() > 0)
                featureId = doc.timeline().entry(doc.timeline().count() - 1).id;

            return okResponse(id, [&](JsonWriter& w) {
                w.writeString("featureId", featureId);
                w.writeString("bodyId", bodyId);
            });
        }
        if (cmdName == "shell") {
            features::ShellParams p;
            p.targetBodyId = cmd.getString("bodyId");
            p.thicknessExpr = cmd.getNumber("thickness", 2.0);
            p.removedFaceIds = getIntArray(cmd, "removedFaceIds");

            std::string bodyId = doc.addShell(p);
            std::string featureId;
            if (doc.timeline().count() > 0)
                featureId = doc.timeline().entry(doc.timeline().count() - 1).id;

            return okResponse(id, [&](JsonWriter& w) {
                w.writeString("featureId", featureId);
                w.writeString("bodyId", bodyId);
            });
        }
        if (cmdName == "mirror") {
            features::MirrorParams p;
            p.targetBodyId = cmd.getString("bodyId");
            const JsonValue* planeOrigin = cmd.getArray("planeOrigin");
            if (planeOrigin && planeOrigin->arrayVal.size() >= 3) {
                p.planeOx = planeOrigin->arrayVal[0]->numberVal;
                p.planeOy = planeOrigin->arrayVal[1]->numberVal;
                p.planeOz = planeOrigin->arrayVal[2]->numberVal;
            }
            const JsonValue* planeNormal = cmd.getArray("planeNormal");
            if (planeNormal && planeNormal->arrayVal.size() >= 3) {
                p.planeNx = planeNormal->arrayVal[0]->numberVal;
                p.planeNy = planeNormal->arrayVal[1]->numberVal;
                p.planeNz = planeNormal->arrayVal[2]->numberVal;
            }
            p.isCombine = cmd.getBool("combine", true);

            std::string bodyId = doc.addMirror(p);
            std::string featureId;
            if (doc.timeline().count() > 0)
                featureId = doc.timeline().entry(doc.timeline().count() - 1).id;

            return okResponse(id, [&](JsonWriter& w) {
                w.writeString("featureId", featureId);
                w.writeString("bodyId", bodyId);
            });
        }
        if (cmdName == "circularPattern") {
            features::CircularPatternParams p;
            p.targetBodyId = cmd.getString("bodyId");
            const JsonValue* axis = cmd.getArray("axis");
            if (axis && axis->arrayVal.size() >= 3) {
                p.axisDx = axis->arrayVal[0]->numberVal;
                p.axisDy = axis->arrayVal[1]->numberVal;
                p.axisDz = axis->arrayVal[2]->numberVal;
            }
            const JsonValue* axisOrigin = cmd.getArray("axisOrigin");
            if (axisOrigin && axisOrigin->arrayVal.size() >= 3) {
                p.axisOx = axisOrigin->arrayVal[0]->numberVal;
                p.axisOy = axisOrigin->arrayVal[1]->numberVal;
                p.axisOz = axisOrigin->arrayVal[2]->numberVal;
            }
            p.count = cmd.getInt("count", 6);
            p.totalAngleDeg = cmd.getNumber("angle", 360.0);

            std::string bodyId = doc.addCircularPattern(p);
            std::string featureId;
            if (doc.timeline().count() > 0)
                featureId = doc.timeline().entry(doc.timeline().count() - 1).id;

            return okResponse(id, [&](JsonWriter& w) {
                w.writeString("featureId", featureId);
                w.writeString("bodyId", bodyId);
            });
        }
        if (cmdName == "rectangularPattern") {
            features::RectangularPatternParams p;
            p.targetBodyId = cmd.getString("bodyId");
            const JsonValue* dir1 = cmd.getArray("dir1");
            if (dir1 && dir1->arrayVal.size() >= 3) {
                p.dir1X = dir1->arrayVal[0]->numberVal;
                p.dir1Y = dir1->arrayVal[1]->numberVal;
                p.dir1Z = dir1->arrayVal[2]->numberVal;
            }
            p.spacing1Expr = cmd.getString("spacing1", "20 mm");
            p.count1 = cmd.getInt("count1", 3);
            const JsonValue* dir2 = cmd.getArray("dir2");
            if (dir2 && dir2->arrayVal.size() >= 3) {
                p.dir2X = dir2->arrayVal[0]->numberVal;
                p.dir2Y = dir2->arrayVal[1]->numberVal;
                p.dir2Z = dir2->arrayVal[2]->numberVal;
            }
            p.spacing2Expr = cmd.getString("spacing2", "20 mm");
            p.count2 = cmd.getInt("count2", 1);

            std::string bodyId = doc.addRectangularPattern(p);
            std::string featureId;
            if (doc.timeline().count() > 0)
                featureId = doc.timeline().entry(doc.timeline().count() - 1).id;

            return okResponse(id, [&](JsonWriter& w) {
                w.writeString("featureId", featureId);
                w.writeString("bodyId", bodyId);
            });
        }
        if (cmdName == "hole") {
            features::HoleParams p;
            p.targetBodyId = cmd.getString("bodyId");
            const JsonValue* pos = cmd.getArray("position");
            if (pos && pos->arrayVal.size() >= 3) {
                p.posX = pos->arrayVal[0]->numberVal;
                p.posY = pos->arrayVal[1]->numberVal;
                p.posZ = pos->arrayVal[2]->numberVal;
            } else {
                p.posX = cmd.getNumber("posX", 0);
                p.posY = cmd.getNumber("posY", 0);
                p.posZ = cmd.getNumber("posZ", 0);
            }
            const JsonValue* dir = cmd.getArray("direction");
            if (dir && dir->arrayVal.size() >= 3) {
                p.dirX = dir->arrayVal[0]->numberVal;
                p.dirY = dir->arrayVal[1]->numberVal;
                p.dirZ = dir->arrayVal[2]->numberVal;
            } else {
                p.dirX = cmd.getNumber("dirX", 0);
                p.dirY = cmd.getNumber("dirY", 0);
                p.dirZ = cmd.getNumber("dirZ", -1);
            }
            p.diameterExpr = cmd.getString("diameter", "10 mm");
            p.depthExpr = cmd.getString("depth", "0");

            std::string bodyId = doc.addHole(p);
            std::string featureId;
            if (doc.timeline().count() > 0)
                featureId = doc.timeline().entry(doc.timeline().count() - 1).id;

            return okResponse(id, [&](JsonWriter& w) {
                w.writeString("featureId", featureId);
                w.writeString("bodyId", bodyId);
            });
        }
        if (cmdName == "combine") {
            features::CombineParams p;
            p.targetBodyId = cmd.getString("targetBodyId");
            p.toolBodyId = cmd.getString("toolBodyId");
            std::string opStr = cmd.getString("operation", "Join");
            if (opStr == "Cut")            p.operation = features::CombineOperation::Cut;
            else if (opStr == "Intersect") p.operation = features::CombineOperation::Intersect;
            else                           p.operation = features::CombineOperation::Join;
            p.keepToolBody = cmd.getBool("keepToolBody", false);

            std::string bodyId = doc.addCombine(p);
            std::string featureId;
            if (doc.timeline().count() > 0)
                featureId = doc.timeline().entry(doc.timeline().count() - 1).id;

            return okResponse(id, [&](JsonWriter& w) {
                w.writeString("featureId", featureId);
                w.writeString("bodyId", bodyId);
            });
        }

        // ── Primitive shortcuts ─────────────────────────────────────────
        if (cmdName == "createBox") {
            // Use ExtrudeFeature with empty profileId to trigger makeBox shortcut
            features::ExtrudeParams p;
            double dx = cmd.getNumber("dx", 10);
            double dy = cmd.getNumber("dy", 10);
            double dz = cmd.getNumber("dz", 10);
            // distanceExpr encodes [dx, dy, dz] as "dx" for the box shortcut
            p.distanceExpr = std::to_string(dz) + " mm";
            // makeBox reads dx, dy from distanceExpr in a special way.
            // Actually, the ExtrudeFeature::execute makeBox path uses OCCTKernel::makeBox
            // with params taken from the expression. Let's directly call the kernel.
            TopoDS_Shape box = doc.kernel().makeBox(dx, dy, dz);
            // Register as a body
            std::string bodyId = "body_" + std::to_string(doc.brepModel().bodyIds().size() + 1);
            // Use addExtrude with empty sketchId to get proper timeline entry
            p.profileId.clear();
            p.sketchId.clear();
            p.distanceExpr = std::to_string(dx) + " " + std::to_string(dy) + " " + std::to_string(dz);
            std::string resultBodyId = doc.addExtrude(p);
            std::string featureId;
            if (doc.timeline().count() > 0)
                featureId = doc.timeline().entry(doc.timeline().count() - 1).id;

            return okResponse(id, [&](JsonWriter& w) {
                w.writeString("featureId", featureId);
                w.writeString("bodyId", resultBodyId);
            });
        }
        if (cmdName == "createCylinder") {
            // Use RevolveFeature with empty profileId to trigger makeCylinder shortcut
            features::RevolveParams p;
            p.profileId.clear();
            p.sketchId.clear();
            double radius = cmd.getNumber("radius", 5);
            double height = cmd.getNumber("height", 10);
            p.angleExpr = std::to_string(radius) + " " + std::to_string(height);

            std::string bodyId = doc.addRevolve(p);
            std::string featureId;
            if (doc.timeline().count() > 0)
                featureId = doc.timeline().entry(doc.timeline().count() - 1).id;

            return okResponse(id, [&](JsonWriter& w) {
                w.writeString("featureId", featureId);
                w.writeString("bodyId", bodyId);
            });
        }
        if (cmdName == "createSphere") {
            double radius = cmd.getNumber("radius", 5);
            TopoDS_Shape sphere = doc.kernel().makeSphere(radius);
            std::string bodyId = "body_sphere_" + std::to_string(doc.brepModel().bodyIds().size() + 1);
            doc.brepModel().addBody(bodyId, sphere);
            return okResponse(id, [&](JsonWriter& w) {
                w.writeString("bodyId", bodyId);
            });
        }
        if (cmdName == "createTorus") {
            double majorR = cmd.getNumber("majorRadius", 20);
            double minorR = cmd.getNumber("minorRadius", 5);
            std::string bodyId = doc.addTorus(majorR, minorR);
            return okResponse(id, [&](JsonWriter& w) {
                w.writeString("bodyId", bodyId);
            });
        }
        if (cmdName == "createPipe") {
            double outerR = cmd.getNumber("outerRadius", 15);
            double innerR = cmd.getNumber("innerRadius", 12);
            double height = cmd.getNumber("height", 30);
            std::string bodyId = doc.addPipe(outerR, innerR, height);
            return okResponse(id, [&](JsonWriter& w) {
                w.writeString("bodyId", bodyId);
            });
        }
        if (cmdName == "stitch") {
            features::StitchParams p;
            p.targetBodyIds = getStringArray(cmd, "bodyIds");
            p.tolerance = cmd.getNumber("tolerance", 1e-3);
            std::string bodyId = doc.addStitch(p);
            return okResponse(id, [&](JsonWriter& w) {
                w.writeString("bodyId", bodyId);
            });
        }
        if (cmdName == "splitFace") {
            features::SplitFaceParams p;
            p.targetBodyId = cmd.getString("targetBodyId");
            p.faceIndex = static_cast<int>(cmd.getNumber("faceIndex", 0));
            p.sketchId = cmd.getString("sketchId", "");
            std::string bodyId = doc.addSplitFace(p);
            return okResponse(id, [&](JsonWriter& w) {
                w.writeString("bodyId", bodyId);
            });
        }
        if (cmdName == "patch") {
            features::PatchParams p;
            p.boundaryBodyId = cmd.getString("boundaryBodyId");
            std::string bodyId = doc.addPatch(p);
            return okResponse(id, [&](JsonWriter& w) {
                w.writeString("bodyId", bodyId);
            });
        }
        if (cmdName == "rib") {
            features::RibParams p;
            p.targetBodyId = cmd.getString("targetBodyId");
            p.sketchId = cmd.getString("sketchId", "");
            p.thickness = cmd.getNumber("thickness", 2.0);
            p.depth = cmd.getNumber("depth", 10.0);
            std::string bodyId = doc.addRib(p);
            return okResponse(id, [&](JsonWriter& w) {
                w.writeString("bodyId", bodyId);
            });
        }
        if (cmdName == "web") {
            features::WebParams p;
            p.targetBodyId = cmd.getString("targetBodyId");
            p.sketchId = cmd.getString("sketchId", "");
            p.thickness = cmd.getNumber("thickness", 2.0);
            p.depth = cmd.getNumber("depth", 10.0);
            p.count = static_cast<int>(cmd.getNumber("count", 3));
            p.spacing = cmd.getNumber("spacing", 10.0);
            std::string bodyId = doc.addWeb(p);
            return okResponse(id, [&](JsonWriter& w) {
                w.writeString("bodyId", bodyId);
            });
        }

        // ── Query commands ──────────────────────────────────────────────
        if (cmdName == "listBodies") {
            auto ids = doc.brepModel().bodyIds();
            return okResponse(id, [&](JsonWriter& w) {
                w.beginArray("bodyIds");
                for (auto& bid : ids) {
                    w.beginObject();
                    w.writeString("id", bid);
                    w.endObject();
                }
                w.endArray();
            });
        }
        if (cmdName == "listFeatures") {
            return okResponse(id, [&](JsonWriter& w) {
                w.beginArray("features");
                for (size_t i = 0; i < doc.timeline().count(); ++i) {
                    auto& entry = doc.timeline().entry(i);
                    w.beginObject();
                    w.writeString("id", entry.id);
                    w.writeString("name", entry.displayName());
                    w.writeString("type", entry.feature
                        ? featureTypeStr(entry.feature->type()) : "Unknown");
                    w.writeBool("isSuppressed", entry.isSuppressed);
                    w.endObject();
                }
                w.endArray();
            });
        }
        if (cmdName == "getProperties") {
            std::string bodyId = cmd.getString("bodyId");
            if (bodyId.empty()) return errResponse(id, "Missing 'bodyId'");
            if (!doc.brepModel().hasBody(bodyId))
                return errResponse(id, "Body not found: " + bodyId);

            double density = cmd.getNumber("density", 0.00785);
            auto props = doc.brepModel().getProperties(bodyId, density);
            return okResponse(id, [&](JsonWriter& w) {
                w.writeNumber("volume", props.volume);
                w.writeNumber("surfaceArea", props.surfaceArea);
                w.writeNumber("mass", props.mass);
                w.writeNumber("cogX", props.cogX);
                w.writeNumber("cogY", props.cogY);
                w.writeNumber("cogZ", props.cogZ);
                w.writeNumber("bboxMinX", props.bboxMinX);
                w.writeNumber("bboxMinY", props.bboxMinY);
                w.writeNumber("bboxMinZ", props.bboxMinZ);
                w.writeNumber("bboxMaxX", props.bboxMaxX);
                w.writeNumber("bboxMaxY", props.bboxMaxY);
                w.writeNumber("bboxMaxZ", props.bboxMaxZ);
            });
        }
        if (cmdName == "getFeatureParams") {
            std::string featureId = cmd.getString("featureId");
            if (featureId.empty()) return errResponse(id, "Missing 'featureId'");

            // Search timeline for the feature
            features::Feature* feat = nullptr;
            for (size_t i = 0; i < doc.timeline().count(); ++i) {
                if (doc.timeline().entry(i).id == featureId) {
                    feat = doc.timeline().entry(i).feature.get();
                    break;
                }
            }
            if (!feat) return errResponse(id, "Feature not found: " + featureId);

            return okResponse(id, [&](JsonWriter& w) {
                w.writeString("type", featureTypeStr(feat->type()));
                // Serialize type-specific params
                if (feat->type() == features::FeatureType::Extrude) {
                    auto& p = static_cast<features::ExtrudeFeature*>(feat)->params();
                    w.writeString("sketchId", p.sketchId);
                    w.writeString("profileId", p.profileId);
                    w.writeString("distance", p.distanceExpr);
                    w.writeString("targetBodyId", p.targetBodyId);
                } else if (feat->type() == features::FeatureType::Fillet) {
                    auto& p = static_cast<features::FilletFeature*>(feat)->params();
                    w.writeString("targetBodyId", p.targetBodyId);
                    w.writeString("radius", p.radiusExpr);
                } else if (feat->type() == features::FeatureType::Chamfer) {
                    auto& p = static_cast<features::ChamferFeature*>(feat)->params();
                    w.writeString("targetBodyId", p.targetBodyId);
                    w.writeString("distance", p.distanceExpr);
                } else if (feat->type() == features::FeatureType::Shell) {
                    auto& p = static_cast<features::ShellFeature*>(feat)->params();
                    w.writeString("targetBodyId", p.targetBodyId);
                    w.writeNumber("thickness", p.thicknessExpr);
                } else if (feat->type() == features::FeatureType::Revolve) {
                    auto& p = static_cast<features::RevolveFeature*>(feat)->params();
                    w.writeString("sketchId", p.sketchId);
                    w.writeString("profileId", p.profileId);
                    w.writeString("angle", p.angleExpr);
                } else if (feat->type() == features::FeatureType::Mirror) {
                    auto& p = static_cast<features::MirrorFeature*>(feat)->params();
                    w.writeString("targetBodyId", p.targetBodyId);
                    w.writeNumber("planeOx", p.planeOx);
                    w.writeNumber("planeOy", p.planeOy);
                    w.writeNumber("planeOz", p.planeOz);
                    w.writeNumber("planeNx", p.planeNx);
                    w.writeNumber("planeNy", p.planeNy);
                    w.writeNumber("planeNz", p.planeNz);
                } else if (feat->type() == features::FeatureType::Sketch) {
                    auto& p = static_cast<features::SketchFeature*>(feat)->params();
                    w.writeString("planeId", p.planeId);
                    w.writeNumber("originX", p.originX);
                    w.writeNumber("originY", p.originY);
                    w.writeNumber("originZ", p.originZ);
                } else if (feat->type() == features::FeatureType::Hole) {
                    auto& p = static_cast<features::HoleFeature*>(feat)->params();
                    w.writeString("targetBodyId", p.targetBodyId);
                    w.writeString("diameter", p.diameterExpr);
                    w.writeString("depth", p.depthExpr);
                } else if (feat->type() == features::FeatureType::Combine) {
                    auto& p = static_cast<features::CombineFeature*>(feat)->params();
                    w.writeString("targetBodyId", p.targetBodyId);
                    w.writeString("toolBodyId", p.toolBodyId);
                }
            });
        }
        if (cmdName == "faceCount") {
            std::string bodyId = cmd.getString("bodyId");
            if (bodyId.empty()) return errResponse(id, "Missing 'bodyId'");
            if (!doc.brepModel().hasBody(bodyId))
                return errResponse(id, "Body not found: " + bodyId);
            int count = doc.kernel().faceCount(doc.brepModel().getShape(bodyId));
            return okResponse(id, [&](JsonWriter& w) {
                w.writeInt("count", count);
            });
        }
        if (cmdName == "edgeCount") {
            std::string bodyId = cmd.getString("bodyId");
            if (bodyId.empty()) return errResponse(id, "Missing 'bodyId'");
            if (!doc.brepModel().hasBody(bodyId))
                return errResponse(id, "Body not found: " + bodyId);
            int count = doc.kernel().edgeCount(doc.brepModel().getShape(bodyId));
            return okResponse(id, [&](JsonWriter& w) {
                w.writeInt("count", count);
            });
        }

        // ── Timeline commands ───────────────────────────────────────────
        if (cmdName == "setMarker") {
            int position = cmd.getInt("position", 0);
            doc.timeline().setMarker(static_cast<size_t>(position));
            return okResponse(id, nullptr);
        }
        if (cmdName == "suppress") {
            std::string featureId = cmd.getString("featureId");
            if (featureId.empty()) return errResponse(id, "Missing 'featureId'");
            bool found = false;
            for (size_t i = 0; i < doc.timeline().count(); ++i) {
                if (doc.timeline().entry(i).id == featureId) {
                    doc.timeline().entry(i).isSuppressed =
                        !doc.timeline().entry(i).isSuppressed;
                    found = true;
                    break;
                }
            }
            if (!found) return errResponse(id, "Feature not found: " + featureId);
            return okResponse(id, nullptr);
        }
        if (cmdName == "deleteFeature") {
            std::string featureId = cmd.getString("featureId");
            if (featureId.empty()) return errResponse(id, "Missing 'featureId'");
            doc.timeline().remove(featureId);
            return okResponse(id, nullptr);
        }
        if (cmdName == "recompute") {
            doc.recompute();
            return okResponse(id, nullptr);
        }

        // ── Mesh data (for LLM vision) ────────────────────────────────
        if (cmdName == "getMesh") {
            std::string bodyId = cmd.getString("bodyId");
            if (bodyId.empty()) {
                // Use first body
                auto ids = doc.brepModel().bodyIds();
                if (ids.empty()) return errResponse(id, "No bodies exist");
                bodyId = ids[0];
            }
            if (!doc.brepModel().hasBody(bodyId))
                return errResponse(id, "Body not found: " + bodyId);

            double deflection = cmd.getNumber("deflection", 0.5);
            auto mesh = doc.kernel().tessellate(doc.brepModel().getShape(bodyId), deflection);

            return okResponse(id, [&](JsonWriter& w) {
                w.writeNumber("vertexCount", static_cast<double>(mesh.vertices.size() / 3));
                w.writeNumber("triangleCount", static_cast<double>(mesh.indices.size() / 3));

                // Bounding box from vertices
                float minX = 1e18f, minY = 1e18f, minZ = 1e18f;
                float maxX = -1e18f, maxY = -1e18f, maxZ = -1e18f;
                for (size_t i = 0; i + 2 < mesh.vertices.size(); i += 3) {
                    minX = std::min(minX, mesh.vertices[i]);
                    minY = std::min(minY, mesh.vertices[i+1]);
                    minZ = std::min(minZ, mesh.vertices[i+2]);
                    maxX = std::max(maxX, mesh.vertices[i]);
                    maxY = std::max(maxY, mesh.vertices[i+1]);
                    maxZ = std::max(maxZ, mesh.vertices[i+2]);
                }
                w.writeNumber("bboxMinX", minX); w.writeNumber("bboxMinY", minY); w.writeNumber("bboxMinZ", minZ);
                w.writeNumber("bboxMaxX", maxX); w.writeNumber("bboxMaxY", maxY); w.writeNumber("bboxMaxZ", maxZ);
                w.writeNumber("sizeX", maxX - minX); w.writeNumber("sizeY", maxY - minY); w.writeNumber("sizeZ", maxZ - minZ);

                // Sample vertices (first 100 for overview, not all)
                size_t sampleCount = std::min<size_t>(mesh.vertices.size() / 3, 100);
                size_t stride = mesh.vertices.size() / 3 / std::max<size_t>(sampleCount, 1);
                w.beginArray("sampleVertices");
                for (size_t i = 0; i < sampleCount; ++i) {
                    size_t idx = i * stride * 3;
                    if (idx + 2 >= mesh.vertices.size()) break;
                    w.beginObject();
                    w.writeNumber("x", mesh.vertices[idx]);
                    w.writeNumber("y", mesh.vertices[idx+1]);
                    w.writeNumber("z", mesh.vertices[idx+2]);
                    w.endObject();
                }
                w.endArray();
            });
        }

        // ── Screenshot (offscreen render to PNG) ────────────────────────
        if (cmdName == "screenshot") {
            std::string path = cmd.getString("path");
            if (path.empty()) return errResponse(id, "Missing 'path'. Example: {\"cmd\":\"screenshot\",\"path\":\"/tmp/preview.png\"}");

            int width = cmd.getInt("width", 800);
            int height = cmd.getInt("height", 600);

            // Use OCCT's built-in STL export + external renderer is complex.
            // Instead, export a simple OBJ file that any viewer can render,
            // or describe the geometry textually for the LLM.
            // For a true screenshot, we'd need QOffscreenSurface + QOpenGLFramebufferObject
            // which requires a QApplication (not available in CLI mode).

            // Practical approach: export STL + return mesh summary
            auto ids = doc.brepModel().bodyIds();
            if (ids.empty()) return errResponse(id, "No bodies to screenshot");

            // Export to STL at the given path (LLM can use external viewer)
            std::string stlPath = path;
            if (stlPath.find(".png") != std::string::npos)
                stlPath = stlPath.substr(0, stlPath.rfind('.')) + ".stl";

            auto compound = compoundAllBodies();
            doc.kernel().exportSTL(compound, stlPath, 0.1);

            // Return a text description the LLM can use
            return okResponse(id, [&](JsonWriter& w) {
                w.writeString("stlPath", stlPath);
                w.writeNumber("bodyCount", static_cast<double>(ids.size()));

                // Describe each body's shape and position
                w.beginArray("bodies");
                for (const auto& bid : ids) {
                    auto props = doc.brepModel().getProperties(bid);
                    w.beginObject();
                    w.writeString("id", bid);
                    w.writeNumber("volume", props.volume);
                    w.writeNumber("surfaceArea", props.surfaceArea);
                    w.writeNumber("faces", static_cast<double>(doc.kernel().faceCount(doc.brepModel().getShape(bid))));
                    w.writeNumber("cogX", props.cogX); w.writeNumber("cogY", props.cogY); w.writeNumber("cogZ", props.cogZ);
                    w.writeNumber("sizeX", props.bboxMaxX - props.bboxMinX);
                    w.writeNumber("sizeY", props.bboxMaxY - props.bboxMinY);
                    w.writeNumber("sizeZ", props.bboxMaxZ - props.bboxMinZ);
                    w.endObject();
                }
                w.endArray();
            });
        }

        return errResponse(id, "Unknown command: " + cmdName);
    }
    catch (const std::exception& ex) {
        return errResponse(id, ex.what());
    }
    catch (...) {
        return errResponse(id, "Unknown error");
    }
}

// ---------------------------------------------------------------------------
// ScriptEngine public API
// ---------------------------------------------------------------------------

ScriptEngine::ScriptEngine() : m_impl(std::make_unique<Impl>()) {}
ScriptEngine::~ScriptEngine() = default;

std::string ScriptEngine::execute(const std::string& jsonCommand)
{
    auto parsed = JsonReader::parse(jsonCommand);
    if (!parsed || parsed->type != JsonValue::Type::Object)
        return errResponse(0, "Invalid JSON");
    return m_impl->dispatch(*parsed);
}

std::string ScriptEngine::executeBatch(const std::string& jsonArray)
{
    auto parsed = JsonReader::parse(jsonArray);
    if (!parsed)
        return errResponse(0, "Invalid JSON");

    // If it's a single object, treat as single command
    if (parsed->type == JsonValue::Type::Object) {
        return m_impl->dispatch(*parsed);
    }

    if (parsed->type != JsonValue::Type::Array)
        return errResponse(0, "Expected JSON array");

    std::string result = "[";
    for (size_t i = 0; i < parsed->arrayVal.size(); ++i) {
        if (i > 0) result += ",";
        if (parsed->arrayVal[i] && parsed->arrayVal[i]->type == JsonValue::Type::Object)
            result += m_impl->dispatch(*parsed->arrayVal[i]);
        else
            result += errResponse(0, "Array element is not a JSON object");
    }
    result += "]";
    return result;
}

document::Document& ScriptEngine::document()
{
    return m_impl->doc;
}

void ScriptEngine::setLogCallback(LogCallback cb)
{
    m_impl->logCb = std::move(cb);
}

} // namespace scripting
