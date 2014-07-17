$gnuradio = "C:\Program Files (x86)\gnuradio"
$fosphor_base = "$($env:ChocolateyInstall)\lib\gnuradio-fosphor.1.0.0"
$bindir = "$fosphor_base\bin"
$pydir = "$fosphor_base\py-fosphor"
$blocksdir = "$fosphor_base\blocks"

foreach($file in (ls $bindir)) {
    rm "$gnuradio\bin\$($file.Name)"
}

foreach($file in (ls $pydir)) {
    rm "$gnuradio\lib\site-packages\gnuradio\fosphor\$($file.Name)"
}
foreach($file in (ls $blocksdir)) {
    rm "$gnuradio\share\gnuradio\grc\blocks\$($file.Name)"
}

