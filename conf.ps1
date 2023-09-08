$command = $args[0]
$item = $args[1]

function Invoke-Make {
    switch ($item) {
        "dll" {  
            Set-Location "lightware"
            &"make"
            if ($LASTEXITCODE -ne 0) {
                Write-Host "make '$item' failed with $LASTEXITCODE"
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
            Write-Host "Finished make '$item'"
            return $true
        }
        "editor" {  
            Set-Location "editor"
            &"make"
            if ($LASTEXITCODE -ne 0) {
                Write-Host "make '$item' failed with $LASTEXITCODE"
                Set-Location "../"
                return $false
            } 

            $location = Get-Location

            $src = Join-Path -Path $location -ChildPath "lightware_editor.exe"
            $dst = Join-Path -Path $location -ChildPath "..\"
            Copy-Item -Path $src -Destination $dst
            Remove-Item -Path $src
            
            Set-Location "../"
            Write-Host "Finished make '$item'"
            return $true
        }
        Default {
            Write-Host "Unknown item '$item' for make"
            return $false
        }
    }
}

function Invoke-Run {
    switch ($item) {
        "editor" {
            &".\lightware_editor.exe"

            if ($LASTEXITCODE -ne 0) {
                Write-Host "'run' '$item' failed with $LASTEXITCODE"
                return $false
            } 

            Write-Host "Finished run '$item'"
            return $true
        }
        Default {
            Write-Host "Unknown item '$item' for run"
            return $false
        }
    }
}

switch ($command) {
    "make" { Invoke-Make }
    "run" { 
        $res = Invoke-Make
        if($res -eq $true) {
            Invoke-Run
        }
    }
    Default {
        Write-Host "Unknown command '$command'"
    }
}