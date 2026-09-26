# SPDX-License-Identifier: AGPL-3.0-or-later
<#
.SYNOPSIS
    把本机 TETRA 相关仓库中的源码内联(vendor)到 tetra-android 工程里。

.DESCRIPTION
    本工程不重复维护第三方源码，而是从 $TetraRoot 下的仓库复制/裁剪出
    Android 构建所需的最小集合：

      osmo-tetra-sq5bpf-2-master/src  ->  app/src/main/cpp/tetra  (TETRA 解码器)
      rtl-sdr-blog-master             ->  app/src/main/cpp/thirdparty/librtlsdr
      liquid-dsp-master               ->  app/src/main/cpp/thirdparty/liquid-dsp
      libusb-1.0.27 (下载或本地)      ->  app/src/main/cpp/thirdparty/libusb

    ETSI 参考语音编解码器由 tools/fetch_etsi_codec.ps1 单独获取(许可原因)。

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File tools/vendor_thirdparty.ps1
#>
[CmdletBinding()]
param(
    [string]$TetraRoot = '',
    [string]$ProjectRoot = ''
)

$ErrorActionPreference = 'Stop'
$ProgressPreference    = 'SilentlyContinue'

# Resolve the project layout from the location of this script
# (works with -File, dot sourcing and from any current directory).
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
if (-not $ProjectRoot) { $ProjectRoot = Split-Path -Parent $scriptDir }
if (-not $TetraRoot)   { $TetraRoot   = Split-Path -Parent $ProjectRoot }

$cpp = Join-Path $ProjectRoot 'app\src\main\cpp'
$tp  = Join-Path $cpp 'thirdparty'

function Say([string]$m) { Write-Host "==> $m" -ForegroundColor Cyan }

# ---------------------------------------------------------------- osmo-tetra
$osmo = Join-Path $TetraRoot 'osmo-tetra-sq5bpf-2-master\src'
if (-not (Test-Path $osmo)) { throw "找不到 osmo-tetra-sq5bpf-2-master: $osmo" }
Say "vendor TETRA decoder  <- $osmo"
$dst = Join-Path $cpp 'tetra'
if (Test-Path $dst) { Remove-Item -Recurse -Force $dst }
New-Item -ItemType Directory -Force -Path $dst | Out-Null

# 需要移植的目录(解码链路)以及顶层文件(不包含命令行 main()/tunctl/tuntap/gsmtap/UDP 调试)
Copy-Item (Join-Path $osmo 'phy')        (Join-Path $dst 'phy')        -Recurse
Copy-Item (Join-Path $osmo 'lower_mac')  (Join-Path $dst 'lower_mac')  -Recurse
Copy-Item (Join-Path $osmo 'crypto')     (Join-Path $dst 'crypto')     -Recurse

$topLevelFiles = @(
    'tetra_cmce_pdu.c', 'tetra_cmce_pdu.h',
    'tetra_common.c',   'tetra_common.h',
    'tetra_llc.c',      'tetra_llc.h',
    'tetra_llc_pdu.c',  'tetra_llc_pdu.h',
    'tetra_mac_pdu.c',  'tetra_mac_pdu.h',
    'tetra_mle.c',      'tetra_mle.h',
    'tetra_mle_pdu.c',  'tetra_mle_pdu.h',
    'tetra_mm_pdu.c',   'tetra_mm_pdu.h',
    'tetra_prim.h',
    'tetra_sndcp_pdu.c','tetra_sndcp_pdu.h',
    'tetra_tdma.c',     'tetra_tdma.h',
    'tetra_upper_mac.c','tetra_upper_mac.h'
)
foreach ($f in $topLevelFiles) {
    Copy-Item (Join-Path $osmo $f) (Join-Path $dst $f)
}

# 编码侧 helper: 只在主机自检 (modulate->demod->decode 回环) 中用到
foreach ($f in @('tetra_tdma.c', 'tetra_common.c')) {
    Copy-Item (Join-Path $osmo $f) (Join-Path $dst $f)
}

# --------------------------------------------------------------- librtlsdr
$rtl = Join-Path $TetraRoot 'rtl-sdr-blog-master'
if (-not (Test-Path $rtl)) { throw "找不到 rtl-sdr-blog-master: $rtl" }
Say "vendor librtlsdr     <- $rtl"
$dst = Join-Path $tp 'librtlsdr'
if (Test-Path $dst) { Remove-Item -Recurse -Force $dst }
New-Item -ItemType Directory -Force -Path (Join-Path $dst 'src')     | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $dst 'include') | Out-Null
Copy-Item (Join-Path $rtl 'include\rtl-sdr.h')        (Join-Path $dst 'include')
Copy-Item (Join-Path $rtl 'include\rtl-sdr_export.h') (Join-Path $dst 'include')
# internal driver headers (rtlsdr_i2c.h, tuner_*.h, reg_field.h) are included
# by the src/*.c files with quoted includes
Get-ChildItem (Join-Path $rtl 'include') -Filter '*.h' | ForEach-Object {
    Copy-Item $_.FullName (Join-Path $dst 'include') -Force
}
foreach ($f in @('librtlsdr.c', 'tuner_e4k.c', 'tuner_fc0012.c', 'tuner_fc0013.c',
                 'tuner_fc2580.c', 'tuner_r82xx.c')) {
    Copy-Item (Join-Path $rtl "src\$f") (Join-Path $dst 'src')
}
Copy-Item (Join-Path $PSScriptRoot 'librtlsdr_android.cmake') (Join-Path $dst 'CMakeLists.txt')
Copy-Item (Join-Path $PSScriptRoot 'librtlsdr_open_fd.patch') (Join-Path $dst 'open_fd.patch')

# 打上 rtlsdr_open_fd() 补丁(Android 必须由 Java 侧提供已打开的 fd)
Push-Location $dst
try {
    # 注意: 这里不要重定向 git 的输出(PowerShell 5.1 在
    # $ErrorActionPreference='Stop' 下会把 stderr 重定向当作致命错误)
    & git apply --check --quiet 'open_fd.patch'
    if ($LASTEXITCODE -eq 0) {
        & git apply 'open_fd.patch'
        if ($LASTEXITCODE -ne 0) { throw "无法应用 open_fd.patch (见上方 git 输出)" }
        Say "librtlsdr: rtlsdr_open_fd() 补丁已应用"
    } elseif (Select-String -Path (Join-Path $dst 'src\librtlsdr.c') -Pattern 'rtlsdr_open_fd' -Quiet) {
        # 已打过补丁时再次运行会走到这里
        Say "librtlsdr: rtlsdr_open_fd() 已存在, 跳过"
    } else {
        throw "无法应用 open_fd.patch (见上方 git 输出)"
    }
} finally { Pop-Location }

# --------------------------------------------------------------- liquid-dsp
$liquid = Join-Path $TetraRoot 'liquid-dsp-master'
if (-not (Test-Path $liquid)) { throw "找不到 liquid-dsp-master: $liquid" }
Say "vendor liquid-dsp    <- $liquid"
$dst = Join-Path $tp 'liquid-dsp'
if (Test-Path $dst) { Remove-Item -Recurse -Force $dst }
New-Item -ItemType Directory -Force -Path $dst | Out-Null
Copy-Item (Join-Path $liquid 'include') (Join-Path $dst 'include') -Recurse
Copy-Item (Join-Path $liquid 'cmake')   (Join-Path $dst 'cmake')   -Recurse
Copy-Item (Join-Path $liquid 'src')     (Join-Path $dst 'src')     -Recurse
# 去掉单元测试/示例数据，只保留库本体
Get-ChildItem -Path (Join-Path $dst 'src') -Recurse -Directory -Filter 'tests' |
    Remove-Item -Recurse -Force
Copy-Item (Join-Path $PSScriptRoot 'liquid_android.cmake') (Join-Path $dst 'CMakeLists.txt')

# ------------------------------------------------------------------ libusb
$dst = Join-Path $tp 'libusb'
if (Test-Path (Join-Path $dst 'libusb\core.c')) {
    Say "vendor libusb: 已存在, 跳过"
} else {
    $ver  = '1.0.27'
    $tarb = Join-Path $env:TEMP "libusb-$ver.tar.bz2"
    if (-not (Test-Path $tarb)) {
        $url = "https://github.com/libusb/libusb/releases/download/v$ver/libusb-$ver.tar.bz2"
        Say "下载 libusb $ver ..."
        Invoke-WebRequest -Uri $url -OutFile $tarb -UseBasicParsing
    }
    Say "解包 libusb -> $dst"
    if (Test-Path $dst) { Remove-Item -Recurse -Force $dst }
    New-Item -ItemType Directory -Force -Path $dst | Out-Null
    tar -xjf $tarb -C $dst --strip-components=1
    Copy-Item (Join-Path $PSScriptRoot 'libusb_android.cmake') (Join-Path $dst 'CMakeLists.txt')
    Copy-Item (Join-Path $PSScriptRoot 'libusb_config_android.h') (Join-Path $dst 'config.h')
}

Say "完成。"
