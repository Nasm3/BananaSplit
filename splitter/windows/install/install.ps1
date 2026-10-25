<#
.SYNOPSIS
    Installs (or updates) the Splitter virtual cables: Splitter 1 to 6.

.DESCRIPTION
    Run install.cmd, which asks for administrator rights and runs this.

    Splitter is one virtual device ("Splitter Virtual Cables", hardware id
    ROOT\Splitter) with six cables. Cable N is a playback device "Splitter N"
    that apps play into and a recording device "Splitter N Output" that
    BananaSplit's input strip N records.

    This script:
      - checks Windows (10 version 2004 or later, 64-bit);
      - for a test-signed package (splitter-test.cer next to this script):
        checks that test signing is on and trusts the certificate on this PC;
      - adds the driver to the driver store and creates the device, or
        updates the driver if Splitter is already installed;
      - lists the cables Windows now shows.

    It changes nothing else: no boot settings, no default devices.
#>
[CmdletBinding()]
param(
    # Install a test-signed package even though test signing looks off
    # (Windows will then refuse to start the driver).
    [switch] $Force
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$here = $PSScriptRoot
$inf = Join-Path $here 'driver\splitter.inf'
$hwid = 'ROOT\Splitter'

function Fail([string] $text) {
    Write-Host ''
    Write-Host $text -ForegroundColor Red
    exit 1
}

# --- checks ----------------------------------------------------------------
$principal = New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Fail 'Run install.cmd (it asks for administrator rights).'
}
if (-not [Environment]::Is64BitOperatingSystem) { Fail 'Splitter needs 64-bit Windows.' }
$build = [Environment]::OSVersion.Version.Build
if ($build -lt 19041) { Fail "Splitter needs Windows 10 version 2004 (build 19041) or later; this is build $build." }
if (-not (Test-Path $inf)) { Fail "driver\splitter.inf is missing next to this script." }

$cer = Join-Path $here 'splitter-test.cer'
if (Test-Path $cer) {
    # A test build: Windows loads it only in test-signing mode.
    # The options this boot started with (the same in every language,
    # unlike bcdedit's output): TESTSIGNING is there in test-signing mode.
    $start = (Get-ItemProperty 'HKLM:\SYSTEM\CurrentControlSet\Control' -Name SystemStartOptions -ErrorAction SilentlyContinue).SystemStartOptions
    $testsigning = "$start" -match '\bTESTSIGNING\b'
    if (-not $testsigning -and -not $Force) {
        Fail (@(
            'This is a TEST build of Splitter (signed with a test certificate, not by Microsoft).',
            'Windows only loads it in test-signing mode, which this PC is not in.',
            '',
            'To try it anyway (only on a PC you can do this on: it needs Secure Boot off,',
            'and some games'' anti-cheat refuse to run in test mode):',
            '  1. run, as administrator:  bcdedit /set testsigning on',
            '  2. restart the PC, then run install.cmd again.',
            'To leave test mode later: bcdedit /set testsigning off, and restart.',
            '',
            'Or wait for a Microsoft-signed release of Splitter, or use VB-CABLE meanwhile.'
        ) -join [Environment]::NewLine)
    }
    Write-Host 'Trusting the Splitter test certificate on this PC...'
    foreach ($store in 'Root', 'TrustedPublisher') {
        Import-Certificate -FilePath $cer -CertStoreLocation "Cert:\LocalMachine\$store" | Out-Null
    }
}

# --- install ---------------------------------------------------------------
Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;

public static class SplitterSetup
{
    [StructLayout(LayoutKind.Sequential)]
    struct SP_DEVINFO_DATA
    {
        public uint cbSize;
        public Guid ClassGuid;
        public uint DevInst;
        public IntPtr Reserved;
    }

    const uint DICD_GENERATE_ID = 0x1;
    const uint SPDRP_HARDWAREID = 0x1;
    const uint DIF_REGISTERDEVICE = 0x19;
    const uint DIF_REMOVE = 0x5;
    const uint INSTALLFLAG_FORCE = 0x1;
    static readonly IntPtr INVALID_HANDLE_VALUE = new IntPtr(-1);

    [DllImport("setupapi.dll", SetLastError = true)]
    static extern IntPtr SetupDiCreateDeviceInfoList(ref Guid classGuid, IntPtr hwndParent);

    [DllImport("setupapi.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    static extern bool SetupDiCreateDeviceInfoW(IntPtr set, string deviceName, ref Guid classGuid,
        string description, IntPtr hwndParent, uint flags, ref SP_DEVINFO_DATA data);

    [DllImport("setupapi.dll", SetLastError = true)]
    static extern bool SetupDiSetDeviceRegistryPropertyW(IntPtr set, ref SP_DEVINFO_DATA data,
        uint property, byte[] buffer, uint size);

    [DllImport("setupapi.dll", SetLastError = true)]
    static extern bool SetupDiCallClassInstaller(uint function, IntPtr set, ref SP_DEVINFO_DATA data);

    [DllImport("setupapi.dll", SetLastError = true)]
    static extern bool SetupDiDestroyDeviceInfoList(IntPtr set);

    [DllImport("newdev.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    static extern bool UpdateDriverForPlugAndPlayDevicesW(IntPtr hwndParent, string hardwareId,
        string fullInfPath, uint flags, out bool rebootRequired);

    // Media class: {4d36e96c-e325-11ce-bfc1-08002be10318}
    static Guid MediaClass = new Guid("4d36e96c-e325-11ce-bfc1-08002be10318");

    // Creates the root-enumerated device (what "devcon install" does) and
    // installs the driver on it. If installing fails, the new device is
    // removed again, so no driverless device is left behind.
    public static bool CreateAndInstall(string hardwareId, string infPath)
    {
        Guid cls = MediaClass;
        IntPtr set = SetupDiCreateDeviceInfoList(ref cls, IntPtr.Zero);
        if (set == INVALID_HANDLE_VALUE) throw new Win32Exception();
        try
        {
            SP_DEVINFO_DATA data = new SP_DEVINFO_DATA();
            data.cbSize = (uint)Marshal.SizeOf(typeof(SP_DEVINFO_DATA));
            if (!SetupDiCreateDeviceInfoW(set, "MEDIA", ref cls, null, IntPtr.Zero, DICD_GENERATE_ID, ref data))
                throw new Win32Exception();
            byte[] ids = System.Text.Encoding.Unicode.GetBytes(hardwareId + "\0\0");
            if (!SetupDiSetDeviceRegistryPropertyW(set, ref data, SPDRP_HARDWAREID, ids, (uint)ids.Length))
                throw new Win32Exception();
            if (!SetupDiCallClassInstaller(DIF_REGISTERDEVICE, set, ref data))
                throw new Win32Exception();
            bool reboot;
            if (!UpdateDriverForPlugAndPlayDevicesW(IntPtr.Zero, hardwareId, infPath, INSTALLFLAG_FORCE, out reboot))
            {
                int err = Marshal.GetLastWin32Error();
                SetupDiCallClassInstaller(DIF_REMOVE, set, ref data);
                throw new Win32Exception(err);
            }
            return reboot;
        }
        finally
        {
            SetupDiDestroyDeviceInfoList(set);
        }
    }

    // Puts the driver from infPath on the existing device(s).
    public static bool Update(string hardwareId, string infPath)
    {
        bool reboot;
        if (!UpdateDriverForPlugAndPlayDevicesW(IntPtr.Zero, hardwareId, infPath, INSTALLFLAG_FORCE, out reboot))
            throw new Win32Exception();
        return reboot;
    }
}
'@

function Get-SplitterDevices {
    @(Get-CimInstance Win32_PnPEntity | Where-Object { $_.HardwareID -contains $hwid })
}

$existing = Get-SplitterDevices
try {
    if ($existing.Count -gt 0) {
        Write-Host 'Splitter is installed already: updating its driver...'
        $reboot = [SplitterSetup]::Update($hwid, $inf)
    } else {
        Write-Host 'Installing Splitter (Splitter 1 to 6)...'
        $reboot = [SplitterSetup]::CreateAndInstall($hwid, $inf)
    }
} catch {
    Fail ("Installing the driver failed: " + $_.Exception.Message +
        [Environment]::NewLine + 'Details: C:\Windows\INF\setupapi.dev.log')
}

# --- report ----------------------------------------------------------------
$dev = Get-SplitterDevices | Select-Object -First 1
if ($dev -and $dev.ConfigManagerErrorCode -ne 0) {
    Write-Host ''
    Write-Host ("The Splitter device reports problem code {0}." -f $dev.ConfigManagerErrorCode) -ForegroundColor Yellow
    if ($dev.ConfigManagerErrorCode -eq 52) {
        Write-Host 'Code 52: Windows does not accept the driver''s signature (test signing off?).' -ForegroundColor Yellow
    }
}

Start-Sleep -Seconds 2
$endpoints = @(Get-CimInstance Win32_PnPEntity -Filter "PNPClass = 'AudioEndpoint'" |
    Where-Object { $_.Name -like '*(Splitter Virtual Cables)' } | Sort-Object Name)
Write-Host ''
if ($endpoints.Count -gt 0) {
    Write-Host 'Splitter cables:'
    foreach ($e in $endpoints) { Write-Host "  $($e.Name)" }
    Write-Host ''
    Write-Host 'In an app, choose "Splitter N" as its output to send it to BananaSplit''s input strip N.'
} else {
    Write-Host 'Splitter is installed; its cables appear in the sound settings in a moment.'
}
if ($reboot) { Write-Host 'Windows asks for a restart to finish.' -ForegroundColor Yellow }
exit 0
