using UnrealBuildTool;
using System.IO;

public class VARCO3DPlugin : ModuleRules
{
    public VARCO3DPlugin(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "Sockets",
            "Networking",
            "HTTP",
            "Json",
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "CoreUObject",
            "Engine",
            "Slate",
            "SlateCore",
            "UnrealEd",
            "AssetTools",
            "AssetRegistry",
            "MaterialEditor",
            "LevelEditor",
            "Projects",
            "ToolMenus",
        });

        // zlib: linked directly to bypass the "zlib" UBT module, which is excluded
        // in installed (binary) engine builds for project plugins.
        // We only need zlib.h (for inflate) and zlibstatic.lib.
        //
        // The zlib version folder varies by engine version (e.g. "1.2.13", "1.3").
        // Find the actual version directory by looking for one that contains "include/zlib.h".
        string ZlibBase = Path.Combine(EngineDirectory, "Source", "ThirdParty", "zlib");
        string ZlibVer  = FindZlibVersion(ZlibBase);
        string ZlibInc  = Path.Combine(ZlibBase, ZlibVer, "include");

        PublicSystemIncludePaths.Add(ZlibInc);

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            string LibPath = Path.Combine(ZlibBase, ZlibVer, "lib", "Win64", "Release", "zlibstatic.lib");
            if (!File.Exists(LibPath))
                LibPath = Path.Combine(ZlibBase, ZlibVer, "lib", "Win64", "VS2015", "x64", "zlibstatic.lib");
            PublicAdditionalLibraries.Add(LibPath);
        }
    }

    /// <summary>
    /// Finds the zlib version subdirectory under the engine's ThirdParty/zlib.
    /// Looks for a subdirectory containing "include/zlib.h".
    /// </summary>
    private static string FindZlibVersion(string ZlibBase)
    {
        if (Directory.Exists(ZlibBase))
        {
            foreach (string dir in Directory.GetDirectories(ZlibBase))
            {
                if (File.Exists(Path.Combine(dir, "include", "zlib.h")))
                    return Path.GetFileName(dir);
            }
        }
        // Fallback for unknown engine layouts
        return "1.3";
    }
}
