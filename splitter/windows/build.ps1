<#
.SYNOPSIS
    Builds the Splitter driver and its install package (a zip).

.DESCRIPTION
    Needs Visual Studio 2022 or later with the C++ desktop workload and the
    Spectre-mitigated libraries, and nuget.exe. The Windows Driver Kit comes
    from NuGet (driver/packages.config); no WDK install is needed.

      1. restores the WDK/SDK NuGet packages,
      2. builds driver/Splitter.sln (Release, x64), unsigned,
      3. runs InfVerif on splitter.inf,
      4. signs the catalog and the driver with a test certificate (a new
         self-signed one, or -CertificatePfx), unless -NoTestSign,
      5. puts driver, installer scripts, licences and the certificate in
         dist/splitter-<version>-windows-x64/ and zips it.

    A test-signed driver loads only on a PC in test-signing mode; see
    README.md ("Signing"). A Microsoft-signed build replaces the files in
    driver/ of the package and drops the certificate.

.EXAMPLE
    pwsh -File splitter/windows/build.ps1
#>
[CmdletBinding()]
param(
    [string] $CertificatePfx,
    [string] $CertificatePassword,
    [switch] $NoTestSign
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$here = $PSScriptRoot
$driverDir = Join-Path $here 'driver'
$version = (Get-Content (Join-Path $here '..\VERSION') -Raw).Trim()
if ($version -notmatch '^\d+\.\d+\.\d+$') { throw "splitter/VERSION must be x.y.z, not '$version'" }

function Find-Tool([string] $name, [string] $under) {
    $hit = Get-ChildItem -Path $under -Recurse -Filter $name -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match '\\x64\\' -or $_.FullName -match '\\amd64\\' } |
        Sort-Object FullName -Descending | Select-Object -First 1
    if (-not $hit) { throw "$name not found under $under" }
    return $hit.FullName
}

# 1. WDK and SDK from NuGet
Push-Location $driverDir
try {
    & nuget restore packages.config -PackagesDirectory packages -NonInteractive
    if ($LASTEXITCODE -ne 0) { throw "nuget restore failed ($LASTEXITCODE)" }
} finally { Pop-Location }
$packages = Join-Path $driverDir 'packages'

# Driver versions must match everywhere they are written.
$inx = Get-Content (Join-Path $driverDir 'Source\Main\Splitter.inx') -Raw
$rc = Get-Content (Join-Path $driverDir 'Source\Main\Splitter.rc') -Raw
$props = Get-Content (Join-Path $driverDir 'Directory.Build.props') -Raw
$v4 = "$version.0"
if ($inx -notmatch "DriverVer\s*=\s*[\d/]+,$([regex]::Escape($v4))") { throw "Splitter.inx DriverVer is not $v4" }
if ($rc -notmatch "SPLITTER_VERSION_STR\s+""$([regex]::Escape($v4))""") { throw "Splitter.rc is not $v4" }
if ($props -notmatch "<TimeStamp>$([regex]::Escape($v4))</TimeStamp>") { throw "Directory.Build.props TimeStamp is not $v4" }

# 2. Build
$msbuildArgs = @(
    (Join-Path $driverDir 'Splitter.sln'),
    '-t:rebuild', '-m',
    '-p:Configuration=Release', '-p:Platform=x64',
    '-p:SignMode=Off',
    '-warnaserror', '-nologo', '-clp:Verbosity=minimal'
)
& msbuild @msbuildArgs
if ($LASTEXITCODE -ne 0) { throw "msbuild failed ($LASTEXITCODE)" }

$inf = Get-ChildItem -Path $driverDir -Recurse -Filter 'splitter.inf' |
    Where-Object { $_.FullName -match '\\Release\\' -and $_.FullName -notmatch '\\packages\\' } |
    Sort-Object { $_.FullName -match '\\package\\' } -Descending | Select-Object -First 1
if (-not $inf) { throw 'the build made no splitter.inf' }
$built = $inf.Directory.FullName
$sys = Join-Path $built 'splitter.sys'
if (-not (Test-Path $sys)) {
    $s = Get-ChildItem -Path $driverDir -Recurse -Filter 'splitter.sys' |
        Where-Object { $_.FullName -match '\\Release\\' } | Select-Object -First 1
    if (-not $s) { throw 'the build made no splitter.sys' }
    $sys = $s.FullName
}
Write-Host "Built: $($inf.FullName)"
Write-Host "Built: $sys"
Get-Content $inf.FullName | Select-String -Pattern '^\s*DriverVer' | ForEach-Object { Write-Host "  $_" }

# 3. InfVerif (the same checks the Hardware Dev Center runs for desktop
#    drivers, /w = Windows Driver requirements).
$infverif = Find-Tool 'infverif.exe' $packages
& $infverif /v /w $inf.FullName
if ($LASTEXITCODE -ne 0) { throw "InfVerif failed ($LASTEXITCODE)" }

# 5 (layout first, signing works on the copies).
$name = "splitter-$version-windows-x64"
$dist = Join-Path $here 'dist'
$out = Join-Path $dist $name
if (Test-Path $out) { Remove-Item $out -Recurse -Force }
New-Item -ItemType Directory -Path (Join-Path $out 'driver') -Force | Out-Null
Copy-Item $inf.FullName, $sys (Join-Path $out 'driver')

# Catalog for exactly these two files.
$inf2cat = Find-Tool 'inf2cat.exe' $packages
$drvOut = Join-Path $out 'driver'
& $inf2cat "/driver:$drvOut" '/os:10_VB_X64,10_CO_X64,10_NI_X64' /uselocaltime
if ($LASTEXITCODE -ne 0) { throw "Inf2Cat failed ($LASTEXITCODE)" }
if (-not (Test-Path (Join-Path $out 'driver\splitter.cat'))) { throw 'Inf2Cat made no splitter.cat' }

# 4. Test signing
if (-not $NoTestSign) {
    $signtool = Find-Tool 'signtool.exe' $packages
    if ($CertificatePfx) {
        $sec = ConvertTo-SecureString -String $CertificatePassword -AsPlainText -Force
        $cert = Import-PfxCertificate -FilePath $CertificatePfx -Password $sec -CertStoreLocation Cert:\CurrentUser\My
    } else {
        $cert = New-SelfSignedCertificate -Type CodeSigningCert -Subject 'CN=Splitter test signing (BananaSplit)' `
            -CertStoreLocation Cert:\CurrentUser\My -KeyUsage DigitalSignature -KeyExportPolicy Exportable `
            -NotAfter (Get-Date).AddYears(3)
    }
    foreach ($f in 'splitter.sys', 'splitter.cat') {
        & $signtool sign /fd sha256 /sha1 $cert.Thumbprint /s My (Join-Path $out "driver\$f")
        if ($LASTEXITCODE -ne 0) { throw "signtool failed on $f ($LASTEXITCODE)" }
    }
    Export-Certificate -Cert $cert -FilePath (Join-Path $out 'splitter-test.cer') -Type CERT | Out-Null
    if (-not $CertificatePfx) { Remove-Item "Cert:\CurrentUser\My\$($cert.Thumbprint)" }
}

# Installer, licences, read-me
Copy-Item (Join-Path $here 'install\*') $out
Copy-Item (Join-Path $here '..\LICENSE') (Join-Path $out 'LICENSE.txt')
Copy-Item (Join-Path $driverDir 'LICENSE-MS-PL.txt') $out
(Get-Content (Join-Path $here 'install\README.txt') -Raw).Replace('@VERSION@', $version) |
    Set-Content -Path (Join-Path $out 'README.txt') -Encoding utf8

$zip = Join-Path $dist "$name.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path $out -DestinationPath $zip
Write-Host "Package: $zip"
Get-ChildItem $out -Recurse | ForEach-Object { Write-Host "  $($_.FullName.Substring($out.Length + 1))" }
