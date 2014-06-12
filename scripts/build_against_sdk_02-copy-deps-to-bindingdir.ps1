$msg_prefix='====================== '

Try{

    $deps = Get-ChildItem -Path $env:MAPNIK_LIB_DIR -Filter *.dll | % { $_.FullName }

    ##DELETE mapnik AND share DIRECTORIES, IF THEY EXIST
    ##POWERSHELL BEHAVES STRANGE IF THEY DO
    @(
        $env:N_MAPNIK_LIB_MAPNIK,
        $env:N_MAPNIK_LIB_SHARE
    ) |
    Where-Object { Test-Path $_ } |
    ForEach-Object { Remove-Item $_ -Recurse -Force -ErrorAction Stop }

	##COPY DEPENDENCIES TO BINDING DIR
	Write-Output "$msg_prefix copying dependencies to binding dir:"
	foreach($dep in $deps){
    	Write-Output $dep
        Copy-Item $dep $env:N_MAPNIK_BINDING_DIR -ErrorAction Stop
	}

	###COPY FONTS AND INPUT PLUGINS TO BINDING DIR
    $srcDir="$env:MAPNIK_LIB_DIR\mapnik"
    $destDir=$env:N_MAPNIK_LIB_MAPNIK
	Write-Output "$msg_prefix copying fonts and input plugins to binding dir:"
	Write-Output "$srcDir --> $destDir"
    Copy-Item -Path $srcDir -Destination $destDir -Force -Recurse

    ##COPY GDAL AND PROJ TO BINDING DIR
    $srcDir="$env:MAPNIK_DIR\share"
    $destDir=$env:N_MAPNIK_LIB_SHARE
	Write-Output "$msg_prefix copying gdal and proj to binding dir:"
	Write-Output "$srcDir --> $destDir"
    Copy-Item -Path $srcDir -Destination $destDir -Force -Recurse

}
Catch {
	Write-Output "`n`n$msg_prefix`n!!!!EXCEPTION!!!`n$msg_prefix`n`n"
	Write-Output "$_.Exception.Message`n"
	Exit 1
}
