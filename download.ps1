$r = Invoke-RestMethod 'https://api.github.com/repos/adafruit/Adafruit_nRF52_Bootloader/releases/latest'
foreach ($asset in $r.assets) {
    if ($asset.name -match 'update-seeed_xiao_nrf52840_sense.*\.uf2') {
        Write-Host "Downloading: " $asset.name
        Invoke-WebRequest -Uri $asset.browser_download_url -OutFile 'adafruit_bootloader.uf2'
        break
    }
}
