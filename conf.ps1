$command = $args[0]
$item = $args[1]

function Invoke-Make {
    switch ($item) {
        "dll" {  
            Set-Location "lightware"
            &"make"
            if ($LASTEXITCODE -ne 0) {
                Write-Host "make '$item' failed with $LASTEXITCODE" -ForegroundColor Red
                Set-Location "../"
                return $false
            } 

            $location = Get-Location

            $src = Join-Path -Path $location -ChildPath "lightware.dll"
            $dst = Join-Path -Path $location -ChildPath "..\"
            Copy-Item -Path $src -Destination $dst
            Remove-Item -Path $src
            
            $src = Join-Path -Path $location -ChildPath "liblightware.dll.a"
            $dst = Join-Path -Path $location -ChildPath "..\lib\"
            Copy-Item -Path $src -Destination $dst
            Remove-Item -Path $src
            
            Set-Location "../"
            Write-Host "Finished make '$item'" -ForegroundColor Green
            return $true
        }
        "editor" {  
            Set-Location "editor"
            &"make"
            if ($LASTEXITCODE -ne 0) {
                Write-Host "make '$item' failed with $LASTEXITCODE" -ForegroundColor Red
                Set-Location "../"
                return $false
            } 

            $location = Get-Location

            $src = Join-Path -Path $location -ChildPath "lightware_editor.exe"
            $dst = Join-Path -Path $location -ChildPath "..\"
            Copy-Item -Path $src -Destination $dst
            Remove-Item -Path $src
            
            Set-Location "../"
            Write-Host "Finished make '$item'" -ForegroundColor Green
            return $true
        }
        Default {
            Write-Host "Unknown item '$item' for make.\nTry 'dll' or 'editor'" -ForegroundColor Red
            return $false
        }
    }
}

function Invoke-Run {
    switch ($item) {
        "editor" {
            &".\lightware_editor.exe"

            if ($LASTEXITCODE -ne 0) {
                Write-Host "'run' '$item' failed with $LASTEXITCODE" -ForegroundColor Red
                return $false
            } 

            Write-Host "Finished run '$item'" -ForegroundColor Green
            return $true
        }
        Default {
            Write-Host "Unknown item '$item' for run.\nTry 'editor'" -ForegroundColor Red
            return $false
        }
    }
}

function Invoke-Clean {
    switch ($item) {
        "dll" {  
            Set-Location "lightware"
            &"make" clean
            if ($LASTEXITCODE -ne 0) {
                Write-Host "clean '$item' failed with $LASTEXITCODE" -ForegroundColor Red
                Set-Location "../"
                return $false
            }
            
            Set-Location "../"
            Write-Host "Finished clean '$item'" -ForegroundColor Green
            return $true
        }
        "editor" {  
            Set-Location "editor"
            &"make" clean
            if ($LASTEXITCODE -ne 0) {
                Write-Host "clean '$item' failed with $LASTEXITCODE" -ForegroundColor Red
                Set-Location "../"
                return $false
            } 

            Set-Location "../"
            Write-Host "Finished clean '$item'" -ForegroundColor Green
            return $true
        }
        Default {
            Write-Host "Unknown item '$item' for clean.\nTry 'dll' or 'editor'" -ForegroundColor Red
            return $false
        }
    }
}

switch ($command) {
    "clean" { Invoke-Clean }
    "make" { Invoke-Make }
    "run" { 
        $res = Invoke-Make
        if($res -eq $true) {
            Invoke-Run
        }
    }
    Default {
        Write-Host "Unknown command '$command'\nTry 'clean', 'make', or 'run'" -ForegroundColor Red
    }
}