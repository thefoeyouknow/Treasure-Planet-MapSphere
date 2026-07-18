$r = Invoke-RestMethod 'https://api.github.com/repos/adafruit/Adafruit_nRF52_Bootloader/releases/latest'
foreach ($a in $r.assets) {
    if ($a.name -match 'seeed_xiao_nrf52840_sense_bootloader') {
        Write-Host "Found:" $a.name
        Invoke-WebRequest -Uri $a.browser_download_url -OutFile 'adafruit_bootloader.uf2'
        Write-Host "Downloaded to adafruit_bootloader.uf2"
        break
    }
}
