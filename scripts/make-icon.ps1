# Rebuild the original Paperdesk icon. Uses only Windows' built-in drawing library.
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$root = Split-Path $PSScriptRoot -Parent
$assets = Join-Path $root 'src/assets'
New-Item -ItemType Directory -Path $assets -Force | Out-Null
$bitmap = New-Object Drawing.Bitmap 64,64
$graphics = [Drawing.Graphics]::FromImage($bitmap)
$graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::AntiAlias
$graphics.Clear([Drawing.Color]::Transparent)
$shadow = New-Object Drawing.SolidBrush ([Drawing.Color]::FromArgb(50,25,25,20))
$graphics.FillRectangle($shadow,7,9,52,50)
$paper = New-Object Drawing.Drawing2D.LinearGradientBrush ([Drawing.Point]::new(0,5)),([Drawing.Point]::new(0,57)),([Drawing.Color]::FromArgb(255,240,164)),([Drawing.Color]::FromArgb(241,214,117))
$shape = [Drawing.Point[]]@([Drawing.Point]::new(6,5),[Drawing.Point]::new(57,5),[Drawing.Point]::new(57,48),[Drawing.Point]::new(47,57),[Drawing.Point]::new(6,57))
$graphics.FillPolygon($paper,$shape)
$fold = New-Object Drawing.SolidBrush ([Drawing.Color]::FromArgb(255,247,198))
$graphics.FillPolygon($fold,[Drawing.Point[]]@([Drawing.Point]::new(47,47),[Drawing.Point]::new(57,48),[Drawing.Point]::new(47,57)))
$pen = New-Object Drawing.Pen ([Drawing.Color]::FromArgb(96,81,48)),2.8
$pen.StartCap = [Drawing.Drawing2D.LineCap]::Round
$pen.EndCap = [Drawing.Drawing2D.LineCap]::Round
$graphics.DrawLine($pen,17,23,44,21)
$graphics.DrawLine($pen,17,33,41,31)
$graphics.DrawLine($pen,17,43,32,42)
$stream = [IO.MemoryStream]::new()
$bitmap.Save($stream,[Drawing.Imaging.ImageFormat]::Png)
$payload = $stream.ToArray()
$file = [IO.File]::Create((Join-Path $assets 'paperdesk.ico'))
$writer = [IO.BinaryWriter]::new($file)
try {
    $writer.Write([uint16]0); $writer.Write([uint16]1); $writer.Write([uint16]1)
    $writer.Write([byte]64); $writer.Write([byte]64); $writer.Write([byte]0); $writer.Write([byte]0)
    $writer.Write([uint16]1); $writer.Write([uint16]32); $writer.Write([uint32]$payload.Length); $writer.Write([uint32]22)
    $writer.Write($payload)
} finally {
    $writer.Dispose(); $stream.Dispose(); $pen.Dispose(); $fold.Dispose(); $paper.Dispose(); $shadow.Dispose(); $graphics.Dispose(); $bitmap.Dispose()
}
