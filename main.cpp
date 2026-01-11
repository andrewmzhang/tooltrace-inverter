#include <TopoDS.hxx>
#include <TopoDS_Shape.hxx>
#include <TopExp_Explorer.hxx>
#include <BRepBndLib.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <STEPControl_Reader.hxx>
#include <StlAPI_Writer.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>
#include <algorithm>
#include <emscripten.h>
#include <format>


void postLog(const std::string &log) {
    auto cppLog = std::format("C++: {}", log);
    auto logBuffer = new char[cppLog.size() + 1];
    std::copy(cppLog.begin(), cppLog.end(), logBuffer);
    logBuffer[cppLog.size()] = '\0'; // Add null terminator
    EM_ASM({
           if (Module.postLog) Module.postLog($0);
           }, logBuffer);
}

// Utility to read STEP file
TopoDS_Shape readStepFile(const std::string &filename) {
    STEPControl_Reader reader;
    if (reader.ReadFile(filename.c_str()) != IFSelect_RetDone) {
        throw std::runtime_error("C++ error reading STEP file.");
    }
    reader.TransferRoots();
    return reader.OneShape();
}

TopoDS_Shape readStepStream(std::istringstream &content) {
    STEPControl_Reader reader;
    if (reader.ReadStream("myname", content) != IFSelect_RetDone) {
        postLog("Error reading STEP file:\n");
        throw std::runtime_error("C++: Error reading STEP file.");
    }
    reader.TransferRoots();
    return reader.OneShape();
}


// Utility to write STL file
void writeStlFile(const TopoDS_Shape &shape, const std::string &filename, const double linearDeflection = 1e-3) {
    StlAPI_Writer writer;
    const BRepMesh_IncrementalMesh mesher(
        shape,
        linearDeflection
    );
    if (!mesher.IsDone()) {
        throw std::runtime_error("C++: Meshing failed");
    }
    writer.Write(shape, filename.c_str());
}

void writeStlStream(const TopoDS_Shape &shape, std::ostream &ostream, const double linearDeflection = 1e-3) {
    StlAPI_Writer writer;
    const BRepMesh_IncrementalMesh mesher(
        shape,
        linearDeflection
    );
    if (!mesher.IsDone()) {
        throw std::runtime_error("C++: Meshing failed");
    }
    writer.Write(shape, ostream);
}

bool isFaceHorizontal(const TopoDS_Face &face, const double tol = 1e-6) {
    Bnd_Box bbox;
    BRepBndLib::Add(face, bbox);
    double xmin, ymin, zmin, xmax, ymax, zmax;
    bbox.Get(xmin, ymin, zmin, xmax, ymax, zmax);
    return zmax - zmin <= tol;
}

double getZElevation(const TopoDS_Shape &shape) {
    Bnd_Box bbox;
    BRepBndLib::Add(shape, bbox);
    double xmin, ymin, zmin, xmax, ymax, zmax;
    bbox.Get(xmin, ymin, zmin, xmax, ymax, zmax);
    return zmin;
}

// Get all faces with a +Z normal approximately
std::tuple<TopoDS_Face, TopoDS_Face> getLowerUpperToolFaceTuple(const TopoDS_Shape &solid) {
    std::vector<TopoDS_Face> faces;
    TopExp_Explorer exp;
    for (exp.Init(solid, TopAbs_FACE); exp.More(); exp.Next()) {
        const auto face = TopoDS::Face(exp.Current());
        if (const auto z = getZElevation(face); isFaceHorizontal(face) and z > 0.01) {
            faces.push_back(face);
        }
    }

    std::sort(faces.begin(), faces.end(), [](const TopoDS_Face &a, const TopoDS_Face &b) {
        return getZElevation(a) < getZElevation(b);
    });
    return std::make_tuple(faces.front(), faces.at(faces.size() - 2));
}

// Create positive tool from negative bin
TopoDS_Shape createPositiveToolFromNegativeBin(const TopoDS_Shape &body) {
    // Ensure single solid
    int solidCount = 0;
    TopExp_Explorer exp;
    for (exp.Init(body, TopAbs_SOLID); exp.More(); exp.Next()) {
        solidCount++;
    }
    if (solidCount != 1) {
        throw std::runtime_error("C++: Body must have exactly one solid.");
    }

    // Get top faces for splitting
    const auto &[lowerToolFace, upperToolFace] = getLowerUpperToolFaceTuple(body);

    // Compute depth and extrusion vector
    double upperZ = getZElevation(upperToolFace);
    double lowerZ = getZElevation(lowerToolFace);
    double toolDepth = upperZ - lowerZ;
    gp_Vec toolDepthVec(0, 0, -toolDepth);

    // Find the outer, enclosing wire on upperToolFace
    TopoDS_Wire outerUpperToolWire;
    auto maxArea = -1.0;
    for (TopExp_Explorer wireExp(upperToolFace, TopAbs_WIRE); wireExp.More(); wireExp.Next()) {
        const auto &wire = TopoDS::Wire(wireExp.Current());

        // Create a face from the wire (assume planar wire)
        BRepBuilderAPI_MakeFace faceMaker(wire);
        if (!faceMaker.IsDone()) continue;
        const auto &wireFace = faceMaker.Face();

        // Calculate area of face
        GProp_GProps props;
        BRepGProp::SurfaceProperties(wireFace, props);
        if (auto area = props.Mass(); area > maxArea) {
            maxArea = area;
            outerUpperToolWire = wire;
        }
    }

    // Create face from outerUpperToolWire. Extrude from face by extrusionVec, cut from this the original body
    return BRepAlgoAPI_Cut(
        BRepPrimAPI_MakePrism(
            BRepBuilderAPI_MakeFace(outerUpperToolWire),
            toolDepthVec),
        body);
}

int generateToolPositiveFromFile(const std::string &fnameInput, const std::string &fnameOutput,
                                 const double tol = 1e-6) {
    try {
        const auto body = readStepFile(fnameInput);
        const auto tool = createPositiveToolFromNegativeBin(body);
        writeStlFile(tool, fnameOutput, tol);
    } catch (const std::exception &e) {
        std::cerr << "C++: Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}

int generateToolPositiveFromStream(
    std::istringstream &content,
    std::ostream &os,
    const double tol = 1e-2
) {
    try {
        postLog("Attempting to read STEP file...");
        const auto body = readStepStream(content);
        postLog("Attempting to create tool positive from input body...");
        const auto tool = createPositiveToolFromNegativeBin(body);
        postLog("Attempting to write STL file...");
        writeStlStream(tool, os, tol);
    } catch (const std::exception &e) {
        std::cerr << "C++ Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}


extern "C" {
// Called from JavaScript
EMSCRIPTEN_KEEPALIVE
void print_file(const char *in_buffer, size_t in_size, char *out_buffer, size_t out_size) {
    postLog(std::format("File contents:\n{}", in_buffer));
}


EMSCRIPTEN_KEEPALIVE
const char *generate_tool_positive(const char *in_buffer, size_t in_size, double tolerance) {
    // Check if tolerance is acceptable
    if (std::isnan(tolerance) || std::isinf(tolerance) || tolerance <= 0) {
        constexpr double default_tolerance = 1e-2;
        tolerance = default_tolerance;
        postLog(std::format("Tolerance value not acceptable. Defaulting to {}", tolerance));
    } else if (tolerance >= 1 || tolerance < 1e-2) {
        postLog(std::format("Tolerance value is not recommended: {}. Proceeding regardless...", tolerance));
    } else {
        postLog(std::format("Linear deflection tolerance value: {}", tolerance));
    }

    // Attempt to generate tool positive
    postLog(std::format("Received file of size: {}", in_size));
    std::istringstream iss(std::string(in_buffer, in_size));
    std::ostringstream oss;
    postLog("Attempting to generate tool positive...");
    generateToolPositiveFromStream(iss, oss, tolerance);

    auto file_content = oss.str();
    auto file_size = oss.str().size();
    postLog("Positive tool generated");
    postLog(std::format("STL file size: {}", file_size));

    // Copy the OSS into a buffer
    auto out_buffer = new char[file_size];
    std::ranges::copy(file_content, out_buffer);
    return out_buffer;
}
}
