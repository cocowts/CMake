/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file Copyright.txt or https://cmake.org/licensing for details.  */
#include "cmGlobalGhsMultiGenerator.h"

#include "cmsys/SystemTools.hxx"

#include "cmAlgorithms.h"
#include "cmDocumentationEntry.h"
#include "cmGeneratedFileStream.h"
#include "cmGeneratorTarget.h"
#include "cmGhsMultiTargetGenerator.h"
#include "cmLocalGhsMultiGenerator.h"
#include "cmMakefile.h"
#include "cmVersion.h"
#include "cmake.h"

const char* cmGlobalGhsMultiGenerator::FILE_EXTENSION = ".gpj";
const char* cmGlobalGhsMultiGenerator::DEFAULT_BUILD_PROGRAM = "gbuild.exe";
const char* cmGlobalGhsMultiGenerator::DEFAULT_TOOLSET_ROOT = "C:/ghs";

cmGlobalGhsMultiGenerator::cmGlobalGhsMultiGenerator(cmake* cm)
  : cmGlobalGenerator(cm)
{
}

cmGlobalGhsMultiGenerator::~cmGlobalGhsMultiGenerator()
{
  cmDeleteAll(TargetFolderBuildStreams);
}

cmLocalGenerator* cmGlobalGhsMultiGenerator::CreateLocalGenerator(
  cmMakefile* mf)
{
  return new cmLocalGhsMultiGenerator(this, mf);
}

void cmGlobalGhsMultiGenerator::GetDocumentation(cmDocumentationEntry& entry)
{
  entry.Name = GetActualName();
  entry.Brief =
    "Generates Green Hills MULTI files (experimental, work-in-progress).";
}

bool cmGlobalGhsMultiGenerator::SetGeneratorToolset(std::string const& ts,
                                                    cmMakefile* mf)
{
  std::string tsp;      /* toolset path */
  std::string tsn = ts; /* toolset name */

  GetToolset(mf, tsp, tsn);

  /* no toolset was found */
  if (tsn.empty()) {
    return false;
  } else if (ts.empty()) {
    std::string message;
    message =
      "Green Hills MULTI: -T <toolset> not specified; defaulting to \"";
    message += tsn;
    message += "\"";
    cmSystemTools::Message(message.c_str());

    /* store the toolset for later use
     * -- already done if -T<toolset> was specified
     */
    mf->AddCacheDefinition("CMAKE_GENERATOR_TOOLSET", tsn.c_str(),
                           "Name of generator toolset.",
                           cmStateEnums::INTERNAL);
  }

  /* set the build tool to use */
  const char* prevTool = mf->GetDefinition("CMAKE_MAKE_PROGRAM");
  std::string gbuild(tsp + "/" + tsn + "/" + DEFAULT_BUILD_PROGRAM);

  /* check if the toolset changed from last generate */
  if (prevTool != NULL && (gbuild != prevTool)) {
    std::string message = "generator toolset: ";
    message += gbuild;
    message += "\nDoes not match the toolset used previously: ";
    message += prevTool;
    message += "\nEither remove the CMakeCache.txt file and CMakeFiles "
               "directory or choose a different binary directory.";
    cmSystemTools::Error(message.c_str());
  } else {
    /* store the toolset that is being used for this build */
    mf->AddCacheDefinition("CMAKE_MAKE_PROGRAM", gbuild.c_str(),
                           "build program to use", cmStateEnums::INTERNAL,
                           true);
  }

  mf->AddDefinition("CMAKE_SYSTEM_VERSION", tsn.c_str());

  // FIXME: compiler detection not implemented
  // gbuild uses the primaryTarget setting in the top-level project
  // file to determine which compiler to use. Because compiler
  // detection is not implemented these variables must be
  // set to skip past these tests. However cmake will verify that
  // the executable pointed to by CMAKE_<LANG>_COMPILER exists.
  // To pass this additional check gbuild is used as a place holder for the
  // actual compiler.
  mf->AddDefinition("CMAKE_C_COMPILER", gbuild.c_str());
  mf->AddDefinition("CMAKE_C_COMPILER_ID_RUN", "TRUE");
  mf->AddDefinition("CMAKE_C_COMPILER_ID", "GHS");
  mf->AddDefinition("CMAKE_C_COMPILER_FORCED", "TRUE");

  mf->AddDefinition("CMAKE_CXX_COMPILER", gbuild.c_str());
  mf->AddDefinition("CMAKE_CXX_COMPILER_ID_RUN", "TRUE");
  mf->AddDefinition("CMAKE_CXX_COMPILER_ID", "GHS");
  mf->AddDefinition("CMAKE_CXX_COMPILER_FORCED", "TRUE");

  return true;
}

bool cmGlobalGhsMultiGenerator::SetGeneratorPlatform(std::string const& p,
                                                     cmMakefile* mf)
{
  if (p == "") {
    cmSystemTools::Message(
      "Green Hills MULTI: -A <arch> not specified; defaulting to \"arm\"");
    std::string arch = "arm";

    /* store the platform name for later use
     * -- already done if -A<arch> was specified
     */
    mf->AddCacheDefinition("CMAKE_GENERATOR_PLATFORM", arch.c_str(),
                           "Name of generator platform.",
                           cmStateEnums::INTERNAL);
  }

  const char* tgtPlatform = mf->GetDefinition("GHS_TARGET_PLATFORM");
  if (tgtPlatform == nullptr) {
    cmSystemTools::Message("Green Hills MULTI: GHS_TARGET_PLATFORM not "
                           "specified; defaulting to \"integrity\"");
    tgtPlatform = "integrity";
  }

  /* store the platform name for later use */
  mf->AddCacheDefinition("GHS_TARGET_PLATFORM", tgtPlatform,
                         "Name of GHS target platform.",
                         cmStateEnums::INTERNAL);

  return true;
}

void cmGlobalGhsMultiGenerator::EnableLanguage(
  std::vector<std::string> const& l, cmMakefile* mf, bool optional)
{
  mf->AddDefinition("CMAKE_SYSTEM_NAME", "GHS-MULTI");

  mf->AddDefinition("GHSMULTI", "1"); // identifier for user CMake files
  this->cmGlobalGenerator::EnableLanguage(l, mf, optional);
}

bool cmGlobalGhsMultiGenerator::FindMakeProgram(cmMakefile* /*mf*/)
{
  // The GHS generator only knows how to lookup its build tool
  // during generation of the project files, but this
  // can only be done after the toolset is specified.

  return true;
}

void cmGlobalGhsMultiGenerator::GetToolset(cmMakefile* mf, std::string& tsd,
                                           std::string& ts)
{
  const char* ghsRoot = mf->GetDefinition("GHS_TOOLSET_ROOT");

  if (!ghsRoot) {
    ghsRoot = DEFAULT_TOOLSET_ROOT;
  }
  tsd = ghsRoot;

  if (ts.empty()) {
    std::vector<std::string> output;

    // Use latest? version
    cmSystemTools::Glob(tsd, "comp_[^;]+", output);

    if (output.empty()) {
      cmSystemTools::Error("GHS toolset not found in ", tsd.c_str());
      ts = "";
    } else {
      ts = output.back();
    }
  } else {
    std::string tryPath = tsd + std::string("/") + ts;
    if (!cmSystemTools::FileExists(tryPath)) {
      cmSystemTools::Error("GHS toolset \"", ts.c_str(), "\" not found in ",
                           tsd.c_str());
      ts = "";
    }
  }
}

void cmGlobalGhsMultiGenerator::OpenBuildFileStream(
  std::string const& filepath, cmGeneratedFileStream** filestream)
{
  // Get a stream where to generate things.
  if (NULL == *filestream) {
    *filestream = new cmGeneratedFileStream(filepath.c_str());
    if (NULL != *filestream) {
      OpenBuildFileStream(*filestream);
    }
  }
}

void cmGlobalGhsMultiGenerator::OpenBuildFileStream(
  cmGeneratedFileStream* filestream)
{
  *filestream << "#!gbuild" << std::endl;
}
/* temporary until all file handling is cleaned up */
void cmGlobalGhsMultiGenerator::OpenBuildFileStream(std::ostream& fout)
{
  fout << "#!gbuild" << std::endl;
}

void cmGlobalGhsMultiGenerator::WriteTopLevelProject(
  std::ostream& fout, cmLocalGenerator* root,
  std::vector<cmLocalGenerator*>& generators)
{
  OpenBuildFileStream(fout);

  this->WriteMacros(fout);
  this->WriteHighLevelDirectives(fout);
  // GhsMultiGpj::WriteGpjTag(GhsMultiGpj::PROJECT, &fout);
  fout << "[Project]" << std::endl;
  this->WriteDisclaimer(&fout);
  fout << "# Top Level Project File" << std::endl;

  // Specify BSP option if supplied by user
  // -- not all platforms require this entry in the project file
  //    integrity platforms require this field; use default if needed
  std::string platform;
  if (const char* p =
        this->GetCMakeInstance()->GetCacheDefinition("GHS_TARGET_PLATFORM")) {
    platform = p;
  }

  std::string bspName;
  if (char const* bspCache =
        this->GetCMakeInstance()->GetCacheDefinition("GHS_BSP_NAME")) {
    bspName = bspCache;
    this->GetCMakeInstance()->MarkCliAsUsed("GHS_BSP_NAME");
  } else {
    bspName = "IGNORE";
  }

  if (platform.find("integrity") != std::string::npos &&
      cmSystemTools::IsOff(bspName.c_str())) {
    const char* a =
      this->GetCMakeInstance()->GetCacheDefinition("CMAKE_GENERATOR_PLATFORM");
    bspName = "sim";
    bspName += (a ? a : "");
  }

  if (!cmSystemTools::IsOff(bspName.c_str())) {
    fout << "    -bsp " << bspName << std::endl;
  }

  // Specify OS DIR if supplied by user
  // -- not all platforms require this entry in the project file
  std::string osDir;
  std::string osDirOption;
  if (char const* osDirCache =
        this->GetCMakeInstance()->GetCacheDefinition("GHS_OS_DIR")) {
    osDir = osDirCache;
  }

  if (char const* osDirOptionCache =
        this->GetCMakeInstance()->GetCacheDefinition("GHS_OS_DIR_OPTION")) {
    osDirOption = osDirOptionCache;
  }

  if (!cmSystemTools::IsOff(osDir.c_str()) ||
      platform.find("integrity") != std::string::npos) {
    std::replace(osDir.begin(), osDir.end(), '\\', '/');
    fout << "    " << osDirOption << "\"" << osDir << "\"" << std::endl;
  }

  WriteSubProjects(fout, root, generators);
}

void cmGlobalGhsMultiGenerator::WriteSubProjects(
  std::ostream& fout, cmLocalGenerator* root,
  std::vector<cmLocalGenerator*>& generators)
{
  // Collect all targets under this root generator and the transitive
  // closure of their dependencies.
  TargetDependSet projectTargets;
  TargetDependSet originalTargets;
  this->GetTargetSets(projectTargets, originalTargets, root, generators);
  OrderedTargetDependSet orderedProjectTargets(projectTargets, "");

  // write out all the sub-projects
  std::string rootBinaryDir = root->GetCurrentBinaryDirectory();
  for (cmGeneratorTarget const* target : orderedProjectTargets) {
    if (target->GetType() == cmStateEnums::INTERFACE_LIBRARY) {
      continue;
    }

    const char* projName = target->GetProperty("GENERATOR_FILE_NAME");
    const char* projType = target->GetProperty("GENERATOR_FILE_NAME_EXT");
    if (projName && projType) {
      cmLocalGenerator* lg = target->GetLocalGenerator();
      std::string dir = lg->GetCurrentBinaryDirectory();
      dir = root->ConvertToRelativePath(rootBinaryDir, dir.c_str());
      if (dir == ".") {
        dir.clear();
      } else {
        if (dir.back() != '/') {
          dir += "/";
        }
      }
      fout << dir << projName << FILE_EXTENSION;
      fout << " " << projType << std::endl;
    }
  }
}

void cmGlobalGhsMultiGenerator::CloseBuildFileStream(
  cmGeneratedFileStream** filestream)
{
  if (filestream) {
    delete *filestream;
    *filestream = NULL;
  } else {
    cmSystemTools::Error("Build file stream was not open.");
  }
}

void cmGlobalGhsMultiGenerator::Generate()
{
  // first do the superclass method
  this->cmGlobalGenerator::Generate();

  // output top-level projects
  for (auto& it : this->ProjectMap) {
    this->OutputTopLevelProject(it.second[0], it.second);
  }
}

void cmGlobalGhsMultiGenerator::OutputTopLevelProject(
  cmLocalGenerator* root, std::vector<cmLocalGenerator*>& generators)
{
  if (generators.empty()) {
    return;
  }

  /* Name top-level projects as filename.top.gpj to avoid name clashes
   * with target projects.  This avoid the issue where the project has
   * the same name as the executable target.
   */
  std::string fname = root->GetCurrentBinaryDirectory();
  fname += "/";
  fname += root->GetProjectName();
  fname += ".top";
  fname += FILE_EXTENSION;

  cmGeneratedFileStream fout(fname.c_str());
  fout.SetCopyIfDifferent(true);

  this->WriteTopLevelProject(fout, root, generators);

  fout.Close();
}

void cmGlobalGhsMultiGenerator::GenerateBuildCommand(
  std::vector<std::string>& makeCommand, const std::string& makeProgram,
  const std::string& projectName, const std::string& projectDir,
  const std::string& targetName, const std::string& /*config*/, bool /*fast*/,
  int jobs, bool /*verbose*/, std::vector<std::string> const& makeOptions)
{
  const char* gbuild =
    this->CMakeInstance->GetCacheDefinition("CMAKE_MAKE_PROGRAM");
  makeCommand.push_back(
    this->SelectMakeProgram(makeProgram, (std::string)gbuild));

  if (jobs != cmake::NO_BUILD_PARALLEL_LEVEL) {
    makeCommand.push_back("-parallel");
    if (jobs != cmake::DEFAULT_BUILD_PARALLEL_LEVEL) {
      makeCommand.push_back(std::to_string(jobs));
    }
  }

  makeCommand.insert(makeCommand.end(), makeOptions.begin(),
                     makeOptions.end());

  /* determine which top-project file to use */
  std::string proj = projectName + ".top" + FILE_EXTENSION;
  std::vector<std::string> files;
  cmSystemTools::Glob(projectDir, ".*\\.top\\.gpj", files);
  if (!files.empty()) {
    auto p = std::find(files.begin(), files.end(), proj);
    if (p == files.end()) {
      proj = files.at(0);
    }
  }

  makeCommand.push_back("-top");
  makeCommand.push_back(proj);
  if (!targetName.empty()) {
    if (targetName == "clean") {
      makeCommand.push_back("-clean");
    } else {
      makeCommand.push_back(targetName);
    }
  }
}

void cmGlobalGhsMultiGenerator::WriteMacros(std::ostream& fout)
{
  char const* ghsGpjMacros =
    this->GetCMakeInstance()->GetCacheDefinition("GHS_GPJ_MACROS");
  if (NULL != ghsGpjMacros) {
    std::vector<std::string> expandedList;
    cmSystemTools::ExpandListArgument(std::string(ghsGpjMacros), expandedList);
    for (std::vector<std::string>::const_iterator expandedListI =
           expandedList.begin();
         expandedListI != expandedList.end(); ++expandedListI) {
      fout << "macro " << *expandedListI << std::endl;
    }
  }
}

void cmGlobalGhsMultiGenerator::WriteHighLevelDirectives(std::ostream& fout)
{
  /* set primary target */
  std::string tgt;
  const char* t =
    this->GetCMakeInstance()->GetCacheDefinition("GHS_PRIMARY_TARGET");
  if (t) {
    tgt = t;
    this->GetCMakeInstance()->MarkCliAsUsed("GHS_PRIMARY_TARGET");
  } else {
    const char* a =
      this->GetCMakeInstance()->GetCacheDefinition("CMAKE_GENERATOR_PLATFORM");
    const char* p =
      this->GetCMakeInstance()->GetCacheDefinition("GHS_TARGET_PLATFORM");
    tgt = (a ? a : "");
    tgt += "_";
    tgt += (p ? p : "");
    tgt += ".tgt";
  }

  fout << "primaryTarget=" << tgt << std::endl;

  char const* const customization =
    this->GetCMakeInstance()->GetCacheDefinition("GHS_CUSTOMIZATION");
  if (NULL != customization && strlen(customization) > 0) {
    fout << "customization=" << trimQuotes(customization) << std::endl;
    this->GetCMakeInstance()->MarkCliAsUsed("GHS_CUSTOMIZATION");
  }
}

void cmGlobalGhsMultiGenerator::WriteDisclaimer(std::ostream* os)
{
  (*os) << "#" << std::endl
        << "# CMAKE generated file: DO NOT EDIT!" << std::endl
        << "# Generated by \"" << GetActualName() << "\""
        << " Generator, CMake Version " << cmVersion::GetMajorVersion() << "."
        << cmVersion::GetMinorVersion() << std::endl
        << "#" << std::endl;
}

void cmGlobalGhsMultiGenerator::AddFilesUpToPath(
  cmGeneratedFileStream* mainBuildFile,
  std::map<std::string, cmGeneratedFileStream*>* targetFolderBuildStreams,
  char const* homeOutputDirectory, std::string const& path,
  GhsMultiGpj::Types projType, std::string const& relPath)
{
  std::string workingPath(path);
  cmSystemTools::ConvertToUnixSlashes(workingPath);
  std::vector<std::string> splitPath = cmSystemTools::SplitString(workingPath);
  std::string workingRelPath(relPath);
  cmSystemTools::ConvertToUnixSlashes(workingRelPath);
  if (!workingRelPath.empty()) {
    workingRelPath += "/";
  }
  std::string pathUpTo;
  for (std::vector<std::string>::const_iterator splitPathI = splitPath.begin();
       splitPath.end() != splitPathI; ++splitPathI) {
    pathUpTo += *splitPathI;
    if (targetFolderBuildStreams->end() ==
        targetFolderBuildStreams->find(pathUpTo)) {
      AddFilesUpToPathNewBuildFile(
        mainBuildFile, targetFolderBuildStreams, homeOutputDirectory, pathUpTo,
        splitPath.begin() == splitPathI, workingRelPath, projType);
    }
    AddFilesUpToPathAppendNextFile(targetFolderBuildStreams, pathUpTo,
                                   splitPathI, splitPath.end(), projType);
    pathUpTo += "/";
  }
}

void cmGlobalGhsMultiGenerator::Open(
  std::string const& mapKeyName, std::string const& fileName,
  std::map<std::string, cmGeneratedFileStream*>* fileMap)
{
  if (fileMap->end() == fileMap->find(fileName)) {
    cmGeneratedFileStream* temp(new cmGeneratedFileStream);
    temp->open(fileName.c_str());
    (*fileMap)[mapKeyName] = temp;
  }
}

void cmGlobalGhsMultiGenerator::AddFilesUpToPathNewBuildFile(
  cmGeneratedFileStream* mainBuildFile,
  std::map<std::string, cmGeneratedFileStream*>* targetFolderBuildStreams,
  char const* homeOutputDirectory, std::string const& pathUpTo,
  bool const isFirst, std::string const& relPath,
  GhsMultiGpj::Types const projType)
{
  // create folders up to file path
  std::string absPath = std::string(homeOutputDirectory) + "/" + relPath;
  std::string newPath = absPath + pathUpTo;
  if (!cmSystemTools::FileExists(newPath.c_str())) {
    cmSystemTools::MakeDirectory(newPath.c_str());
  }

  // Write out to filename for first time
  std::string relFilename(GetFileNameFromPath(pathUpTo));
  std::string absFilename = absPath + relFilename;
  Open(pathUpTo, absFilename, targetFolderBuildStreams);
  OpenBuildFileStream((*targetFolderBuildStreams)[pathUpTo]);
  GhsMultiGpj::WriteGpjTag(projType, (*targetFolderBuildStreams)[pathUpTo]);
  WriteDisclaimer((*targetFolderBuildStreams)[pathUpTo]);

  // Add to main build file
  if (isFirst) {
    *mainBuildFile << relFilename << " ";
    GhsMultiGpj::WriteGpjTag(projType, mainBuildFile);
  }
}

void cmGlobalGhsMultiGenerator::AddFilesUpToPathAppendNextFile(
  std::map<std::string, cmGeneratedFileStream*>* targetFolderBuildStreams,
  std::string const& pathUpTo,
  std::vector<std::string>::const_iterator splitPathI,
  std::vector<std::string>::const_iterator end,
  GhsMultiGpj::Types const projType)
{
  std::vector<std::string>::const_iterator splitPathNextI = splitPathI + 1;
  if (end != splitPathNextI &&
      targetFolderBuildStreams->end() ==
        targetFolderBuildStreams->find(pathUpTo + "/" + *splitPathNextI)) {
    std::string nextFilename(*splitPathNextI);
    nextFilename = GetFileNameFromPath(nextFilename);
    *(*targetFolderBuildStreams)[pathUpTo] << nextFilename << " ";
    GhsMultiGpj::WriteGpjTag(projType, (*targetFolderBuildStreams)[pathUpTo]);
  }
}

std::string cmGlobalGhsMultiGenerator::GetFileNameFromPath(
  std::string const& path)
{
  std::string output(path);
  if (!path.empty()) {
    cmSystemTools::ConvertToUnixSlashes(output);
    std::vector<std::string> splitPath = cmSystemTools::SplitString(output);
    output += "/" + splitPath.back() + FILE_EXTENSION;
  }
  return output;
}

bool cmGlobalGhsMultiGenerator::IsTgtForBuild(const cmGeneratorTarget* tgt)
{
  const std::string config =
    tgt->Target->GetMakefile()->GetSafeDefinition("CMAKE_BUILD_TYPE");
  std::vector<cmSourceFile*> tgtSources;
  tgt->GetSourceFiles(tgtSources, config);
  bool tgtInBuild = true;
  char const* excludeFromAll = tgt->GetProperty("EXCLUDE_FROM_ALL");
  if (NULL != excludeFromAll && '1' == excludeFromAll[0] &&
      '\0' == excludeFromAll[1]) {
    tgtInBuild = false;
  }
  return !tgtSources.empty() && tgtInBuild;
}

std::string cmGlobalGhsMultiGenerator::trimQuotes(std::string const& str)
{
  std::string result;
  result.reserve(str.size());
  for (const char* ch = str.c_str(); *ch != '\0'; ++ch) {
    if (*ch != '"') {
      result += *ch;
    }
  }
  return result;
}

bool cmGlobalGhsMultiGenerator::TargetCompare::operator()(
  cmGeneratorTarget const* l, cmGeneratorTarget const* r) const
{
  // Make sure a given named target is ordered first,
  // e.g. to set ALL_BUILD as the default active project.
  // When the empty string is named this is a no-op.
  if (r->GetName() == this->First) {
    return false;
  }
  if (l->GetName() == this->First) {
    return true;
  }
  return l->GetName() < r->GetName();
}

cmGlobalGhsMultiGenerator::OrderedTargetDependSet::OrderedTargetDependSet(
  TargetDependSet const& targets, std::string const& first)
  : derived(TargetCompare(first))
{
  this->insert(targets.begin(), targets.end());
}
