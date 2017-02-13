/*
  ==============================================================================

  This file is part of the JUCE library.
  Copyright (c) 2015 - ROLI Ltd.

  Permission is granted to use this software under the terms of either:
  a) the GPL v2 (or any later version)
  b) the Affero GPL v3

  Details of these licenses can be found at: www.gnu.org/licenses

  JUCE is distributed in the hope that it will be useful, but WITHOUT ANY
  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
  A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

  ------------------------------------------------------------------------------

  To release a closed-source product which uses JUCE, commercial licenses are
  available: visit www.juce.com for more information.

  ==============================================================================
*/

class MSVCProjectExporterBase   : public ProjectExporter
{
public:
    MSVCProjectExporterBase (Project& p, const ValueTree& t, const char* const folderName)
        : ProjectExporter (p, t)
    {
        if (getTargetLocationString().isEmpty())
            getTargetLocationValue() = getDefaultBuildsRootFolder() + folderName;

        updateOldSettings();

        initialiseDependencyPathValues();
    }

    //==============================================================================
    class MSVCBuildConfiguration  : public BuildConfiguration
    {
    public:
        MSVCBuildConfiguration (Project& p, const ValueTree& settings, const ProjectExporter& e)
            : BuildConfiguration (p, settings, e)
        {
            if (getWarningLevel() == 0)
                getWarningLevelValue() = 4;

            setValueIfVoid (shouldGenerateManifestValue(), true);
        }

        Value getWarningLevelValue()                { return getValue (Ids::winWarningLevel); }
        int getWarningLevel() const                 { return config [Ids::winWarningLevel]; }

        Value getWarningsTreatedAsErrors()          { return getValue (Ids::warningsAreErrors); }
        bool areWarningsTreatedAsErrors() const     { return config [Ids::warningsAreErrors]; }

        Value getPrebuildCommand()                  { return getValue (Ids::prebuildCommand); }
        String getPrebuildCommandString() const     { return config [Ids::prebuildCommand]; }
        Value getPostbuildCommand()                 { return getValue (Ids::postbuildCommand); }
        String getPostbuildCommandString() const    { return config [Ids::postbuildCommand]; }

        Value shouldGenerateDebugSymbolsValue()     { return getValue (Ids::alwaysGenerateDebugSymbols); }
        bool shouldGenerateDebugSymbols() const     { return config [Ids::alwaysGenerateDebugSymbols]; }

        Value shouldGenerateManifestValue()         { return getValue (Ids::generateManifest); }
        bool shouldGenerateManifest() const         { return config [Ids::generateManifest]; }

        Value shouldLinkIncrementalValue()          { return getValue (Ids::enableIncrementalLinking); }
        bool shouldLinkIncremental() const          { return config [Ids::enableIncrementalLinking]; }

        Value getWholeProgramOptValue()             { return getValue (Ids::wholeProgramOptimisation); }
        bool shouldDisableWholeProgramOpt() const   { return static_cast<int> (config [Ids::wholeProgramOptimisation]) > 0; }

        Value getUsingRuntimeLibDLL()               { return getValue (Ids::useRuntimeLibDLL); }
        bool isUsingRuntimeLibDLL() const           { return config [Ids::useRuntimeLibDLL]; }

        String getIntermediatesPath() const         { return config [Ids::intermediatesPath].toString(); }
        Value getIntermediatesPathValue()           { return getValue (Ids::intermediatesPath); }

        String getCharacterSet() const              { return config [Ids::characterSet].toString(); }
        Value getCharacterSetValue()                { return getValue (Ids::characterSet); }

        String createMSVCConfigName() const
        {
            return getName() + "|" + (config [Ids::winArchitecture] == "x64" ? "x64" : "Win32");
        }

        String getOutputFilename (const String& suffix, bool forceSuffix) const
        {
            const String target (File::createLegalFileName (getTargetBinaryNameString().trim()));

            if (forceSuffix || ! target.containsChar ('.'))
                return target.upToLastOccurrenceOf (".", false, false) + suffix;

            return target;
        }

        var getDefaultOptimisationLevel() const override    { return var ((int) (isDebug() ? optimisationOff : optimiseMaxSpeed)); }

        void createConfigProperties (PropertyListBuilder& props) override
        {
            static const char* optimisationLevels[] = { "No optimisation", "Minimise size", "Maximise speed", 0 };
            const int optimisationLevelValues[]     = { optimisationOff, optimiseMinSize, optimiseMaxSpeed, 0 };

            props.add (new ChoicePropertyComponent (getOptimisationLevel(), "Optimisation",
                                                    StringArray (optimisationLevels),
                                                    Array<var> (optimisationLevelValues)),
                       "The optimisation level for this configuration");

            props.add (new TextPropertyComponent (getIntermediatesPathValue(), "Intermediates path", 2048, false),
                       "An optional path to a folder to use for the intermediate build files. Note that Visual Studio allows "
                       "you to use macros in this path, e.g. \"$(TEMP)\\MyAppBuildFiles\\$(Configuration)\", which is a handy way to "
                       "send them to the user's temp folder.");

            static const char* warningLevelNames[] = { "Low", "Medium", "High", nullptr };
            const int warningLevels[] = { 2, 3, 4 };

            props.add (new ChoicePropertyComponent (getWarningLevelValue(), "Warning Level",
                                                    StringArray (warningLevelNames), Array<var> (warningLevels, numElementsInArray (warningLevels))));

            props.add (new BooleanPropertyComponent (getWarningsTreatedAsErrors(), "Warnings", "Treat warnings as errors"));

            {
                static const char* runtimeNames[] = { "(Default)", "Use static runtime", "Use DLL runtime", nullptr };
                const var runtimeValues[] = { var(), var (false), var (true) };

                props.add (new ChoicePropertyComponent (getUsingRuntimeLibDLL(), "Runtime Library",
                                                        StringArray (runtimeNames), Array<var> (runtimeValues, numElementsInArray (runtimeValues))),
                           "If the static runtime is selected then your app/plug-in will not be dependent upon users having Microsoft's redistributable "
                           "C++ runtime installed. However, if you are linking libraries from different sources you must select the same type of runtime "
                           "used by the libraries.");
            }

            {
                static const char* wpoNames[] = { "Enable link-time code generation when possible",
                                                  "Always disable link-time code generation", nullptr };
                const var wpoValues[] = { var(), var (1) };

                props.add (new ChoicePropertyComponent (getWholeProgramOptValue(), "Whole Program Optimisation",
                                                        StringArray (wpoNames), Array<var> (wpoValues, numElementsInArray (wpoValues))));
            }

            {
                props.add (new BooleanPropertyComponent (shouldLinkIncrementalValue(), "Incremental Linking", "Enable"),
                           "Enable to avoid linking from scratch for every new build. "
                           "Disable to ensure that your final release build does not contain padding or thunks.");
            }

            if (! isDebug())
                props.add (new BooleanPropertyComponent (shouldGenerateDebugSymbolsValue(), "Debug Symbols", "Force generation of debug symbols"));

            props.add (new TextPropertyComponent (getPrebuildCommand(),  "Pre-build Command",  2048, true));
            props.add (new TextPropertyComponent (getPostbuildCommand(), "Post-build Command", 2048, true));
            props.add (new BooleanPropertyComponent (shouldGenerateManifestValue(), "Manifest", "Generate Manifest"));

            {
                static const char* characterSetNames[] = { "Default", "MultiByte", "Unicode", nullptr };
                const var charSets[]                   = { var(),     "MultiByte", "Unicode", };

                props.add (new ChoicePropertyComponent (getCharacterSetValue(), "Character Set",
                                                        StringArray (characterSetNames), Array<var> (charSets, numElementsInArray (charSets))));
            }
        }

        String getLibrarySubdirPath () const override
        {
            auto result = String ("$(Platform)\\");
            result += isUsingRuntimeLibDLL() ? "MD" : "MT";
            if (isDebug())
                result += "d";

            return result;
        }
    };

    //==============================================================================
    class MSVCTargetBase : public ProjectType::Target
    {
    public:
        MSVCTargetBase (ProjectType::Target::Type targetType, const MSVCProjectExporterBase& exporter)
            : ProjectType::Target (targetType), owner (exporter)
        {
            projectGuid = createGUID (owner.getProject().getProjectUID() + getName());
        }

        virtual ~MSVCTargetBase() {}

        const MSVCProjectExporterBase& getOwner() const { return owner; }
        virtual String getTopLevelXmlEntity() const = 0;
        const String& getProjectGuid() const { return projectGuid; }

        //==============================================================================
        void writeProjectFile()
        {
            XmlElement projectXml (getTopLevelXmlEntity());
            fillInProjectXml (projectXml);
            writeXmlOrThrow (projectXml, getVCProjFile(), "UTF-8", 10);
        }

        virtual void fillInProjectXml (XmlElement& projectXml) const = 0;

        String getSolutionTargetPath (const BuildConfiguration& config) const
        {
            const String binaryPath (config.getTargetBinaryRelativePathString().trim());
            if (binaryPath.isEmpty())
                return "$(SolutionDir)$(Configuration)";

            RelativePath binaryRelPath (binaryPath, RelativePath::projectFolder);

            if (binaryRelPath.isAbsolute())
                return binaryRelPath.toWindowsStyle();

            return prependDot (binaryRelPath.rebased (getOwner().projectFolder, getOwner().getTargetFolder(), RelativePath::buildTargetFolder)
                               .toWindowsStyle());
        }

        String getConfigTargetPath (const BuildConfiguration& config) const
        {
            String solutionTargetFolder (getSolutionTargetPath (config));
            return solutionTargetFolder + String ("\\") + getName();
        }

        String getIntermediatesPath (const MSVCBuildConfiguration& config) const
        {
            String intDir = (config.getIntermediatesPath().isNotEmpty() ? config.getIntermediatesPath() : "$(Configuration)");
            if (! intDir.endsWithChar (L'\\'))
                intDir += L'\\';

            return intDir + getName();
        }

        static const char* getOptimisationLevelString (int level)
        {
            switch (level)
            {
            case optimiseMaxSpeed:  return "Full";
            case optimiseMinSize:   return "MinSpace";
            default:                return "Disabled";
            }
        }

        String getTargetSuffix() const
        {
            const ProjectType::Target::TargetFileType fileType = getTargetFileType();

            switch (fileType)
            {
            case executable:
                return ".exe";
            case staticLibrary:
                return ".lib";
            case sharedLibraryOrDLL:
                return ".dll";
            case pluginBundle:
                switch (type)
                {
                case VST3PlugIn:
                    return ".vst3";
                case AAXPlugIn:
                    return ".aaxdll";
                case RTASPlugIn:
                    return ".dpm";
                default:
                    break;
                }

                return ".dll";
            default:
                break;
            }

            return String();
        }

        XmlElement* createToolElement (XmlElement& parent, const String& toolName) const
        {
            XmlElement* const e = parent.createNewChildElement ("Tool");
            e->setAttribute ("Name", toolName);
            return e;
        }

        String getPreprocessorDefs (const BuildConfiguration& config, const String& joinString) const
        {
            StringPairArray defines (getOwner().msvcExtraPreprocessorDefs);
            defines.set ("WIN32", "");
            defines.set ("_WINDOWS", "");

            if (config.isDebug())
            {
                defines.set ("DEBUG", "");
                defines.set ("_DEBUG", "");
            }
            else
            {
                defines.set ("NDEBUG", "");
            }

            defines = mergePreprocessorDefs (defines, getOwner().getAllPreprocessorDefs (config, type));
            addExtraPreprocessorDefines (defines);

            if (getTargetFileType() == staticLibrary || getTargetFileType() == sharedLibraryOrDLL)
                defines.set("_LIB", "");

            StringArray result;

            for (int i = 0; i < defines.size(); ++i)
            {
                String def (defines.getAllKeys()[i]);
                const String value (defines.getAllValues()[i]);
                if (value.isNotEmpty())
                    def << "=" << value;

                result.add (def);
            }

            return result.joinIntoString (joinString);
        }

        //==============================================================================
        RelativePath getAAXIconFile() const
        {
            const RelativePath aaxSDK (getOwner().getAAXPathValue().toString(), RelativePath::projectFolder);
            const RelativePath projectIcon ("icon.ico", RelativePath::buildTargetFolder);

            if (getOwner().getTargetFolder().getChildFile ("icon.ico").existsAsFile())
                return projectIcon.rebased (getOwner().getTargetFolder(),
                                            getOwner().getProject().getProjectFolder(),
                                            RelativePath::projectFolder);
            else
                return aaxSDK.getChildFile ("Utilities").getChildFile ("PlugIn.ico");
        }

        String getExtraPostBuildSteps (const MSVCBuildConfiguration& config) const
        {
            if (type == AAXPlugIn)
            {
                const RelativePath aaxSDK (getOwner().getAAXPathValue().toString(), RelativePath::projectFolder);
                const RelativePath aaxLibsFolder = aaxSDK.getChildFile ("Libs");
                const RelativePath bundleScript  = aaxSDK.getChildFile ("Utilities").getChildFile ("CreatePackage.bat");
                const RelativePath iconFilePath  = getAAXIconFile();

                const bool is64Bit = (config.config [Ids::winArchitecture] == "x64");
                const String bundleDir      = getOwner().getOutDirFile (config, config.getOutputFilename (".aaxplugin", true));
                const String bundleContents = bundleDir + "\\Contents";
                const String macOSDir       = bundleContents + String ("\\") + (is64Bit ? "x64" : "Win32");
                const String executable     = macOSDir + String ("\\") + config.getOutputFilename (".aaxplugin", true);

                return String ("copy /Y \"") + getOutputFilePath (config) + String ("\" \"") + executable + String ("\"\r\n") +
                    createRebasedPath (bundleScript) + String (" \"") + macOSDir + String ("\" ") + createRebasedPath (iconFilePath);
            }

            return String();
        }

        String getExtraPreBuildSteps (const MSVCBuildConfiguration& config) const
        {
            if (type == AAXPlugIn)
            {
                String script;

                const bool is64Bit = (config.config [Ids::winArchitecture] == "x64");
                const String bundleDir      = getOwner().getOutDirFile (config, config.getOutputFilename (".aaxplugin", false));

                const String bundleContents = bundleDir + "\\Contents";
                const String macOSDir       = bundleContents + String ("\\") + (is64Bit ? "x64" : "Win32");

                StringArray folders = {bundleDir.toRawUTF8(), bundleContents.toRawUTF8(), macOSDir.toRawUTF8()};
                for (int i = 0; i < folders.size(); ++i)
                    script += String ("if not exist \"") + folders[i] + String ("\" mkdir \"") + folders[i] + String ("\"\r\n");

                return script;
            }

            return String();
        }

        String getPostBuildSteps (const MSVCBuildConfiguration& config) const
        {
            String postBuild            = config.getPostbuildCommandString();
            const String extraPostBuild = getExtraPostBuildSteps (config);

            postBuild += String (postBuild.isNotEmpty() && extraPostBuild.isNotEmpty() ? "\r\n" : "") + extraPostBuild;

            return postBuild;
        }

        String getPreBuildSteps (const MSVCBuildConfiguration& config) const
        {
            String preBuild            = config.getPrebuildCommandString();
            const String extraPreBuild = getExtraPreBuildSteps (config);

            preBuild += String (preBuild.isNotEmpty() && extraPreBuild.isNotEmpty() ? "\r\n" : "") + extraPreBuild;

            return preBuild;
        }

        void addExtraPreprocessorDefines (StringPairArray& defines) const
        {
            switch (type)
            {
            case AAXPlugIn:
                {
                    const RelativePath aaxLibsFolder = RelativePath (getOwner().getAAXPathValue().toString(), RelativePath::projectFolder).getChildFile ("Libs");
                    defines.set ("JucePlugin_AAXLibs_path", createRebasedPath (aaxLibsFolder));
                }
                break;
            case RTASPlugIn:
                {
                    const RelativePath rtasFolder (getOwner().getRTASPathValue().toString(), RelativePath::projectFolder);
                    defines.set ("JucePlugin_WinBag_path", createRebasedPath (rtasFolder.getChildFile ("WinBag")));
                }
                break;
            default:
                break;
            }
        }

        String getExtraLinkerFlags() const
        {
            if (type == RTASPlugIn) return "/FORCE:multiple";

            return String();
        }

        StringArray getExtraSearchPaths() const
        {
            StringArray searchPaths;
            if (type == RTASPlugIn)
            {
                const RelativePath rtasFolder (getOwner().getRTASPathValue().toString(), RelativePath::projectFolder);
                static const char* p[] = { "AlturaPorts/TDMPlugins/PluginLibrary/EffectClasses",
                                           "AlturaPorts/TDMPlugins/PluginLibrary/ProcessClasses",
                                           "AlturaPorts/TDMPlugins/PluginLibrary/ProcessClasses/Interfaces",
                                           "AlturaPorts/TDMPlugins/PluginLibrary/Utilities",
                                           "AlturaPorts/TDMPlugins/PluginLibrary/RTASP_Adapt",
                                           "AlturaPorts/TDMPlugins/PluginLibrary/CoreClasses",
                                           "AlturaPorts/TDMPlugins/PluginLibrary/Controls",
                                           "AlturaPorts/TDMPlugins/PluginLibrary/Meters",
                                           "AlturaPorts/TDMPlugins/PluginLibrary/ViewClasses",
                                           "AlturaPorts/TDMPlugins/PluginLibrary/DSPClasses",
                                           "AlturaPorts/TDMPlugins/PluginLibrary/Interfaces",
                                           "AlturaPorts/TDMPlugins/common",
                                           "AlturaPorts/TDMPlugins/common/Platform",
                                           "AlturaPorts/TDMPlugins/common/Macros",
                                           "AlturaPorts/TDMPlugins/SignalProcessing/Public",
                                           "AlturaPorts/TDMPlugIns/DSPManager/Interfaces",
                                           "AlturaPorts/SADriver/Interfaces",
                                           "AlturaPorts/DigiPublic/Interfaces",
                                           "AlturaPorts/DigiPublic",
                                           "AlturaPorts/Fic/Interfaces/DAEClient",
                                           "AlturaPorts/NewFileLibs/Cmn",
                                           "AlturaPorts/NewFileLibs/DOA",
                                           "AlturaPorts/AlturaSource/PPC_H",
                                           "AlturaPorts/AlturaSource/AppSupport",
                                           "AvidCode/AVX2sdk/AVX/avx2/avx2sdk/inc",
                                           "xplat/AVX/avx2/avx2sdk/inc" };

                for (int i = 0; i < numElementsInArray (p); ++i)
                    searchPaths.add (createRebasedPath (rtasFolder.getChildFile (p[i])));
            }

            return searchPaths;
        }

        String getBinaryNameWithSuffix (const MSVCBuildConfiguration& config) const
        {
            return config.getOutputFilename (getTargetSuffix(), true);
        }

        String getOutputFilePath (const MSVCBuildConfiguration& config) const
        {
            return getOwner().getOutDirFile (config, getBinaryNameWithSuffix (config));
        }

        StringArray getLibrarySearchPaths (const BuildConfiguration& config) const
        {
            StringArray librarySearchPaths (config.getLibrarySearchPaths());

            if (type != SharedCodeTarget)
                if (const MSVCTargetBase* shared = getOwner().getSharedCodeTarget())
                    librarySearchPaths.add (shared->getConfigTargetPath (config));

            return librarySearchPaths;
        }

        String getExternalLibraries (const MSVCBuildConfiguration& config, const String& otherLibs) const
        {
            StringArray libraries;

            if (otherLibs.isNotEmpty())
                libraries.add (otherLibs);

            StringArray moduleLibs = getOwner().getModuleLibs();
            if (! moduleLibs.isEmpty())
                libraries.addArray (moduleLibs);

            if (type != SharedCodeTarget)
                if (const MSVCTargetBase* shared = getOwner().getSharedCodeTarget())
                    libraries.add (shared->getBinaryNameWithSuffix (config));

            return libraries.joinIntoString (";");
        }

        String getDelayLoadedDLLs() const
        {
            String delayLoadedDLLs = getOwner().msvcDelayLoadedDLLs;

            if (type == RTASPlugIn)
                delayLoadedDLLs += "DAE.dll; DigiExt.dll; DSI.dll; PluginLib.dll; "
                    "DSPManager.dll; DSPManager.dll; DSPManagerClientLib.dll; RTASClientLib.dll";

            return delayLoadedDLLs;
        }

        bool shouldUseRuntimeDLL (const MSVCBuildConfiguration& config) const
        {
            return (config.config [Ids::useRuntimeLibDLL].isVoid() ? (getOwner().hasTarget (AAXPlugIn) || getOwner().hasTarget (RTASPlugIn))
                    : config.isUsingRuntimeLibDLL());
        }

        virtual String getProjectFileSuffix() const = 0;

        File getVCProjFile() const    { return getOwner().getProjectFile (getProjectFileSuffix(), getName()); }

        String createRebasedPath (const RelativePath& path) const    {  return getOwner().createRebasedPath (path); }

        //==============================================================================
        virtual String getProjectVersionString() const = 0;
    protected:
        const MSVCProjectExporterBase& owner;
        String projectGuid;
    };

    //==============================================================================
    bool usesMMFiles() const override            { return false; }
    bool canCopeWithDuplicateFiles() override    { return false; }
    bool supportsUserDefinedConfigurations() const override { return true; }

    bool isXcode() const override                { return false; }
    bool isVisualStudio() const override         { return true; }
    bool isCodeBlocks() const override           { return false; }
    bool isMakefile() const override             { return false; }
    bool isAndroidStudio() const override        { return false; }

    bool isAndroid() const override              { return false; }
    bool isWindows() const override              { return true; }
    bool isLinux() const override                { return false; }
    bool isOSX() const override                  { return false; }
    bool isiOS() const override                  { return false; }

    bool supportsTargetType (ProjectType::Target::Type type) const override
    {
        switch (type)
        {
        case ProjectType::Target::StandalonePlugIn:
        case ProjectType::Target::GUIApp:
        case ProjectType::Target::ConsoleApp:
        case ProjectType::Target::StaticLibrary:
        case ProjectType::Target::SharedCodeTarget:
        case ProjectType::Target::AggregateTarget:
        case ProjectType::Target::VSTPlugIn:
        case ProjectType::Target::VST3PlugIn:
        case ProjectType::Target::AAXPlugIn:
        case ProjectType::Target::RTASPlugIn:
        case ProjectType::Target::DynamicLibrary:
            return true;
        default:
            break;
        }

        return false;
    }

    //==============================================================================
    const String& getProjectName() const         { return projectName; }

    virtual int getVisualStudioVersion() const = 0;

    bool launchProject() override
    {
       #if JUCE_WINDOWS
        return getSLNFile().startAsProcess();
       #else
        return false;
       #endif
    }

    bool canLaunchProject() override
    {
       #if JUCE_WINDOWS
        return true;
       #else
        return false;
       #endif
    }

    void createExporterProperties (PropertyListBuilder&) override
    {
    }

    enum OptimisationLevel
    {
        optimisationOff = 1,
        optimiseMinSize = 2,
        optimiseMaxSpeed = 3
    };

    //==============================================================================
    void addPlatformSpecificSettingsForProjectType (const ProjectType& type) override
    {
        msvcExtraPreprocessorDefs.set ("_CRT_SECURE_NO_WARNINGS", "");

        if (type.isCommandLineApp())
            msvcExtraPreprocessorDefs.set("_CONSOLE", "");
    }

    const MSVCTargetBase* getSharedCodeTarget() const
    {
        for (auto target : targets)
            if (target->type == ProjectType::Target::SharedCodeTarget)
                return target;

        return nullptr;
    }

    bool hasTarget (ProjectType::Target::Type type) const
    {
        for (auto target : targets)
            if (target->type == type)
                return true;

        return false;
    }

private:
    //==============================================================================
    String createRebasedPath (const RelativePath& path) const
    {
        String rebasedPath = rebaseFromProjectFolderToBuildTarget (path).toWindowsStyle();

        return getVisualStudioVersion() < 10  // (VS10 automatically adds escape characters to the quotes for this definition)
                                          ? CppTokeniserFunctions::addEscapeChars (rebasedPath.quoted())
                                          : CppTokeniserFunctions::addEscapeChars (rebasedPath).quoted();
    }

protected:
    //==============================================================================
    mutable File rcFile, iconFile;
    OwnedArray<MSVCTargetBase> targets;

    File getProjectFile (const String& extension, const String& target) const
    {
        String filename = project.getProjectFilenameRoot();

        if (target.isNotEmpty())
            filename += String (" (") + target + String (")");

        return getTargetFolder().getChildFile (filename).withFileExtension (extension);
    }

    File getSLNFile() const     { return getProjectFile (".sln", String()); }

    static String prependIfNotAbsolute (const String& file, const char* prefix)
    {
        if (File::isAbsolutePath (file) || file.startsWithChar ('$'))
            prefix = "";

        return prefix + FileHelpers::windowsStylePath (file);
    }

    String getIntDirFile (const BuildConfiguration& config, const String& file) const  { return prependIfNotAbsolute (replacePreprocessorTokens (config, file), "$(IntDir)\\"); }
    String getOutDirFile (const BuildConfiguration& config, const String& file) const  { return prependIfNotAbsolute (replacePreprocessorTokens (config, file), "$(OutDir)\\"); }

    void updateOldSettings()
    {
        {
            const String oldStylePrebuildCommand (getSettingString (Ids::prebuildCommand));
            settings.removeProperty (Ids::prebuildCommand, nullptr);

            if (oldStylePrebuildCommand.isNotEmpty())
                for (ConfigIterator config (*this); config.next();)
                    dynamic_cast<MSVCBuildConfiguration&> (*config).getPrebuildCommand() = oldStylePrebuildCommand;
        }

        {
            const String oldStyleLibName (getSettingString ("libraryName_Debug"));
            settings.removeProperty ("libraryName_Debug", nullptr);

            if (oldStyleLibName.isNotEmpty())
                for (ConfigIterator config (*this); config.next();)
                    if (config->isDebug())
                        config->getTargetBinaryName() = oldStyleLibName;
        }

        {
            const String oldStyleLibName (getSettingString ("libraryName_Release"));
            settings.removeProperty ("libraryName_Release", nullptr);

            if (oldStyleLibName.isNotEmpty())
                for (ConfigIterator config (*this); config.next();)
                    if (! config->isDebug())
                        config->getTargetBinaryName() = oldStyleLibName;
        }
    }

    BuildConfiguration::Ptr createBuildConfig (const ValueTree& v) const override
    {
        return new MSVCBuildConfiguration (project, v, *this);
    }

    StringArray getHeaderSearchPaths (const BuildConfiguration& config) const
    {
        StringArray searchPaths (extraSearchPaths);
        searchPaths.addArray (config.getHeaderSearchPaths());
        return getCleanedStringArray (searchPaths);
    }

    String getSharedCodeGuid() const
    {
        String sharedCodeGuid;

        for (int i = 0; i < targets.size(); ++i)
            if (MSVCTargetBase* target = targets[i])
                if (target->type == ProjectType::Target::SharedCodeTarget)
                    return target->getProjectGuid();

        return String();
    }

    //==============================================================================
    virtual void addSolutionFiles (OutputStream&, StringArray&) const
    {
         // older VS targets do not support solution files, so do nothing!
    }

    void writeProjectDependencies (OutputStream& out) const
    {
        const String sharedCodeGuid = getSharedCodeGuid();
        for (int i = 0; i < targets.size(); ++i)
        {
            if (MSVCTargetBase* target = targets[i])
            {
                out << "Project(\"{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}\") = \"" << projectName << " ("
                    << target->getName() << ")\", \""
                    << target->getVCProjFile().getFileName() << "\", \"" << target->getProjectGuid() << '"' << newLine;

                if (sharedCodeGuid.isNotEmpty() && target->type != ProjectType::Target::SharedCodeTarget)
                    out << "\tProjectSection(ProjectDependencies) = postProject" << newLine
                        << "\t\t" << sharedCodeGuid << " = " << sharedCodeGuid << newLine
                        << "\tEndProjectSection" << newLine;

                out << "EndProject" << newLine;
            }
        }
    }

    void writeSolutionFile (OutputStream& out, const String& versionString, String commentString) const
    {
        StringArray nestedProjects;

        if (commentString.isNotEmpty())
            commentString += newLine;

        out << "Microsoft Visual Studio Solution File, Format Version " << versionString << newLine
            << commentString << newLine;

        writeProjectDependencies (out);
        addSolutionFiles (out, nestedProjects);

        out << "Global" << newLine
            << "\tGlobalSection(SolutionConfigurationPlatforms) = preSolution" << newLine;

        for (ConstConfigIterator i (*this); i.next();)
        {
            const MSVCBuildConfiguration& config = dynamic_cast<const MSVCBuildConfiguration&> (*i);
            const String configName = config.createMSVCConfigName();
            out << "\t\t" << configName << " = " << configName << newLine;
        }

        out << "\tEndGlobalSection" << newLine
            << "\tGlobalSection(ProjectConfigurationPlatforms) = postSolution" << newLine;

        for (auto& target : targets)
            for (ConstConfigIterator i (*this); i.next();)
            {
                const MSVCBuildConfiguration& config = dynamic_cast<const MSVCBuildConfiguration&> (*i);
                const String configName = config.createMSVCConfigName();
                out << "\t\t" << target->getProjectGuid() << "." << configName << ".Build.0 = " << configName << newLine;
            }

        out << "\tEndGlobalSection" << newLine
            << "\tGlobalSection(SolutionProperties) = preSolution" << newLine
            << "\t\tHideSolutionNode = FALSE" << newLine
            << "\tEndGlobalSection" << newLine;

        if (nestedProjects.size() > 0)
        {
            out << "\tGlobalSection(NestedProjects) = preSolution" << newLine << "\t\t";
            out << nestedProjects.joinIntoString ("\n\t\t") << newLine;
            out << "\tEndGlobalSection" << newLine;
        }

        out << "EndGlobal" << newLine;
    }

    //==============================================================================
    static void writeBMPImage (const Image& image, const int w, const int h, MemoryOutputStream& out)
    {
        const int maskStride = (w / 8 + 3) & ~3;

        out.writeInt (40); // bitmapinfoheader size
        out.writeInt (w);
        out.writeInt (h * 2);
        out.writeShort (1); // planes
        out.writeShort (32); // bits
        out.writeInt (0); // compression
        out.writeInt ((h * w * 4) + (h * maskStride)); // size image
        out.writeInt (0); // x pixels per meter
        out.writeInt (0); // y pixels per meter
        out.writeInt (0); // clr used
        out.writeInt (0); // clr important

        const Image::BitmapData bitmap (image, Image::BitmapData::readOnly);
        const int alphaThreshold = 5;

        int y;
        for (y = h; --y >= 0;)
        {
            for (int x = 0; x < w; ++x)
            {
                const Colour pixel (bitmap.getPixelColour (x, y));

                if (pixel.getAlpha() <= alphaThreshold)
                {
                    out.writeInt (0);
                }
                else
                {
                    out.writeByte ((char) pixel.getBlue());
                    out.writeByte ((char) pixel.getGreen());
                    out.writeByte ((char) pixel.getRed());
                    out.writeByte ((char) pixel.getAlpha());
                }
            }
        }

        for (y = h; --y >= 0;)
        {
            int mask = 0, count = 0;

            for (int x = 0; x < w; ++x)
            {
                const Colour pixel (bitmap.getPixelColour (x, y));

                mask <<= 1;
                if (pixel.getAlpha() <= alphaThreshold)
                    mask |= 1;

                if (++count == 8)
                {
                    out.writeByte ((char) mask);
                    count = 0;
                    mask = 0;
                }
            }

            if (mask != 0)
                out.writeByte ((char) mask);

            for (int i = maskStride - w / 8; --i >= 0;)
                out.writeByte (0);
        }
    }

    static void writeIconFile (const Array<Image>& images, MemoryOutputStream& out)
    {
        out.writeShort (0); // reserved
        out.writeShort (1); // .ico tag
        out.writeShort ((short) images.size());

        MemoryOutputStream dataBlock;

        const int imageDirEntrySize = 16;
        const int dataBlockStart = 6 + images.size() * imageDirEntrySize;

        for (int i = 0; i < images.size(); ++i)
        {
            const size_t oldDataSize = dataBlock.getDataSize();

            const Image& image = images.getReference (i);
            const int w = image.getWidth();
            const int h = image.getHeight();

            if (w >= 256 || h >= 256)
            {
                PNGImageFormat pngFormat;
                pngFormat.writeImageToStream (image, dataBlock);
            }
            else
            {
                writeBMPImage (image, w, h, dataBlock);
            }

            out.writeByte ((char) w);
            out.writeByte ((char) h);
            out.writeByte (0);
            out.writeByte (0);
            out.writeShort (1); // colour planes
            out.writeShort (32); // bits per pixel
            out.writeInt ((int) (dataBlock.getDataSize() - oldDataSize));
            out.writeInt (dataBlockStart + (int) oldDataSize);
        }

        jassert (out.getPosition() == dataBlockStart);
        out << dataBlock;
    }

    bool hasResourceFile() const
    {
        return ! projectType.isStaticLibrary();
    }

    void createResourcesAndIcon() const
    {
        if (hasResourceFile())
        {
            Array<Image> images;
            const int sizes[] = { 16, 32, 48, 256 };

            for (int i = 0; i < numElementsInArray (sizes); ++i)
            {
                Image im (getBestIconForSize (sizes[i], true));
                if (im.isValid())
                    images.add (im);
            }

            if (images.size() > 0)
            {
                iconFile = getTargetFolder().getChildFile ("icon.ico");

                MemoryOutputStream mo;
                writeIconFile (images, mo);
                overwriteFileIfDifferentOrThrow (iconFile, mo);
            }

            createRCFile();
        }
    }

    void createRCFile() const
    {
        rcFile = getTargetFolder().getChildFile ("resources.rc");

        const String version (project.getVersionString());

        MemoryOutputStream mo;

        mo << "#ifdef JUCE_USER_DEFINED_RC_FILE" << newLine
           << " #include JUCE_USER_DEFINED_RC_FILE" << newLine
           << "#else" << newLine
           << newLine
           << "#undef  WIN32_LEAN_AND_MEAN" << newLine
           << "#define WIN32_LEAN_AND_MEAN" << newLine
           << "#include <windows.h>" << newLine
           << newLine
           << "VS_VERSION_INFO VERSIONINFO" << newLine
           << "FILEVERSION  " << getCommaSeparatedVersionNumber (version) << newLine
           << "BEGIN" << newLine
           << "  BLOCK \"StringFileInfo\"" << newLine
           << "  BEGIN" << newLine
           << "    BLOCK \"040904E4\"" << newLine
           << "    BEGIN" << newLine;

        writeRCValue (mo, "CompanyName", project.getCompanyName().toString());
        writeRCValue (mo, "FileDescription", project.getTitle());
        writeRCValue (mo, "FileVersion", version);
        writeRCValue (mo, "ProductName", project.getTitle());
        writeRCValue (mo, "ProductVersion", version);

        mo << "    END" << newLine
           << "  END" << newLine
           << newLine
           << "  BLOCK \"VarFileInfo\"" << newLine
           << "  BEGIN" << newLine
           << "    VALUE \"Translation\", 0x409, 1252" << newLine
           << "  END" << newLine
           << "END" << newLine
           << newLine
           << "#endif" << newLine;

        if (iconFile != File())
            mo << newLine
               << "IDI_ICON1 ICON DISCARDABLE " << iconFile.getFileName().quoted()
               << newLine
               << "IDI_ICON2 ICON DISCARDABLE " << iconFile.getFileName().quoted();

        overwriteFileIfDifferentOrThrow (rcFile, mo);
    }

    static void writeRCValue (MemoryOutputStream& mo, const String& name, const String& value)
    {
        if (value.isNotEmpty())
            mo << "      VALUE \"" << name << "\",  \""
               << CppTokeniserFunctions::addEscapeChars (value) << "\\0\"" << newLine;
    }

    static String getCommaSeparatedVersionNumber (const String& version)
    {
        StringArray versionParts;
        versionParts.addTokens (version, ",.", "");
        versionParts.trim();
        versionParts.removeEmptyStrings();
        while (versionParts.size() < 4)
            versionParts.add ("0");

        return versionParts.joinIntoString (",");
    }

    static String prependDot (const String& filename)
    {
        return FileHelpers::isAbsolutePath (filename) ? filename
                                                      : (".\\" + filename);
    }

    void initialiseDependencyPathValues()
    {
        vst3Path.referTo (Value (new DependencyPathValueSource (getSetting (Ids::vst3Folder),
                                                                Ids::vst3Path,
                                                                TargetOS::windows)));

        aaxPath.referTo (Value (new DependencyPathValueSource (getSetting (Ids::aaxFolder),
                                                               Ids::aaxPath,
                                                               TargetOS::windows)));

        rtasPath.referTo (Value (new DependencyPathValueSource (getSetting (Ids::rtasFolder),
                                                                Ids::rtasPath,
                                                                TargetOS::windows)));
    }

    static bool shouldUseStdCall (const RelativePath& path)
    {
        return path.getFileNameWithoutExtension().startsWithIgnoreCase ("juce_audio_plugin_client_RTAS_");
    }

    StringArray getModuleLibs () const
    {
        StringArray result;
        for (auto& lib : windowsLibs)
            result.add (lib + ".lib");

        return result;
    }

    JUCE_DECLARE_NON_COPYABLE (MSVCProjectExporterBase)
};

//==============================================================================
class MSVCProjectExporterVC2010   : public MSVCProjectExporterBase
{
public:
    MSVCProjectExporterVC2010 (Project& p, const ValueTree& t, const char* folderName = "VisualStudio2010")
        : MSVCProjectExporterBase (p, t, folderName)
    {
        name = getName();
    }

    //==============================================================================
    class MSVC2010Target : public MSVCTargetBase
    {
    public:
        MSVC2010Target (ProjectType::Target::Type targetType, const MSVCProjectExporterVC2010& exporter)
            : MSVCTargetBase (targetType, exporter)
        {}

        const MSVCProjectExporterVC2010& getOwner() const  { return dynamic_cast<const MSVCProjectExporterVC2010&> (owner); }
        String getProjectVersionString() const override    { return "10.00"; }
        String getProjectFileSuffix() const override       { return ".vcxproj"; }
        String getTopLevelXmlEntity() const override       { return "Project"; }

        //==============================================================================
        void fillInProjectXml (XmlElement& projectXml) const override
        {
            projectXml.setAttribute ("DefaultTargets", "Build");
            projectXml.setAttribute ("ToolsVersion", getOwner().getToolsVersion());
            projectXml.setAttribute ("xmlns", "http://schemas.microsoft.com/developer/msbuild/2003");

            {
                XmlElement* configsGroup = projectXml.createNewChildElement ("ItemGroup");
                configsGroup->setAttribute ("Label", "ProjectConfigurations");

                for (ConstConfigIterator i (owner); i.next();)
                {
                    const MSVCBuildConfiguration& config = dynamic_cast<const MSVCBuildConfiguration&> (*i);
                    XmlElement* e = configsGroup->createNewChildElement ("ProjectConfiguration");
                    e->setAttribute ("Include", config.createMSVCConfigName());
                    e->createNewChildElement ("Configuration")->addTextElement (config.getName());
                    e->createNewChildElement ("Platform")->addTextElement (is64Bit (config) ? "x64" : "Win32");
                }
            }

            {
                XmlElement* globals = projectXml.createNewChildElement ("PropertyGroup");
                globals->setAttribute ("Label", "Globals");
                globals->createNewChildElement ("ProjectGuid")->addTextElement (getProjectGuid());
            }

            {
                XmlElement* imports = projectXml.createNewChildElement ("Import");
                imports->setAttribute ("Project", "$(VCTargetsPath)\\Microsoft.Cpp.Default.props");
            }

            for (ConstConfigIterator i (owner); i.next();)
            {
                const VC2010BuildConfiguration& config = dynamic_cast<const VC2010BuildConfiguration&> (*i);

                XmlElement* e = projectXml.createNewChildElement ("PropertyGroup");
                setConditionAttribute (*e, config);
                e->setAttribute ("Label", "Configuration");
                e->createNewChildElement ("ConfigurationType")->addTextElement (getProjectType());
                e->createNewChildElement ("UseOfMfc")->addTextElement ("false");

                const String charSet (config.getCharacterSet());

                if (charSet.isNotEmpty())
                    e->createNewChildElement ("CharacterSet")->addTextElement (charSet);

                if (! (config.isDebug() || config.shouldDisableWholeProgramOpt()))
                    e->createNewChildElement ("WholeProgramOptimization")->addTextElement ("true");

                if (config.shouldLinkIncremental())
                    e->createNewChildElement ("LinkIncremental")->addTextElement ("true");

                if (config.is64Bit())
                    e->createNewChildElement ("PlatformToolset")->addTextElement (getOwner().getPlatformToolset());
            }

            {
                XmlElement* e = projectXml.createNewChildElement ("Import");
                e->setAttribute ("Project", "$(VCTargetsPath)\\Microsoft.Cpp.props");
            }

            {
                XmlElement* e = projectXml.createNewChildElement ("ImportGroup");
                e->setAttribute ("Label", "ExtensionSettings");
            }

            {
                XmlElement* e = projectXml.createNewChildElement ("ImportGroup");
                e->setAttribute ("Label", "PropertySheets");
                XmlElement* p = e->createNewChildElement ("Import");
                p->setAttribute ("Project", "$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props");
                p->setAttribute ("Condition", "exists('$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props')");
                p->setAttribute ("Label", "LocalAppDataPlatform");
            }

            {
                XmlElement* e = projectXml.createNewChildElement ("PropertyGroup");
                e->setAttribute ("Label", "UserMacros");
            }

            {
                XmlElement* props = projectXml.createNewChildElement ("PropertyGroup");
                props->createNewChildElement ("_ProjectFileVersion")->addTextElement ("10.0.30319.1");
                props->createNewChildElement ("TargetExt")->addTextElement (getTargetSuffix());

                for (ConstConfigIterator i (owner); i.next();)
                {
                    const VC2010BuildConfiguration& config = dynamic_cast<const VC2010BuildConfiguration&> (*i);

                    if (getConfigTargetPath (config).isNotEmpty())
                    {
                        XmlElement* outdir = props->createNewChildElement ("OutDir");
                        setConditionAttribute (*outdir, config);
                        outdir->addTextElement (FileHelpers::windowsStylePath (getConfigTargetPath (config)) + "\\");
                    }

                    {
                        XmlElement* intdir = props->createNewChildElement("IntDir");
                        setConditionAttribute(*intdir, config);

                        String intermediatesPath = getIntermediatesPath (config);
                        if (!intermediatesPath.endsWith("\\"))
                            intermediatesPath << "\\";

                        intdir->addTextElement (FileHelpers::windowsStylePath (intermediatesPath));
                    }


                    {
                        XmlElement* targetName = props->createNewChildElement ("TargetName");
                        setConditionAttribute (*targetName, config);
                        targetName->addTextElement (config.getOutputFilename ("", false));
                    }

                    {
                        XmlElement* manifest = props->createNewChildElement ("GenerateManifest");
                        setConditionAttribute (*manifest, config);
                        manifest->addTextElement (config.shouldGenerateManifest() ? "true" : "false");
                    }

                    const StringArray librarySearchPaths (getLibrarySearchPaths (config));
                    if (librarySearchPaths.size() > 0)
                    {
                        XmlElement* libPath = props->createNewChildElement ("LibraryPath");
                        setConditionAttribute (*libPath, config);
                        libPath->addTextElement ("$(LibraryPath);" + librarySearchPaths.joinIntoString (";"));
                    }
                }
            }

            for (ConstConfigIterator i (owner); i.next();)
            {
                const VC2010BuildConfiguration& config = dynamic_cast<const VC2010BuildConfiguration&> (*i);

                const bool isDebug = config.isDebug();

                XmlElement* group = projectXml.createNewChildElement ("ItemDefinitionGroup");
                setConditionAttribute (*group, config);

                {
                    XmlElement* midl = group->createNewChildElement ("Midl");
                    midl->createNewChildElement ("PreprocessorDefinitions")->addTextElement (isDebug ? "_DEBUG;%(PreprocessorDefinitions)"
                                                                                             : "NDEBUG;%(PreprocessorDefinitions)");
                    midl->createNewChildElement ("MkTypLibCompatible")->addTextElement ("true");
                    midl->createNewChildElement ("SuppressStartupBanner")->addTextElement ("true");
                    midl->createNewChildElement ("TargetEnvironment")->addTextElement ("Win32");
                    midl->createNewChildElement ("HeaderFileName");
                }

                bool isUsingEditAndContinue = false;

                {
                    XmlElement* cl = group->createNewChildElement ("ClCompile");

                    cl->createNewChildElement ("Optimization")->addTextElement (getOptimisationLevelString (config.getOptimisationLevelInt()));

                    if (isDebug && config.getOptimisationLevelInt() <= optimisationOff)
                    {
                        isUsingEditAndContinue = ! config.is64Bit();

                        cl->createNewChildElement ("DebugInformationFormat")
                            ->addTextElement (isUsingEditAndContinue ? "EditAndContinue"
                                              : "ProgramDatabase");
                    }

                    StringArray includePaths (getOwner().getHeaderSearchPaths (config));
                    includePaths.addArray (getExtraSearchPaths());
                    includePaths.add ("%(AdditionalIncludeDirectories)");

                    cl->createNewChildElement ("AdditionalIncludeDirectories")->addTextElement (includePaths.joinIntoString (";"));
                    cl->createNewChildElement ("PreprocessorDefinitions")->addTextElement (getPreprocessorDefs (config, ";") + ";%(PreprocessorDefinitions)");

                    const bool runtimeDLL = shouldUseRuntimeDLL (config);
                    cl->createNewChildElement ("RuntimeLibrary")->addTextElement (runtimeDLL ? (isDebug ? "MultiThreadedDebugDLL" : "MultiThreadedDLL")
                                                                                  : (isDebug ? "MultiThreadedDebug"    : "MultiThreaded"));
                    cl->createNewChildElement ("RuntimeTypeInfo")->addTextElement ("true");
                    cl->createNewChildElement ("PrecompiledHeader");
                    cl->createNewChildElement ("AssemblerListingLocation")->addTextElement ("$(IntDir)\\");
                    cl->createNewChildElement ("ObjectFileName")->addTextElement ("$(IntDir)\\");
                    cl->createNewChildElement ("ProgramDataBaseFileName")->addTextElement ("$(IntDir)\\");
                    cl->createNewChildElement ("WarningLevel")->addTextElement ("Level" + String (config.getWarningLevel()));
                    cl->createNewChildElement ("SuppressStartupBanner")->addTextElement ("true");
                    cl->createNewChildElement ("MultiProcessorCompilation")->addTextElement ("true");

                    if (config.isFastMathEnabled())
                        cl->createNewChildElement ("FloatingPointModel")->addTextElement ("Fast");

                    const String extraFlags (getOwner().replacePreprocessorTokens (config, getOwner().getExtraCompilerFlagsString()).trim());
                    if (extraFlags.isNotEmpty())
                        cl->createNewChildElement ("AdditionalOptions")->addTextElement (extraFlags + " %(AdditionalOptions)");

                    if (config.areWarningsTreatedAsErrors())
                        cl->createNewChildElement ("TreatWarningAsError")->addTextElement ("true");
                }

                {
                    XmlElement* res = group->createNewChildElement ("ResourceCompile");
                    res->createNewChildElement ("PreprocessorDefinitions")->addTextElement (isDebug ? "_DEBUG;%(PreprocessorDefinitions)"
                                                                                            : "NDEBUG;%(PreprocessorDefinitions)");
                }

                {
                    XmlElement* link = group->createNewChildElement ("Link");
                    link->createNewChildElement ("OutputFile")->addTextElement (getOutputFilePath (config));
                    link->createNewChildElement ("SuppressStartupBanner")->addTextElement ("true");
                    link->createNewChildElement ("IgnoreSpecificDefaultLibraries")->addTextElement (isDebug ? "libcmt.lib; msvcrt.lib;;%(IgnoreSpecificDefaultLibraries)"
                                                                                                    : "%(IgnoreSpecificDefaultLibraries)");
                    link->createNewChildElement ("GenerateDebugInformation")->addTextElement ((isDebug || config.shouldGenerateDebugSymbols()) ? "true" : "false");
                    link->createNewChildElement ("ProgramDatabaseFile")->addTextElement (getOwner().getIntDirFile (config, config.getOutputFilename (".pdb", true)));
                    link->createNewChildElement ("SubSystem")->addTextElement (type == ConsoleApp ? "Console" : "Windows");

                    if (! config.is64Bit())
                        link->createNewChildElement ("TargetMachine")->addTextElement ("MachineX86");

                    if (isUsingEditAndContinue)
                        link->createNewChildElement ("ImageHasSafeExceptionHandlers")->addTextElement ("false");

                    if (! isDebug)
                    {
                        link->createNewChildElement ("OptimizeReferences")->addTextElement ("true");
                        link->createNewChildElement ("EnableCOMDATFolding")->addTextElement ("true");
                    }

                    const StringArray librarySearchPaths (config.getLibrarySearchPaths());
                    if (librarySearchPaths.size() > 0)
                        link->createNewChildElement ("AdditionalLibraryDirectories")->addTextElement (getOwner().replacePreprocessorTokens (config, librarySearchPaths.joinIntoString (";"))
                                                                                                      + ";%(AdditionalLibraryDirectories)");

                    link->createNewChildElement ("LargeAddressAware")->addTextElement ("true");

                    const String externalLibraries (getExternalLibraries (config, getOwner().getExternalLibrariesString()));
                    if (externalLibraries.isNotEmpty())
                        link->createNewChildElement ("AdditionalDependencies")->addTextElement (getOwner().replacePreprocessorTokens (config, externalLibraries).trim()
                                                                                                + ";%(AdditionalDependencies)");

                    String extraLinkerOptions (getOwner().getExtraLinkerFlagsString());
                    if (extraLinkerOptions.isNotEmpty())
                        link->createNewChildElement ("AdditionalOptions")->addTextElement (getOwner().replacePreprocessorTokens (config, extraLinkerOptions).trim()
                                                                                           + " %(AdditionalOptions)");

                    const String delayLoadedDLLs (getDelayLoadedDLLs());
                    if (delayLoadedDLLs.isNotEmpty())
                        link->createNewChildElement ("DelayLoadDLLs")->addTextElement (delayLoadedDLLs);

                    if (config.config [Ids::msvcModuleDefinitionFile].toString().isNotEmpty())
                        link->createNewChildElement ("ModuleDefinitionFile")
                            ->addTextElement (config.config [Ids::msvcModuleDefinitionFile].toString());
                }

                {
                    XmlElement* bsc = group->createNewChildElement ("Bscmake");
                    bsc->createNewChildElement ("SuppressStartupBanner")->addTextElement ("true");
                    bsc->createNewChildElement ("OutputFile")->addTextElement (getOwner().getIntDirFile (config, config.getOutputFilename (".bsc", true)));
                }

                if (getTargetFileType() == staticLibrary && ! config.is64Bit())
                {
                    XmlElement* lib = group->createNewChildElement ("Lib");
                    lib->createNewChildElement ("TargetMachine")->addTextElement ("MachineX86");
                }

                const String preBuild = getPreBuildSteps (config);
                if (preBuild.isNotEmpty())
                    group->createNewChildElement ("PreBuildEvent")
                        ->createNewChildElement ("Command")
                        ->addTextElement (preBuild);

                const String postBuild = getPostBuildSteps (config);
                if (postBuild.isNotEmpty())
                    group->createNewChildElement ("PostBuildEvent")
                        ->createNewChildElement ("Command")
                        ->addTextElement (postBuild);
            }

            ScopedPointer<XmlElement> otherFilesGroup (new XmlElement ("ItemGroup"));

            {
                XmlElement* cppFiles    = projectXml.createNewChildElement ("ItemGroup");
                XmlElement* headerFiles = projectXml.createNewChildElement ("ItemGroup");

                for (int i = 0; i < getOwner().getAllGroups().size(); ++i)
                {
                    const Project::Item& group = getOwner().getAllGroups().getReference (i);

                    if (group.getNumChildren() > 0)
                        addFilesToCompile (group, *cppFiles, *headerFiles, *otherFilesGroup);
                }
            }

            if (getOwner().iconFile != File())
            {
                XmlElement* e = otherFilesGroup->createNewChildElement ("None");
                e->setAttribute ("Include", prependDot (getOwner().iconFile.getFileName()));
            }

            if (otherFilesGroup->getFirstChildElement() != nullptr)
                projectXml.addChildElement (otherFilesGroup.release());

            if (getOwner().hasResourceFile())
            {
                XmlElement* rcGroup = projectXml.createNewChildElement ("ItemGroup");
                XmlElement* e = rcGroup->createNewChildElement ("ResourceCompile");
                e->setAttribute ("Include", prependDot (getOwner().rcFile.getFileName()));
            }

            {
                XmlElement* e = projectXml.createNewChildElement ("Import");
                e->setAttribute ("Project", "$(VCTargetsPath)\\Microsoft.Cpp.targets");
            }

            {
                XmlElement* e = projectXml.createNewChildElement ("ImportGroup");
                e->setAttribute ("Label", "ExtensionTargets");
            }

            getOwner().addPlatformToolsetToPropertyGroup (projectXml);
            getOwner().addIPPSettingToPropertyGroup (projectXml);
        }

        String getProjectType() const
        {
            switch (getTargetFileType())
            {
            case executable:
                return "Application";
            case staticLibrary:
                return "StaticLibrary";
            default:
                break;
            }

            return "DynamicLibrary";
        }

        //==============================================================================
        void addFilesToCompile (const Project::Item& projectItem, XmlElement& cpps, XmlElement& headers, XmlElement& otherFiles) const
        {
            const Type targetType = (getOwner().getProject().getProjectType().isAudioPlugin() ? type : SharedCodeTarget);

            if (projectItem.isGroup())
            {
                for (int i = 0; i < projectItem.getNumChildren(); ++i)
                    addFilesToCompile (projectItem.getChild (i), cpps, headers, otherFiles);
            }
            else if (projectItem.shouldBeAddedToTargetProject())
            {
                const RelativePath path (projectItem.getFile(), getOwner().getTargetFolder(), RelativePath::buildTargetFolder);

                jassert (path.getRoot() == RelativePath::buildTargetFolder);

                if (projectItem.shouldBeCompiled() && (path.hasFileExtension (cOrCppFileExtensions) || path.hasFileExtension (asmFileExtensions))
                    && getOwner().getProject().getTargetTypeFromFilePath (projectItem.getFile(), true) == targetType)
                {
                    XmlElement* e = cpps.createNewChildElement ("ClCompile");
                    e->setAttribute ("Include", path.toWindowsStyle());

                    if (shouldUseStdCall (path))
                        e->createNewChildElement ("CallingConvention")->addTextElement ("StdCall");
                }
            }
        }

        void setConditionAttribute (XmlElement& xml, const BuildConfiguration& config) const
        {
            const MSVCBuildConfiguration& msvcConfig = dynamic_cast<const MSVCBuildConfiguration&> (config);
            xml.setAttribute ("Condition", "'$(Configuration)|$(Platform)'=='" + msvcConfig.createMSVCConfigName() + "'");
        }

    };

    static const char* getName()                { return "Visual Studio 2010"; }
    static const char* getValueTreeTypeName()   { return "VS2010"; }
    int getVisualStudioVersion() const override { return 10; }
    virtual String getSolutionComment() const   { return "# Visual Studio 2010"; }
    virtual String getToolsVersion() const      { return "4.0"; }
    virtual String getDefaultToolset() const    { return "Windows7.1SDK"; }
    Value getPlatformToolsetValue()             { return getSetting (Ids::toolset); }
    Value getIPPLibraryValue()                  { return getSetting (Ids::IPPLibrary); }
    String getIPPLibrary() const                { return settings [Ids::IPPLibrary]; }

    String getPlatformToolset() const
    {
        const String s (settings [Ids::toolset].toString());
        return s.isNotEmpty() ? s : getDefaultToolset();
    }

    static MSVCProjectExporterVC2010* createForSettings (Project& project, const ValueTree& settings)
    {
        if (settings.hasType (getValueTreeTypeName()))
            return new MSVCProjectExporterVC2010 (project, settings);

        return nullptr;
    }

    void addToolsetProperty (PropertyListBuilder& props, const char** names, const var* values, int num)
    {
        props.add (new ChoicePropertyComponent (getPlatformToolsetValue(), "Platform Toolset",
                                                StringArray (names, num), Array<var> (values, num)));
    }

    void addIPPLibraryProperty (PropertyListBuilder& props)
    {
        static const char* ippOptions[] = { "No",  "Yes (Default Mode)", "Multi-Threaded Static Library", "Single-Threaded Static Library", "Multi-Threaded DLL", "Single-Threaded DLL" };
        static const var ippValues[]    = { var(), "true",               "Parallel_Static",               "Sequential",                     "Parallel_Dynamic",   "Sequential_Dynamic" };

        props.add (new ChoicePropertyComponent (getIPPLibraryValue(), "Use IPP Library",
                                                StringArray (ippOptions, numElementsInArray (ippValues)),
                                                Array<var> (ippValues, numElementsInArray (ippValues))));
    }

    void createExporterProperties (PropertyListBuilder& props) override
    {
        MSVCProjectExporterBase::createExporterProperties (props);

        static const char* toolsetNames[] = { "(default)", "v100", "v100_xp", "Windows7.1SDK", "CTP_Nov2013" };
        const var toolsets[]              = { var(),       "v100", "v100_xp", "Windows7.1SDK", "CTP_Nov2013" };

        addToolsetProperty (props, toolsetNames, toolsets, numElementsInArray (toolsets));
        addIPPLibraryProperty (props);
    }

    //==============================================================================
    void addSolutionFiles (OutputStream& out, StringArray& nestedProjects) const override
    {
        const bool ignoreRootGroup = (getAllGroups().size() == 1);
        const int numberOfGroups = (ignoreRootGroup ? getAllGroups().getReference (0).getNumChildren()
                                    : getAllGroups().size());
        for (int i = 0; i < numberOfGroups; ++i)
        {
            const Project::Item& group = (ignoreRootGroup ? getAllGroups().getReference (0).getChild (i)
                                          : getAllGroups().getReference (i));

            if (group.getNumChildren() > 0)
                addSolutionFiles (out, group, nestedProjects, String());
        }
    }

    void addSolutionFiles (OutputStream& out, const Project::Item& item, StringArray& nestedProjectList, const String& parentGuid) const
    {
        jassert (item.isGroup());

        const String groupGuid = createGUID (item.getID());

        out << "Project(\"{2150E333-8FDC-42A3-9474-1A3956D46DE8}\") = \"" << item.getName() << "\""
            << ", \"" << item.getName() << "\", \"" << groupGuid << "\"" << newLine;

        bool hasSubFiles = false;
        const int n = item.getNumChildren();

        for (int i = 0; i < n; ++i)
        {
            const Project::Item& child = item.getChild (i);

            if (child.isFile())
            {
                if (! hasSubFiles)
                {
                    out << "\tProjectSection(SolutionItems) = preProject" << newLine;
                    hasSubFiles = true;
                }

                const RelativePath path = RelativePath (child.getFilePath(), RelativePath::projectFolder)
                    .rebased (getProject().getProjectFolder(), getSLNFile().getParentDirectory(),
                              RelativePath::buildTargetFolder);

                out << "\t\t" << path.toWindowsStyle() << " = " << path.toWindowsStyle() << newLine;
            }
        }

        if (hasSubFiles)
            out << "\tEndProjectSection" << newLine;

        out << "EndProject" << newLine;

        for (int i = 0; i < n; ++i)
        {
            const Project::Item& child = item.getChild (i);

            if (child.isGroup())
                addSolutionFiles (out, child, nestedProjectList, groupGuid);
        }

        if (parentGuid.isNotEmpty())
            nestedProjectList.add (groupGuid + String (" = ") + parentGuid);
    }

    //==============================================================================
    void addPlatformSpecificSettingsForProjectType (const ProjectType& type) override
    {
        MSVCProjectExporterBase::addPlatformSpecificSettingsForProjectType (type);

        callForAllSupportedTargets ([this] (ProjectType::Target::Type targetType)
                                    {
                                        if (MSVCTargetBase* target = new MSVC2010Target (targetType, *this))
                                        {
                                            if (targetType != ProjectType::Target::AggregateTarget)
                                                targets.add (target);
                                        }
                                    });

        // If you hit this assert, you tried to generate a project for an exporter
        // that does not support any of your targets!
        jassert (targets.size() > 0);
    }

    void create (const OwnedArray<LibraryModule>&) const override
    {
        createResourcesAndIcon();

        for (int i = 0; i < targets.size(); ++i)
            if (MSVCTargetBase* target = targets[i])
                target->writeProjectFile();

        {
            MemoryOutputStream mo;
            writeSolutionFile (mo, "11.00", getSolutionComment());

            overwriteFileIfDifferentOrThrow (getSLNFile(), mo);
        }
    }

protected:
    //==============================================================================
    class VC2010BuildConfiguration  : public MSVCBuildConfiguration
    {
    public:
        VC2010BuildConfiguration (Project& p, const ValueTree& settings, const ProjectExporter& e)
            : MSVCBuildConfiguration (p, settings, e)
        {
            if (getArchitectureType().toString().isEmpty())
                getArchitectureType() = get64BitArchName();
        }

        //==============================================================================
        static const char* get32BitArchName()   { return "32-bit"; }
        static const char* get64BitArchName()   { return "x64"; }

        Value getArchitectureType()             { return getValue (Ids::winArchitecture); }
        bool is64Bit() const                    { return config [Ids::winArchitecture].toString() == get64BitArchName(); }

        Value getFastMathValue()                { return getValue (Ids::fastMath); }
        bool isFastMathEnabled() const          { return config [Ids::fastMath]; }

        //==============================================================================
        void createConfigProperties (PropertyListBuilder& props) override
        {
            MSVCBuildConfiguration::createConfigProperties (props);

            const char* const archTypes[] = { get32BitArchName(), get64BitArchName() };

            props.add (new ChoicePropertyComponent (getArchitectureType(), "Architecture",
                                                    StringArray (archTypes, numElementsInArray (archTypes)),
                                                    Array<var> (archTypes, numElementsInArray (archTypes))));

            props.add (new BooleanPropertyComponent (getFastMathValue(), "Relax IEEE compliance", "Enabled"),
                       "Enable this to use FAST_MATH non-IEEE mode. (Warning: this can have unexpected results!)");
        }
    };

    virtual void addPlatformToolsetToPropertyGroup (XmlElement&) const {}

    void addIPPSettingToPropertyGroup (XmlElement& p) const
    {
        String ippLibrary = getIPPLibrary();

        if (ippLibrary.isNotEmpty())
            forEachXmlChildElementWithTagName (p, e, "PropertyGroup")
                e->createNewChildElement ("UseIntelIPP")->addTextElement (ippLibrary);
    }

    BuildConfiguration::Ptr createBuildConfig (const ValueTree& v) const override
    {
        return new VC2010BuildConfiguration (project, v, *this);
    }

    static bool is64Bit (const BuildConfiguration& config)
    {
        return dynamic_cast<const VC2010BuildConfiguration&> (config).is64Bit();
    }

    //==============================================================================
    File getVCProjFile() const            { return getProjectFile (".vcxproj", "App"); }
    File getVCProjFiltersFile() const     { return getProjectFile (".vcxproj.filters", String()); }

    JUCE_DECLARE_NON_COPYABLE (MSVCProjectExporterVC2010)
};

//==============================================================================
class MSVCProjectExporterVC2012 : public MSVCProjectExporterVC2010
{
public:
    MSVCProjectExporterVC2012 (Project& p, const ValueTree& t,
                               const char* folderName = "VisualStudio2012")
        : MSVCProjectExporterVC2010 (p, t, folderName)
    {
        name = getName();
    }

    static const char* getName()                { return "Visual Studio 2012"; }
    static const char* getValueTreeTypeName()   { return "VS2012"; }
    int getVisualStudioVersion() const override { return 11; }
    String getSolutionComment() const override  { return "# Visual Studio 2012"; }
    String getDefaultToolset() const override   { return "v110"; }

    static MSVCProjectExporterVC2012* createForSettings (Project& project, const ValueTree& settings)
    {
        if (settings.hasType (getValueTreeTypeName()))
            return new MSVCProjectExporterVC2012 (project, settings);

        return nullptr;
    }

    void createExporterProperties (PropertyListBuilder& props) override
    {
        MSVCProjectExporterBase::createExporterProperties (props);

        static const char* toolsetNames[] = { "(default)", "v110", "v110_xp", "Windows7.1SDK", "CTP_Nov2013" };
        const var toolsets[]              = { var(),       "v110", "v110_xp", "Windows7.1SDK", "CTP_Nov2013" };

        addToolsetProperty (props, toolsetNames, toolsets, numElementsInArray (toolsets));
        addIPPLibraryProperty (props);
    }

private:
    void addPlatformToolsetToPropertyGroup (XmlElement& p) const override
    {
        forEachXmlChildElementWithTagName (p, e, "PropertyGroup")
            e->createNewChildElement ("PlatformToolset")->addTextElement (getPlatformToolset());
    }

    JUCE_DECLARE_NON_COPYABLE (MSVCProjectExporterVC2012)
};

//==============================================================================
class MSVCProjectExporterVC2013  : public MSVCProjectExporterVC2012
{
public:
    MSVCProjectExporterVC2013 (Project& p, const ValueTree& t)
        : MSVCProjectExporterVC2012 (p, t, "VisualStudio2013")
    {
        name = getName();
    }

    static const char* getName()                { return "Visual Studio 2013"; }
    static const char* getValueTreeTypeName()   { return "VS2013"; }
    int getVisualStudioVersion() const override { return 12; }
    String getSolutionComment() const override  { return "# Visual Studio 2013"; }
    String getToolsVersion() const override     { return "12.0"; }
    String getDefaultToolset() const override   { return "v120"; }

    static MSVCProjectExporterVC2013* createForSettings (Project& project, const ValueTree& settings)
    {
        if (settings.hasType (getValueTreeTypeName()))
            return new MSVCProjectExporterVC2013 (project, settings);

        return nullptr;
    }

    void createExporterProperties (PropertyListBuilder& props) override
    {
        MSVCProjectExporterBase::createExporterProperties (props);

        static const char* toolsetNames[] = { "(default)", "v120", "v120_xp", "Windows7.1SDK", "CTP_Nov2013" };
        const var toolsets[]              = { var(),       "v120", "v120_xp", "Windows7.1SDK", "CTP_Nov2013" };

        addToolsetProperty (props, toolsetNames, toolsets, numElementsInArray (toolsets));
        addIPPLibraryProperty (props);
    }

    JUCE_DECLARE_NON_COPYABLE (MSVCProjectExporterVC2013)
};

//==============================================================================
class MSVCProjectExporterVC2015  : public MSVCProjectExporterVC2012
{
public:
    MSVCProjectExporterVC2015 (Project& p, const ValueTree& t)
        : MSVCProjectExporterVC2012 (p, t, "VisualStudio2015")
    {
        name = getName();
    }

    static const char* getName()                { return "Visual Studio 2015"; }
    static const char* getValueTreeTypeName()   { return "VS2015"; }
    int getVisualStudioVersion() const override { return 14; }
    String getSolutionComment() const override  { return "# Visual Studio 2015"; }
    String getToolsVersion() const override     { return "14.0"; }
    String getDefaultToolset() const override   { return "v140"; }

    static MSVCProjectExporterVC2015* createForSettings (Project& project, const ValueTree& settings)
    {
        if (settings.hasType (getValueTreeTypeName()))
            return new MSVCProjectExporterVC2015 (project, settings);

        return nullptr;
    }

    void createExporterProperties (PropertyListBuilder& props) override
    {
        MSVCProjectExporterBase::createExporterProperties (props);

        static const char* toolsetNames[] = { "(default)", "v140", "v140_xp", "CTP_Nov2013" };
        const var toolsets[]              = { var(),       "v140", "v140_xp", "CTP_Nov2013" };

        addToolsetProperty (props, toolsetNames, toolsets, numElementsInArray (toolsets));
        addIPPLibraryProperty (props);
    }

    JUCE_DECLARE_NON_COPYABLE (MSVCProjectExporterVC2015)
};
