$gnuradio = "C:\Program Files (x86)\gnuradio"
$fosphor_base = "$($env:ChocolateyInstall)\lib\gnuradio-fosphor.1.0.0"
$bindir = "$fosphor_base\bin"
$pydir = "$fosphor_base\py-fosphor"
$blocksdir = "$fosphor_base\blocks"


Function mkdirp {
    param ( [String] $path)
    if (-not (test-path $path -pathtype container)) {
        mkdir $path
    }
}

foreach($file in (ls $bindir)) {
    cp "$($file.FullName)" "$gnuradio\bin"
}

$pyfosphor = "$gnuradio\lib\site-packages\gnuradio\fosphor\"
mkdirp $pyfosphor
foreach($file in (ls $pydir)) {
    cp "$($file.FullName)" "$pyfosphor"
}
foreach($file in (ls $blocksdir)) {
    cp "$($file.FullName)" "$gnuradio\share\gnuradio\grc\blocks"
}

