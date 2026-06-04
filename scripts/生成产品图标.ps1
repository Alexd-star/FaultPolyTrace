param(
    [string]$OutputDir = $(Join-Path (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")) "assets\icons")
)

$ErrorActionPreference = "Stop"

Add-Type -AssemblyName System.Drawing

function New-FaultPolyTraceBitmap {
    param([int]$Size)

    $bmp = New-Object System.Drawing.Bitmap $Size, $Size, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $g.Clear([System.Drawing.Color]::Transparent)

    $scale = $Size / 64.0
    function S([double]$v) { return [float]($v * $scale) }

    $bgRect = New-Object System.Drawing.RectangleF (S 5), (S 5), (S 54), (S 54)
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $radius = S 9
    $diam = $radius * 2
    $path.AddArc($bgRect.X, $bgRect.Y, $diam, $diam, 180, 90)
    $path.AddArc($bgRect.Right - $diam, $bgRect.Y, $diam, $diam, 270, 90)
    $path.AddArc($bgRect.Right - $diam, $bgRect.Bottom - $diam, $diam, $diam, 0, 90)
    $path.AddArc($bgRect.X, $bgRect.Bottom - $diam, $diam, $diam, 90, 90)
    $path.CloseFigure()

    $brush = [System.Drawing.Drawing2D.LinearGradientBrush]::new(
        $bgRect,
        [System.Drawing.Color]::FromArgb(255, 245, 251, 255),
        [System.Drawing.Color]::FromArgb(255, 187, 222, 247),
        [System.Drawing.Drawing2D.LinearGradientMode]::ForwardDiagonal
    )
    $g.FillPath($brush, $path)
    $g.DrawPath((New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(255, 41, 117, 184)), (S 2.2)), $path)

    $gridPen = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(105, 35, 125, 195)), (S 1.25)
    foreach ($x in @(16, 26, 36, 46)) {
        $g.DrawLine($gridPen, (S $x), (S 9), (S ($x - 3)), (S 55))
    }
    foreach ($y in @(17, 27, 37, 47)) {
        $g.DrawLine($gridPen, (S 9), (S $y), (S 55), (S ($y + 2)))
    }

    $p0 = [pscustomobject]@{ X = 15.0; Y = 47.0 }
    $p1 = [pscustomobject]@{ X = 22.0; Y = 31.0 }
    $p2 = [pscustomobject]@{ X = 37.0; Y = 34.0 }
    $p3 = [pscustomobject]@{ X = 49.0; Y = 15.0 }
    function BezierPoint([double]$t) {
        $u = 1.0 - $t
        $b0 = $u * $u * $u
        $b1 = 3.0 * $u * $u * $t
        $b2 = 3.0 * $u * $t * $t
        $b3 = $t * $t * $t
        return [pscustomobject]@{
            X = ($b0 * $p0.X) + ($b1 * $p1.X) + ($b2 * $p2.X) + ($b3 * $p3.X)
            Y = ($b0 * $p0.Y) + ($b1 * $p1.Y) + ($b2 * $p2.Y) + ($b3 * $p3.Y)
        }
    }
    function BezierTangent([double]$t) {
        $u = 1.0 - $t
        return [pscustomobject]@{
            X = (3.0 * $u * $u * ($p1.X - $p0.X)) + (6.0 * $u * $t * ($p2.X - $p1.X)) + (3.0 * $t * $t * ($p3.X - $p2.X))
            Y = (3.0 * $u * $u * ($p1.Y - $p0.Y)) + (6.0 * $u * $t * ($p2.Y - $p1.Y)) + (3.0 * $t * $t * ($p3.Y - $p2.Y))
        }
    }

    $faultPath = New-Object System.Drawing.Drawing2D.GraphicsPath
    $faultPath.AddBezier((S $p0.X), (S $p0.Y), (S $p1.X), (S $p1.Y), (S $p2.X), (S $p2.Y), (S $p3.X), (S $p3.Y))
    $polyPath = New-Object System.Drawing.Drawing2D.GraphicsPath
    $polyPath.AddPath($faultPath, $false)
    $widePen = New-Object System.Drawing.Pen ([System.Drawing.Color]::Black), (S 10.6)
    $widePen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
    $widePen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
    $widePen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
    $polyPath.Widen($widePen)

    $polyBrush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(70, 255, 224, 90))
    $polyPen = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(255, 245, 166, 35)), (S 2.3)
    $polyPen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
    $polyPen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
    $polyPen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
    $g.FillPath($polyBrush, $polyPath)

    $beltPen = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(255, 236, 73, 62)), (S 6.2)
    $beltPen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
    $beltPen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
    $beltPen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
    $g.DrawPath($beltPen, $faultPath)

    $faultPen = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(255, 142, 20, 32)), (S 2.3)
    $faultPen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
    $faultPen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
    $faultPen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
    $g.DrawPath($faultPen, $faultPath)

    $g.DrawPath($polyPen, $polyPath)

    $dotBrush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 33, 112, 181))
    foreach ($t in @(0.08, 0.36, 0.64, 0.92)) {
        $pt = BezierPoint $t
        $tan = BezierTangent $t
        $len = [Math]::Sqrt(($tan.X * $tan.X) + ($tan.Y * $tan.Y))
        if ($len -lt 0.001) { $len = 1.0 }
        $nx = -$tan.Y / $len
        $ny = $tan.X / $len
        $side = if ($t -lt 0.5) { 1.0 } else { -1.0 }
        $dotX = S ($pt.X + $nx * 5.3 * $side)
        $dotY = S ($pt.Y + $ny * 5.3 * $side)
        $g.FillEllipse($dotBrush, $dotX - (S 1.75), $dotY - (S 1.75), (S 3.5), (S 3.5))
    }

    $g.Dispose()
    return $bmp
}

function Save-Png {
    param(
        [System.Drawing.Bitmap]$Bitmap,
        [string]$Path
    )
    $Bitmap.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
}

function Save-Ico {
    param(
        [int[]]$Sizes,
        [string]$Path
    )

    $entries = @()
    foreach ($size in $Sizes) {
        $bmp = New-FaultPolyTraceBitmap -Size $size
        $stream = New-Object System.IO.MemoryStream
        $bmp.Save($stream, [System.Drawing.Imaging.ImageFormat]::Png)
        $entries += [pscustomobject]@{
            Size = $size
            Bytes = $stream.ToArray()
        }
        $stream.Dispose()
        $bmp.Dispose()
    }

    $fs = [System.IO.File]::Create($Path)
    $bw = New-Object System.IO.BinaryWriter $fs
    $bw.Write([uint16]0)
    $bw.Write([uint16]1)
    $bw.Write([uint16]$entries.Count)

    $offset = 6 + (16 * $entries.Count)
    foreach ($entry in $entries) {
        $dim = if ($entry.Size -ge 256) { 0 } else { $entry.Size }
        $bw.Write([byte]$dim)
        $bw.Write([byte]$dim)
        $bw.Write([byte]0)
        $bw.Write([byte]0)
        $bw.Write([uint16]1)
        $bw.Write([uint16]32)
        $bw.Write([uint32]$entry.Bytes.Length)
        $bw.Write([uint32]$offset)
        $offset += $entry.Bytes.Length
    }

    foreach ($entry in $entries) {
        $bw.Write($entry.Bytes)
    }

    $bw.Dispose()
    $fs.Dispose()
}

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

$sizes = @(16, 24, 32, 48, 64, 128, 256)
foreach ($size in $sizes) {
    $bmp = New-FaultPolyTraceBitmap -Size $size
    Save-Png -Bitmap $bmp -Path (Join-Path $OutputDir "faultpolytrace_${size}.png")
    $bmp.Dispose()
}

Save-Ico -Sizes $sizes -Path (Join-Path $OutputDir "faultpolytrace.ico")

Write-Host "产品图标已生成：$OutputDir"
Write-Host "推荐在 GeoEast 菜单定制中选择：$(Join-Path $OutputDir "faultpolytrace.ico")"
