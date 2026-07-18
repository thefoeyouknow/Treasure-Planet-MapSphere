$r = Invoke-RestMethod 'https://api.github.com/repos/adafruit/Adafruit_nRF52_Bootloader/releases/latest'
$r.assets | Select-Object -ExpandProperty name > assets.txt
