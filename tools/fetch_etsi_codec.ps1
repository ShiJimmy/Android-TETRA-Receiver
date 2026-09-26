# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Shi Jimmy
<#
.SYNOPSIS
    下载 ETSI EN 300 395-2 TETRA 语音编解码器参考源码, 打补丁, 并放入 Android 工程。

.DESCRIPTION
    ETSI 的参考源码不能随本工程一起分发(许可原因), 所以这里在构建前按需获取:

      1. 下载 en_30039502v010301p0.zip (校验 MD5 a8115fe68ef8f8cc466f4192572a1e3e)
      2. 解压并把文件名统一转成小写(unzip -L 的等价操作)
      3. 套用 osmo-tetra-sq5bpf-2 提供的补丁系列(etsi_codec-patches/series)
      4. 额外把 channel.h 里的 Word16/Word32 固定为 int16_t/int32_t
         (上游 fix_64bit.patch 只改了 source.h, 在 64 位 ABI 下两处定义
           不一致会导致函数调用约定不匹配)
      5. 复制到 app/src/main/cpp/thirdparty/etsi_codec/

    结果:
      thirdparty/etsi_codec/amr-code/  信道编解码 (cdecoder 侧)
      thirdparty/etsi_codec/c-code/    语音编解码 (scoder/sdecoder 侧)

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File tools/fetch_etsi_codec.ps1
#>
[CmdletBinding()]
param(
    [string]$TetraRoot = '',
    [string]$ProjectRoot = '',
    [switch]$Force
)

$ErrorActionPreference = 'Stop'
$ProgressPreference    = 'SilentlyContinue'

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
if (-not $ProjectRoot) { $ProjectRoot = Split-Path -Parent $scriptDir }
if (-not $TetraRoot)   { $TetraRoot   = Split-Path -Parent $ProjectRoot }

$URL      = 'http://www.etsi.org/deliver/etsi_en/300300_300399/30039502/01.03.01_60/en_30039502v010301p0.zip'
$MD5_EXP  = 'A8115FE68EF8F8CC466F4192572A1E3E'
$DLDIR    = Join-Path $ProjectRoot 'thirdparty-src'
$ZIP      = Join-Path $DLDIR 'etsi_tetra_codec.zip'
$TMPDIR   = Join-Path $DLDIR 'extract'
$DST      = Join-Path $ProjectRoot 'app\src\main\cpp\thirdparty\etsi_codec'
$PATCHDIR = Join-Path $TetraRoot 'osmo-tetra-sq5bpf-2-master\etsi_codec-patches'

function Say([string]$m) { Write-Host "==> $m" -ForegroundColor Cyan }

if ((Test-Path (Join-Path $DST 'c-code\sdec_tet.c')) -and -not $Force) {
    Say "ETSI codec 已存在: $DST (需要重新生成请加 -Force)"
    exit 0
}
if (-not (Test-Path $PATCHDIR)) { throw "找不到补丁目录: $PATCHDIR" }

New-Item -ItemType Directory -Force -Path $DLDIR | Out-Null

# ------------------------------------------------------------------ 1. 下载
if (-not (Test-Path $ZIP)) {
    Say "下载 ETSI 参考编解码器 ..."
    Invoke-WebRequest -Uri $URL -OutFile $ZIP -UseBasicParsing
}
$md5 = (Get-FileHash $ZIP -Algorithm MD5).Hash
if ($md5 -ne $MD5_EXP) { throw "MD5 校验失败: $md5 != $MD5_EXP" }
Say "MD5 校验通过 ($md5)"

# ------------------------------------------------------------------ 2. 解压 + 小写化
Say "解压 ..."
if (Test-Path $TMPDIR) { Remove-Item -Recurse -Force $TMPDIR }
Expand-Archive -Path $ZIP -DestinationPath $TMPDIR -Force

function Rename-ToLower([string]$path) {
    $items = Get-ChildItem -Recurse -Force $path | Sort-Object { $_.FullName.Length } -Descending
    foreach ($it in $items) {
        $lower = $it.Name.ToLowerInvariant()
        if ($lower -cne $it.Name) {
            $tmp   = Join-Path $it.Parent.FullName ('__lc__' + $lower)
            $final = Join-Path $it.Parent.FullName $lower
            Move-Item -LiteralPath $it.FullName -Destination $tmp   -Force
            Move-Item -LiteralPath $tmp            -Destination $final -Force
        }
    }
}
Rename-ToLower $TMPDIR

# ------------------------------------------------------------------ 3. 打补丁
$series = Get-Content (Join-Path $PATCHDIR 'series')
Push-Location $TMPDIR
try {
    foreach ($p in $series) {
        $pf = Join-Path $PATCHDIR $p
        Say "应用补丁 $p"
        # 不重定向 git 的 stderr: PowerShell 5.1 + EAP=Stop 会把重定向当成致命错误
        & git apply -p1 --quiet $pf
        if ($LASTEXITCODE -ne 0) { throw "补丁失败 $p (见上方 git 输出)" }
    }
} finally { Pop-Location }

# --------------------------------------------- 4. 统一 64 位下的定点类型
foreach ($h in @('amr-code\channel.h', 'c-code\channel.h')) {
    $f = Join-Path $TMPDIR $h
    if (-not (Test-Path $f)) { continue }
    $txt = [System.IO.File]::ReadAllText($f)
    $txt = [regex]::Replace($txt,
        'typedef\s+short\s+Word16;\s*\r?\n\s*typedef\s+long\s+Word32;',
        "#include <stdint.h>`r`ntypedef int16_t Word16;`r`ntypedef int32_t Word32;")
    [System.IO.File]::WriteAllText($f, $txt)
    Say "修正 $h 的 Word16/Word32 类型"
}

# ------------------------------------------------------------------ 5. 安装
Say "复制到 $DST"
if (Test-Path $DST) { Remove-Item -Recurse -Force $DST }
New-Item -ItemType Directory -Force -Path $DST | Out-Null
foreach ($d in @('amr-code', 'c-code')) {
    Copy-Item (Join-Path $TMPDIR $d) (Join-Path $DST $d) -Recurse
}
Copy-Item (Join-Path $PATCHDIR 'series') $DST
New-Item -ItemType Directory -Force -Path (Join-Path $DST 'patches') | Out-Null
foreach ($p in $series) { Copy-Item (Join-Path $PATCHDIR $p) (Join-Path $DST 'patches') }

@"
ETSI EN 300 395-2 TETRA speech codec reference software
=======================================================

Source  : $URL
Archive : en_30039502v010301p0.zip (md5 $MD5_EXP)
Fetched : $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')
Command : tools/fetch_etsi_codec.ps1

Patches : tools/../patches (from osmo-tetra-sq5bpf-2/etsi_codec-patches):
$(($series | ForEach-Object { "          - $_" }) -join "`r`n")
          - channel.h Word16/Word32 -> int16_t/int32_t (tetra-android addition)

The TETRA codec is NOT covered by this project's licence and is not
redistributed with it: the archive is downloaded from ETSI on demand.
Please read the licence notes shipped inside the archive
(C-WORD/C_WORD_*.DOC) before using or redistributing the codec.
"@ | Set-Content -Path (Join-Path $DST 'PROVENANCE.txt') -Encoding utf8

Say "完成: $DST"
