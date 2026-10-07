# Lighthouse PWA S3 Deployer (AWS CLI / PowerShell)
param (
    [string]$Bucket,
    [string]$Region = "us-east-1",
    [string]$DistributionId,
    [switch]$Public
)

$ErrorActionPreference = "Stop"
$HtmlDir = "$PSScriptRoot/html"

# Auto-load .env if present
$EnvCandidates = @("$PSScriptRoot/.env", "$PSScriptRoot/../.env")
foreach ($EnvFile in $EnvCandidates) {
    if (Test-Path $EnvFile) {
        Get-Content $EnvFile | ForEach-Object {
            $line = $_.Trim()
            if ($line -and -not $line.StartsWith("#")) {
                $parts = $line -split '=', 2
                if ($parts.Count -eq 2) {
                    $key = $parts[0].Trim()
                    $val = $parts[1].Trim().Trim('"').Trim("'")
                    if (-not [System.Environment]::GetEnvironmentVariable($key)) {
                        [System.Environment]::SetEnvironmentVariable($key, $val)
                    }
                }
            }
        }
    }
}

if (-not $Bucket) { $Bucket = $env:S3_BUCKET }
if (-not $Region -and $env:AWS_REGION) { $Region = $env:AWS_REGION }
if (-not $DistributionId -and $env:CLOUDFRONT_DISTRIBUTION_ID) { $DistributionId = $env:CLOUDFRONT_DISTRIBUTION_ID }
if ($env:S3_PUBLIC -eq "true") { $Public = $true }

if (-not $Bucket -or $Bucket -eq "your-lighthouse-pwa-bucket") {
    Write-Error "Bucket name is required. Set S3_BUCKET in tools/.env or pass -Bucket <name>"
    exit 1
}

$AclArgs = @()
if ($Public) {
    $AclArgs = @("--acl", "public-read")
}

Write-Host "=== Deploying Lighthouse PWA to S3: $Bucket ===" -ForegroundColor Cyan

# Upload static assets with long cache
aws s3 sync $HtmlDir "s3://$Bucket" `
    --exclude "*" `
    --include "*.css" --include "*.mjs" --include "*.png" --include "*.svg" --include "*.webmanifest" `
    --cache-control "public, max-age=31536000, immutable" @AclArgs

# Upload index.html and sw.js with no-cache
aws s3 cp "$HtmlDir/index.html" "s3://$Bucket/index.html" `
    --content-type "text/html; charset=utf-8" `
    --cache-control "no-cache, no-store, must-revalidate" @AclArgs

aws s3 cp "$HtmlDir/sw.js" "s3://$Bucket/sw.js" `
    --content-type "text/javascript; charset=utf-8" `
    --cache-control "no-cache, no-store, must-revalidate" @AclArgs

if ($DistributionId) {
    Write-Host "Clearing CloudFront/Lightsail CDN cache (invalidation /*)..." -ForegroundColor Yellow
    aws cloudfront create-invalidation --distribution-id $DistributionId --paths "/*"
}

Write-Host "Deployment completed successfully!" -ForegroundColor Green
