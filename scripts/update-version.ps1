param(
    [string]$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$ProjectRoot = $ProjectRoot.Trim().Trim('"').TrimEnd('\', '/')
$ProjectRoot = [System.IO.Path]::GetFullPath($ProjectRoot).TrimEnd('\', '/')

function Get-GitOutput {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Arguments
    )

    $git = Get-Command git -ErrorAction SilentlyContinue
    if (-not $git) {
        return $null
    }

    $argumentList = @('-C', $ProjectRoot) + $Arguments
    $startInfo = New-Object System.Diagnostics.ProcessStartInfo
    $startInfo.FileName = $git.Source
    $startInfo.WorkingDirectory = $ProjectRoot
    $startInfo.UseShellExecute = $false
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $quotedArguments = $argumentList | ForEach-Object {
            if ($_ -match '\s') {
                '"' + $_ + '"'
        }
        else {
            $_
        }
    }
    $startInfo.Arguments = [string]::Join(' ', $quotedArguments)

    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $startInfo
    [void]$process.Start()
    $stdout = $process.StandardOutput.ReadToEnd()
    $stderr = $process.StandardError.ReadToEnd()
    $process.WaitForExit()

    if ($process.ExitCode -ne 0) {
        return $null
    }

    return $stdout.Trim()
}

function Get-VersionInfo {
    $exactTag = Get-GitOutput -Arguments @('describe', '--tags', '--exact-match', 'HEAD')
    if ($exactTag -and $exactTag -match '^[vV]?(?<core>\d+\.\d+\.\d+)(?<suffix>[-+].+)?$') {
        return @{
            DisplayVersion = "v$($Matches.core)$($Matches.suffix)"
            ManifestVersion = "$($Matches.core).0"
            Source = "exact-tag:$exactTag"
        }
    }

    # Use describe to get the nearest tag and commit info. 
    # This will be used for dev builds and CI builds on non-tag commits.
    $describe = Get-GitOutput -Arguments @('describe', '--tags', '--long', '--dirty', '--always', '--match', 'v[0-9]*')
    if ($describe -and $describe -match '^[vV]?(?<core>\d+\.\d+\.\d+)-(?<count>\d+)-g(?<sha>[0-9a-f]+)(?<dirty>-dirty)?$') {
        $buildMetadata = "$($Matches.count).g$($Matches.sha)"
        if ($Matches.dirty) {
            $buildMetadata += '.dirty'
        }

        return @{
            DisplayVersion = "v$($Matches.core)-dev+$buildMetadata"
            ManifestVersion = "$($Matches.core).0"
            Source = "describe:$describe"
        }
    }

    # fallbacks when no tags are found.
    $shortSha = Get-GitOutput -Arguments @('rev-parse', '--short', 'HEAD')
    if ($shortSha) {
        return @{
            DisplayVersion = "0.0.0-dev+$shortSha"
            ManifestVersion = '0.0.0.0'
            Source = "sha:$shortSha"
        }
    }

    return @{
        DisplayVersion = '0.0.0-dev'
        ManifestVersion = '0.0.0.0'
        Source = 'fallback'
    }
}

$versionInfo = Get-VersionInfo

$manifestPath = Join-Path $ProjectRoot 'app.manifest'
[xml]$manifestXml = Get-Content -Path $manifestPath -Raw -Encoding UTF8
$namespaceManager = New-Object System.Xml.XmlNamespaceManager($manifestXml.NameTable)
$namespaceManager.AddNamespace('asmv1', 'urn:schemas-microsoft-com:asm.v1')
$assemblyIdentity = $manifestXml.SelectSingleNode('/asmv1:assembly/asmv1:assemblyIdentity', $namespaceManager)
$xmlDeclaration = $manifestXml.FirstChild -as [System.Xml.XmlDeclaration]

if (-not $assemblyIdentity) {
    throw 'assemblyIdentity element not found in app.manifest'
}

$assemblyIdentity.SetAttribute('version', $versionInfo.ManifestVersion)
if ($xmlDeclaration) {
    $xmlDeclaration.Encoding = 'utf-8'
}

$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$xmlSettings = New-Object System.Xml.XmlWriterSettings
$xmlSettings.Encoding = $utf8NoBom
$xmlSettings.Indent = $true
$xmlSettings.OmitXmlDeclaration = $false

$memoryStream = New-Object System.IO.MemoryStream
$xmlWriter = [System.Xml.XmlWriter]::Create($memoryStream, $xmlSettings)
$manifestXml.Save($xmlWriter)
$xmlWriter.Close()
[System.IO.File]::WriteAllBytes($manifestPath, $memoryStream.ToArray())

$generatedDir = Join-Path $ProjectRoot 'Generated Files'
if (-not (Test-Path $generatedDir)) {
    New-Item -Path $generatedDir -ItemType Directory | Out-Null
}

$headerPath = Join-Path $generatedDir 'AppVersion.g.h'
$headerContent = @"
#pragma once

inline constexpr wchar_t AppVersion[] = L"$($versionInfo.DisplayVersion)";
"@

Set-Content -Path $headerPath -Value $headerContent -Encoding UTF8

Write-Host "Generated version info: $($versionInfo.DisplayVersion) -> $($versionInfo.ManifestVersion) [$($versionInfo.Source)]"